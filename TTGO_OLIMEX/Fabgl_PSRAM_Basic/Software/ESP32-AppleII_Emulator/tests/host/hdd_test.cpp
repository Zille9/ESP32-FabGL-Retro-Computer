/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : hdd_test.cpp
 *  Module : Host-side hard disk card test: firmware ID bytes, the
 *           ProDOS STATUS/READ/WRITE commands against a fake bus and
 *           the shimmed SD card, .2mg headers, error codes, and which
 *           images the card takes. Driven by tests/host/run-hdd-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <string>

#include "HardDiskCard.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

struct FakeBus : public CardBus
{
	BYTE mem[0x10000];
	FakeBus() { memset(mem, 0, sizeof(mem)); }
	BYTE Read(WORD addr) override { return mem[addr]; }
	void Write(WORD addr, BYTE value) override { mem[addr] = value; }
};

static const int BLOCKS = 1600;              // an 800K .po

static std::string BlockImage(int blocks)
{
	std::string s((size_t)blocks * 512, '\0');
	for (size_t i = 0; i < s.size(); i++)
		s[i] = (char)((i / 512) * 7 + (i % 512));
	return s;
}

// The ProDOS command block in zero page, then the driver's STA to register 0
static BYTE Command(HardDiskCard& card, FakeBus& bus, BYTE cmd, BYTE unit, WORD buf, WORD block)
{
	bus.mem[0x42] = cmd;
	bus.mem[0x43] = unit;
	bus.mem[0x44] = buf & 0xFF;
	bus.mem[0x45] = buf >> 8;
	bus.mem[0x46] = block & 0xFF;
	bus.mem[0x47] = block >> 8;
	card.Io(0x0, 0, true);
	return card.Io(0x1, 0, false);
}

static void TestTypes()
{
	HostFiles().clear();
	HostFiles()["/small.po"] = std::string(143360, '\0');
	HostFiles()["/big.po"] = std::string(819200, '\0');
	expect(HardDiskCard::IsHardDiskImage("/a.hdv"), ".hdv is a hard disk");
	expect(HardDiskCard::IsHardDiskImage("/b.2MG"), ".2mg is a hard disk");
	expect(HardDiskCard::IsHardDiskImage("/big.po"), "an 800K .po is a hard disk");
	expect(!HardDiskCard::IsHardDiskImage("/small.po"), "a 140K .po is a floppy");
	expect(!HardDiskCard::IsHardDiskImage("/c.dsk"), ".dsk is a floppy");
	expect(!HardDiskCard::IsHardDiskImage("/missing.po"), "a .po that cannot be opened is not taken");
}

static void TestFirmware()
{
	HostFiles().clear();
	HostFiles()["/hd.hdv"] = BlockImage(BLOCKS);
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);
	expect(card.SlotRom() == NULL, "no image: no firmware, the boot scan passes the slot by");
	expect(card.Mount("/hd.hdv"), "mounts a .hdv");
	BYTE* rom = card.SlotRom();
	expect(rom != NULL, "image: firmware present");
	if (!rom)
		return;
	expect(rom[1] == 0x20 && rom[3] == 0x00 && rom[5] == 0x03, "autostart boot ID bytes");
	expect(rom[7] == 0x3C, "a block device, not SmartPort");
	expect(rom[0xFF] == 0x40, "driver entry at $Cn40");
	expect(rom[0xFE] == 0x07, "status byte: read, write, status; one volume");
	expect(rom[0xFC] == (BLOCKS & 0xFF) && rom[0xFD] == (BLOCKS >> 8), "block count");
	expect(rom[0x40] == 0x8D && rom[0x41] == 0xF0 && rom[0x42] == 0xC0, "driver: STA $C0F0 for slot 7");
	expect(rom[0x1C] == 0x20 && rom[0x1D] == 0x40 && rom[0x1E] == 0xC7, "boot: JSR $C740");
	expect(rom[0x0D] == 0x70 && rom[0x27] == 0x70, "boot: unit and X are slot 7 * 16");

	// a blank image (block 0 starting $00) or a read error must not hang the
	// boot: back to the autostart ROM's slot scan, so the Disk II still boots
	const int fail = 0x2B;
	expect(rom[0x1F] == 0xB0 && 0x21 + rom[0x20] == fail, "boot: BCS to fail on a read error");
	expect(rom[0x21] == 0xAD && rom[0x22] == 0x00 && rom[0x23] == 0x08, "boot: LDA $0800");
	expect(rom[0x24] == 0xF0 && 0x26 + rom[0x25] == fail, "boot: BEQ to fail on an empty boot block");
	expect(rom[0x28] == 0x4C && rom[0x29] == 0x01 && rom[0x2A] == 0x08, "boot: JMP $0801");
	expect(rom[fail] == 0xA5 && rom[fail + 1] == 0x01 && rom[fail + 2] == 0xC9 && rom[fail + 3] == 0xC7,
	       "fail: came from the slot scan ($01 = $C7)?");
	expect(rom[fail + 4] == 0xD0 && fail + 6 + rom[fail + 5] == 0x34, "fail: BNE to BASIC otherwise");
	expect(rom[fail + 6] == 0x4C && rom[fail + 7] == 0xBA && rom[fail + 8] == 0xFA, "fail: JMP $FABA, the scan's next slot");
	expect(rom[0x34] == 0x4C && rom[0x35] == 0x00 && rom[0x36] == 0xE0, "BASIC: JMP $E000");
	card.Unmount();
	expect(card.SlotRom() == NULL && !card.HasImage(), "unmounted: firmware gone");
}

static void TestCommands()
{
	HostFiles().clear();
	std::string image = BlockImage(BLOCKS);
	HostFiles()["/hd.po"] = image;
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);
	card.Mount("/hd.po");

	expect(Command(card, bus, 0, 0x70, 0, 0) == HardDiskCard::ERR_NONE, "STATUS ok");
	expect(card.Io(0x2, 0, false) == (BLOCKS & 0xFF) && card.Io(0x3, 0, false) == (BLOCKS >> 8),
	       "STATUS: block count in registers 2/3");

	expect(Command(card, bus, 1, 0x70, 0x2000, 5) == HardDiskCard::ERR_NONE, "READ block 5 ok");
	expect(memcmp(bus.mem + 0x2000, image.data() + 5 * 512, 512) == 0, "READ: block 5 in memory");

	for (int i = 0; i < 512; i++)
		bus.mem[0x4000 + i] = (BYTE)(0xA5 ^ i);
	expect(Command(card, bus, 2, 0x70, 0x4000, 7) == HardDiskCard::ERR_NONE, "WRITE block 7 ok");
	expect(memcmp(HostFiles()["/hd.po"].data() + 7 * 512, bus.mem + 0x4000, 512) == 0, "WRITE: block 7 in the file");
	expect(HostFiles()["/hd.po"].size() == image.size(), "WRITE: file keeps its size");

	expect(Command(card, bus, 1, 0x70, 0x2000, BLOCKS) == HardDiskCard::ERR_IO, "block past the end: I/O error");
	expect(Command(card, bus, 1, 0xF0, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "drive 2: no device");
	expect(Command(card, bus, 1, 0x60, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "another slot: no device");
	expect(Command(card, bus, 9, 0x70, 0x2000, 0) == HardDiskCard::ERR_IO, "unknown command: I/O error");

	card.Unmount();
	expect(Command(card, bus, 1, 0x70, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "no image: no device");
}

static std::string TwoImg(uint32_t format, uint32_t flags, const std::string& data)
{
	std::string h(64, '\0');
	memcpy(&h[0], "2IMG", 4);
	auto le32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; i++) h[at + i] = (char)(v >> (8 * i)); };
	le32(0x0C, format);
	le32(0x10, flags);
	le32(0x14, (uint32_t)(data.size() / 512));
	le32(0x18, 64);
	le32(0x1C, (uint32_t)data.size());
	return h + data;
}

static void Test2mg()
{
	HostFiles().clear();
	std::string data = BlockImage(280);
	HostFiles()["/p.2mg"] = TwoImg(1, 0, data);
	HostFiles()["/dos.2mg"] = TwoImg(0, 0, data);
	HostFiles()["/locked.2mg"] = TwoImg(1, 0x80000000u, data);
	HostFiles()["/junk.2mg"] = std::string(1024, 'x');
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);

	expect(card.Mount("/p.2mg") && card.Blocks() == 280, ".2mg: mounts, 280 blocks");
	expect(Command(card, bus, 1, 0x70, 0x2000, 0) == HardDiskCard::ERR_NONE
	       && memcmp(bus.mem + 0x2000, data.data(), 512) == 0, ".2mg: block 0 is after the header");
	expect(!card.Mount("/dos.2mg"), ".2mg in DOS order is refused");
	expect(!card.Mount("/junk.2mg"), "a file without the 2IMG header is refused");
	expect(card.Mount("/locked.2mg"), "a locked .2mg mounts");
	expect(Command(card, bus, 2, 0x70, 0x2000, 0) == HardDiskCard::ERR_WRITE_PROTECT, "and refuses writes");
}

int main()
{
	TestTypes();
	TestFirmware();
	TestCommands();
	Test2mg();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
