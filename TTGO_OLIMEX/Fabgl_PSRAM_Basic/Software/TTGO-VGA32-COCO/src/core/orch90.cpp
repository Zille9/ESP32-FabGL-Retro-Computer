/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : orch90.cpp
 *  Module : Orchestra-90/CC sound cartridge — two 8-bit DAC latches
 * ============================================================
*/

#include "orch90.h"
#include "sound.h"

bool g_orch90_enabled = false;

// The latches survive a machine reset, as in XRoar (orch90_reset only resets
// the ROM) and on the real cartridge.
static uint8_t s_left  = 0;
static uint8_t s_right = 0;

void orch90_write(uint16_t addr, uint8_t val) {
    if (addr == ORCH90_LEFT_ADDR)       s_left  = val;
    else if (addr == ORCH90_RIGHT_ADDR) s_right = val;
    else return;
    sound_set_external(s_left, s_right);
}

uint8_t orch90_left(void)  { return s_left; }
uint8_t orch90_right(void) { return s_right; }
