/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleMem.h
 *  Module : Apple II memory subsystem (interface). Block pointers,
 *           Language Card and IIe MMU switches, the read/write
 *           page tables and the ReadByte/WriteByte entry points.
 * ============================================================
*/

#ifndef MEMORY_H
#define MEMORY_H

#include "Predef.h"
#include "Apple2Device.h"

// The 64K address space as 256 pages of 256 bytes. readPage/writePage hold
// the host buffer behind each page for the current bank-switch settings; a
// NULL entry sends the access down the slow path, to the soft switches and
// peripheral ROM logic in $C000-$CFFF. The tables are rebuilt by Remap()
// and RemapLanguageCard(), which must run whenever a banking flag changes.
class Memory
{
	public:
		// LC -> Language Card
		bool LCWritable;
		bool LCReadable;
		bool LCBank2Enable;			// bank 2 enabled
		bool LCPreWriteFlipflop;	// pre-write flip flop

		// IIe MMU switches: written at $C000-$C00F, reported at $C011-$C018.
		// They all stay false on the ][+.
		bool store80;               // 80STORE: PAGE2 picks main/aux for the display pages
		bool ramRd;                 // RAMRD: $0200-$BFFF reads come from aux
		bool ramWrt;                // RAMWRT: $0200-$BFFF writes go to aux
		bool altZp;                 // ALTZP: page zero, stack and Language Card in aux
		bool intCxRom;              // INTCXROM: all of $C100-$CFFF from internal ROM
		bool slotC3Rom;             // SLOTC3ROM: $C300 from slot 3 instead of internal ROM
		bool intC8Rom;              // internal $C800-$CFFF: set by touching $C3xx
		int  expSlot;               // slot whose card holds $C800-$CFFF, 0 = none
		bool page2;                 // copies of the video switches 80STORE depends on
		bool hires;

		BYTE *ram;                  // 48K main RAM, $0000-$BFFF
		BYTE *rom;                  // system ROM: 12K = $D000-$FFFF, 16K = $C000-$FFFF
		BYTE *lgc;                  // Language Card 12K, $D000-$FFFF (bank 1 at $D000)
		BYTE *bk2;                  // Language Card bank 2, 4K at $D000-$DFFF
		BYTE *auxRam;               // IIe: 48K auxiliary RAM (NULL on the ][+)
		BYTE *auxLgc;               // IIe: auxiliary Language Card
		BYTE *auxBk2;
		int   romSize;
		bool  iie;                  // the IIe MMU and its 64K of aux memory are present

		BYTE **readPage;            // [256], in internal SRAM like the blocks
		BYTE **writePage;           // [256]

		Apple2Device* device;

	public:
		Memory();
		~Memory();

		void Create(int romSize, bool iie);
		void Destroy();
		void Reset();
		// The RESET line: MMU and Language Card switches back to power-up,
		// memory contents kept.
		void ResetSwitches();
		void Remap();
		void RemapLanguageCard();   // $D000-$FFFF only, after an LC switch

		// Every CPU access comes through here: one table lookup on the fast
		// path. Out of line and in IRAM on purpose, see AppleMem.cpp.
		BYTE ReadByte(int addr);
		void WriteByte(int addr, BYTE value);

		WORD ReadWord(int addr);
		void WriteWord(WORD value, int addr);

		WORD UpLoadProgram(BYTE *code, int codesize);
		void UpLoadProgram(int startPos, BYTE *code, int codesize);

		void UpLoadToRom(BYTE* code);
		void ResetRam();

		void Dump(FILE* fp);
		void LoadDump(FILE* fp);

	private:
		BYTE CxAccess(int addr, BYTE value, bool write);
};


#endif
