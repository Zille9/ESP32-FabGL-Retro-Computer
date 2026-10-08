/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_fujinet.h
 *  Module : Supervisor DriveWire / FujiNet settings screen
 * ============================================================
 *
 * Shows the live Becker-port link and edits the saved bus settings: External
 * Server on/off, and while it is on the host, port and HDB-DOS ROM variant.
 * Turning it on asks for those in a popup and refuses while the ROM file is
 * missing from the SD card. Edits are held locally and only written by
 * "Save & Restart", because the bus mode is fixed at boot.
 */
#ifndef SV_FUJINET_H
#define SV_FUJINET_H

#include <stdint.h>

typedef struct Supervisor_t Supervisor_t;

void sv_fujinet_open(Supervisor_t* sv);
// Force the next render to repaint the whole screen.
void sv_fujinet_invalidate(void);
void sv_fujinet_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_fujinet_tick(Supervisor_t* sv);     // redraw when the link state changes
void sv_fujinet_render(Supervisor_t* sv);

#endif // SV_FUJINET_H
