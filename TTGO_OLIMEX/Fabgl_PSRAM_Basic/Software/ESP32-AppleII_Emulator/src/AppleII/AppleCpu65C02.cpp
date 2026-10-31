/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleCpu65C02.cpp
 *  Module : 65C02 extensions to the 6502 core. Every opcode the
 *           65C02 adds is undocumented on the NMOS part, so
 *           CPU::Run dispatches here from its default case when
 *           CPU::cmos is set (enhanced IIe, later the IIc). The
 *           Rockwell bit instructions are gated separately on
 *           CPU::rockwell; without it they are 1-byte NOPs, as on
 *           the 65C02 fitted to the enhanced IIe.
 * ============================================================
*/

#include "AppleCpu.h"
#include "AppleMem.h"

// (zp) : 65C02 zero page indirect. The pointer wraps within page zero.
WORD CPU::addr_mode_ZPI(Memory& mem, Cycles& cycle)
{
	BYTE zp = Fetch(mem, cycle);
	BYTE lo = ReadByte(mem, zp, cycle);
	BYTE hi = ReadByte(mem, (BYTE)(zp + 1), cycle);
	return lo | (hi << 8);
}

bool CPU::ExecuteCmos(BYTE inst, Memory& mem, Cycles& cycle)
{
	// Rockwell / WDC bit instructions: RMBn/SMBn zp ($x7), BBRn/BBSn zp,rel ($xF)
	if ((inst & 0x0F) == 0x07 || (inst & 0x0F) == 0x0F)
	{
		if (!rockwell)
			return true;                     // 1-byte, 1-cycle NOP
		BYTE bit = 1 << ((inst >> 4) & 7);
		bool set = (inst & 0x80) != 0;
		WORD zp = addr_mode_ZP(mem, cycle);
		BYTE v = ReadByte(mem, zp, cycle);
		if ((inst & 0x0F) == 0x07)
		{
			v = set ? (v | bit) : (v & ~bit);
			cycle--;
			WriteByte(mem, v, zp, cycle);
		}
		else
			Execute_BRANCH((v & bit) != 0, set, mem, cycle);
		return true;
	}

	switch (inst)
	{
		case 0x80:                           // BRA
			Execute_BRANCH(true, true, mem, cycle);
			return true;

		case 0xDA:                           // PHX
			PushStackByte(mem, X, cycle);
			return true;
		case 0x5A:                           // PHY
			PushStackByte(mem, Y, cycle);
			return true;
		case 0xFA:                           // PLX
			X = PopStackByte(mem, cycle);
			cycle--;
			SetZeroNegative(X);
			return true;
		case 0x7A:                           // PLY
			Y = PopStackByte(mem, cycle);
			cycle--;
			SetZeroNegative(Y);
			return true;

		case 0x64:                           // STZ zp
			WriteByte(mem, 0, addr_mode_ZP(mem, cycle), cycle);
			return true;
		case 0x74:                           // STZ zp,X
			WriteByte(mem, 0, addr_mode_ZPX(mem, cycle), cycle);
			return true;
		case 0x9C:                           // STZ abs
			WriteByte(mem, 0, addr_mode_ABS(mem, cycle), cycle);
			return true;
		case 0x9E:                           // STZ abs,X
			WriteByte(mem, 0, addr_mode_ABSX_NoPage(mem, cycle), cycle);
			return true;

		case 0x04: case 0x0C:                // TSB zp / abs
		case 0x14: case 0x1C:                // TRB zp / abs
		{
			WORD addr = (inst & 0x08) ? addr_mode_ABS(mem, cycle) : addr_mode_ZP(mem, cycle);
			BYTE v = ReadByte(mem, addr, cycle);
			Flag.Z = (A & v) == 0;
			v = (inst & 0x10) ? (v & ~A) : (v | A);
			cycle--;
			WriteByte(mem, v, addr, cycle);
			return true;
		}

		case 0x1A:                           // INC A
			A++;
			cycle--;
			SetZeroNegative(A);
			return true;
		case 0x3A:                           // DEC A
			A--;
			cycle--;
			SetZeroNegative(A);
			return true;

		case 0x89:                           // BIT # : immediate affects only Z
			Flag.Z = (A & Fetch(mem, cycle)) == 0;
			return true;
		case 0x34: case 0x3C:                // BIT zp,X / abs,X
		{
			WORD addr = (inst == 0x34) ? addr_mode_ZPX(mem, cycle) : addr_mode_ABSX(mem, cycle);
			BYTE R = ReadByte(mem, addr, cycle);
			Flag.Z = (A & R) == 0;
			Flag.N = (R & FLAG_NEGATIVE) != 0;
			Flag.V = (R & FLAG_OVERFLOW) != 0;
			return true;
		}

		case 0x7C:                           // JMP (abs,X)
		{
			WORD ptr = FetchWord(mem, cycle) + X;
			BYTE lo = ReadByte(mem, ptr, cycle);
			BYTE hi = ReadByte(mem, (WORD)(ptr + 1), cycle);
			PC = lo | (hi << 8);
			cycle--;
			return true;
		}

		// (zp) addressing for the eight accumulator instructions
		case 0x12:                           // ORA (zp)
			A |= ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle);
			SetZeroNegative(A);
			return true;
		case 0x32:                           // AND (zp)
			A &= ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle);
			SetZeroNegative(A);
			return true;
		case 0x52:                           // EOR (zp)
			A ^= ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle);
			SetZeroNegative(A);
			return true;
		case 0x72:                           // ADC (zp)
			Execute_ADC(ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle));
			return true;
		case 0x92:                           // STA (zp)
			WriteByte(mem, A, addr_mode_ZPI(mem, cycle), cycle);
			return true;
		case 0xB2:                           // LDA (zp)
			A = ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle);
			SetZeroNegative(A);
			return true;
		case 0xD2:                           // CMP (zp)
			Execute_CMP(ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle));
			return true;
		case 0xF2:                           // SBC (zp)
			Execute_SBC(ReadByte(mem, addr_mode_ZPI(mem, cycle), cycle));
			return true;

		// The rest are NOPs on the 65C02, but with fixed lengths that code
		// relying on them (or skipping over them) depends on.
		case 0x02: case 0x22: case 0x42: case 0x62:
		case 0x82: case 0xC2: case 0xE2:     // 2 bytes, 2 cycles
			Fetch(mem, cycle);
			return true;
		case 0x44:                           // 2 bytes, 3 cycles
			Fetch(mem, cycle);
			cycle--;
			return true;
		case 0x54: case 0xD4: case 0xF4:     // 2 bytes, 4 cycles
			Fetch(mem, cycle);
			cycle -= 2;
			return true;
		case 0x5C:                           // 3 bytes, 8 cycles
			FetchWord(mem, cycle);
			cycle -= 5;
			return true;
		case 0xDC: case 0xFC:                // 3 bytes, 4 cycles
			FetchWord(mem, cycle);
			cycle--;
			return true;
	}

	// $x3 and $xB, WDC's WAI ($CB) and STP ($DB) included: 1-byte, 1-cycle
	// NOPs. The opcode fetch has already paid the cycle.
	if ((inst & 0x0F) == 0x03 || (inst & 0x0F) == 0x0B)
		return true;

	return false;
}
