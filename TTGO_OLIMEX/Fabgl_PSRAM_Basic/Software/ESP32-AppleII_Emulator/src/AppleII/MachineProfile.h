/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : MachineProfile.h
 *  Module : The supported Apple II models and what distinguishes
 *           them: CPU type, auxiliary memory, ROM files and slot
 *           population. The core is written against this table
 *           rather than model names, so adding a model (IIc, IIc
 *           Plus) means a new row plus whatever hardware it adds.
 * ============================================================
*/

#ifndef MACHINE_PROFILE_H
#define MACHINE_PROFILE_H

#include "Predef.h"

// The numeric values are stored in NVS (Tools/Settings.h): never renumber
// them, append new models before MACHINE_COUNT.
enum MachineId : uint8_t
{
	MACHINE_APPLE2PLUS       = 0,
	MACHINE_APPLE2E_ENHANCED = 1,
	MACHINE_COUNT
};

enum CpuType : uint8_t
{
	CPU_NMOS6502,               // Apple ][+, unenhanced IIe
	CPU_65C02                   // enhanced IIe, IIc, IIc Plus
};

struct MachineProfile
{
	MachineId   id;
	const char* name;           // supervisor / ABOUT text: uppercase font
	CpuType     cpu;
	bool        hasAux;         // 64K auxiliary RAM (IIe 80-column card)
	bool        iieMmu;         // IIe MMU and switches: $C000-$C01F, internal $Cxxx ROM, aux banking
	const char* systemRom;      // file name in /roms
	uint16_t    systemRomSize;  // 12K = $D000-$FFFF, 16K = $C000-$FFFF
	bool        embeddedRom;    // may fall back to the built-in ][+ image
	const char* charRom;        // character generator in /roms, NULL = built-in font
	uint16_t    charRomSize;
	bool        diskIISlot6;    // Disk II controller card in slot 6
};

// Disk II boot PROM (341-0027), shared by every model with a Disk II card.
// Optional: the built-in copy is used when it is not in /roms.
#define DISKII_ROM "diskii.rom"

// Unknown ids (an NVS value from a newer firmware, say) give the ][+.
const MachineProfile& GetMachineProfile(uint8_t id);

#endif
