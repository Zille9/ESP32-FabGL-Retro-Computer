/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : MachineProfile.cpp
 *  Module : The machine profile table, indexed by MachineId.
 * ============================================================
*/

#include "MachineProfile.h"

// Indexed by MachineId: keep the rows in enum order.
static const MachineProfile profiles[MACHINE_COUNT] =
{
	// id                        name                   cpu           aux    IIe MMU
	{ MACHINE_APPLE2PLUS,       "APPLE ][+",           CPU_NMOS6502, false, false,
	  // system ROM              size    embedded  char ROM                        size    disk II
	  "apple2plus.rom",          0x3000, true,     NULL,                           0,      true },

	{ MACHINE_APPLE2E_ENHANCED, "APPLE //E ENHANCED",  CPU_65C02,    true,  true,
	  "apple2e_enhanced.rom",    0x4000, false,    "apple2e_enhanced_video.rom",   0x1000, true },
};

const MachineProfile& GetMachineProfile(uint8_t id)
{
	if (id >= MACHINE_COUNT)
		id = MACHINE_APPLE2PLUS;
	return profiles[id];
}
