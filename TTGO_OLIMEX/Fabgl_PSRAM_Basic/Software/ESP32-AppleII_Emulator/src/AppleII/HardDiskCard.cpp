/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HardDiskCard.cpp
 *  Module : ProDOS block device card, see HardDiskCard.h.
 * ============================================================
*/

#include "HardDiskCard.h"
#include "DskImage.h"

static const int DRIVER = 0x40;      // driver entry, $Cn40

HardDiskCard::HardDiskCard()
	: slot(7), bus(NULL), dataOffset(0), blocks(0), readOnly(false), result(ERR_NONE)
{
	filename[0] = '\0';
	BuildRom();
}

void HardDiskCard::Configure(int s, CardBus* b)
{
	slot = s;
	bus = b;
	BuildRom();
}

// The firmware, for slot n:
//   $Cn00  LDX #$20 / LDY #$00 / LDX #$03 / LDX #$3C
//          the ID bytes the autostart ROM ($Cn01/03/05) and ProDOS ($Cn07,
//          not $00: a plain block device, not SmartPort) look for
//   $Cn08  boot: READ block 0 of unit $n0 to $0800, then JMP $0801 with X =
//          $n0 as boot code expects. A read error or a blank boot block
//          (first byte $00: an image not yet formatted) goes back to the
//          autostart ROM's slot scan at $FABA - both the ][+ and the //e
//          ROMs have it there - so the Disk II still boots; if the scan did
//          not bring us here ($01 is not $Cn), to BASIC instead
//   $Cn40  driver: STA $C080+n0 runs the command; A = error code, X/Y =
//          block count (for STATUS); CMP #1 sets carry on an error
//   $CnFC  block count, $CnFE status byte, $CnFF driver entry
void HardDiskCard::BuildRom()
{
	const BYTE s16 = (BYTE)(slot << 4);
	const BYTE io  = (BYTE)(0x80 + s16);         // $C0x0: register 0
	const BYTE cn  = (BYTE)(0xC0 + slot);
	const BYTE code[] =
	{
		0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0xA2, 0x3C,   // $00 ID bytes
		0xA9, 0x01, 0x85, 0x42,                           // $08 LDA #1 STA $42   READ
		0xA9, s16,  0x85, 0x43,                           // $0C LDA #$n0 STA $43 unit
		0xA9, 0x00, 0x85, 0x44, 0x85, 0x46, 0x85, 0x47,   // $10 buffer lo, block 0
		0xA9, 0x08, 0x85, 0x45,                           // $18 buffer $0800
		0x20, DRIVER, cn,                                 // $1C JSR driver
		0xB0, 0x0A,                                       // $1F BCS fail
		0xAD, 0x00, 0x08,                                 // $21 LDA $0800
		0xF0, 0x05,                                       // $24 BEQ fail   nothing to boot
		0xA2, s16,                                        // $26 LDX #$n0
		0x4C, 0x01, 0x08,                                 // $28 JMP $0801
		0xA5, 0x01,                                       // $2B fail: LDA $01
		0xC9, cn,                                         // $2D CMP #$Cn   from the slot scan?
		0xD0, 0x03,                                       // $2F BNE basic
		0x4C, 0xBA, 0xFA,                                 // $31 JMP $FABA  scan the next slot
		0x4C, 0x00, 0xE0,                                 // $34 basic: JMP $E000
	};
	const BYTE driver[] =
	{
		0x8D, io, 0xC0,                                   // STA $C0x0  run it
		0xAD, (BYTE)(io + 1), 0xC0,                       // LDA $C0x1  error code
		0xAE, (BYTE)(io + 2), 0xC0,                       // LDX $C0x2  blocks lo
		0xAC, (BYTE)(io + 3), 0xC0,                       // LDY $C0x3  blocks hi
		0xC9, 0x01,                                       // CMP #1     carry = error
		0x60,                                             // RTS
	};
	memset(rom, 0, sizeof(rom));
	memcpy(rom, code, sizeof(code));
	memcpy(rom + DRIVER, driver, sizeof(driver));
	rom[0xFC] = blocks & 0xFF;
	rom[0xFD] = blocks >> 8;
	rom[0xFE] = 0x07;                                     // status, read, write; 1 volume
	rom[0xFF] = DRIVER;
}

BYTE* HardDiskCard::SlotRom()
{
	return HasImage() ? rom : NULL;
}

BYTE HardDiskCard::Io(int reg, BYTE value, bool write)
{
	switch (reg)
	{
		case 0x0:
			if (write)
				result = Execute();
			break;
		case 0x1: return result;
		case 0x2: return blocks & 0xFF;
		case 0x3: return blocks >> 8;
	}
	return 0;
}

BYTE HardDiskCard::Execute()
{
	if (!bus)
		return ERR_IO;
	BYTE cmd  = bus->Read(0x42);
	BYTE unit = bus->Read(0x43);
	WORD buf  = bus->Read(0x44) | (bus->Read(0x45) << 8);
	uint32_t blk = bus->Read(0x46) | (bus->Read(0x47) << 8);

	// one drive: drive 1 of our own slot
	if (!HasImage() || (unit & 0x80) || ((unit >> 4) & 7) != slot)
		return ERR_NO_DEVICE;
	if (cmd == 0)                                          // STATUS
		return readOnly ? ERR_WRITE_PROTECT : ERR_NONE;
	if (cmd == 3)                                          // FORMAT: nothing to lay down
		return readOnly ? ERR_WRITE_PROTECT : ERR_NONE;
	if ((cmd != 1 && cmd != 2) || blk >= blocks)
		return ERR_IO;

	uint32_t pos = dataOffset + blk * BLOCK_BYTES;
	if (cmd == 1)                                          // READ
	{
		if (!file.seek(pos) || (int)file.read(block, BLOCK_BYTES) != BLOCK_BYTES)
			return ERR_IO;
		for (int i = 0; i < BLOCK_BYTES; i++)
			bus->Write((WORD)(buf + i), block[i]);
		return ERR_NONE;
	}

	if (readOnly)                                          // WRITE
		return ERR_WRITE_PROTECT;
	for (int i = 0; i < BLOCK_BYTES; i++)
		block[i] = bus->Read((WORD)(buf + i));
	if (!file.seek(pos) || file.write(block, BLOCK_BYTES) != (size_t)BLOCK_BYTES)
		return ERR_IO;
	file.flush();                                          // a power cut keeps the block
	return ERR_NONE;
}

static uint32_t Le32(const BYTE* p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool HardDiskCard::IsHardDiskImage(const char* path)
{
	DskImage::ImageType type = DskImage::TypeFromPath(path);
	if (type == DskImage::IMAGE_HDV || type == DskImage::IMAGE_2MG)
		return true;
	if (type != DskImage::IMAGE_PRODOS)
		return false;
	File f = FileSystem::Open(path);
	if (!f)
		return false;
	bool big = f.size() != (size_t)DskImage::IMAGE_BYTES;
	f.close();
	return big;
}

bool HardDiskCard::Mount(const char* path)
{
	Unmount();

	// writable if the card lets us, read-only otherwise
	bool ro = false;
	File f = SD.open(path, "r+");
	if (!f)
	{
		f = FileSystem::Open(path);
		ro = true;
	}
	if (!f)
	{
		LOGF("[hdd] cannot open %s\n", path);
		return false;
	}

	uint32_t size = f.size(), offset = 0, length = size;
	if (DskImage::TypeFromPath(path) == DskImage::IMAGE_2MG)
	{
		// 2IMG header: format at $0C (1 = ProDOS order), flags at $10
		// (bit 31: locked), data offset at $18, data length at $1C
		BYTE h[64];
		if (size < 64 || (int)f.read(h, 64) != 64 || memcmp(h, "2IMG", 4) != 0)
		{
			LOGF("[hdd] %s: not a 2IMG file\n", path);
			f.close();
			return false;
		}
		offset = Le32(h + 0x18);
		length = Le32(h + 0x1C);
		if (Le32(h + 0x0C) != 1 || offset + length > size)
		{
			LOGF("[hdd] %s: only ProDOS-order .2mg images\n", path);
			f.close();
			return false;
		}
		if (Le32(h + 0x10) & 0x80000000u)
			ro = true;
	}
	if (length < (uint32_t)BLOCK_BYTES || length % BLOCK_BYTES)
	{
		LOGF("[hdd] %s: not a whole number of 512-byte blocks\n", path);
		f.close();
		return false;
	}

	file = f;
	dataOffset = offset;
	uint32_t n = length / BLOCK_BYTES;
	blocks = (uint16_t)(n > (uint32_t)MAX_BLOCKS ? MAX_BLOCKS : n);
	readOnly = ro;
	snprintf(filename, sizeof(filename), "%s", path);
	BuildRom();
	LOGF("[hdd] %s: %u blocks%s\n", path, (unsigned)blocks, ro ? ", read-only" : "");
	return true;
}

void HardDiskCard::Unmount()
{
	if (HasImage())
		file.close();
	filename[0] = '\0';
	blocks = 0;
	dataOffset = 0;
	readOnly = false;
	BuildRom();
}
