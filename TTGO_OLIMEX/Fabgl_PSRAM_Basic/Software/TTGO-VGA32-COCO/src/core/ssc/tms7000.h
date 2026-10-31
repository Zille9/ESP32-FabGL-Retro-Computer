/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : tms7000.h
 *  Module : Speech/Sound Cartridge — TMS7040 ("PIC7040") microcontroller
 * ============================================================
*/

// Provenance: VCC ssc/tms7000.h (commit db8f7d7, 780TECH), a port of MAME's
// tms7000 core (license: BSD-3-Clause, copyright-holders: hap, Tim Lindner).
// The ports are wired to ssc.cpp through ssc_bus.h instead of std::function.

#ifndef SSC_TMS7000_H
#define SSC_TMS7000_H

#include <stdint.h>
#include <string.h>
#include <stddef.h>

// Forces the small helpers and the opcode handlers into the dispatch switch:
// on the Xtensa each real call costs a register-window rotation and, a few
// levels deep, a spill, which made one emulated instruction ~220 cycles.
#define SSC_TMS_INLINE inline __attribute__((always_inline))

namespace ssc {

// External interrupt line identifiers (INT2/4/5 are internally generated
// by the timer logic and are not exposed here).
enum class Tms7000Irq {
    Int1 = 0,   // wired to SP0256 DRQ in the SSC
    Int3 = 1,   // wired to "host wrote a command byte" in the SSC
};

class Tms7000 {
public:
    Tms7000();

    // --- host wiring -------------------------------------------------
    // Internal ROM image ($F000-$FFFF), 4096 bytes, before Reset(). The core
    // reads it in place and never writes it: the caller keeps it alive (on the
    // device it sits in PSRAM, so only ~200 bytes of this object are internal).
    void LoadRom(const uint8_t* rom4k);

    // --- execution -----------------------------------------------------
    void Reset();

    // Runs instructions until at least `cycles` worth of icount has been
    // consumed (icount units == input-clock cycles / 2, matching the
    // TMS7000's fixed /2 internal clock divider). Returns the number of
    // cycles actually consumed (>= cycles, since instructions are atomic).
    int Execute(int cycles);

    // External interrupt line control (level-sensitive, active-high
    // "asserted" semantics matching MAME's execute_set_input: call with
    // asserted=true to assert, false to clear).
    void SetIrqLine(Tms7000Irq line, bool asserted);

    // Debug/introspection
    uint16_t PC() const { return m_pc; }
    uint8_t  PeekReg(uint8_t r) const { return m_rf[r & 0x7f]; }
    // Count of undefined-opcode fetches since Reset() -- a real firmware
    // image running correctly should keep this at 0; a nonzero count is
    // a strong signal the CPU has run off into data or a decode bug.
    uint32_t IllegalCount() const { return m_illegal_count; }

private:
    // ---- addressable state ----
    uint8_t  m_rf[128];       // internal register file RAM ($00-$7F); R0=A, R1=B
    const uint8_t* m_rom = nullptr;  // internal ROM ($F000-$FFFF), 4096 bytes

    uint16_t m_pc = 0;
    uint32_t m_illegal_count = 0;
    uint8_t  m_sp = 0;
    uint8_t  m_sr = 0;        // status register: C N Z I . . . .
    uint8_t  m_op = 0;
    int      m_icount = 0;

    bool     m_irq_state[2] = { false, false };
    bool     m_idle_state = false;

    uint8_t  m_io_control[2] = { 0, 0 };   // IOCNT0 (only [0] used by TMS7040)

    // one hardware timer (TMS7040 family only implements timer 1)
    uint8_t  m_timer_data = 0;
    uint8_t  m_timer_control = 0;
    int      m_timer_decrementer = 0;
    int      m_timer_prescaler = 0;
    uint16_t m_timer_capture_latch = 0;
    bool     m_timer_running = false;
    long     m_timer_ticks_remaining = 0;  // icount ticks until next prescaler decrement

    uint8_t  m_port_latch[4] = { 0, 0, 0, 0 };  // A,B,C,D output latches
    uint8_t  m_port_ddr[4]   = { 0, 0, 0, 0 };  // A,B,C,D direction regs

    // ---- memory access ----
    uint8_t  Read8(uint16_t address);
    void     Write8(uint16_t address, uint8_t data);
    // Register-file access (addresses $00-$FF): $00-$7F is RAM, $80-$FF is
    // unmapped on the TMS7040 (reads 0, writes ignored) -- what Read8/Write8
    // do for those addresses, without the call. Same for the stack (m_sp is
    // 8 bits) and for opcode/operand fetches from the internal ROM.
    SSC_TMS_INLINE uint8_t ReadR8(uint8_t r) { return (r < 0x80) ? m_rf[r] : 0; }
    SSC_TMS_INLINE void    WriteR8(uint8_t r, uint8_t v) { if (r < 0x80) m_rf[r] = v; }
    uint16_t ReadR16(uint8_t r) { return (uint16_t(ReadR8(uint8_t(r - 1))) << 8) | ReadR8(r); }
    void     WriteR16(uint8_t r, uint16_t v) { WriteR8(uint8_t(r - 1), uint8_t(v >> 8)); WriteR8(r, uint8_t(v & 0xff)); }

    uint8_t  ReadP(uint8_t offset);
    void     WriteP(uint8_t offset, uint8_t data);

    SSC_TMS_INLINE uint8_t Fetch8(uint16_t a) { return (a >= 0xf000) ? m_rom[a - 0xf000] : Read8(a); }
    SSC_TMS_INLINE uint8_t Imm8() { return Fetch8(m_pc++); }
    uint16_t Imm16() { uint16_t hi = Imm8(); return (hi << 8) | Imm8(); }

    uint8_t  Pull8() { return ReadR8(m_sp--); }
    void     Push8(uint8_t data) { WriteR8(++m_sp, data); }
    uint16_t Pull16() { uint16_t lo = ReadR8(m_sp--); return lo | (uint16_t(ReadR8(m_sp--)) << 8); }
    void     Push16(uint16_t data) { WriteR8(++m_sp, data >> 8 & 0xff); WriteR8(++m_sp, data & 0xff); }

    // ---- status flags ----
    enum { SR_C = 0x80, SR_N = 0x40, SR_Z = 0x20, SR_I = 0x10 };

    // ---- interrupts ----
    void FlagExtInterrupt(int extline);
    void CheckInterrupts();
    void DoInterrupt(int irqline);

    // ---- timer ----
    void TimerRun();
    void TimerReload();
    void TimerTickLow();
    void TickTimers(int elapsed_icount);

    // ---- opcode dispatch ----
    void ExecuteOne(uint8_t op);

    typedef int (Tms7000::*OpFunc)(uint8_t, uint8_t);
    int OpClr(uint8_t, uint8_t);
    int OpDec(uint8_t, uint8_t);
    int OpInc(uint8_t, uint8_t);
    int OpInv(uint8_t, uint8_t);
    int OpRl(uint8_t, uint8_t);
    int OpRlc(uint8_t, uint8_t);
    int OpRr(uint8_t, uint8_t);
    int OpRrc(uint8_t, uint8_t);
    int OpSwap(uint8_t, uint8_t);
    int OpXchb(uint8_t, uint8_t);
    int OpAdc(uint8_t, uint8_t);
    int OpAdd(uint8_t, uint8_t);
    int OpAnd(uint8_t, uint8_t);
    int OpCmp(uint8_t, uint8_t);
    int OpDac(uint8_t, uint8_t);
    int OpDsb(uint8_t, uint8_t);
    int OpMpy(uint8_t, uint8_t);
    int OpMov(uint8_t, uint8_t);
    int OpOr(uint8_t, uint8_t);
    int OpSbb(uint8_t, uint8_t);
    int OpSub(uint8_t, uint8_t);
    int OpXor(uint8_t, uint8_t);
    int OpDjnz(uint8_t, uint8_t);
    int OpBtjo(uint8_t, uint8_t);
    int OpBtjz(uint8_t, uint8_t);

    void AmA(OpFunc op);
    void AmB(OpFunc op);
    void AmR(OpFunc op);
    void AmA2a(OpFunc op);
    void AmA2b(OpFunc op);
    void AmA2r(OpFunc op);
    void AmA2p(OpFunc op);
    void AmB2a(OpFunc op);
    void AmB2b(OpFunc op);
    void AmB2r(OpFunc op);
    void AmB2p(OpFunc op);
    void AmR2a(OpFunc op);
    void AmR2b(OpFunc op);
    void AmR2r(OpFunc op);
    void AmI2a(OpFunc op);
    void AmI2b(OpFunc op);
    void AmI2r(OpFunc op);
    void AmI2p(OpFunc op);
    void AmP2a(OpFunc op);
    void AmP2b(OpFunc op);

    void ShortBranch(bool check);
    void Jmp(bool check);
    void BrDir(); void BrInx(); void BrInd();
    void CallDir(); void CallInx(); void CallInd();
    void CmpaDir(); void CmpaInx(); void CmpaInd();
    void DecdA(); void DecdB(); void DecdR();
    void Dint(); void Eint(); void Idle();
    void LdaDir(); void LdaInx(); void LdaInd();
    void Ldsp();
    void MovdDir(); void MovdInx(); void MovdInd();
    void Nop();
    void PopA(); void PopB(); void PopR(); void PopSt();
    void PushA(); void PushB(); void PushR(); void PushSt();
    void Reti(); void Rets();
    void Setc();
    void StaDir(); void StaInx(); void StaInd();
    void Stsp();
    void Trap(uint8_t address);
    void Illegal(uint8_t op);
};

} // namespace ssc

#endif // SSC_TMS7000_H
