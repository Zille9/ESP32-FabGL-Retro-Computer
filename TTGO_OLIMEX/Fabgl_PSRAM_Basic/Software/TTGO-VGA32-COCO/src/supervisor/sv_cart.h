/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_cart.h
 *  Module : Supervisor Load Cart screen
 * ============================================================
 *
 * Browses the SD card (starting in /carts) for cartridge ROMs (.ccc / .rom:
 * up to 16 KB on the CoCo 2, up to 128 KB bank-switched on the CoCo 3) and stores the chosen one as the cartridge loaded at $C000 in
 * place of Disk BASIC; "Unload cartridge" clears it. The ROM is read at boot,
 * so each change asks whether to restart now: No keeps it for the next
 * restart, Yes restarts, ESC cancels the change.
 */
#ifndef SV_CART_H
#define SV_CART_H

#include <stdint.h>

typedef struct Supervisor_t Supervisor_t;

// Open the screen (sets state, scans the folder).
void sv_cart_open(Supervisor_t* sv);

// HID key handler (Up/Down/PgUp/PgDn/Home/End move, ENTER select, ESC back
// to the main menu).
void sv_cart_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);

// Render the screen.
void sv_cart_render(Supervisor_t* sv);

// Force the next render to repaint the whole Load Cart screen.
void sv_cart_invalidate(void);

#endif // SV_CART_H
