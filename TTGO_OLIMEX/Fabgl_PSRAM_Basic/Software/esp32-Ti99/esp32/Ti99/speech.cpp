// Classic99 for ESP32 - TI Speech Synthesizer (TMS5200 / CD2501E + speech ROM)
// Glue derived from Classic99 console/Tiemul.cpp (rspeechbyte, wspeechbyte, SpeechUpdate, do1)
// and SpeechDll/SpeechDll.cpp (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
// The chip itself is the MAME TMS5220 core in speech/ (BSD-3-Clause, see speech/license.txt).
//
// Port notes: the chip runs in the emulation task, clocked by CPU cycles, and its 8 kHz
// samples go through a ring buffer to the sound interrupt (sound9919.cpp), which
// resamples them. Samples are queued only around speech, so the buffer drains between
// phrases and cannot drift.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include "speech.h"
#include "cpu9900.h"
#include "mame_wannabe.h"
#include "tms5220.h"

#define HALT_SPEECH   0
#define SPEECH_ROM_SIZE 0x8000
#define RING_SIZE     1024              // 8 kHz samples, power of two
#define RING_START    256               // queued before playback starts: covers a frame's burst
#define RING_TAIL     150               // chunks of silence kept flowing after speech (0.5 s)
#define OUT_LEVEL     96                // peak output, on FabGL's +/-127 scale

extern CPU9900 *pCPU;

bool speechOn = false;
int speechCycles = 0;
bool speechHalt = false;

static tms5200_device *pChip = nullptr;
static speechrom_device *pRom = nullptr;
static Byte *speechROM = nullptr;
static Byte haltByte = 0;
static volatile int tailChunks = 0;

// written by the emulation task (head) and the sound interrupt (tail)
static int16_t ring[RING_SIZE];
static volatile uint32_t ringHead = 0, ringTail = 0;
static volatile uint32_t nSamples = 0, nOverflows = 0, nUnderruns = 0, nHalts = 0;

void speechInit() {
    speechROM = (Byte *)heap_caps_calloc(1, SPEECH_ROM_SIZE, MALLOC_CAP_SPIRAM);
    if (!speechROM) return;
    FILE *fp = fopen(SPEECH_ROM_FILE, "rb");
    size_t n = fp ? fread(speechROM, 1, SPEECH_ROM_SIZE, fp) : 0;
    if (fp) fclose(fp);
    if (n == 0) {
        DBG("Missing: %s (no speech)\n", SPEECH_ROM_FILE);
        heap_caps_free(speechROM);
        speechROM = nullptr;
        return;
    }
    DBG("Loaded %s (%u bytes)\n", SPEECH_ROM_FILE, (unsigned)n);

    machine_config x;
    const int clock = 640000;           // rate for 8Khz output
    pChip = new tms5200_device(x, NULL, NULL, clock);
    pRom = new speechrom_device(x, NULL, NULL, clock);
    pRom->device_start(speechROM, SPEECH_ROM_SIZE);
    pChip->device_clock_changed();
    pChip->device_start(pRom);
    pChip->device_reset();
}

bool speechAvailable() {
    return pChip != nullptr;
}

// call with the emulation paused (menu, serial console)
void speechSetEnabled(bool on) {
    speechOn = on && speechAvailable();
    if (speechAvailable()) speechReset();
}

void speechReset() {
    if (!pChip) return;
    pChip->device_reset();
    if (pCPU) pCPU->StopHalt(HALT_SPEECH);
    speechHalt = false;
    speechCycles = 0;
    tailChunks = 0;
    ringHead = ringTail;                // drop what is queued
}

// from Classic99 rspeechbyte
Byte speechRead() {
    // don't clear interrupt, I don't think it's wired up...
    Byte ret = pChip->status_read(0);
    // speech chip, if attached, reads eat 48 additional cycles (verified hardware)
    pCPU->nCycleCount += 48;
    return ret;
}

// from Classic99 wspeechbyte
void speechWrite(Byte c) {
    if (!pChip->data_write(c)) {
        // speak external FIFO is full: halt the CPU until audio is processed
        if (!speechHalt) {
            speechHalt = true;
            haltByte = c;
            pCPU->StartHalt(HALT_SPEECH);
            ++nHalts;
        }
    } else {
        // always clear it, just to be safe
        pCPU->StopHalt(HALT_SPEECH);
        speechHalt = false;
        // speech chip, if attached, writes eat 64 additional cycles (verified hardware)
        // but we don't eat those cycles if we are halted, to allow finer grain resolution
        // of the halt...?
        pCPU->nCycleCount += 64;
    }
}

// from Classic99 do1 and SpeechUpdate
void speechService() {
    while (speechCycles >= SPEECH_CHUNK_CYCLES) {
        speechCycles -= SPEECH_CHUNK_CYCLES;

        // at 5 times per frame that's 300 updates per second, which at 8khz is 26.6 samples
        static int nCnt = 0;
        int n = 26;
        if (++nCnt > 2) nCnt = 0; else n++;     // handle 2/3

        int16_t buf[27];
        bool active = pChip->talking();
        pChip->process(buf, n);
        if (active || pChip->talking()) tailChunks = RING_TAIL;
        if (tailChunks == 0) continue;          // silent: nothing to queue
        tailChunks = tailChunks - 1;

        uint32_t head = ringHead;
        if (head - ringTail + n > RING_SIZE) {
            ++nOverflows;
            continue;
        }
        for (int i = 0; i < n; i++) ring[(head + i) & (RING_SIZE - 1)] = buf[i];
        ringHead = head + n;
        nSamples += n;
    }

    // see if we can resolve a halt condition: this unlocks it if the byte is accepted
    if (speechHalt) speechWrite(haltByte);
}

// Sound interrupt side: resample 8 kHz to the output rate, holding each sample.
// Playback of a phrase starts once RING_START samples are queued and runs until empty.
int IRAM_ATTR speechSample(int sampleRate) {
    static uint32_t phase = 0, step = 0;
    static int current = 0;
    static bool playing = false;

    uint32_t fill = ringHead - ringTail;
    if (!playing) {
        if (fill < RING_START) return 0;
        playing = true;
    }
    if (step == 0) step = ((uint32_t)SPEECHRATE << 16) / sampleRate;
    phase += step;
    while (phase >= 0x10000) {
        phase -= 0x10000;
        if (fill == 0) {
            if (tailChunks > 0) nUnderruns = nUnderruns + 1;    // ran dry while still being fed
            playing = false;
            current = 0;
            break;
        }
        current = ring[ringTail & (RING_SIZE - 1)] * OUT_LEVEL / 32768;
        ringTail = ringTail + 1;
        --fill;
    }
    return current;
}

void speechGetStats(SpeechStats *st) {
    st->samples = nSamples;
    st->overflows = nOverflows;
    st->underruns = nUnderruns;
    st->halts = nHalts;
    st->talking = pChip && pChip->talking();
    st->halted = speechHalt;
}
