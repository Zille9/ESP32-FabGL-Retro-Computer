/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : DskImage.h
 *  Module : Sector disk images (interface). Turns a 140K .dsk /
 *           .do (DOS 3.3 order) or .po (ProDOS order) image into
 *           the 35 x 0x1A00 nibble tracks DiskIICard streams, as
 *           16-sector 6-and-2 GCR. No FabGL or Arduino types, so
 *           it is tested on the host (tests/host/run-dsk-tests.sh).
 * ============================================================
*/

#pragma once

#include <cstdint>

namespace DskImage
{
	const int TRACKS        = 35;
	const int SECTORS       = 16;
	const int SECTOR_BYTES  = 256;
	const int TRACK_BYTES   = SECTORS * SECTOR_BYTES;   // 4096 in the image
	const int IMAGE_BYTES   = TRACKS * TRACK_BYTES;     // 143360
	const int NIB_TRACK     = 0x1A00;                   // 6656 on the nibble track
	const int NIB_BYTES     = TRACKS * NIB_TRACK;       // 232960, same as DISKSIZE
	const uint8_t VOLUME    = 254;

	enum SectorOrder { ORDER_DOS, ORDER_PRODOS };
	enum ImageType { IMAGE_NONE, IMAGE_NIB, IMAGE_DOS, IMAGE_PRODOS, IMAGE_HDV, IMAGE_2MG };

	// From the file extension, case-insensitive: .nib, .dsk/.do, .po, .hdv, .2mg.
	ImageType TypeFromPath(const char* path);

	// File sector number stored in physical sector 'phys' of a track.
	int LogicalSector(int phys, SectorOrder order);

	// One track: 16 sectors of the image in, NIB_TRACK nibbles out.
	void NibblizeTrack(const uint8_t* sectors, int track, SectorOrder order, uint8_t* out);

	// Whole disk in place. buf is NIB_BYTES long and holds the IMAGE_BYTES
	// image at its tail (offset NIB_BYTES - IMAGE_BYTES); on return it holds
	// the nibble tracks. Each track's output ends before the next track's
	// input starts, so only one track of scratch is needed.
	void NibblizeInPlace(uint8_t* buf, SectorOrder order);

	// Reads a nibble track back the way RWTS does, as a ring: every sector
	// whose address field names 'track' and whose data field checksums, into
	// sectors[physical sector]. Returns a bitmask of the physical sectors
	// found (0xFFFF: all 16).
	int DenibblizeTrack(const uint8_t* nib, int track, uint8_t sectors[SECTORS][SECTOR_BYTES]);

	// The whole track back into image order, TRACK_BYTES at out. False, with
	// out untouched, unless all 16 sectors decode. Not reentrant.
	bool DenibblizeTrackToImage(const uint8_t* nib, int track, SectorOrder order, uint8_t* out);
}
