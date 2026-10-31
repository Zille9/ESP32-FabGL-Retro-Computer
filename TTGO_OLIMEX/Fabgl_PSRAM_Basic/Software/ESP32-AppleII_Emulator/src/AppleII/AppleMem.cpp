/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleMem.cpp
 *  Module : Apple II memory subsystem. Allocates the RAM / ROM /
 *           Language Card blocks (plus the IIe's auxiliary 64K) and
 *           the page tables in internal SRAM (never PSRAM), rebuilds
 *           the page tables on bank switches - Language Card and the
 *           IIe MMU - maps slot and internal $Cxxx ROM, and routes
 *           the I/O page to Apple2Device::SoftSwitch.
 * ============================================================
*/

#include "AppleMem.h"
#include "../Tools/Log.h"
#include <esp_heap_caps.h>

// Writes to ROM land here and are never read back. A sink rather than a
// NULL page keeps the write fast path free of any ROM check.
static BYTE romSink[256];

Memory::Memory()
{
	DEBUG_PRINTLN("Construct Memory");
	device = NULL;
	ram = rom = lgc = bk2 = NULL;
	auxRam = auxLgc = auxBk2 = NULL;
	readPage = writePage = NULL;
	romSize = 0;
	iie = false;
}

Memory::~Memory()
{
	Destroy();
}

// The 6502 core hits these blocks on nearly every emulated cycle, so they must
// live in internal SRAM. PSRAM is on a 40MHz SPI bus and the 32K data cache
// cannot hold a 76K working set, so a PSRAM-backed address space stalls the
// interpreter on almost every access.
//
// Plain malloc() is not enough: the Arduino ESP32 core builds with
// CONFIG_SPIRAM_USE_MALLOC, so allocations above the always-internal threshold
// may be served from PSRAM. Ask for MALLOC_CAP_INTERNAL explicitly.
static BYTE* AllocFast(const char* name, size_t size)
{
	BYTE* p = (BYTE*)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	if (p)
	{
		Serial.printf("[mem] %-4s %6u bytes @ %p (internal)\n", name, (unsigned)size, p);
		return p;
	}

	// Not enough internal RAM left - fall back so we degrade in speed, not function.
	p = (BYTE*)ps_malloc(size);
	Serial.printf("[mem] %-4s %6u bytes @ %p (PSRAM FALLBACK - slow)\n", name, (unsigned)size, p);
	return p;
}

void Memory::Create(int size, bool withIIe)
{
	romSize = size;
	iie = withIIe;

	// Allocated hottest-first so that if internal RAM runs short, the least
	// frequently accessed blocks are the ones pushed out to PSRAM. The page
	// tables are read on every single access; aux memory is used least.
	readPage  = (BYTE**)AllocFast("rdpg", 256 * sizeof(BYTE*));
	writePage = (BYTE**)AllocFast("wrpg", 256 * sizeof(BYTE*));
	ram = AllocFast("ram", RAMSIZE);  // 48K of ram in $000-$BFFF
	rom = AllocFast("rom", romSize);  // 12K or 16K of system rom
	lgc = AllocFast("lgc", LGCSIZE);
	bk2 = AllocFast("bk2", BK2SIZE);
	if (iie)
	{
		auxRam = AllocFast("aux", RAMSIZE);
		auxLgc = AllocFast("alc", LGCSIZE);
		auxBk2 = AllocFast("ab2", BK2SIZE);
	}

	Serial.printf("[mem] internal free after alloc: %u bytes\n",
	              (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

	memset(rom, 0, romSize);          // filled by Apple2Machine::LoadRoms
	Reset();
}

void Memory::Destroy()
{
	free(readPage);
	free(writePage);
	free(ram);
	free(rom);
	free(lgc);
	free(bk2);
	free(auxRam);
	free(auxLgc);
	free(auxBk2);
	readPage = writePage = NULL;
	ram = rom = lgc = bk2 = NULL;
	auxRam = auxLgc = auxBk2 = NULL;
}

void Memory::Reset()
{
	// The system ROM is loaded once at boot and survives resets.
	memset(ram, 0, RAMSIZE);
	memset(lgc, 0, LGCSIZE);
	memset(bk2, 0, BK2SIZE);
	if (iie)
	{
		memset(auxRam, 0, RAMSIZE);
		memset(auxLgc, 0, LGCSIZE);
		memset(auxBk2, 0, BK2SIZE);
	}
	page2 = hires = false;

	ResetSwitches();
}

void Memory::ResetSwitches()
{
	LCWritable = true;
	LCReadable = false;
	LCBank2Enable = true;
	LCPreWriteFlipflop = false;

	store80 = ramRd = ramWrt = altZp = false;
	intCxRom = slotC3Rom = intC8Rom = false;
	expSlot = 0;

	Remap();
}

void Memory::Remap()
{
	// $0000-$BFFF: main or aux. ALTZP decides for page zero and the stack,
	// RAMRD/RAMWRT for the rest, except that with 80STORE on PAGE2 picks the
	// bank for the text page (and the hires page when HIRES is on).
	for (int p = 0x00; p < 0xC0; p++)
	{
		bool rdAux, wrAux;
		if (p < 0x02)
			rdAux = wrAux = altZp;
		else
		{
			rdAux = ramRd;
			wrAux = ramWrt;
			if (store80 && ((p >= 0x04 && p < 0x08) || (hires && p >= 0x20 && p < 0x40)))
				rdAux = wrAux = page2;
		}
		readPage[p]  = (rdAux ? auxRam : ram) + (p << 8);
		writePage[p] = (wrAux ? auxRam : ram) + (p << 8);
	}

	// $C000-$CFFF: soft switches and peripheral ROM. Pages mapped below read
	// as plain memory; the rest take the slow path. Before the device is
	// wired up (during Create) there are no cards yet.
	for (int p = 0xC0; p < 0xD0; p++)
		readPage[p] = writePage[p] = NULL;

	for (int slot = 1; slot < 8; slot++)
	{
		BYTE* page = NULL;
		bool internal = iie && (intCxRom || (slot == 3 && !slotC3Rom));
		if (internal)
		{
			// the internal $C3xx ROM stays on the slow path until the first
			// access to it has set INTC8ROM
			if (slot != 3 || intCxRom || intC8Rom)
				page = rom + (slot << 8);
		}
		else if (device && device->slots[slot])
		{
			// a card with an expansion ROM keeps its $Cn00 page on the slow
			// path until an access there has selected the slot, which is how
			// the access gets seen at all
			Card* card = device->slots[slot];
			if (!card->ExpansionRom() || expSlot == slot)
				page = card->SlotRom();
		}
		readPage[0xC0 + slot] = page;
	}

	// $C800-$CEFF: the IIe's internal ROM, which wins, or the expansion ROM
	// of the selected slot. $CFxx always stays on the slow path: an access
	// to $CFFF releases both.
	if (iie && (intCxRom || intC8Rom))
	{
		for (int p = 0xC8; p < 0xCF; p++)
			readPage[p] = rom + ((p - 0xC0) << 8);
	}
	else if (expSlot && device && device->slots[expSlot])
	{
		BYTE* exp = device->slots[expSlot]->ExpansionRom();
		if (exp)
			for (int p = 0xC8; p < 0xCF; p++)
				readPage[p] = exp + ((p - 0xC8) << 8);
	}

	RemapLanguageCard();
}

// $D000-$FFFF reads ROM, or Language Card RAM when it is read-enabled; writes
// reach the Language Card only when it is write-enabled. Bank 2 replaces bank
// 1 in $D000-$DFFF. On the IIe, ALTZP puts the aux Language Card there.
void Memory::RemapLanguageCard()
{
	BYTE* romD000 = rom + romSize - 0x3000;
	BYTE* lcMain  = altZp ? auxLgc : lgc;
	BYTE* lcBank2 = altZp ? auxBk2 : bk2;
	for (int p = 0xD0; p < 0x100; p++)
	{
		int off = (p - 0xD0) << 8;
		BYTE* lc = (LCBank2Enable && p < 0xE0) ? lcBank2 + off : lcMain + off;
		readPage[p]  = LCReadable ? lc : romD000 + off;
		writePage[p] = LCWritable ? lc : romSink;
	}
}

// $C100-$CFFF on the slow path: pages whose access changes which ROM is
// visible (a card's $Cn00 selects its expansion ROM, the IIe's $C3xx sets
// INTC8ROM, $CFFF releases both), and pages with nothing mapped. Writes
// only have the side effects.
BYTE Memory::CxAccess(int address, BYTE value, bool write)
{
	int page = address >> 8;
	if (address == 0xCFFF)
	{
		if (intC8Rom || expSlot)
		{
			intC8Rom = false;
			expSlot = 0;
			Remap();
		}
	}
	else if (iie && page == 0xC3 && !slotC3Rom && !intC8Rom)
	{
		intC8Rom = true;
		Remap();
	}
	else if (page >= 0xC1 && page <= 0xC7 && !(iie && intCxRom))
	{
		int slot = page & 0x07;
		Card* card = (device && expSlot != slot) ? device->slots[slot] : NULL;
		if (card && card->ExpansionRom())
		{
			expSlot = slot;
			Remap();
		}
	}

	if (write)
		return 0;

	BYTE* p = readPage[page];
	if (p)
		return p[address & 0xFF];

	bool internal = iie && (intCxRom || (page == 0xC3 && !slotC3Rom) || (page >= 0xC8 && intC8Rom));
	if (internal)
		return rom[address - 0xC000];
	// $CFxx of the selected card: the only expansion ROM page never mapped
	if (page == 0xCF && expSlot && device && device->slots[expSlot])
	{
		BYTE* exp = device->slots[expSlot]->ExpansionRom();
		if (exp)
			return exp[address - 0xC800];
	}
	return 0;
}

// The CPU's only way into memory. A NULL page means $C000-$CFFF: soft
// switches and peripheral ROM, handled by the device (and on the IIe, the
// $Cxxx ROM switching above). In IRAM because it runs for every emulated
// memory access, and IRAM cannot miss the flash cache that the rest of the
// interpreter competes for.
BYTE IRAM_ATTR Memory::ReadByte(int address)
{
	BYTE* page = readPage[(address >> 8) & 0xFF];
	if (page)
		return page[address & 0xFF];
	if ((address & 0xF000) == 0xC000)
	{
		if (address >= 0xC100)
			return CxAccess(address, 0, false);
		return device->SoftSwitch(this, address, 0, false);
	}
	return 0;
}

void IRAM_ATTR Memory::WriteByte(int address, BYTE value)
{
	BYTE* page = writePage[(address >> 8) & 0xFF];
	if (page)
		page[address & 0xFF] = value;
	else if ((address & 0xF000) == 0xC000)
	{
		if (address >= 0xC100)
			CxAccess(address, value, true);
		else
			device->SoftSwitch(this, address, value, true);
	}
}

WORD Memory::ReadWord(int addr)
{
	BYTE m0 = ReadByte(addr);
	BYTE m1 = ReadByte(addr + 1);
	WORD w = (m1 << 8) | m0;
	return w;
}

// little-endian, the mirror of ReadWord
void Memory::WriteWord(WORD value, int addr)
{
	WriteByte(addr, value & 0xFF);
	WriteByte(addr + 1, value >> 8);
}


void Memory::UpLoadToRom(BYTE* code)
{
	memcpy(rom, code, ROMSIZE);
}

void Memory::ResetRam()
{
	memset(ram, 0, RAMSIZE);
}

void Memory::Dump(FILE* fp)
{

}

void Memory::LoadDump(FILE* fp)
{

}
