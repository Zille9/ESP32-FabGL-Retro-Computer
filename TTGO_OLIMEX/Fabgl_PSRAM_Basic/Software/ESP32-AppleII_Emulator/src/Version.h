/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Version.h
 *  Module : Single source of truth for the firmware version. The
 *           supervisor's ABOUT page displays it, and
 *           tools/build-firmware.sh parses FW_VERSION_STR out of
 *           this file to name the release image. Bump it here and
 *           nowhere else.
 * ============================================================
*/

#ifndef VERSION_H
#define VERSION_H

#define FW_VERSION_STR   "0.10.0"
#define FW_BUILD_DATE    __DATE__

#endif
