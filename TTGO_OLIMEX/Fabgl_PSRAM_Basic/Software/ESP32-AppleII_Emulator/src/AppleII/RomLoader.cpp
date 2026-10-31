/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : RomLoader.cpp
 *  Module : Loads ROM images from /roms on the SD card, refusing
 *           files of the wrong size and logging each image's CRC32.
 * ============================================================
*/

#include <SD.h>
#include "RomLoader.h"
#include "../Tools/FileSystem.h"

// Plain bitwise CRC-32 (the zip/MAME polynomial). It runs once per ROM at
// boot, over at most 16K, so a table is not worth the RAM.
static uint32_t Crc32(const BYTE* p, int len)
{
	uint32_t crc = 0xFFFFFFFF;
	while (len--)
	{
		crc ^= *p++;
		for (int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
	}
	return ~crc;
}

static void RomPath(const char* file, char* out, int outlen)
{
	snprintf(out, outlen, "%s/%s", ROM_DIR, file);
}

RomStatus RomLoader::Check(const char* file, int size)
{
	char path[64];
	RomPath(file, path, sizeof(path));

	File f = FileSystem::Open(path);
	if (!f || f.isDirectory())
		return ROM_MISSING;
	RomStatus status = ((int)f.size() == size) ? ROM_OK : ROM_BAD_SIZE;
	f.close();
	return status;
}

RomStatus RomLoader::Load(const char* file, BYTE* dest, int size)
{
	char path[64];
	RomPath(file, path, sizeof(path));

	File f = FileSystem::Open(path);
	if (!f || f.isDirectory())
	{
		LOGF("[rom] %s: missing\n", path);
		return ROM_MISSING;
	}
	if ((int)f.size() != size)
	{
		LOGF("[rom] %s: %u bytes, expected %d - ignored\n", path, (unsigned)f.size(), size);
		f.close();
		return ROM_BAD_SIZE;
	}

	int got = f.read(dest, size);
	f.close();
	if (got != size)
	{
		LOGF("[rom] %s: short read (%d of %d bytes)\n", path, got, size);
		return ROM_BAD_SIZE;
	}

	LOGF("[rom] %s: %d bytes, crc32 %08x\n", path, size, (unsigned)Crc32(dest, size));
	return ROM_OK;
}

const char* RomLoader::FirstMissing(const MachineProfile& p)
{
	if (!p.embeddedRom && Check(p.systemRom, p.systemRomSize) != ROM_OK)
		return p.systemRom;
	if (p.charRom && Check(p.charRom, p.charRomSize) != ROM_OK)
		return p.charRom;
	return NULL;
}

const char* RomLoader::StatusText(RomStatus status)
{
	switch (status)
	{
		case ROM_OK:       return "OK";
		case ROM_MISSING:  return "MISSING";
		case ROM_BAD_SIZE: return "BAD SIZE";
	}
	return "?";
}
