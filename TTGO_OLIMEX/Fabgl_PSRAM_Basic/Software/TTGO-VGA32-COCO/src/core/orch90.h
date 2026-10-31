/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : orch90.h
 *  Module : Orchestra-90/CC sound cartridge — two 8-bit DAC latches
 * ============================================================
*/

#ifndef ORCH90_H
#define ORCH90_H

#include <stdint.h>

// Port of XRoar's orch90.c. The cartridge is two write-only 8-bit unsigned
// DAC latches; there is no status register, timer or interrupt. Its 8 KB ROM
// is an ordinary cartridge ROM, loaded through g_cart_rom_request.
#define ORCH90_LEFT_ADDR    0xFF7A
#define ORCH90_RIGHT_ADDR   0xFF7B

// Set once at boot from NVS (supervisor_load_cart_config), before
// machine_init(). Gates the $FF7A/$FF7B decode in machine.cpp.
extern bool g_orch90_enabled;

static inline bool orch90_enabled(void) {
    return g_orch90_enabled;
}

// Bus write to ORCH90_LEFT_ADDR / ORCH90_RIGHT_ADDR (other addresses ignored).
void orch90_write(uint16_t addr, uint8_t val);

// Last values written, for the debug screens.
uint8_t orch90_left(void);
uint8_t orch90_right(void);

#endif // ORCH90_H
