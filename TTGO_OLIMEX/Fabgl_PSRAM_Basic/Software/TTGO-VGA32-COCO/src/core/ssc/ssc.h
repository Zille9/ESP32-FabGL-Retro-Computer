/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : ssc.h
 *  Module : Speech/Sound Cartridge — TMS7040 + SP0256-AL2 + AY-3-8913
 * ============================================================
*/

#ifndef SSC_H
#define SSC_H

#include <stdint.h>
#include <stddef.h>

// Tandy Speech/Sound Cartridge (26-3144). Port of VCC's ssc_core.cpp, itself
// ported from MAME's coco_ssc.cpp. The cartridge has no CoCo-side ROM: the
// host sees two I/O ports, and text-to-speech runs in the cartridge's own
// TMS7040 firmware. It is decoded beside the FDC, like a built-in Multi-Pak.
#define SSC_CTRL_ADDR   0xFF7D      // write bit 0: reset line; read: $FF
#define SSC_DATA_ADDR   0xFF7E      // write: command byte; read: status

// $FF7E status bits
#define SSC_ST_READY    0x80        // set = not busy, the next byte can go
#define SSC_ST_STANDBY  0x40        // set = SP0256 idle (not speaking)
#define SSC_ST_QUIET    0x20        // set = no PSG sound activity

// Set once at boot from NVS (supervisor_load_cart_config), before
// machine_init(). Cleared again by ssc_load_roms() if a ROM is missing.
extern bool g_ssc_enabled;

static inline bool ssc_enabled(void) {
    return g_ssc_enabled;
}

// Reads /roms/pic-7040-510.bin (4096 bytes) and /roms/sp0256-al2.bin
// (2048 bytes) and allocates the chips. False if either file is missing or
// the wrong size, or memory is short. Repeat calls after success are no-ops.
bool ssc_load_roms(void);

// Same, from buffers (host tests).
bool ssc_load_roms_from(const uint8_t* pic, size_t pic_len,
                        const uint8_t* speech, size_t speech_len);

// Cartridge reset (CoCo reset). No-op before the ROMs are loaded.
void ssc_reset(void);

// Bus access to SSC_CTRL_ADDR / SSC_DATA_ADDR.
uint8_t ssc_read(uint16_t addr);
void    ssc_write(uint16_t addr, uint8_t val);

// One CoCo scanline (1/15720 s): plays the next mixed sample of the TMS7040
// (~57 cycles), AY (~14 steps) and SP0256 (~0.64 samples). On the device the
// chips run ahead on core 0 (ssc.cpp); host builds run them inline in batches.
void    ssc_scanline_tick(void);

// Introspection for /api/status and the host tests.
bool     ssc_busy(void);
uint32_t ssc_command_count(void);
uint32_t ssc_ald_speech_count(void);    // allophones 5-63 strobed (not pauses)
uint32_t ssc_ay_write_count(void);
int8_t   ssc_level(void);               // last mixed output, -127..127
uint32_t ssc_underruns(void);           // scanlines core 0 had no sample ready
uint32_t ssc_queued(void);              // samples core 0 has ready
uint32_t ssc_stack_free(void);          // core-0 task's unused stack, lowest (bytes)

#endif // SSC_H
