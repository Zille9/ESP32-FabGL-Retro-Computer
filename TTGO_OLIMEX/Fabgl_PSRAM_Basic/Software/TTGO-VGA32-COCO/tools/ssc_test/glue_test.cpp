// Host tests for the SSC glue (src/core/ssc/ssc.cpp) built with the ported
// chips, and, with -DHAVE_VCC_SSC, a side-by-side run against VCC's original
// ssc_core.cpp (built with -Dssc=vcc_ssc so the namespaces do not clash).
//
//   glue_test                        synthetic firmware + VCC comparison
//   glue_test random                 random firmware, must not hang (smoke_ssc_core)
//   glue_test PIC.bin SPEECH.bin     real ROMs: HELLO, WAV, comparison
// ssc.cpp holds a single instance, so each mode runs in its own process.
#include "../../src/core/ssc/ssc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef HAVE_VCC_SSC
#define ssc vcc_ssc
#include "ssc_core.h"
#undef ssc
#endif

#ifndef SSC_BATCH
#define SSC_BATCH 8     // as ssc.cpp
#endif

// Scanlines from a $FF7E write to READY: the batch, plus (core-0 build) the
// samples queued ahead (SSC_AHEAD in ssc.cpp) and the batch being produced.
#ifdef SSC_CORE0
#include <thread>
#define SSC_LATENCY (48 + 2 * SSC_BATCH + 4)
#else
#define SSC_LATENCY (SSC_BATCH + 4)
#endif

// One scanline. The core-0 build waits until the engine thread has a sample
// ready, so the test runs at the engine's pace instead of racing it.
static void tick(void) {
#ifdef SSC_CORE0
    while (ssc_queued() == 0) std::this_thread::yield();
#endif
    ssc_scanline_tick();
}

static int s_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); s_fail++; } } while (0)

// sound.cpp stand-in
static int8_t s_sound_level = 0;
static int    s_sound_calls = 0;
void sound_set_ssc(int8_t level) { s_sound_level = level; s_sound_calls++; }

// Synthetic PIC firmware. Reset: ports C/D outputs, AY channel A volume 15,
// port C idle $FF, INT3 enabled, spin. INT3: read the command from port A, write it to AY register 0,
// strobe it (6 bits) into the SP0256 as an allophone, release BUSY, RETI.
static std::vector<uint8_t> synthetic_pic(void) {
    std::vector<uint8_t> r(4096, 0xff);
    const uint8_t reset[] = {
        0xA2, 0xFF, 0x09,   // MOVP %>FF,P9    port C DDR
        0xA2, 0xFF, 0x0B,   // MOVP %>FF,P11   port D DDR
        0xA2, 0x08, 0x0A,   // MOVP %>08,P10   D = AY register 8
        0xA2, 0x39, 0x08,   // MOVP %>39,P8    AY latch address
        0xA2, 0x0F, 0x0A,   // MOVP %>0F,P10   D = 15
        0xA2, 0x38, 0x08,   // MOVP %>38,P8    AY write data
        0xA2, 0xFF, 0x08,   // MOVP %>FF,P8    port C idle (BUSY high)
        0xA2, 0x10, 0x00,   // MOVP %>10,P0    IOCNT0: INT3 enable
        0x05,               // EINT
        0xE0, 0xFE,         // JMP $           spin
    };
    const uint8_t int3[] = {
        0x80, 0x04,         // MOVP P4,A       command byte, clears INT3
        0xA2, 0x00, 0x0A,   // MOVP %>00,P10   D = AY register 0
        0xA2, 0x39, 0x08,   // MOVP %>39,P8    AY latch address (BUSY low)
        0x82, 0x0A,         // MOVP A,P10      D = command
        0xA2, 0x38, 0x08,   // MOVP %>38,P8    AY write data
        0x23, 0x3F,         // AND %>3F,A
        0x82, 0x0A,         // MOVP A,P10      D = allophone
        0xA2, 0x50, 0x08,   // MOVP %>50,P8    ALD falling edge
        0xA2, 0xF0, 0x08,   // MOVP %>F0,P8    BUSY rising edge
        0x0B,               // RETI
    };
    memcpy(&r[0x000], reset, sizeof(reset));
    memcpy(&r[0x100], int3, sizeof(int3));
    r[0xFF8] = 0xF1; r[0xFF9] = 0x00;     // INT3 vector -> $F100
    r[0xFFE] = 0xF0; r[0xFFF] = 0x00;     // reset vector -> $F000
    return r;
}

static std::vector<uint8_t> random_bytes(size_t n, unsigned seed) {
    std::vector<uint8_t> r(n);
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; r[i] = (uint8_t)(seed >> 16); }
    return r;
}

static std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> r;
    FILE* f = fopen(path, "rb");
    if (!f) return r;
    int c;
    while ((c = fgetc(f)) != EOF) r.push_back((uint8_t)c);
    fclose(f);
    return r;
}

// A host program as the BASIC test does it: poll $FF7E bit 7, write the next
// byte. Records the output each tick and how many ticks each byte stayed busy.
struct Trace {
    std::vector<uint8_t> out;       // output level + 128, one per tick
    std::vector<uint8_t> quiet;     // status QUIET bit, one per tick
    std::vector<int>     busy;      // ticks from write to READY, per byte
};

template <class Core>
static Trace drive(Core& core, const std::vector<uint8_t>& bytes, int tail_ticks) {
    Trace t;
    size_t next = 0;
    int busy_for = -1;
    int idle = 0;
    for (long tick = 0; tick < 50L * 15720; tick++) {
        uint8_t st = core.read(SSC_DATA_ADDR);
        if (st & SSC_ST_READY) {
            if (busy_for >= 0) { t.busy.push_back(busy_for); busy_for = -1; }
            if (next < bytes.size()) { core.write(SSC_DATA_ADDR, bytes[next++]); busy_for = 0; }
            else if (++idle > tail_ticks) break;
        }
        core.tick();
        if (busy_for >= 0) busy_for++;
        t.out.push_back(core.sample());
        t.quiet.push_back(core.read(SSC_DATA_ADDR) & SSC_ST_QUIET);
    }
    return t;
}

struct Port {
    uint8_t read(uint16_t a)             { return ssc_read(a); }
    void    write(uint16_t a, uint8_t v) { ssc_write(a, v); }
    void    tick()                       { ::tick(); }
    uint8_t sample()                     { return (uint8_t)(ssc_level() + 128); }
};

#ifdef HAVE_VCC_SSC
struct Vcc {
    vcc_ssc::SscCore c;
    Vcc(const std::vector<uint8_t>& pic, const std::vector<uint8_t>& sp) {
        c.LoadPicRom(pic.data(), pic.size());
        c.LoadSpeechRom(sp.data(), sp.size());
        c.SetTickRateHz(15720.0);
        c.Reset();
    }
    uint8_t read(uint16_t a)             { return c.ReadPort((uint8_t)a); }
    void    write(uint16_t a, uint8_t v) { c.WritePort((uint8_t)a, v); }
    void    tick()                       { c.Tick(); }
    uint8_t sample()                     { return c.LatchedSample(); }
};

// Same audio within one LSB (rounding) and two ticks (VCC drops the cycles an
// instruction runs past a tick and keeps its accumulators in double).
static int compare_audio(const Trace& a, const Trace& b) {
    size_t n = a.out.size() < b.out.size() ? a.out.size() : b.out.size();
    int bad = 0;
    for (size_t i = 0; i < n; i++) {
        bool ok = false;
        for (int d = -2; d <= 2 && !ok; d++) {
            long j = (long)i + d;
            if (j < 0 || j >= (long)n) continue;
            int diff = (int)a.out[i] - (int)b.out[j];
            if (diff >= -1 && diff <= 1) ok = true;
        }
        if (!ok) bad++;
    }
    return bad;
}

static void compare_busy(const char* what, const Trace& a, const Trace& b) {
    CHECK(a.busy.size() == b.busy.size(), "%s: %zu bytes accepted vs VCC %zu", what, a.busy.size(), b.busy.size());
    int worst = 0;
    for (size_t i = 0; i < a.busy.size() && i < b.busy.size(); i++) {
        int d = a.busy[i] - b.busy[i];
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    printf("%s: BUSY time differs from VCC by at most %d tick(s)\n", what, worst);
    CHECK(worst <= 2, "%s: BUSY timing off by %d ticks", what, worst);

    size_t n = a.quiet.size() < b.quiet.size() ? a.quiet.size() : b.quiet.size();
    int bad = 0, active = 0;
    for (size_t i = 0; i < n; i++) {
        if (!a.quiet[i]) active++;
        bool ok = false;
        for (int d = -2; d <= 2 && !ok; d++) {
            long j = (long)i + d;
            if (j >= 0 && j < (long)n && a.quiet[i] == b.quiet[j]) ok = true;
        }
        if (!ok) bad++;
    }
    printf("%s: sound-activity bit low on %d ticks, %d differ from VCC\n", what, active, bad);
    CHECK(bad == 0, "%s: sound-activity bit differs from VCC on %d ticks", what, bad);
}
#endif

static void test_synthetic(void) {
    std::vector<uint8_t> pic = synthetic_pic();
    std::vector<uint8_t> sp  = random_bytes(2048, 4242);

    CHECK(!ssc_load_roms_from(pic.data(), 4095, sp.data(), 2048), "accepted a 4095-byte PIC ROM");
    CHECK(ssc_load_roms_from(pic.data(), 4096, sp.data(), 2048), "ROM load failed");
    CHECK(ssc_read(SSC_CTRL_ADDR) == 0xFF, "$FF7D read is not $FF");

    for (int i = 0; i < 100; i++) tick();      // let the firmware set up
    uint8_t st = ssc_read(SSC_DATA_ADDR);
    CHECK(st == (SSC_ST_READY | SSC_ST_STANDBY | SSC_ST_QUIET | 0x1f), "idle status $%02X", st);

    uint32_t ay_boot = ssc_ay_write_count();                // the firmware's own setup
    ssc_write(SSC_DATA_ADDR, 0x0F);                        // AY reg 0 = $0F, allophone 15
    CHECK(!(ssc_read(SSC_DATA_ADDR) & SSC_ST_READY), "READY did not drop after a write");
    int n = 0;
    while (!(ssc_read(SSC_DATA_ADDR) & SSC_ST_READY) && n < 1000) { tick(); n++; }
    printf("synthetic: command serviced in %d scanline(s)\n", n);
    CHECK(n > 0 && n <= SSC_LATENCY, "BUSY lasted %d scanlines", n);
    CHECK(ssc_command_count() == 1, "command count %u", ssc_command_count());
    CHECK(ssc_ay_write_count() - ay_boot == 1, "AY write count %u", ssc_ay_write_count() - ay_boot);
    CHECK(ssc_ald_speech_count() == 1, "allophone count %u", ssc_ald_speech_count());
    CHECK(!(ssc_read(SSC_DATA_ADDR) & SSC_ST_STANDBY), "SP0256 not speaking after an allophone");

    // Cartridge reset through $FF7D: falling bit 0 resets the PIC.
    ssc_write(SSC_CTRL_ADDR, 0x01);
    ssc_write(SSC_CTRL_ADDR, 0x00);
    CHECK(ssc_read(SSC_DATA_ADDR) & SSC_ST_READY, "not READY after $FF7D reset");

    // A sentence through the polling driver, and the same against VCC. The
    // counters are not cleared by a reset (as in VCC), so compare deltas.
    ssc_reset();
    for (int i = 0; i < 100; i++) tick();
    uint32_t ay0 = ssc_ay_write_count(), ald0 = ssc_ald_speech_count();
    const char* text = "HELLO I AM A COLOR COMPUTER\r";
    std::vector<uint8_t> bytes(text, text + strlen(text));
    Port port;
    Trace a = drive(port, bytes, 15720);
    CHECK(a.busy.size() == bytes.size(), "accepted %zu of %zu bytes", a.busy.size(), bytes.size());
    CHECK(ssc_ay_write_count() - ay0 == bytes.size(), "AY writes %u for %zu bytes", ssc_ay_write_count() - ay0, bytes.size());
    long nonzero = 0;
    for (size_t i = 0; i < a.out.size(); i++) if (a.out[i] != 128) nonzero++;
    long active = 0;
    for (size_t i = 0; i < a.quiet.size(); i++) if (!a.quiet[i]) active++;
    printf("synthetic: sentence %zu ticks, %ld non-silent, %ld with sound activity\n", a.out.size(), nonzero, active);
    CHECK(nonzero > 0, "the sentence made no sound");
    CHECK(active > 0, "the AY tone never set the sound-activity status");
#ifdef HAVE_VCC_SSC
    Vcc vcc(pic, sp);
    for (int i = 0; i < 100; i++) vcc.tick();
    uint32_t vay0 = vcc.c.AyWriteCount(), vald0 = vcc.c.AldSpeechCount();
    Trace b = drive(vcc, bytes, 15720);
    compare_busy("synthetic", a, b);
    CHECK(vcc.c.AyWriteCount() - vay0 == ssc_ay_write_count() - ay0, "AY writes %u vs VCC %u",
          ssc_ay_write_count() - ay0, vcc.c.AyWriteCount() - vay0);
    CHECK(vcc.c.AldSpeechCount() - vald0 == ssc_ald_speech_count() - ald0, "allophones %u vs VCC %u",
          ssc_ald_speech_count() - ald0, vcc.c.AldSpeechCount() - vald0);
    int bad = compare_audio(a, b);
    printf("synthetic: %zu ticks of audio, %d differ from VCC\n", a.out.size(), bad);
    CHECK(bad == 0, "synthetic audio differs from VCC on %d ticks", bad);
#else
    printf("synthetic: VCC comparison skipped (set VCC_SSC to VCC's ssc/ directory)\n");
#endif
}

static void write_wav(const char* path, const std::vector<uint8_t>& s) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    uint32_t sr = 15720, n = (uint32_t)s.size(), riff = 36 + n, fmt = 16, rate = sr;
    uint16_t pcm = 1, ch = 1, bits = 8, align = 1;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&rate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(s.data(), 1, n, f);
    fclose(f);
}

static void test_real_roms(const char* pic_path, const char* sp_path, const char* wav) {
    std::vector<uint8_t> pic = read_file(pic_path), sp = read_file(sp_path);
    if (pic.size() != 4096 || sp.size() != 2048) {
        printf("real ROMs: skipped (%s: %zu bytes, %s: %zu bytes)\n", pic_path, pic.size(), sp_path, sp.size());
        return;
    }
    CHECK(ssc_load_roms_from(pic.data(), 4096, sp.data(), 2048), "real ROM load failed");
    for (int i = 0; i < 15720 / 2; i++) tick();     // firmware boot
    CHECK(ssc_ay_write_count() > 0, "firmware did not program the AY at boot");
    uint32_t ald0 = ssc_ald_speech_count();

    const char* text = "HELLO\r";
    std::vector<uint8_t> bytes(text, text + strlen(text));
    Port port;
    Trace a = drive(port, bytes, 3 * 15720);
    CHECK(a.busy.size() == bytes.size(), "real ROMs: accepted %zu of %zu bytes", a.busy.size(), bytes.size());
    CHECK(ssc_ald_speech_count() > ald0, "real ROMs: no allophones spoken for HELLO");
    write_wav(wav, a.out);
    printf("real ROMs: HELLO -> %u allophones, wrote %s (%zu samples at 15720 Hz)\n",
           ssc_ald_speech_count() - ald0, wav, a.out.size());
#ifdef HAVE_VCC_SSC
    Vcc vcc(pic, sp);
    for (int i = 0; i < 15720 / 2; i++) vcc.tick();
    uint32_t vald0 = vcc.c.AldSpeechCount();
    Trace b = drive(vcc, bytes, 3 * 15720);
    compare_busy("real ROMs", a, b);
    CHECK(vcc.c.AldSpeechCount() - vald0 == ssc_ald_speech_count() - ald0, "real ROMs: allophones %u vs VCC %u",
          ssc_ald_speech_count() - ald0, vcc.c.AldSpeechCount() - vald0);
    int bad = compare_audio(a, b);
    printf("real ROMs: %zu ticks of audio, %d differ from VCC\n", a.out.size(), bad);
    // The firmware is timer-driven and VCC runs the PIC a little fast, so
    // allow drift; the allophone sequence above is the hard check.
    write_wav("vcc_hello.wav", b.out);
#endif
}

int main(int argc, char** argv) {
    if (argc == 2 && !strcmp(argv[1], "random")) {
        // smoke_ssc_core: random PIC firmware and speech ROM, three seconds,
        // ports poked as 6809 code would; must not crash or hang.
        std::vector<uint8_t> pic = random_bytes(4096, 42), sp = random_bytes(2048, 43);
        pic[0xffe] = 0xf0; pic[0xfff] = 0x00;
        CHECK(ssc_load_roms_from(pic.data(), 4096, sp.data(), 2048), "ROM load failed");
        for (int i = 0; i < 3 * 15720; i++) {
            tick();
            if (i % 977 == 0) ssc_write(SSC_DATA_ADDR, (uint8_t)i);
            if (i % 5000 == 0) (void)ssc_read(SSC_DATA_ADDR);
            if (i % 10000 == 9999) { ssc_write(SSC_CTRL_ADDR, 0x00); ssc_write(SSC_CTRL_ADDR, 0x01); }
        }
        printf("random firmware: 3 s of scanlines without a hang\n");
    } else if (argc == 3) {
        test_real_roms(argv[1], argv[2], "ssc_hello.wav");
    } else {
        test_synthetic();
    }
    printf(s_fail ? "glue: FAIL (%d)\n" : "glue: PASS\n", s_fail);
    return s_fail ? 1 : 0;
}
