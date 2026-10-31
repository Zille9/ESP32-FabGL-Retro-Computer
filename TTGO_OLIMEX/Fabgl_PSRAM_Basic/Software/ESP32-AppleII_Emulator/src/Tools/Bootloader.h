/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Bootloader.h
 *  Module : Hand-over with ESP32_Bootloader (BuildConfig.h). The
 *           bootloader lives in the factory partition and flashes
 *           this app into ota_0, then sets otadata to boot it.
 *           Erasing otadata at startup sends the next power-up
 *           back to the bootloader menu; a software restart (the
 *           machine switch) points otadata back at this app first
 *           so it comes straight back up here. In the standalone
 *           build both are no-ops.
 * ============================================================
*/

#pragma once

#include <Arduino.h>
#include "../BuildConfig.h"

#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
#include "esp_ota_ops.h"
#include "esp_partition.h"
#endif

class Bootloader
{
public:
	// First thing setup() does. With otadata blank the ROM bootloader falls
	// back to the factory partition, i.e. ESP32_Bootloader's menu.
	static void ReleaseOtadata()
	{
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
		const esp_partition_t* otadata = esp_partition_find_first(
			ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
		if (otadata)
			esp_partition_erase_range(otadata, 0, otadata->size);
#endif
	}

	// ESP.restart() that comes back to this app instead of the menu: select
	// the running partition again, and setup() erases otadata once more.
	static void Restart()
	{
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
		const esp_partition_t* running = esp_ota_get_running_partition();
		if (running && running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
		    running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
		{
			esp_err_t err = esp_ota_set_boot_partition(running);
			if (err != ESP_OK)
				Serial.printf("[boot] cannot reselect %s (%d): restarting into the menu\n",
				              running->label, (int)err);
		}
#endif
		ESP.restart();
	}

	static const char* TargetName()
	{
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
		return "ESP32_Bootloader";
#else
		return "standalone";
#endif
	}
};
