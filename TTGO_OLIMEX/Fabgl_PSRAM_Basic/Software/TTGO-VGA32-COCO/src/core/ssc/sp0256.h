/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sp0256.h
 *  Module : Speech/Sound Cartridge — SP0256-AL2 speech processor
 * ============================================================
*/

// Provenance: VCC ssc/sp0256.h (commit db8f7d7, 780TECH). VCC's notes:
//
// Standalone, portable emulation of the General Instrument SP0256-AL2
// Narrator Speech Processor, adapted for use inside a Tandy Speech/Sound
// Cartridge (SSC) emulation for VCC.
//
// This is a from-source port of the LPC-12 filter, microsequencer and
// allophone data-format tables from MAME's src/devices/sound/sp0256.cpp /
// sp0256.h (license: BSD-3-Clause, copyright Joseph Zbiciak, Tim Lindner,
// originally written by Joe Zbiciak). The MAME device_sound_interface /
// sound_stream / device_t framework has been replaced with a simple
// pull-based sample generator so the core can run standalone inside a
// Windows DLL. The DSP algorithm, quantization table, opcode data-format
// tables and microsequencer logic are preserved unchanged from the
// original.
//
// The chip requires its own 2KB internal mask ROM image (sp0256-al2.bin,
// CRC32 b504ac15) to be supplied by the caller at runtime -- this code
// does not embed or distribute that ROM. It is placed at byte offset
// $1000 within a 64KB address window, matching the physical chip's
// internal addressing and matching how MAME's coco_ssc.cpp loads it
// (ROM_LOAD("sp0256-al2.bin", 0x1000, 0x0800, ...)).
//
// license:BSD-3-Clause
// Original copyright-holders: Joseph Zbiciak, Tim Lindner (MAME sp0256 core)

//
// Changes for the ESP32: the DRQ callback is a direct call (ssc_bus.h), and
// the ROM is stored as its 2 KB instead of a zero-filled 64 KB window. The
// whole object (~10 KB) is allocated in PSRAM by ssc.cpp.

#ifndef SSC_SP0256_H
#define SSC_SP0256_H

#include <stdint.h>
#include <string.h>
#include <stddef.h>

namespace ssc {

class Sp0256 {
public:
    Sp0256();

    // Load the 2KB SP0256-AL2 mask ROM dump. It answers at $1000-$17FF of
    // the chip's 64KB address window; the rest of the window reads as 0.
    void LoadRom(const uint8_t* al2_2k, size_t len);

    void Reset();

    // ALD (Address LoaD): write an allophone/command byte (6 bits used).
    void Ald(uint8_t data);

    // True while the chip is speaking (not on standby).
    bool Standby() const { return m_sby_line != 0; }

    // Generates `count` mono samples at the chip's native output rate
    // (clock/CLOCK_DIVIDER, ~10.02kHz at the standard 3.12MHz clock) into
    // `out`, running the microsequencer as needed. Samples are signed
    // 16-bit PCM.
    void GenerateSamples(int16_t* out, int count);

    // Native output sample rate for a given input clock, matching the
    // original CLOCK_DIVIDER constant.
    static int SampleRateForClock(int clock_hz) { return clock_hz / kClockDivider; }

private:
    static constexpr int kClockDivider = 6 * 4 * 13;
    static constexpr int kScbufSize = 4096;
    static constexpr int kScbufMask = kScbufSize - 1;
    static constexpr int kPerPause = 64;
    static constexpr int kPerNoise = 64;
    static constexpr int kFifoAddr = (0x1800 << 3);

    struct Lpc12 {
        int      rpt = 0, cnt = 0;
        uint32_t per = 0, rng = 1;
        int      amp = 0;
        int16_t  f_coef[6] = {0};
        int16_t  b_coef[6] = {0};
        int16_t  z_data[6][2] = {{0}};
        uint8_t  r[16] = {0};
        int      interp = 0;

        int Update(int num_samp, int16_t* out, uint32_t* optr);
        void Regdec();
        static int16_t Limit(int16_t s);
    };

    static constexpr int kRomBase = 0x1000;
    static constexpr int kRomSize = 0x800;
    uint8_t  m_rom[kRomSize];     // answers at kRomBase in the 64KB window
    Lpc12 m_filt;

    int      m_sby_line = 1;
    bool     m_silent = 1;

    int16_t  m_scratch[kScbufSize];
    uint32_t m_sc_head = 0, m_sc_tail = 0;

    int      m_lrq = 1;
    int      m_ald = 0;
    int      m_pc = 0;
    int      m_stack = 0;
    int      m_fifo_sel = 0;
    int      m_halted = 1;
    uint32_t m_mode = 0;
    uint32_t m_page = 0x1000 << 3;

    uint32_t m_fifo_head = 0, m_fifo_tail = 0, m_fifo_bitp = 0;
    uint16_t m_fifo[64] = {0};

    void SetSby(int line_state);
    uint8_t  RomByte(int idx) const {
        idx = (idx & 0xffff) - kRomBase;
        return (idx >= 0 && idx < kRomSize) ? m_rom[idx] : 0;
    }
    uint32_t Getb(int len);
    void Micro();
};

} // namespace ssc

#endif // SSC_SP0256_H
