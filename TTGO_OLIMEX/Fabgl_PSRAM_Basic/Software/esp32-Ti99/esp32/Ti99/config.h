// Classic99 for ESP32 - build configuration
//
// BUILD_TARGET selects how the firmware is started:
//
//   BUILD_TARGET_STANDALONE  Flashed over USB; the firmware owns the whole board. Default.
//   BUILD_TARGET_BOOTLOADER  Loaded from the SD card menu of ESP32_Bootloader
//                            (https://github.com/ESP-WORKS/ESP32_Bootloader), which
//                            flashes it into its ota_0 partition.
//
// Change the default below, or leave it alone and override it from the build:
// "pio run -e bootloader" and tools/build.sh pass -DBUILD_TARGET=1.
#ifndef CLASSIC99_ESP32_CONFIG_H
#define CLASSIC99_ESP32_CONFIG_H

#define BUILD_TARGET_STANDALONE 1
#define BUILD_TARGET_BOOTLOADER 0

#ifndef BUILD_TARGET
#define BUILD_TARGET BUILD_TARGET_STANDALONE
#endif

#if BUILD_TARGET != BUILD_TARGET_STANDALONE && BUILD_TARGET != BUILD_TARGET_BOOTLOADER
#error "BUILD_TARGET must be BUILD_TARGET_STANDALONE (0) or BUILD_TARGET_BOOTLOADER (1)"
#endif

// Size of ota_0 in the ESP32_Bootloader partition table (2816 KB): the largest
// firmware it can load. tools/build.sh and [env:bootloader] check against it.
#define BOOTLOADER_OTA0_MAX_BYTES 0x2C0000

#endif
