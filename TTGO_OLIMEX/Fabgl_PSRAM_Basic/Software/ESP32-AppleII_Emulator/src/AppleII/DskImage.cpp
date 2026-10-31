/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : DskImage.cpp
 *  Module : Sector disk images. Writes each track the way DOS 3.3
 *           INIT lays it out: sync gap, then per sector an address
 *           field (D5 AA 96, 4-and-4 volume/track/sector/checksum,
 *           DE AA EB), a short gap, and a data field (D5 AA AD,
 *           342+1 6-and-2 nibbles, DE AA EB). Also reads tracks
 *           back into sectors, for writing a changed disk back to
 *           its image.
 * ============================================================
*/

#include "DskImage.h"
#include <cstring>
#include <strings.h>

namespace DskImage
{

static const int GAP1 = 48;         // before the first sector
static const int GAP2 = 6;          // address field to data field
static const int GAP3 = 27;         // between sectors

// 6-bit value to disk nibble
static const uint8_t WRITE_TABLE[64] = {
	0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
	0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
	0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
	0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

// physical sector -> sector number in the image file
static const uint8_t DOS_ORDER[16]    = { 0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15 };
static const uint8_t PRODOS_ORDER[16] = { 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15 };

static bool EndsWith(const char* s, const char* ext)
{
	size_t n = strlen(s), e = strlen(ext);
	return n > e && strcasecmp(s + n - e, ext) == 0;
}

ImageType TypeFromPath(const char* path)
{
	if (EndsWith(path, ".nib"))
		return IMAGE_NIB;
	if (EndsWith(path, ".dsk") || EndsWith(path, ".do"))
		return IMAGE_DOS;
	if (EndsWith(path, ".po"))
		return IMAGE_PRODOS;
	if (EndsWith(path, ".hdv"))
		return IMAGE_HDV;
	if (EndsWith(path, ".2mg"))
		return IMAGE_2MG;
	return IMAGE_NONE;
}

int LogicalSector(int phys, SectorOrder order)
{
	return (order == ORDER_PRODOS ? PRODOS_ORDER : DOS_ORDER)[phys & 15];
}

static uint8_t* Put44(uint8_t* p, uint8_t v)
{
	*p++ = (v >> 1) | 0xAA;
	*p++ = v | 0xAA;
	return p;
}

static uint8_t* Sync(uint8_t* p, int n)
{
	memset(p, 0xFF, n);
	return p + n;
}

// 256 bytes -> 342 six-bit values, then XOR-chained into 343 nibbles.
// The first 86 hold the low two bits of bytes i, i+86 and i+172 (each pair
// bit-swapped), the other 256 the top six bits of every byte.
static uint8_t* Put62(uint8_t* p, const uint8_t* data)
{
	uint8_t six[342];
	for (int i = 0; i < 86; i++)
	{
		uint8_t v = 0;
		for (int k = 2; k >= 0; k--)
		{
			int idx = i + 86 * k;
			uint8_t b = idx < 256 ? data[idx] : 0;
			v = (v << 2) | ((b & 1) << 1) | ((b >> 1) & 1);
		}
		six[i] = v;
	}
	for (int i = 0; i < 256; i++)
		six[86 + i] = data[i] >> 2;

	uint8_t prev = 0;
	for (int i = 0; i < 342; i++)
	{
		*p++ = WRITE_TABLE[six[i] ^ prev];
		prev = six[i];
	}
	*p++ = WRITE_TABLE[prev];                                 // checksum
	return p;
}

void NibblizeTrack(const uint8_t* sectors, int track, SectorOrder order, uint8_t* out)
{
	uint8_t* p = Sync(out, GAP1);
	for (int phys = 0; phys < SECTORS; phys++)
	{
		*p++ = 0xD5; *p++ = 0xAA; *p++ = 0x96;
		p = Put44(p, VOLUME);
		p = Put44(p, track);
		p = Put44(p, phys);
		p = Put44(p, VOLUME ^ track ^ phys);
		*p++ = 0xDE; *p++ = 0xAA; *p++ = 0xEB;
		p = Sync(p, GAP2);

		*p++ = 0xD5; *p++ = 0xAA; *p++ = 0xAD;
		p = Put62(p, sectors + LogicalSector(phys, order) * SECTOR_BYTES);
		*p++ = 0xDE; *p++ = 0xAA; *p++ = 0xEB;
		p = Sync(p, GAP3);
	}
	Sync(p, NIB_TRACK - (int)(p - out));                      // 6384 used, rest is gap
}

void NibblizeInPlace(uint8_t* buf, SectorOrder order)
{
	static uint8_t scratch[TRACK_BYTES];
	const uint8_t* image = buf + (NIB_BYTES - IMAGE_BYTES);
	for (int t = 0; t < TRACKS; t++)
	{
		// the last two tracks' output overlaps their own input
		memcpy(scratch, image + t * TRACK_BYTES, TRACK_BYTES);
		NibblizeTrack(scratch, t, order, buf + t * NIB_TRACK);
	}
}

// disk nibble -> 6-bit value, 0xFF for a byte that is not a disk nibble
static uint8_t readTable[256];
static bool readTableBuilt = false;

static void BuildReadTable()
{
	if (readTableBuilt)
		return;
	memset(readTable, 0xFF, sizeof(readTable));
	for (int i = 0; i < 64; i++)
		readTable[WRITE_TABLE[i]] = i;
	readTableBuilt = true;
}

// A track read as a ring, from pos on
struct TrackReader
{
	const uint8_t* nib;
	int pos;
	uint8_t Next() { uint8_t v = nib[pos]; pos = (pos + 1) % NIB_TRACK; return v; }
	uint8_t Get44() { uint8_t a = Next(); uint8_t b = Next(); return ((a << 1) | 1) & b; }
	bool At(uint8_t a, uint8_t b, uint8_t c) const
	{
		return nib[pos] == a && nib[(pos + 1) % NIB_TRACK] == b && nib[(pos + 2) % NIB_TRACK] == c;
	}
};

int DenibblizeTrack(const uint8_t* nib, int track, uint8_t sectors[SECTORS][SECTOR_BYTES])
{
	BuildReadTable();
	int found = 0;
	for (int start = 0; start < NIB_TRACK; start++)
	{
		TrackReader r = { nib, start };
		if (!r.At(0xD5, 0xAA, 0x96))
			continue;
		r.Next(); r.Next(); r.Next();
		uint8_t vol = r.Get44(), trk = r.Get44(), sec = r.Get44(), sum = r.Get44();
		if ((vol ^ trk ^ sec) != sum || trk != track || sec >= SECTORS)
			continue;

		// the data field follows within a few dozen nibbles
		int n = 0;
		while (n < 64 && !r.At(0xD5, 0xAA, 0xAD))
		{
			r.Next();
			n++;
		}
		if (n == 64)
			continue;
		r.Next(); r.Next(); r.Next();

		uint8_t six[342];
		uint8_t prev = 0;
		bool bad = false;
		for (int i = 0; i < 342; i++)
		{
			uint8_t v = readTable[r.Next()];
			if (v == 0xFF) { bad = true; break; }
			prev ^= v;
			six[i] = prev;
		}
		if (bad || (readTable[r.Next()] ^ prev) != 0)
			continue;

		for (int i = 0; i < SECTOR_BYTES; i++)
		{
			int aux = six[i % 86] >> (2 * (i / 86));
			sectors[sec][i] = (six[86 + i] << 2) | ((aux & 1) << 1) | ((aux >> 1) & 1);
		}
		found |= 1 << sec;
	}
	return found;
}

bool DenibblizeTrackToImage(const uint8_t* nib, int track, SectorOrder order, uint8_t* out)
{
	// static: 4K is too much for the emulation task's 8K stack
	static uint8_t sectors[SECTORS][SECTOR_BYTES];
	if (DenibblizeTrack(nib, track, sectors) != 0xFFFF)
		return false;
	for (int p = 0; p < SECTORS; p++)
		memcpy(out + LogicalSector(p, order) * SECTOR_BYTES, sectors[p], SECTOR_BYTES);
	return true;
}

}
