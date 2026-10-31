/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : RomLoader.h
 *  Module : Loads ROM images from /roms on the SD card (interface).
 *           Only the size is enforced; the CRC32 is logged so a dump
 *           can be checked against MAME's hash lists, but a patched
 *           ROM of the right size still loads. Embedded fallbacks
 *           are the caller's business (see Apple2Machine).
 * ============================================================
*/

#ifndef ROM_LOADER_H
#define ROM_LOADER_H

#include "Predef.h"
#include "MachineProfile.h"

#define ROM_DIR "/roms"

enum RomStatus : uint8_t
{
	ROM_OK,
	ROM_MISSING,                // no card, no /roms, or no such file
	ROM_BAD_SIZE                // wrong size, or a short read
};

class RomLoader
{
public:
	// Reads /roms/<file> into dest, which must hold size bytes. On anything
	// but ROM_OK dest may be partly overwritten; reload a fallback into it.
	static RomStatus Load(const char* file, BYTE* dest, int size);
	// Same checks without reading, for the supervisor's machine picker.
	static RomStatus Check(const char* file, int size);
	static const char* StatusText(RomStatus status);
	// The first ROM file a profile cannot boot without, or NULL when it can
	// boot. A model with a built-in system ROM (the ][+) always can.
	static const char* FirstMissing(const MachineProfile& profile);
};

#endif
