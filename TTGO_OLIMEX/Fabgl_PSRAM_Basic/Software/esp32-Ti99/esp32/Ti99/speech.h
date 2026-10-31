// Classic99 for ESP32 - TI Speech Synthesizer (TMS5200 / CD2501E + speech ROM)
// Glue derived from Classic99 console/Tiemul.cpp (rspeechbyte, wspeechbyte, SpeechUpdate, do1)
// and SpeechDll/SpeechDll.cpp (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
// The chip itself is the MAME TMS5220 core in speech/ (BSD-3-Clause, see speech/license.txt).

#pragma once
#include "ti_types.h"

#define SPEECH_ROM_FILE     TI_ROM_DIR "/SPCHROM.BIN"
#define SPEECHRATE          8000
#define SPEECH_CHUNK_CYCLES (DEFAULT_60HZ_CPF / 5)     // chip is run 5 times per frame, as in Classic99

struct SpeechStats { uint32_t samples, overflows, underruns, halts; bool talking, halted; };

void speechInit();                  // loads the speech ROM (optional file) and creates the chip
bool speechAvailable();             // speech ROM found
void speechSetEnabled(bool on);     // Setup > Speech; stays off without the ROM
void speechReset();
Byte speechRead();                  // CPU read of >9000
void speechWrite(Byte c);           // CPU write to >9400
void speechService();               // run the chip for the elapsed cycles, retry a held byte
int  speechSample(int sampleRate);  // next output sample (+/-96) for the sound interrupt
void speechGetStats(SpeechStats *st);

extern bool speechOn;               // synthesiser attached
extern int speechCycles;
extern bool speechHalt;             // CPU halted on a full FIFO

static inline bool speechEnabled() { return speechOn; }

// once per instruction with its cycle count (only when enabled)
static inline void speechAdvance(int cycles) {
    speechCycles += cycles;
    if (speechCycles >= SPEECH_CHUNK_CYCLES || speechHalt) speechService();
}
