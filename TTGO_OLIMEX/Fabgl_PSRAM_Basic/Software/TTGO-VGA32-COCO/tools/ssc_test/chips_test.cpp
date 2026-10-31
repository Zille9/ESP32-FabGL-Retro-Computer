// Host tests for the ported SSC chip cores, one chip at a time. Adapted from
// VCC's ssc/test/smoke_cpu.cpp, smoke_sp0256.cpp and smoke_ay8913.cpp. The
// chips' bus lines (ssc_bus.h) go to the stubs below instead of ssc.cpp.
#include "../../src/core/ssc/tms7000.h"
#include "../../src/core/ssc/sp0256.h"
#include "../../src/core/ssc/ay8913.h"
#include "../../src/core/ssc/ssc_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>

#ifdef HAVE_VCC_SSC
#define ssc vcc_ssc
#include "tms7000.h"
#undef ssc
#endif

using namespace ssc;

static int s_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); s_fail++; } } while (0)

static bool s_drq_seen = false;
uint8_t ssc_bus_porta_read(void)        { return 0xff; }
void    ssc_bus_portb_write(uint8_t)    {}
uint8_t ssc_bus_portc_read(void)        { return 0xff; }
void    ssc_bus_portc_write(uint8_t)    {}
uint8_t ssc_bus_portd_read(void)        { return 0xff; }
void    ssc_bus_portd_write(uint8_t)    {}
void    ssc_bus_speech_drq(bool ready)  { if (ready) s_drq_seen = true; }

// smoke_cpu: INC A; JMP back, from the reset vector.
static void test_cpu(void) {
    uint8_t rom[4096];
    for (int i = 0; i < 4096; i++) rom[i] = 0xff;
    rom[0x000] = 0xB3;              // F000: INC A
    rom[0x001] = 0xE0;              // F001: JMP -3
    rom[0x002] = (uint8_t)-3;
    rom[0xFFE] = 0xF0;              // reset vector -> $F000
    rom[0xFFF] = 0x00;

    static Tms7000 cpu;
    cpu.LoadRom(rom);
    cpu.Reset();
    CHECK(cpu.PC() == 0xF000, "TMS7000 reset vector, PC=$%04X", cpu.PC());

    // One loop = INC A (5) + JMP taken (7) = 12 icount; run 10 loops exactly.
    int used = cpu.Execute(12 * 10);
    CHECK(used == 120, "TMS7000 cycle count %d (expected 120)", used);
    uint8_t a0 = cpu.PeekReg(0);
    cpu.Execute(12 * 300);
    CHECK((uint8_t)(cpu.PeekReg(0) - a0) == (uint8_t)300, "TMS7000 INC A count, A=$%02X", cpu.PeekReg(0));
    CHECK(cpu.PC() >= 0xF000 && cpu.PC() <= 0xF003, "TMS7000 PC left the loop: $%04X", cpu.PC());
    CHECK(cpu.IllegalCount() == 0, "TMS7000 illegal opcodes: %u", cpu.IllegalCount());
    printf("TMS7000: A=$%02X PC=$%04X\n", cpu.PeekReg(0), cpu.PC());
}

// smoke_sp0256: random ROM, long run, ALD wakes the chip, ROM window.
static void test_sp0256(void) {
    std::vector<uint8_t> rom(0x800);
    unsigned seed = 12345;
    for (size_t i = 0; i < rom.size(); i++) { seed = seed * 1103515245u + 12345u; rom[i] = (uint8_t)(seed >> 16); }

    static Sp0256 chip;
    chip.LoadRom(rom.data(), rom.size());
    chip.Reset();
    CHECK(s_drq_seen, "SP0256 reset did not raise DRQ");
    CHECK(chip.Standby(), "SP0256 not on standby after reset");
    chip.Ald(0x00);
    CHECK(!chip.Standby(), "SP0256 ALD did not leave standby");

    std::vector<int16_t> buf(4096);
    long total = 0, nonzero = 0;
    for (int block = 0; block < 500; block++) {
        chip.GenerateSamples(buf.data(), (int)buf.size());
        for (size_t i = 0; i < buf.size(); i++) if (buf[i]) nonzero++;
        total += (long)buf.size();
        if (block % 37 == 0) chip.Ald((uint8_t)(block & 0x3f));
    }
    CHECK(nonzero > 0, "SP0256 produced only silence");
    printf("SP0256: %ld samples, %ld non-zero, standby=%d\n", total, nonzero, chip.Standby());
}

// smoke_ay8913: tone frequency f = clock / (16 * TP), silence and mixer-off.
static const uint32_t AY_CLOCK = 1789772;

static int16_t render_at(Ay8913& ay, uint32_t rate, uint64_t* acc) {
    *acc += AY_CLOCK;
    int steps = (int)(*acc / (8ull * rate));
    *acc -= (uint64_t)steps * 8ull * rate;
    return ay.Render(steps);
}

static void test_ay8913(void) {
    int periods[] = { 100, 255, 512, 1000 };
    for (int k = 0; k < 4; k++) {
        int tp = periods[k];
        Ay8913 ay;
        ay.SelectRegister(0); ay.WriteData((uint8_t)(tp & 0xff));
        ay.SelectRegister(1); ay.WriteData((uint8_t)((tp >> 8) & 0x0f));
        ay.SelectRegister(8); ay.WriteData(0x0f);
        ay.SelectRegister(7); ay.WriteData(0x3e);   // tone A only
        double expected = AY_CLOCK / (16.0 * tp);
        uint32_t rate = 50000;
        if (expected * 40 > rate) rate = (uint32_t)(expected * 40);
        int n = (int)(rate / 5);
        uint64_t acc = 0;
        int16_t prev = render_at(ay, rate, &acc);
        int transitions = 0;
        for (int i = 1; i < n; i++) {
            int16_t s = render_at(ay, rate, &acc);
            if (s != prev) transitions++;
            prev = s;
        }
        double measured = (transitions / 2.0) / (n / (double)rate);
        double err = fabs(measured - expected) / expected * 100.0;
        printf("AY: TP=%4d expected=%8.2f Hz measured=%8.2f Hz err=%.2f%%\n", tp, expected, measured, err);
        CHECK(err <= 2.0, "AY tone TP=%d off by %.2f%%", tp, err);
    }
    {
        Ay8913 ay;
        ay.SelectRegister(0); ay.WriteData(50);
        ay.SelectRegister(8); ay.WriteData(0x00);
        ay.SelectRegister(7); ay.WriteData(0xff);
        bool silent = true;
        for (int i = 0; i < 1000; i++) if (ay.Render(4) != 0) silent = false;
        CHECK(silent, "AY amplitude 0 is not silent");
    }
    {
        Ay8913 ay;
        ay.SelectRegister(0); ay.WriteData(50);
        ay.SelectRegister(8); ay.WriteData(0x0f);
        ay.SelectRegister(7); ay.WriteData(0xff);
        int16_t first = ay.Render(4);
        bool constant = true;
        for (int i = 0; i < 1000; i++) if (ay.Render(4) != first) constant = false;
        CHECK(constant && first != 0, "AY mixer-off level not a constant non-zero");
    }
}

#ifdef HAVE_VCC_SSC
// Random programs on the port and on VCC's original TMS7000, in slices of one
// scanline's cycles with the interrupt lines toggled now and then: PC, the
// register file, the cycles used and the illegal-opcode count must match.
static void test_cpu_vs_vcc(void) {
    int checked = 0, first_bad = -1;
    for (unsigned seed = 1; seed <= 200 && first_bad < 0; seed++) {
        std::vector<uint8_t> rom(4096);
        unsigned x = seed;
        for (size_t i = 0; i < rom.size(); i++) { x = x * 1103515245u + 12345u; rom[i] = (uint8_t)(x >> 16); }
        rom[0xffe] = 0xf0; rom[0xfff] = 0x00;

        static Tms7000 a;
        static vcc_ssc::Tms7000 b;
        b.InPortA = []() { return (uint8_t)0xff; };
        b.InPortC = []() { return (uint8_t)0xff; };
        b.InPortD = []() { return (uint8_t)0xff; };
        b.OutPortB = b.OutPortC = b.OutPortD = [](uint8_t) {};
        a.LoadRom(rom.data()); a.Reset();
        b.LoadRom(rom.data(), rom.size()); b.Reset();
        for (int slice = 0; slice < 2000; slice++) {
            if (slice % 97 == 5)  { a.SetIrqLine(Tms7000Irq::Int3, true);  b.SetIrqLine(vcc_ssc::Tms7000Irq::Int3, true); }
            if (slice % 97 == 40) { a.SetIrqLine(Tms7000Irq::Int3, false); b.SetIrqLine(vcc_ssc::Tms7000Irq::Int3, false); }
            if (slice % 61 == 7)  { a.SetIrqLine(Tms7000Irq::Int1, (slice & 1) != 0); b.SetIrqLine(vcc_ssc::Tms7000Irq::Int1, (slice & 1) != 0); }
            int ua = a.Execute(57), ub = b.Execute(57);
            bool same = ua == ub && a.PC() == b.PC() && a.IllegalCount() == b.IllegalCount();
            for (int r = 0; r < 128 && same; r++) same = a.PeekReg((uint8_t)r) == b.PeekReg((uint8_t)r);
            checked++;
            if (!same) {
                printf("FAIL: TMS7000 differs from VCC: seed %u slice %d PC $%04X/$%04X cycles %d/%d\n",
                       seed, slice, a.PC(), b.PC(), ua, ub);
                s_fail++; first_bad = slice;
                break;
            }
        }
    }
    printf("TMS7000 vs VCC: %d slices of random code compared\n", checked);
}
#endif

int main(void) {
    test_cpu();
#ifdef HAVE_VCC_SSC
    test_cpu_vs_vcc();
#endif
    test_sp0256();
    test_ay8913();
    printf(s_fail ? "chips: FAIL (%d)\n" : "chips: PASS\n", s_fail);
    return s_fail ? 1 : 0;
}
