/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : fabgl.h (host shim)
 *  Module : Stand-in for the FabGL umbrella header. The layout test
 *           needs only the keyboard layout tables, which are plain
 *           data and compile on the host; the rest of FabGL is
 *           ESP32-only. kbdlayouts.h comes from the real library.
 * ============================================================
*/

#pragma once

#include "kbdlayouts.h"
