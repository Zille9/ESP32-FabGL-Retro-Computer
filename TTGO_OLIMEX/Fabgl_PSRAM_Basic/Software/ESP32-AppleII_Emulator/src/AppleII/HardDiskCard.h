/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HardDiskCard.h
 *  Module : A ProDOS block device, after AppleWin's Harddisk.cpp:
 *           one hard disk image (.hdv, .2mg, or a .po bigger than a
 *           floppy) read and written 512 bytes at a time straight
 *           from the SD card, with no disk-byte emulation. The
 *           256-byte firmware is a ProDOS driver and a boot routine
 *           that both hand the work to Io(): the driver leaves its
 *           command block in zero page $42-$47 and writes register 0,
 *           and the transfer is done before the next instruction.
 * ============================================================
*/

#ifndef HARD_DISK_CARD_H
#define HARD_DISK_CARD_H

#include "Card.h"
#include "../Tools/FileSystem.h"

// What the card's transfers need of the machine: the Apple's memory as the
// CPU sees it at that moment.
class CardBus
{
public:
	virtual ~CardBus() {}
	virtual BYTE Read(WORD addr) = 0;
	virtual void Write(WORD addr, BYTE value) = 0;
};

class HardDiskCard : public Card
{
public:
	static const int BLOCK_BYTES = 512;
	static const int MAX_BLOCKS  = 65535;            // ProDOS's 32 MB

	// ProDOS error codes, returned in A with carry set
	static const BYTE ERR_NONE          = 0x00;
	static const BYTE ERR_IO            = 0x27;
	static const BYTE ERR_NO_DEVICE     = 0x28;
	static const BYTE ERR_WRITE_PROTECT = 0x2B;

	HardDiskCard();

	// The firmware carries the slot number: call before the card goes in.
	void Configure(int slot, CardBus* bus);

	BYTE Io(int reg, BYTE value, bool write) override;
	// NULL with no image, so the autostart ROM's boot scan passes it by
	BYTE* SlotRom() override;

	// The images this card takes rather than the Disk II: .hdv, .2mg, and a
	// .po that is not exactly 140K.
	static bool IsHardDiskImage(const char* path);

	bool Mount(const char* path);
	void Unmount();
	bool HasImage() const { return filename[0] != '\0'; }
	const char* ImageName() const { return filename; }
	uint16_t Blocks() const { return blocks; }

private:
	BYTE rom[256];
	int slot;
	CardBus* bus;
	File file;                   // open for as long as the image is mounted
	char filename[400];
	uint32_t dataOffset;         // 64 after a .2mg header, else 0
	uint16_t blocks;
	bool readOnly;
	BYTE result;                 // register 1: the last command's error code
	BYTE block[BLOCK_BYTES];

	void BuildRom();
	BYTE Execute();
};

#endif
