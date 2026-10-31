/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : cpu_test.cpp
 *  Module : Host-side CPU test. Runs Klaus Dormann's 6502 / 65C02
 *           functional test images against the real AppleCpu.cpp
 *           over a flat 64K memory, on the Pi rather than the ESP32.
 *           Driven by tests/host/run-cpu-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "AppleCpu.h"
#include "AppleMem.h"

// Flat 64K address space: the test images expect plain RAM everywhere,
// vectors at $FFFA-$FFFF included. These definitions stand in for
// AppleMem.cpp, which is not linked into the host build.
static BYTE flat[0x10000];

Memory::Memory() { device = NULL; ram = flat; }
Memory::~Memory() {}
BYTE Memory::ReadByte(int addr) { return flat[addr & 0xFFFF]; }
void Memory::WriteByte(int addr, BYTE value) { flat[addr & 0xFFFF] = value; }
WORD Memory::ReadWord(int addr) { return ReadByte(addr) | (ReadByte(addr + 1) << 8); }
void Memory::WriteWord(WORD value, int addr) { WriteByte(addr, value & 0xFF); WriteByte(addr + 1, value >> 8); }
void Memory::ResetRam() {}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s <image.bin> <success-addr-hex> [nmos|cmos|cmos-rockwell]\n", argv[0]);
		return 2;
	}

	FILE* fp = fopen(argv[1], "rb");
	if (!fp)
	{
		perror(argv[1]);
		return 2;
	}
	size_t n = fread(flat, 1, sizeof(flat), fp);
	fclose(fp);
	if (n != sizeof(flat))
	{
		fprintf(stderr, "%s: expected a 64K image, got %zu bytes\n", argv[1], n);
		return 2;
	}

	WORD success = (WORD)strtol(argv[2], NULL, 16);
	const char* variant = argc > 3 ? argv[3] : "nmos";

	Memory mem;
	CPU cpu;
	cpu.fastDiskDelay = false;          // $BD9E is ordinary test code here
	cpu.cmos = strncmp(variant, "cmos", 4) == 0;
	cpu.rockwell = strcmp(variant, "cmos-rockwell") == 0;
	cpu.PC = 0x0400;

	// Every failure in these suites is a jump or branch to itself, and so is
	// success, at a known address. Stop when an instruction leaves PC alone.
	for (long long steps = 0; steps < 200000000LL; steps++)
	{
		WORD pc = cpu.PC;
		cpu.Run(mem, 1);
		if (cpu.PC != pc)
			continue;

		if (pc == success)
		{
			printf("PASS %s [%s] (%lld instructions)\n", argv[1], variant, steps);
			return 0;
		}
		// $0200 holds the number of the test case in progress
		printf("FAIL %s [%s]: trapped at $%04X, test case $%02X, "
		       "A=%02X X=%02X Y=%02X P=%02X SP=%02X\n",
		       argv[1], variant, pc, flat[0x200], cpu.A, cpu.X, cpu.Y, cpu._PS, cpu.SP);
		// the suites keep their operands and expected results in page zero
		printf("  zp $00:");
		for (int i = 0; i < 0x20; i++)
			printf(" %02X", flat[i]);
		printf("\n");
		return 1;
	}

	printf("FAIL %s [%s]: no trap after 200M instructions (PC=$%04X)\n", argv[1], variant, cpu.PC);
	return 1;
}
