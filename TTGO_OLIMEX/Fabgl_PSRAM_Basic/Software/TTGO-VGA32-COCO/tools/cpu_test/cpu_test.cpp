/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : cpu_test.cpp
 *  Module : Host-side tests for the MC6809 / HD6309 CPU core
 * ============================================================
*/

// Three checks, all against a flat 64 KB RAM:
//
//   1. Vectors      — hand-written cases for the HD6309 additions, including
//                     the parts fuzzing cannot reach (interrupts, RTI, FIRQ
//                     mode, TFM resuming after an interrupt).
//   2. 6809 == 6309 — every legal MC6809 instruction must behave and time the
//                     same on the HD6309 build in emulation mode.
//   3. vs XRoar     — optional (XROAR_SRC, see run.sh): random instructions,
//                     emulation and native mode, compared with XRoar's
//                     hd6309.c for registers, memory and cycle count.
//
// Build and run with tools/cpu_test/run.sh.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <initializer_list>

#include "../../src/core/mc6809.h"
#include "../../src/core/mc6809_opcodes.h"
#ifdef HAVE_XROAR_ORACLE
#include "xroar_oracle.h"
#endif

// ============================================================
// Memory with an undo log
// ============================================================
struct Mem {
    uint8_t b[65536];
    std::vector<std::pair<uint16_t, uint8_t> > log;   // (address, previous value)

    void undo() {
        for (size_t i = log.size(); i-- > 0;) b[log[i].first] = log[i].second;
        log.clear();
    }
};

static Mem  mem_a, mem_b, mem_x;
static Mem* cur = &mem_a;

static uint8_t rd(uint16_t a) { return cur->b[a]; }
static void    wr(uint16_t a, uint8_t v) { cur->log.push_back(std::make_pair(a, cur->b[a])); cur->b[a] = v; }
#ifdef HAVE_XROAR_ORACLE
static uint8_t xrd(uint16_t a) { return mem_x.b[a]; }
static void    xwr(uint16_t a, uint8_t v) { mem_x.log.push_back(std::make_pair(a, mem_x.b[a])); mem_x.b[a] = v; }
#endif

static void cpu_new(MC6809* c, uint8_t variant) {
    mc6809_init(c);
    c->read = rd;
    c->write = wr;
    c->variant = variant;
}

// One instruction: a budget of 1 lets exactly one through. Returns its cycles.
static int step(MC6809* c, Mem* m) {
    cur = m;
    return mc6809_run_variant(c, 1);
}

// ============================================================
// 1. Vectors
// ============================================================
static int g_fail = 0, g_checks = 0;
static const char* g_name = "";

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fail++; \
    printf("  FAIL [%s] line %d: %s\n", g_name, __LINE__, #cond); } } while (0)
#define CHECK_EQ(got, want) do { g_checks++; long g_ = (long)(got), w_ = (long)(want); if (g_ != w_) { g_fail++; \
    printf("  FAIL [%s] line %d: %s = $%lX, want $%lX\n", g_name, __LINE__, #got, g_, w_); } } while (0)

static const uint16_t ORG = 0x1000;
static MC6809 t;

// Fresh HD6309 at ORG with `code` loaded; md selects emulation/native.
static void setup(const char* name, uint8_t md, std::initializer_list<uint8_t> code) {
    g_name = name;
    memset(mem_a.b, 0, sizeof(mem_a.b));
    mem_a.log.clear();
    cpu_new(&t, CPU_VARIANT_HD6309);
    t.pc = ORG;
    t.s = 0x8000;
    t.u = 0x7000;
    t.cc = 0;
    t.md = md;
    uint16_t a = ORG;
    for (uint8_t v : code) mem_a.b[a++] = v;
}
static int run1() { return step(&t, &mem_a); }
static void poke16(uint16_t a, uint16_t v) { mem_a.b[a] = v >> 8; mem_a.b[a + 1] = v & 0xFF; }
static uint16_t peek16(uint16_t a) { return (uint16_t)((mem_a.b[a] << 8) | mem_a.b[a + 1]); }

enum { C = 0x01, V = 0x02, Z = 0x04, N = 0x08, I = 0x10, H = 0x20, F = 0x40, E = 0x80 };
enum { EMU = 0, NAT = HD6309_MD_NM };

static void test_vectors() {
    // ---- registers: E, F, W, Q ----
    setup("LDE #", EMU, {0x11, 0x86, 0x80});
    CHECK_EQ(run1(), 3); CHECK_EQ(t.w, 0x8000); CHECK(t.cc & N);

    setup("LDF #", EMU, {0x11, 0xC6, 0x00});
    t.w = 0x1234;
    CHECK_EQ(run1(), 3); CHECK_EQ(t.w, 0x1200); CHECK(t.cc & Z);

    setup("LDW #", EMU, {0x10, 0x86, 0xBE, 0xEF});
    CHECK_EQ(run1(), 4); CHECK_EQ(t.w, 0xBEEF); CHECK_EQ(t.pc, ORG + 4);

    setup("LDQ #", EMU, {0xCD, 0x12, 0x34, 0x56, 0x78});
    CHECK_EQ(run1(), 5); CHECK_EQ(t.d, 0x1234); CHECK_EQ(t.w, 0x5678);

    setup("STQ <dp (emu)", EMU, {0x10, 0xDD, 0x40});
    t.d = 0xDEAD; t.w = 0xBEEF; t.dp = 0x20;
    CHECK_EQ(run1(), 8); CHECK_EQ(peek16(0x2040), 0xDEAD); CHECK_EQ(peek16(0x2042), 0xBEEF); CHECK(t.cc & N);

    setup("STQ <dp (native)", NAT, {0x10, 0xDD, 0x40});
    CHECK_EQ(run1(), 7);

    setup("LDQ ext", EMU, {0x10, 0xFC, 0x30, 0x00});
    poke16(0x3000, 0x0000); poke16(0x3002, 0x0000);
    CHECK_EQ(run1(), 9); CHECK(t.cc & Z);

    // ---- the classic 6309 detect: CLRD is illegal ($10 $4F) on a 6809 ----
    setup("CLRD", EMU, {0x10, 0x4F});
    t.d = 0xFFFF;
    CHECK_EQ(run1(), 3); CHECK_EQ(t.d, 0); CHECK(t.cc & Z); CHECK(!(t.cc & (N | V | C)));

    setup("CLRD (native)", NAT, {0x10, 0x4F});
    CHECK_EQ(run1(), 2);

    // ---- D / W inherent ----
    setup("NEGD", EMU, {0x10, 0x40});
    t.d = 0x0001;
    run1(); CHECK_EQ(t.d, 0xFFFF); CHECK(t.cc & N); CHECK(t.cc & C);

    setup("NEGD $8000 overflows", EMU, {0x10, 0x40});
    t.d = 0x8000;
    run1(); CHECK_EQ(t.d, 0x8000); CHECK(t.cc & V);

    setup("INCW wrap", EMU, {0x10, 0x5C});
    t.w = 0x7FFF;
    run1(); CHECK_EQ(t.w, 0x8000); CHECK(t.cc & V); CHECK(t.cc & N);

    setup("DECW to zero", EMU, {0x10, 0x5A});
    t.w = 0x0001;
    run1(); CHECK_EQ(t.w, 0); CHECK(t.cc & Z);

    setup("RORW", EMU, {0x10, 0x56});
    t.w = 0x0001; t.cc = C;
    run1(); CHECK_EQ(t.w, 0x8000); CHECK(t.cc & C); CHECK(t.cc & N);

    setup("NEGW does not exist", EMU, {0x10, 0x50});
    poke16(0xFFF0, 0x4321);
    run1(); CHECK_EQ(t.pc, 0x4321); CHECK(t.md & HD6309_MD_IL);

    // ---- 16-bit arithmetic ----
    setup("ADDW #", EMU, {0x10, 0x8B, 0x00, 0x01});
    t.w = 0xFFFF;
    CHECK_EQ(run1(), 5); CHECK_EQ(t.w, 0); CHECK(t.cc & Z); CHECK(t.cc & C);

    setup("CMPW ext (native)", NAT, {0x10, 0xB1, 0x30, 0x00});
    t.w = 0x1000; poke16(0x3000, 0x1000);
    CHECK_EQ(run1(), 6); CHECK(t.cc & Z); CHECK_EQ(t.w, 0x1000);

    setup("ADCD #", EMU, {0x10, 0x89, 0x00, 0x01});
    t.d = 0x00FF; t.cc = C;
    run1(); CHECK_EQ(t.d, 0x0101);

    setup("SBCD #", EMU, {0x10, 0x82, 0x00, 0x01});
    t.d = 0x0000; t.cc = C;
    run1(); CHECK_EQ(t.d, 0xFFFE); CHECK(t.cc & C); CHECK(t.cc & N);

    setup("BITD # leaves D", EMU, {0x10, 0x85, 0x0F, 0x00});
    t.d = 0xF0FF;
    run1(); CHECK_EQ(t.d, 0xF0FF); CHECK(t.cc & Z);

    // ---- register to register ----
    setup("ADDR X,Y", EMU, {0x10, 0x30, 0x12});
    t.x = 0x1000; t.y = 0x0234;
    CHECK_EQ(run1(), 4); CHECK_EQ(t.y, 0x1234); CHECK_EQ(t.x, 0x1000);

    setup("SUBR A,B", EMU, {0x10, 0x32, 0x89});
    t.d = 0x0105;
    run1(); CHECK_EQ(t.d, 0x0104);

    setup("CMPR W,D", EMU, {0x10, 0x37, 0x60});
    t.d = 0x1234; t.w = 0x1234;
    run1(); CHECK(t.cc & Z); CHECK_EQ(t.d, 0x1234);

    setup("ADDR 0,X clears nothing but flags", EMU, {0x10, 0x30, 0xC1});
    t.x = 0x8000;
    run1(); CHECK_EQ(t.x, 0x8000); CHECK(t.cc & N);

    // ---- TFR / EXG, 6309 rules ----
    setup("TFR A,X duplicates the byte", EMU, {0x1F, 0x81});
    t.d = 0x12AB;
    CHECK_EQ(run1(), 6); CHECK_EQ(t.x, 0x1212);

    setup("TFR X,A takes the high byte", EMU, {0x1F, 0x18});
    t.x = 0x12AB; t.d = 0;
    run1(); CHECK_EQ(t.d, 0x1200);

    setup("TFR X,B takes the low byte", EMU, {0x1F, 0x19});
    t.x = 0x12AB; t.d = 0;
    run1(); CHECK_EQ(t.d, 0x00AB);

    setup("TFR 0,D", EMU, {0x1F, 0xC0});
    t.d = 0xFFFF;
    run1(); CHECK_EQ(t.d, 0);

    setup("TFR D,V then EXG V,W (native)", NAT, {0x1F, 0x07, 0x1E, 0x76});
    t.d = 0xCAFE; t.w = 0x0102;
    CHECK_EQ(run1(), 4); CHECK_EQ(t.v, 0xCAFE);
    CHECK_EQ(run1(), 5); CHECK_EQ(t.w, 0xCAFE); CHECK_EQ(t.v, 0x0102);

    // ---- memory with immediate ----
    setup("OIM <dp", EMU, {0x01, 0x0F, 0x40});
    t.dp = 0x20; mem_a.b[0x2040] = 0xF0;
    CHECK_EQ(run1(), 6); CHECK_EQ(mem_a.b[0x2040], 0xFF); CHECK(t.cc & N);

    setup("AIM ext", EMU, {0x72, 0x0F, 0x30, 0x00});
    mem_a.b[0x3000] = 0xF0;
    CHECK_EQ(run1(), 7); CHECK_EQ(mem_a.b[0x3000], 0x00); CHECK(t.cc & Z);

    setup("EIM ,X", EMU, {0x65, 0xFF, 0x84});
    t.x = 0x3000; mem_a.b[0x3000] = 0x55;
    CHECK_EQ(run1(), 7); CHECK_EQ(mem_a.b[0x3000], 0xAA);

    setup("TIM does not write", EMU, {0x0B, 0x80, 0x40});
    t.dp = 0x20; mem_a.b[0x2040] = 0x7F;
    run1(); CHECK_EQ(mem_a.b[0x2040], 0x7F); CHECK(t.cc & Z); CHECK(mem_a.log.empty());

    // ---- bit operations ----
    setup("LDBT A.7 <- mem.0", EMU, {0x11, 0x36, 0x47, 0x40});   // reg A, src bit 0, dst bit 7
    t.dp = 0x20; mem_a.b[0x2040] = 0x01; t.d = 0x0000;
    CHECK_EQ(run1(), 7); CHECK_EQ(t.d, 0x8000);

    setup("STBT mem.3 <- B.1", EMU, {0x11, 0x37, 0x8B, 0x40});   // reg B, src bit 1, dst bit 3
    t.dp = 0x20; mem_a.b[0x2040] = 0x00; t.d = 0x0002;
    CHECK_EQ(run1(), 8); CHECK_EQ(mem_a.b[0x2040], 0x08);

    setup("BAND CC.0 & mem.0", EMU, {0x11, 0x30, 0x00, 0x40});
    t.dp = 0x20; mem_a.b[0x2040] = 0x00; t.cc = C;
    run1(); CHECK(!(t.cc & C));

    // ---- W addressing ----
    setup("LDA ,W", EMU, {0xA6, 0x8F});
    t.w = 0x3000; mem_a.b[0x3000] = 0x5A;
    CHECK_EQ(run1(), 4); CHECK_EQ(t.d >> 8, 0x5A);

    setup("LDA ,W++", EMU, {0xA6, 0xCF});
    t.w = 0x3000; mem_a.b[0x3000] = 0x5A;
    CHECK_EQ(run1(), 5); CHECK_EQ(t.w, 0x3002);

    setup("LDA ,--W", EMU, {0xA6, 0xEF});
    t.w = 0x3002; mem_a.b[0x3000] = 0x5A;
    run1(); CHECK_EQ(t.w, 0x3000); CHECK_EQ(t.d >> 8, 0x5A);

    setup("LDA $10,W", EMU, {0xA6, 0xAF, 0x00, 0x10});
    t.w = 0x3000; mem_a.b[0x3010] = 0x77;
    CHECK_EQ(run1(), 6); CHECK_EQ(t.d >> 8, 0x77);

    setup("LDA [,W]", EMU, {0xA6, 0x90});
    t.w = 0x3000; poke16(0x3000, 0x3100); mem_a.b[0x3100] = 0x99;
    CHECK_EQ(run1(), 7); CHECK_EQ(t.d >> 8, 0x99);

    setup("LDA E,X / F,X / W,X", EMU, {0xA6, 0x87, 0xA6, 0x8A, 0xA6, 0x8E});
    t.x = 0x3000; t.w = 0x0102; mem_a.b[0x3001] = 0x11; mem_a.b[0x3002] = 0x22; mem_a.b[0x3102] = 0x33;
    run1(); CHECK_EQ(t.d >> 8, 0x11);
    run1(); CHECK_EQ(t.d >> 8, 0x22);
    run1(); CHECK_EQ(t.d >> 8, 0x33);

    // ---- stack ----
    setup("PSHSW / PULSW", EMU, {0x10, 0x38, 0x10, 0x39});
    t.w = 0xA1B2;
    CHECK_EQ(run1(), 6); CHECK_EQ(t.s, 0x7FFE); CHECK_EQ(peek16(0x7FFE), 0xA1B2);
    t.w = 0;
    CHECK_EQ(run1(), 6); CHECK_EQ(t.w, 0xA1B2); CHECK_EQ(t.s, 0x8000);

    setup("PSHUW", EMU, {0x10, 0x3A});
    t.w = 0xA1B2;
    run1(); CHECK_EQ(t.u, 0x6FFE); CHECK_EQ(peek16(0x6FFE), 0xA1B2);

    // ---- SEXW ----
    setup("SEXW negative", EMU, {0x14});
    t.w = 0x8001;
    CHECK_EQ(run1(), 4); CHECK_EQ(t.d, 0xFFFF); CHECK(t.cc & N);

    // ---- multiply / divide ----
    setup("MULD #", EMU, {0x11, 0x8F, 0x00, 0x03});
    t.d = 0xFFFE;                                     // -2 * 3
    CHECK_EQ(run1(), 28); CHECK_EQ(t.d, 0xFFFF); CHECK_EQ(t.w, 0xFFFA); CHECK(t.cc & N);

    setup("DIVD #", EMU, {0x11, 0x8D, 0x07});
    t.d = 100;                                        // 100 / 7 = 14 r 2
    CHECK_EQ(run1(), 25); CHECK_EQ(t.d, 0x020E);

    setup("DIVD # negative dividend", EMU, {0x11, 0x8D, 0x07});
    t.d = (uint16_t)-100;                             // -14 r -2
    CHECK_EQ(run1(), 26); CHECK_EQ(t.d, 0xFEF2); CHECK(t.cc & N);

    setup("DIVD range overflow", EMU, {0x11, 0x8D, 0x01});
    t.d = 0x1234;
    CHECK_EQ(run1(), 12); CHECK(t.cc & V); CHECK_EQ(t.d, 0x1234);

    setup("DIVD by zero traps", EMU, {0x11, 0x8D, 0x00});
    t.d = 100; poke16(0xFFF0, 0x4321);
    run1();
    CHECK_EQ(t.pc, 0x4321); CHECK(t.md & HD6309_MD_D0); CHECK(!(t.md & HD6309_MD_IL));
    CHECK_EQ(t.s, 0x8000 - 12); CHECK_EQ(peek16(0x8000 - 2), ORG + 3);
    CHECK(mem_a.b[t.s] & E); CHECK(t.cc & I); CHECK(t.cc & F);

    setup("DIVQ #", EMU, {0x11, 0x8E, 0x03, 0xE8});
    t.d = 0x000F; t.w = 0x4240;                       // 1,000,000 / 1000
    CHECK_EQ(run1(), 34); CHECK_EQ(t.w, 1000); CHECK_EQ(t.d, 0);

    // ---- MD register ----
    setup("LDMD / BITMD", EMU, {0x11, 0x3D, 0xFF, 0x11, 0x3C, 0xC0, 0x11, 0x3C, 0xC0});
    t.md = HD6309_MD_IL;
    CHECK_EQ(run1(), 5); CHECK_EQ(t.md, HD6309_MD_IL | HD6309_MD_FM | HD6309_MD_NM);
    CHECK_EQ(run1(), 4); CHECK(!(t.cc & Z)); CHECK_EQ(t.md, HD6309_MD_FM | HD6309_MD_NM);   // flag read, then cleared
    run1(); CHECK(t.cc & Z);

    // ---- traps ----
    setup("illegal opcode traps (emu frame)", EMU, {0x15});
    poke16(0xFFF0, 0x4321);
    CHECK_EQ(run1(), 20);
    CHECK_EQ(t.pc, 0x4321); CHECK(t.md & HD6309_MD_IL); CHECK_EQ(t.s, 0x8000 - 12);
    CHECK_EQ(peek16(0x8000 - 2), ORG + 1);

    setup("illegal opcode traps (native frame)", NAT, {0x15});
    t.w = 0xE1F2; poke16(0xFFF0, 0x4321);
    CHECK_EQ(run1(), 22);
    CHECK_EQ(t.s, 0x8000 - 14);
    // frame: CC A B E F DP X Y U PC
    CHECK_EQ(mem_a.b[t.s + 3], 0xE1); CHECK_EQ(mem_a.b[t.s + 4], 0xF2);

    // ---- native mode timing of plain 6809 instructions ----
    setup("LDA <dp native", NAT, {0x96, 0x40});
    CHECK_EQ(run1(), 3);
    setup("LDA <dp emu", EMU, {0x96, 0x40});
    CHECK_EQ(run1(), 4);
    setup("LBEQ taken native", NAT, {0x10, 0x27, 0x00, 0x10});
    t.cc = Z;
    CHECK_EQ(run1(), 5); CHECK_EQ(t.pc, ORG + 4 + 0x10);
    setup("LBEQ taken emu", EMU, {0x10, 0x27, 0x00, 0x10});
    t.cc = Z;
    CHECK_EQ(run1(), 6);
    setup("LDA ,X+ native", NAT, {0xA6, 0x80});
    t.x = 0x3000;
    CHECK_EQ(run1(), 5);
    setup("LDA ,X+ emu", EMU, {0xA6, 0x80});
    t.x = 0x3000;
    CHECK_EQ(run1(), 6);
    setup("PSHS A,B native", NAT, {0x34, 0x06});
    CHECK_EQ(run1(), 6);

    // ---- interrupts ----
    setup("IRQ native stacks E and F; RTI restores", NAT, {0x12});
    t.d = 0xA1B2; t.w = 0xE1F2; t.x = 0x1111; t.y = 0x2222; t.dp = 0x33;
    poke16(MC6809_VEC_IRQ, 0x5000); mem_a.b[0x5000] = 0x3B;       // RTI
    mc6809_irq(&t, true);
    run1();                                                        // takes the IRQ, runs the RTI
    CHECK_EQ(t.pc, ORG); CHECK_EQ(t.s, 0x8000);
    {
        // frame as left on the stack below S
        uint16_t f = 0x8000 - 14;
        CHECK(mem_a.b[f] & E);
        CHECK_EQ(mem_a.b[f + 1], 0xA1); CHECK_EQ(mem_a.b[f + 2], 0xB2);
        CHECK_EQ(mem_a.b[f + 3], 0xE1); CHECK_EQ(mem_a.b[f + 4], 0xF2);
        CHECK_EQ(mem_a.b[f + 5], 0x33);
        CHECK_EQ(peek16(f + 12), ORG);
    }
    CHECK_EQ(t.w, 0xE1F2); CHECK_EQ(t.d, 0xA1B2);

    setup("IRQ emulation frame is 12 bytes", EMU, {0x12});
    poke16(MC6809_VEC_IRQ, 0x5000); mem_a.b[0x5000] = 0x12;
    mc6809_irq(&t, true);
    run1();
    CHECK_EQ(t.s, 0x8000 - 12); CHECK_EQ(t.pc, 0x5001);

    setup("FIRQ default frame is 3 bytes", NAT, {0x12});
    poke16(MC6809_VEC_FIRQ, 0x5000); mem_a.b[0x5000] = 0x12;
    mc6809_firq(&t, true);
    run1();
    CHECK_EQ(t.s, 0x8000 - 3); CHECK(!(mem_a.b[t.s] & E));

    setup("FIRQ with MD.FM stacks everything", NAT | HD6309_MD_FM, {0x12});
    poke16(MC6809_VEC_FIRQ, 0x5000); mem_a.b[0x5000] = 0x12;
    mc6809_firq(&t, true);
    run1();
    CHECK_EQ(t.s, 0x8000 - 14); CHECK(mem_a.b[t.s] & E); CHECK(t.cc & F); CHECK(t.cc & I);

    setup("SWI native", NAT, {0x3F});
    poke16(MC6809_VEC_SWI, 0x5000);
    CHECK_EQ(run1(), 21); CHECK_EQ(t.s, 0x8000 - 14);

    // ---- TFM ----
    setup("TFM X+,Y+", EMU, {0x11, 0x38, 0x12, 0x12});
    t.x = 0x3000; t.y = 0x3100; t.w = 4;
    memcpy(&mem_a.b[0x3000], "ABCD", 4);
    {
        int cyc = 0, passes = 0;
        while (t.pc == ORG && passes < 10) { cyc += run1(); passes++; }
        CHECK_EQ(passes, 4); CHECK_EQ(cyc, 6 + 3 * 4);
    }
    CHECK(memcmp(&mem_a.b[0x3100], "ABCD", 4) == 0);
    CHECK_EQ(t.x, 0x3004); CHECK_EQ(t.y, 0x3104); CHECK_EQ(t.w, 0); CHECK_EQ(t.pc, ORG + 3); CHECK(t.cc & Z);

    setup("TFM with W = 0 moves nothing", EMU, {0x11, 0x38, 0x12});
    t.x = 0x3000; t.y = 0x3100; t.w = 0;
    CHECK_EQ(run1(), 6); CHECK_EQ(t.pc, ORG + 3); CHECK(mem_a.log.empty()); CHECK(t.cc & Z);

    setup("TFM X+,Y fills from a buffer", EMU, {0x11, 0x3A, 0x12});
    t.x = 0x3000; t.y = 0x3100; t.w = 3;
    memcpy(&mem_a.b[0x3000], "XYZ", 3);
    while (t.pc == ORG) run1();
    CHECK_EQ(t.y, 0x3100); CHECK_EQ(t.x, 0x3003); CHECK_EQ(mem_a.b[0x3100], 'Z');

    setup("TFM X-,Y-", EMU, {0x11, 0x39, 0x12});
    t.x = 0x3003; t.y = 0x3103; t.w = 2;
    mem_a.b[0x3003] = 1; mem_a.b[0x3002] = 2;
    while (t.pc == ORG) run1();
    CHECK_EQ(mem_a.b[0x3103], 1); CHECK_EQ(mem_a.b[0x3102], 2); CHECK_EQ(t.x, 0x3001);

    setup("TFM with an illegal register traps", EMU, {0x11, 0x38, 0x16});
    poke16(0xFFF0, 0x4321); t.w = 4;
    run1(); CHECK_EQ(t.pc, 0x4321); CHECK(t.md & HD6309_MD_IL);

    setup("TFM is interruptible and resumes", EMU, {0x11, 0x38, 0x12, 0x12});
    t.x = 0x3000; t.y = 0x3100; t.w = 4;
    memcpy(&mem_a.b[0x3000], "ABCD", 4);
    poke16(MC6809_VEC_IRQ, 0x5000); mem_a.b[0x5000] = 0x3B;       // RTI
    run1(); run1();                                                // two bytes moved
    CHECK_EQ(t.w, 2); CHECK_EQ(t.pc, ORG);
    mc6809_irq(&t, true);
    run1();                                                        // IRQ entry + RTI
    mc6809_irq(&t, false);
    CHECK_EQ(t.pc, ORG); CHECK_EQ(t.w, 2); CHECK_EQ(t.s, 0x8000);
    CHECK_EQ(peek16(0x8000 - 2), ORG);                             // stacked PC was the TFM itself
    while (t.pc == ORG) run1();
    CHECK(memcmp(&mem_a.b[0x3100], "ABCD", 4) == 0); CHECK_EQ(t.w, 0);

    // ---- reset ----
    setup("reset leaves native mode, keeps V and W", NAT | HD6309_MD_FM, {0x12});
    t.v = 0x1234; t.w = 0x5678; poke16(MC6809_VEC_RESET, 0xA027);
    cur = &mem_a;
    mc6809_reset(&t);
    CHECK_EQ(t.md, 0); CHECK_EQ(t.v, 0x1234); CHECK_EQ(t.w, 0x5678); CHECK_EQ(t.pc, 0xA027);

    // ---- the MC6809 build is untouched by all this ----
    g_name = "MC6809: $10 $4F is not CLRD";
    memset(mem_a.b, 0, sizeof(mem_a.b)); mem_a.log.clear();
    {
        MC6809 c;
        cpu_new(&c, CPU_VARIANT_MC6809);
        c.pc = ORG; c.d = 0xFFFF; c.s = 0x8000;
        mem_a.b[ORG] = 0x10; mem_a.b[ORG + 1] = 0x4F;
        step(&c, &mem_a);
        CHECK_EQ(c.d, 0xFFFF); CHECK_EQ(c.pc, ORG + 2); CHECK_EQ(c.s, 0x8000);
    }
}

// ============================================================
// Random instruction generation
// ============================================================
static uint32_t rng_state = 0x6309C0C0u;
static uint32_t rnd() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

struct Regs {
    uint16_t pc, d, x, y, u, s, w, v;
    uint8_t  dp, cc, md;
};

static void regs_random(Regs* r, uint16_t pc) {
    r->pc = pc;
    r->d = rnd(); r->x = rnd(); r->y = rnd(); r->u = rnd(); r->s = rnd();
    r->w = rnd(); r->v = rnd(); r->dp = rnd(); r->cc = rnd();
    r->md = 0;
    // Small values now and then: zero flags, short TFMs, in-range divides
    if ((rnd() & 3) == 0) r->w &= 0x001F;
    if ((rnd() & 7) == 0) r->d &= 0x00FF;
    if ((rnd() & 15) == 0) r->d = 0;
}

static void regs_to_cpu(const Regs* r, MC6809* c) {
    c->pc = r->pc; c->d = r->d; c->x = r->x; c->y = r->y; c->u = r->u; c->s = r->s;
    c->w = r->w; c->v = r->v; c->dp = r->dp; c->cc = r->cc; c->md = r->md;
    c->halted = false; c->wait_for_interrupt = false; c->cwai_state = false;
    c->nmi_armed = false; c->nmi_line = false; c->nmi_pending = false;
    c->firq_pending = false; c->irq_pending = false; c->tfm_busy = false;
}

static void regs_from_cpu(Regs* r, const MC6809* c) {
    r->pc = c->pc; r->d = c->d; r->x = c->x; r->y = c->y; r->u = c->u; r->s = c->s;
    r->w = c->w; r->v = c->v; r->dp = c->dp; r->cc = c->cc; r->md = c->md;
}

static bool regs_equal(const Regs* a, const Regs* b, bool with_6309) {
    if (a->pc != b->pc || a->d != b->d || a->x != b->x || a->y != b->y ||
        a->u != b->u || a->s != b->s || a->dp != b->dp || a->cc != b->cc) return false;
    if (with_6309 && (a->w != b->w || a->v != b->v || a->md != b->md)) return false;
    return true;
}

// Every address either side wrote must hold the same value on both.
static bool writes_equal(const Mem* a, const Mem* b) {
    for (size_t i = 0; i < a->log.size(); i++) { uint16_t ad = a->log[i].first; if (a->b[ad] != b->b[ad]) return false; }
    for (size_t i = 0; i < b->log.size(); i++) { uint16_t ad = b->log[i].first; if (a->b[ad] != b->b[ad]) return false; }
    return true;
}

static bool wrote_near(const Mem* m, uint16_t pc) {
    for (size_t i = 0; i < m->log.size(); i++) if ((uint16_t)(m->log[i].first - pc) < 8) return true;
    return false;
}

static void print_regs(const char* who, const Regs* r, int cyc) {
    printf("    %-6s PC=%04X D=%04X X=%04X Y=%04X U=%04X S=%04X W=%04X V=%04X DP=%02X CC=%02X MD=%02X cyc=%d\n",
           who, r->pc, r->d, r->x, r->y, r->u, r->s, r->w, r->v, r->dp, r->cc, r->md, cyc);
}

static const uint16_t FUZZ_PC = 0x4000;

static void put_code(const uint8_t* code, int n) {
    for (int i = 0; i < n; i++) {
        mem_a.b[FUZZ_PC + i] = code[i];
        mem_b.b[FUZZ_PC + i] = code[i];
        mem_x.b[FUZZ_PC + i] = code[i];
    }
}

// Indexed postbyte follows these opcodes (any page).
static bool has_postbyte_p1(uint8_t op) {
    return (op >= 0x60 && op <= 0x6F) || (op >= 0xA0 && op <= 0xAF) ||
           (op >= 0xE0 && op <= 0xEF) || (op >= 0x30 && op <= 0x33);
}

// ============================================================
// 2. MC6809 == HD6309 (emulation mode) for legal 6809 code
// ============================================================
static bool legal_6809_p2(uint8_t op) {
    if (op >= 0x21 && op <= 0x2F) return true;
    switch (op) {
        case 0x3F: case 0x83: case 0x93: case 0xA3: case 0xB3:
        case 0x8C: case 0x9C: case 0xAC: case 0xBC:
        case 0x8E: case 0x9E: case 0xAE: case 0xBE:
        case 0x9F: case 0xAF: case 0xBF:
        case 0xCE: case 0xDE: case 0xEE: case 0xFE:
        case 0xDF: case 0xEF: case 0xFF: return true;
    }
    return false;
}
static bool legal_6809_p3(uint8_t op) {
    switch (op) {
        case 0x3F: case 0x83: case 0x93: case 0xA3: case 0xB3:
        case 0x8C: case 0x9C: case 0xAC: case 0xBC: return true;
    }
    return false;
}
// Postbytes the MC6809 defines (the rest mean something else on a 6309).
static bool legal_6809_index(uint8_t pb) {
    if (!(pb & 0x80)) return true;
    uint8_t mode = pb & 0x0F;
    bool ind = pb & 0x10;
    switch (mode) {
        case 0x0: case 0x2: return !ind;
        case 0x1: case 0x3: case 0x4: case 0x5: case 0x6:
        case 0x8: case 0x9: case 0xB: case 0xC: case 0xD: return true;
        case 0xF: return ind && (pb & 0x60) == 0;      // only $9F
    }
    return false;
}
static bool legal_6809_tfr(uint8_t pb) {
    uint8_t s = pb >> 4, d = pb & 0x0F;
    bool s16 = s <= 5, d16 = d <= 5, s8 = s >= 8 && s <= 0xB, d8 = d >= 8 && d <= 0xB;
    return (s16 && d16) || (s8 && d8);
}

static bool gen_legal_6809(uint8_t* code) {
    for (int i = 0; i < 6; i++) code[i] = rnd();
    uint8_t op = code[0];
    if (op == 0x10) {
        if (!legal_6809_p2(code[1])) return false;
        if ((code[1] & 0xF0) == 0xA0 || (code[1] & 0xF0) == 0xE0) return legal_6809_index(code[2]);
        return true;
    }
    if (op == 0x11) {
        if (!legal_6809_p3(code[1])) return false;
        if ((code[1] & 0xF0) == 0xA0) return legal_6809_index(code[2]);
        return true;
    }
    if (mc6809_cycles_page1[op] == 0) return false;
    if (op == 0x13 || op == 0x3C) return false;       // SYNC, CWAI: wait states
    if (op == 0x1E || op == 0x1F) return legal_6809_tfr(code[1]);
    if (has_postbyte_p1(op)) return legal_6809_index(code[1]);
    return true;
}

static int test_6809_equals_6309(int iterations) {
    MC6809 c9, c3;
    cpu_new(&c9, CPU_VARIANT_MC6809);
    cpu_new(&c3, CPU_VARIANT_HD6309);
    int bad = 0, done = 0;
    while (done < iterations) {
        uint8_t code[6];
        if (!gen_legal_6809(code)) continue;
        done++;
        put_code(code, 6);
        Regs r0, r9, r3;
        regs_random(&r0, FUZZ_PC);
        regs_to_cpu(&r0, &c9);
        regs_to_cpu(&r0, &c3);
        int cyc9 = step(&c9, &mem_a);
        int cyc3 = step(&c3, &mem_b);
        regs_from_cpu(&r9, &c9);
        regs_from_cpu(&r3, &c3);
        // The 6309 build arms NMI on TFR/EXG into S (as XRoar does); the
        // 6809 build never has, so that flag is not compared for those two.
        bool tfr = code[0] == 0x1E || code[0] == 0x1F;
        bool ok = regs_equal(&r9, &r3, false) && cyc9 == cyc3 && writes_equal(&mem_a, &mem_b) &&
                  (tfr || c9.nmi_armed == c3.nmi_armed) && r3.w == r0.w && r3.v == r0.v && r3.md == 0;
        if (!ok && bad++ < 10) {
            printf("  MISMATCH code %02X %02X %02X %02X %02X\n", code[0], code[1], code[2], code[3], code[4]);
            print_regs("before", &r0, 0);
            print_regs("6809", &r9, cyc9);
            print_regs("6309", &r3, cyc3);
        }
        mem_a.undo();
        mem_b.undo();
    }
    return bad;
}

// ============================================================
// 3. HD6309 vs XRoar
// ============================================================
#ifdef HAVE_XROAR_ORACLE

// Cases left out of the comparison, each for a stated reason.
static bool xroar_skip(const uint8_t* code) {
    int i = 0;
    uint8_t page = 0;
    if (code[0] == 0x10 || code[0] == 0x11) {
        page = code[0];
        i = 1;
        while (i < 4 && (code[i] == 0x10 || code[i] == 0x11)) i++;
    }
    uint8_t op = code[i];
    bool indexed;
    if (page == 0) {
        if (op == 0x13 || op == 0x3C) return true;                    // SYNC, CWAI: wait states
        indexed = has_postbyte_p1(op);
    } else {
        indexed = (op & 0xF0) == 0xA0 || (op & 0xF0) == 0xE0 || (op & 0xF0) == 0x60;
        // The M latch is not modelled: it is the low byte when DP feeds a
        // 16-bit register-to-register operation.
        if (page == 0x10 && op >= 0x30 && op <= 0x37 && (code[i + 1] >> 4) == 0xB && !(code[i + 1] & 0x08)) return true;
    }
    if (indexed) {
        // OIM/AIM/EIM/TIM carry their immediate before the postbyte
        uint8_t lo = op & 0x0F;
        bool imm_first = page == 0 && (op & 0xF0) == 0x60 && (lo == 1 || lo == 2 || lo == 5 || lo == 0xB);
        uint8_t pb = code[i + (imm_first ? 2 : 1)];
        // [,-R] is undefined on both CPUs and the two models disagree on it.
        if ((pb & 0x9F) == 0x92) return true;
    }
    return false;
}

static bool is_tfm(const uint8_t* code) {
    int i = 0;
    while (i < 4 && (code[i] == 0x10 || code[i] == 0x11)) i++;
    return code[0] == 0x11 && code[i] >= 0x38 && code[i] <= 0x3B;
}

struct CycDiff { int count; int ours, theirs; uint8_t ex[5]; };

// Opcode after any prefix bytes; *page gets $00, $10 or $11.
static uint8_t decode_op(const uint8_t* code, uint8_t* page) {
    int i = 0;
    *page = 0;
    if (code[0] == 0x10 || code[0] == 0x11) {
        *page = code[0];
        while (i < 4 && (code[i] == 0x10 || code[i] == 0x11)) i++;
    }
    return code[i];
}

// Cycle counts where we knowingly differ from XRoar (see hd6309_ops.h).
static bool known_cycle_difference(const uint8_t* code, int delta) {
    uint8_t page;
    uint8_t op = decode_op(code, &page);
    uint8_t lo = op & 0x0F, hi = op >> 4;
    // TSTA/TSTB/TSTE/TSTF: documented 2/1 and 3/2; XRoar charges one more.
    if (delta == -1 && (op == 0x4D || op == 0x5D) && page != 0x10) return true;
    // OIM/AIM/EIM/TIM: documented counts; XRoar marks its own as unverified.
    if (delta == +1 && page == 0 && (hi == 0x0 || hi == 0x6 || hi == 0x7) &&
        (lo == 0x1 || lo == 0x2 || lo == 0x5 || lo == 0xB)) return true;
    return false;
}

static int test_vs_xroar(int iterations) {
    MC6809 c3;
    cpu_new(&c3, CPU_VARIANT_HD6309);
    xo_init(xrd, xwr);

    int bad = 0, done = 0, cyc_bad = 0, cyc_known = 0;
    std::map<std::string, CycDiff> cyc_diffs;
    std::map<std::string, int> bad_kinds;

    while (done < iterations) {
        uint8_t code[6];
        for (int i = 0; i < 6; i++) code[i] = rnd();
        // Favour the prefixed pages: they hold most of the 6309 additions.
        uint32_t pick = rnd() % 10;
        if (pick < 3) code[0] = 0x10;
        else if (pick < 6) code[0] = 0x11;
        if (xroar_skip(code)) continue;
        done++;
        put_code(code, 6);

        Regs r0, r3, rx;
        regs_random(&r0, FUZZ_PC);
        r0.md = rnd() & (HD6309_MD_NM | HD6309_MD_FM | HD6309_MD_IL | HD6309_MD_D0);
        regs_to_cpu(&r0, &c3);

        int cyc3 = step(&c3, &mem_a);
        while (c3.tfm_busy && c3.pc == c3.tfm_pc) cyc3 += step(&c3, &mem_a);   // XRoar runs a whole TFM per step
        regs_from_cpu(&r3, &c3);

        xo_regs xr;
        xr.pc = r0.pc; xr.d = r0.d; xr.x = r0.x; xr.y = r0.y; xr.u = r0.u; xr.s = r0.s;
        xr.w = r0.w; xr.v = r0.v; xr.dp = r0.dp; xr.cc = r0.cc; xr.md = r0.md;
        xo_set(&xr);
        int cycx = xo_step();
        xo_get(&xr);
        rx.pc = xr.pc; rx.d = xr.d; rx.x = xr.x; rx.y = xr.y; rx.u = xr.u; rx.s = xr.s;
        rx.w = xr.w; rx.v = xr.v; rx.dp = xr.dp; rx.cc = xr.cc; rx.md = xr.md;

        // An instruction that overwrote itself is fetched at different times
        // by the two models; not a meaningful comparison.
        bool self_mod = wrote_near(&mem_a, FUZZ_PC) || wrote_near(&mem_x, FUZZ_PC);
        if (!self_mod) {
            // A TFM naming an illegal register traps on both, but only we
            // raise MD.IL for it.
            if (c3.tfm_busy == false && (r3.md ^ rx.md) == HD6309_MD_IL && is_tfm(code)) rx.md |= HD6309_MD_IL;
            // DAA and SEX are shared with the MC6809 build, which clears V
            // where XRoar leaves it alone (SEX) or derives it (DAA). Left
            // as it is: changing it would change the 6809 too.
            if (code[0] == 0x19 || code[0] == 0x1D) { r3.cc &= ~V; rx.cc &= ~V; }

            uint8_t page;
            uint8_t op = decode_op(code, &page);
            char key[64];

            // MULD with a zero high word: we set Z (as LDQ does); XRoar sets
            // N, which looks like a slip in its source.
            if (page == 0x11 && (op & 0xCF) == 0x8F && r3.d == 0 && rx.d == 0) {
                r3.cc &= ~(N | Z);
                rx.cc &= ~(N | Z);
            }
            bool ok = regs_equal(&r3, &rx, true) && writes_equal(&mem_a, &mem_x);
            if (!ok) {
                snprintf(key, sizeof(key), "%s %02X %02X", (r0.md & 1) ? "nat" : "emu", page, op);
                bad_kinds[key]++;
                if (bad++ < 12) {
                    printf("  MISMATCH code %02X %02X %02X %02X %02X\n", code[0], code[1], code[2], code[3], code[4]);
                    print_regs("before", &r0, 0);
                    print_regs("ours", &r3, cyc3);
                    print_regs("xroar", &rx, cycx);
                }
            }
            if (ok && cyc3 != cycx) {
                if (known_cycle_difference(code, cyc3 - cycx)) {
                    cyc_known++;
                } else {
                    cyc_bad++;
                    snprintf(key, sizeof(key), "%s %02X %02X d=%+d", (r0.md & 1) ? "nat" : "emu", page, op, cyc3 - cycx);
                    CycDiff& d = cyc_diffs[key];
                    if (d.count++ == 0) { d.ours = cyc3; d.theirs = cycx; memcpy(d.ex, code, 5); }
                }
            }
        }
        mem_a.undo();
        mem_x.undo();
    }

    if (!bad_kinds.empty()) {
        printf("  state mismatches by mode / page / opcode:\n");
        for (std::map<std::string, int>::iterator it = bad_kinds.begin(); it != bad_kinds.end(); ++it)
            printf("    %-14s x%d\n", it->first.c_str(), it->second);
    }
    if (!cyc_diffs.empty()) {
        printf("  unexpected cycle differences vs XRoar (%d cases, %zu kinds):\n", cyc_bad, cyc_diffs.size());
        for (std::map<std::string, CycDiff>::iterator it = cyc_diffs.begin(); it != cyc_diffs.end(); ++it) {
            const CycDiff& d = it->second;
            printf("    %-20s x%-6d ours=%d xroar=%d  e.g. %02X %02X %02X %02X\n", it->first.c_str(), d.count,
                   d.ours, d.theirs, d.ex[0], d.ex[1], d.ex[2], d.ex[3]);
        }
    }
    printf("   cycle counts: %d known differences (TST r, OIM/AIM/EIM/TIM), %d unexpected\n", cyc_known, cyc_bad);
    return bad + cyc_bad;
}
#endif

// ============================================================
int main(int argc, char** argv) {
    int iterations = (argc > 1) ? atoi(argv[1]) : 400000;
    int failed = 0;

    printf("1. HD6309 vectors\n");
    test_vectors();
    printf("   %d checks, %d failed\n", g_checks, g_fail);
    failed += g_fail;

    // Shared random background for the fuzzers
    for (int i = 0; i < 65536; i++) mem_a.b[i] = mem_b.b[i] = mem_x.b[i] = rnd();
    mem_a.log.clear();

    printf("2. MC6809 == HD6309 (emulation mode), %d legal 6809 instructions\n", iterations);
    int bad = test_6809_equals_6309(iterations);
    printf("   %d mismatches\n", bad);
    failed += bad;

#ifdef HAVE_XROAR_ORACLE
    printf("3. HD6309 vs XRoar hd6309.c, %d random instructions\n", iterations);
    bad = test_vs_xroar(iterations);
    printf("   %d failures\n", bad);
    failed += bad;
#else
    printf("3. HD6309 vs XRoar: skipped (set XROAR_SRC to enable)\n");
#endif

    printf(failed ? "FAILED\n" : "PASSED\n");
    return failed ? 1 : 0;
}
