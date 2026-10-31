/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : BuildConfig.h
 *  Module : Build target switch. The default is the standalone
 *           firmware flashed over USB; tools/package-bootloader.sh
 *           builds with -DBUILD_TARGET=1 for ESP32_Bootloader,
 *           which flashes the app from the SD card into ota_0:
 *           https://github.com/ESP-WORKS/ESP32_Bootloader
 * ============================================================
*/

#ifndef APPLE2_BUILD_CONFIG_H
#define APPLE2_BUILD_CONFIG_H

// BUILD_TARGET_STANDALONE  Normal USB flash; the app owns the whole device.
// BUILD_TARGET_BOOTLOADER  Launched from ota_0 by ESP32_Bootloader.
#define BUILD_TARGET_STANDALONE 1
#define BUILD_TARGET_BOOTLOADER 0

#ifndef BUILD_TARGET
#define BUILD_TARGET BUILD_TARGET_STANDALONE
#endif

// Size of ota_0 in the ESP32_Bootloader partition table (2816 KB).
#define BOOTLOADER_OTA0_MAX_BYTES 0x2C0000

// PERF_TRACE 1: a measurement build. 20 s after boot it mounts
// /karateka.nib in drive 1, resets, and logs every Disk II motor change
// and the time spent drawing, which tools/capture-log.py turns into a load
// time. Build it with tools/build-dev.sh build/perf -DPERF_TRACE=1; never
// commit it switched on.
#ifndef PERF_TRACE
#define PERF_TRACE 0
#endif

#endif
