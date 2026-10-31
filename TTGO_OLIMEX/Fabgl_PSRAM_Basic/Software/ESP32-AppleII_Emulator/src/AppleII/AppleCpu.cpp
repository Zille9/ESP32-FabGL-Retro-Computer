/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleCpu.cpp
 *  Module : MOS 6502 CPU emulator. Instruction-level interpreter
 *           for the full instruction set; every memory access is
 *           routed through Memory. 65C02-only opcodes live in
 *           AppleCpu65C02.cpp. Also implements fastDiskDelay,
 *           which skips the DOS 3.3 RWTS drive spin-up wait loop
 *           at $BD9E after verifying its opcode signature.
 * ============================================================
*/


/*
	MOS 6502 CPU Emulator
*/
#include <assert.h>
#include "AppleCpu.h"
#include "AppleMem.h"
#include "../Tools/Log.h"

#define USEOLD	0

CPU::CPU()
{
	fastDiskDelay = true;
	cmos = false;
	rockwell = false;
	runCycle = NULL;
	runStartTick = 0;
	runBudget = 0;
	DEBUG_PRINTLN("Construct CPU");
	tick = 0;
	InitInstructionName();
	Reset();
	enableLog = false;
}

CPU::~CPU()
{
}

void CPU::InitInstructionName()
{
/*
	constexpr int instNum = 151;
	BYTE opcode[instNum] = {
		0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1, 0xA2, 0xA6, 0xB6, 0xAE, 0xBE, 0xA0, 0xA4, 0xB4, 0xAC,
		0xBC, 0xEA, 0x20, 0x4C, 0x6C, 0x60, 0x85, 0x95, 0x8D, 0x9D, 0x99, 0x81, 0x91, 0x86, 0x96, 0x8E, 0x84,
		0x94, 0x8C, 0xBA, 0x9A, 0X48, 0x68, 0x28, 0x08, 0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31, 0x09,
		0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11, 0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51, 0x24, 0x2C,
		0xAA, 0xA8, 0x8A, 0x98, 0xE8, 0xC8, 0xCA, 0x88, 0xE6, 0xF6, 0xEE, 0xFE, 0xC6, 0xD6, 0xCE, 0xDE, 0x69,
		0x65, 0x75, 0x6D, 0x7D, 0x79, 0x61, 0x71, 0xE9, 0xE5, 0xF5, 0xED, 0xFD, 0xF9, 0xE1, 0xF1, 0xC9, 0xC5,
		0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1, 0xE0, 0xE4, 0xEC, 0XC0, 0XC4, 0XCC, 0x0A, 0x06, 0x16, 0x0E, 0x1E,
		0x4A, 0x46, 0x56, 0x4E, 0x5E, 0x2A, 0x26, 0x36, 0x2E, 0x3E, 0x6A, 0x66, 0x76, 0x6E, 0x7E, 0x00, 0x40,
		0x90, 0xB0, 0xF0, 0x30, 0xD0, 0x10, 0x50, 0x70, 0x18, 0xD8, 0x58, 0xB8, 0x38, 0xF8, 0x78,
	};

	std::string inst_str[instNum] = {
		"LDA_IM", "LDA_ZP", "LDA_ZPX", "LDA_ABS", "LDA_ABSX", "LDA_ABSY", "LDA_INDX", "LDA_INDY",
		"LDX_IM", "LDX_ZP", "LDX_ZPY", "LDX_ABS", "LDX_ABSY", "LDY_IM", "LDY_ZP", "LDY_ZPX",
		"LDY_ABS", "LDY_ABSX", "NOP", "JSR", "JMP_ABS", "JMP_IND", "RTS", "STA_ZP", "STA_ZPX",
		"STA_ABS", "STA_ABSX", "STA_ABSY", "STA_INDX", "STA_INDY", "STX_ZP", "STX_ZPY", "STX_ABS", "STY_ZP",
		"STY_ZPX", "STY_ABS", "TSX", "TXS", "PHA", "PLA", "PLP", "PHP", "AND_IM", "AND_ZP", "AND_ZPX",
		"AND_ABS", "AND_ABSX", "AND_ABSY", "AND_INDX", "AND_INDY", "ORA_IM", "ORA_ZP", "ORA_ZPX", "ORA_ABS",
		"ORA_ABSX", "ORA_ABSY", "ORA_INDX", "ORA_INDY", "EOR_IM", "EOR_ZP", "EOR_ZPX", "EOR_ABS", "EOR_ABSX",
		"EOR_ABSY", "EOR_INDX", "EOR_INDY", "BIT_ZP", "BIT_ABS", "TAX", "TAY", "TXA", "TYA",
		"INX", "INY", "DEX", "DEY", "INC_ZP", "INC_ZPX", "INC_ABS", "INC_ABSX", "DEC_ZP", "DEC_ZPX",
		"DEC_ABS", "DEC_ABSX", "ADC_IM", "ADC_ZP", "ADC_ZPX", "ADC_ABS", "ADC_ABSX", "ADC_ABSY", "ADC_INDX",
		"ADC_INDY", "SBC_IM", "SBC_ZP", "SBC_ZPX", "SBC_ABS", "SBC_ABSX", "SBC_ABSY", "SBC_INDX", "SBC_INDY",
		"CMP_IM", "CMP_ZP", "CMP_ZPX", "CMP_ABS", "CMP_ABSX", "CMP_ABSY", "CMP_INDX", "CMP_INDY", "CPX_IM",
		"CPX_ZP", "CPX_ABS", "CPY_IM", "CPY_ZP", "CPY_ABS", "ASL", "ASL_ZP", "ASL_ZPX", "ASL_ABS",
		"ASL_ABSX", "LSR", "LSR_ZP", "LSR_ZPX", "LSR_ABS", "LSR_ABSX", "ROL", "ROL_ZP", "ROL_ZPX", "ROL_ABS",
		"ROL_ABSX", "ROR", "ROR_ZP", "ROR_ZPX", "ROR_ABS", "ROR_ABSX", "BRK", "RTI", "BCC", "BCS",
		"BEQ", "BMI", "BNE", "BPL", "BVC", "BVS", "CLC", "CLD", "CLI", "CLV", "SEC", "SED", "SEI",
	};
*/
//	for(int i=0; i<instNum; i++) 
//		instructionName.insert(std::make_pair(opcode[i], inst_str[i].c_str()));
}

/*
std::string CPU::GetInstName(BYTE opcode)
{
	//return instructionName[opcode];
}
*/
void CPU::Reset()
{
	A = 0;
	X = 0;
	Y = 0;
	_PS = 0;				// processor status (flags)
	SP = STACK_POS;	// Stack pointer
	PC = PC_START;		// program control
}

// reset all register
void CPU::Reset(Memory &mem)
{
	DEBUG_PRINTLN("Reset CPU");

	A = 0;
	X = 0;
	Y = 0;
	//PS = 0;				// processor status (flags)
	_PS = (_PS | FLAG_INTERRUPT_DISABLE) & ~FLAG_DECIMAL_MODE;
	SP = STACK_POS;	// Stack pointer
	// Decided after the ROM is loaded
	PC = mem.ReadByte(0xFFFC) | (mem.ReadByte(0xFFFD) << 8);
	tick = 0;
}

void CPU::Reboot(Memory& mem)
{
	DEBUG_PRINTLN("Reboot CPU");

	mem.WriteByte(0x3F4, 0);
	PC = mem.ReadByte(0xFFFC) | (mem.ReadByte(0xFFFD) << 8);
	SP = 0xFD;
	Flag.I = 1;
	Flag.Unused = 1;

	mem.ResetRam();
// 	mem.WriteByte(0x4D, 0xAA);   // Joust crashes if this memory location equals zero
// 	mem.WriteByte(0xD0, 0xAA);   // Planetoids won't work if this memory location equals zero
}
void CPU::SetPCAddress(WORD addr)
{
	PC = addr;
}

void CPU::SetRegister(BYTE type, BYTE value)
{
	switch (type)
	{
		case REGISTER_A :
			A = value;
			break;
		case REGISTER_X:
			X = value;
			break;
		case REGISTER_Y:
			Y = value;
			break;
	}
}

BYTE CPU::GetRegister(BYTE type)
{
	switch (type)
	{
	case REGISTER_A:
		return A;
	case REGISTER_X:
		return X;
	case REGISTER_Y:
		return Y;
	}

	assert(-1);
	return 0;
}

void CPU::SetFlag(BYTE flag, bool set)
{
	if (set)
		_PS |= flag;
	else
		_PS &= ~flag;
}

bool CPU::GetFlag(BYTE flag)
{
	return _PS & flag;
}

void CPU::SetZeroNegative(BYTE Register)
{
	//SetFlag(FLAG_ZERO, Register == 0);
	Flag.Z = (Register == 0);

	//SetFlag(FLAG_NEGATIVE, Register & FLAG_NEGATIVE);
	Flag.N = (Register & FLAG_NEGATIVE) > 0;
}

void CPU::SetCarryFlag(WORD value)
{
	Flag.C = (value & 0xFF00) > 0;
	//SetFlag(FLAG_CARRY, (value & 0xFF00) > 0);

}

void CPU::SetCarryFlagNegative(WORD value)
{
	Flag.C = (value < 0x100);
}

void CPU::SetOverflow(BYTE oldv0, BYTE v0, BYTE v1)
{
	bool sign0 = !((oldv0 ^ v1) & FLAG_NEGATIVE);	// sign before the calculation
	bool sign1 = ((v0 ^ v1) & FLAG_NEGATIVE);		// sign after the calculation

	// Overflow: two values of the same sign were added but the result has a different sign
	//SetFlag(FLAG_OVERFLOW, (sign0 != sign1));
	//Flag.V = (sign0 != sign1);
	Flag.V = sign0 && sign1;
}

BYTE CPU::Fetch(Memory& mem, Cycles& cycle)
{
	BYTE c = mem.ReadByte(PC++);
	cycle--;
	return c;
}

WORD CPU::FetchWord(Memory& mem, Cycles& cycle)
{
	BYTE c0 = mem.ReadByte(PC++);
	BYTE c1 = mem.ReadByte(PC++);

	// Depending on endianness, c0 and c1 may need to be swapped
	WORD w = (c1 << 8) | c0;
	cycle-=2;
	return w;
}

// Reads memory without consuming a cycle, independent of PC (zero page and the like)
BYTE CPU::ReadByte(Memory& mem, WORD addr, Cycles& cycle)
{
	BYTE c = mem.ReadByte(addr);
	cycle--;
	return c;
}

WORD CPU::ReadWord(Memory& mem, WORD addr, Cycles& cycle)
{
	WORD c = mem.ReadWord(addr);
	cycle -= 2;
	return c;
}

void CPU::WriteByte(Memory& mem, BYTE value, int addr, Cycles& cycle)
{
	mem.WriteByte(addr, value);
	cycle --;
}

void CPU::WriteWord(Memory& mem, WORD value, int addr, Cycles& cycle)
{
	mem.WriteWord(value, addr);
	cycle-=2;
}

// Why this is needed:
/*
	SP is 1 byte, and stack memory runs 0x01FF -> 0x0100, so 256 bytes.
	The address is a WORD and the stack grows downward, so it has to be computed this way.
*/
WORD CPU::GetStackAddress()
{
	WORD sp = STACK_ADDRESS + SP;
	return sp;
}

// Push a byte onto the stack
void CPU::PushStackByte(Memory& mem, BYTE value, Cycles& cycle)
{
	WriteByte(mem, value, GetStackAddress(), cycle);
	SP--;
	cycle--;
}

// Push a word onto the stack
void CPU::PushStackWord(Memory& mem, WORD value, Cycles& cycle)
{
	// Hi byte first
	WriteByte(mem, value >> 8, GetStackAddress(), cycle);
	SP--;
	// Lo byte second
	WriteByte(mem, value & 0xFF, GetStackAddress(), cycle);
	SP--;
}

// Pop 1 byte off the stack
BYTE CPU::PopStackByte(Memory& mem, Cycles& cycle)
{
 	SP++;
 	BYTE popbyte = ReadByte(mem, GetStackAddress(), cycle);
	cycle--;
	return popbyte;
}

// Pop a word off the stack
WORD CPU::PopStackWord(Memory& mem, Cycles& cycle)
{
	SP++;
	BYTE lo = ReadByte(mem, GetStackAddress(), cycle);
	SP++;
	BYTE hi = ReadByte(mem, GetStackAddress(), cycle);
	WORD popWord = lo | (hi << 8);
	cycle--;

	return popWord;
}

int CPU::Run(Memory &mem, Cycles _cycle)
{

	Cycles cycle = _cycle;
	runStartTick = tick;
	runBudget = _cycle;
	runCycle = &cycle;
	while (cycle > 0)
	{
		Cycles prevcycle = cycle;

		WORD prevPC = PC;
		if (PC == 0xBD9E && fastDiskDelay)
		{
			// DOS 3.3 RWTS waits here for a physical drive to reach speed:
			//   BD9E A0 12     LDY #$12
			//   BDA0 88        DEY
			//   BDA1 D0 FD     BNE $BDA0
			//   BDA3 E6 46     INC $46
			//   BDA5 D0 F7     BNE $BD9E
			//   BDA7 E6 47     INC $47
			//   BDA9 D0 F3     BNE $BD9E
			// It is pure cycle-burning with no I/O, and our drive is always
			// up to speed. Verify the exact opcode signature before touching
			// anything, then finish the counter and fall through to $BDAB.
			static const BYTE sig[] = { 0xA0,0x12,0x88,0xD0,0xFD,0xE6,0x46,
			                            0xD0,0xF7,0xE6,0x47,0xD0,0xF3 };
			bool match = true;
			for (int i = 0; i < (int)sizeof(sig); i++)
				if (mem.ReadByte(0xBD9E + i) != sig[i]) { match = false; break; }

			if (match)
			{
				mem.WriteByte(0x46, 0);   // what the loop would have left behind
				mem.WriteByte(0x47, 0);
				PC = 0xBDAB;
				cycle -= 10;              // token cost, not the real ~830k
				continue;
			}
		}
		// One cycle is consumed here
		BYTE inst = Fetch(mem, cycle);
		lastInst = inst;

#if CPU_TRACE
		if (enableLog)
		{
			printf("A:[%2X] X:[%2X] Y:[%2X] PC:[%4X] ", A, X, Y, prevPC);
			printf("INST : [%2X] / C:[%d] Z:[%d] I:[%d] D:[%d] B:[%d] U:[%d] V:[%d] N:[%d]\n", inst,
				Flag.C, Flag.Z, Flag.I, Flag.D, Flag.B, Flag.Unused, Flag.V, Flag.N);
		}
#endif

		switch (inst)
		{
			case LDA_IM: // 2 cycle
			{
				//LoadToRegister(mem, cycle, A);
				A = Fetch(mem, cycle);
				SetZeroNegative(A);
			}
			break;

			case LDA_ZP: // 3 cycle
			{
				// $0000 to $00FF
				// Read from zero page into A
				WORD addr = addr_mode_ZP(mem, cycle);
				// Reading zero page consumes a cycle
				A = ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case LDA_ZPX : // 4 cycle
			{
				// Read into A from (zero page address + X register)
				// With X = $0F, LDA $80,X reads into A from $80+$0F = $8F
/*				
				BYTE zpa = Fetch(mem, cycle);
				zpa += X;
				cycle--;
*/
				WORD addr = addr_mode_ZPX(mem, cycle);
				// Reading zero page consumes a cycle
				A = ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			// Instructions using absolute addressing carry a full 16-bit address to identify the target location.
			case LDA_ABS: // 4 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				A = ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case LDA_ABSX:// 4 cycle / +1 cycle when a page boundary is crossed
			{
				// Crossing a page boundary on a memory access costs an extra cycle (just how the hardware is built?)
/*				
				BYTE lo = Fetch(mem, cycle);
				BYTE hi = Fetch(mem, cycle);
				
				WORD t = lo + X;
				if( t > 0xFF ) cycle--;
				WORD addr = (lo | (hi << 8)) + X;
*/				
// 				WORD addr = FetchWord(mem, cycle);
// 				if ( (addr + X) - addr >= 0xFF )
// 					cycle--;	// page boundary crossed

				WORD addr = addr_mode_ABSX(mem, cycle);
				A = ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case LDA_ABSY:	// 4 cycle / +1 cycle when a page boundary is crossed
			{
/*				
				BYTE lo = Fetch(mem, cycle);
				BYTE hi = Fetch(mem, cycle);

				WORD t = lo + Y;
				if (t > 0xFF) cycle--;
				WORD addr = (lo | (hi << 8)) + Y;
*/
				WORD addr = addr_mode_ABSY(mem, cycle);
				A = ReadByte(mem, addr, cycle);
				SetZeroNegative(A);

			}
			break;

			case LDA_INDX:	// 6 cycle
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				A = ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			case LDA_INDY: // 5 ~ 6 cycle
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				A = ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			// LDX
			case LDX_IM:	// 2cycle
			{
				//LoadToRegister(mem, cycle, X);
				X = Fetch(mem, cycle);
				SetZeroNegative(X);
			}
			break;

			case LDX_ZP:	// 3cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
 				X = ReadByte(mem, addr, cycle);
 				SetZeroNegative(X);
			}
			break;

			case LDX_ZPY:	// 4cycle
			{
/*				
				BYTE zpage = Fetch(mem, cycle);
				zpage += Y;
				cycle--;
*/
				WORD addr = addr_mode_ZPY(mem, cycle);
				X = ReadByte(mem, addr, cycle);
				SetZeroNegative(X);
			}
			break;

			case LDX_ABS:	// 4cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				X = ReadByte(mem, addr, cycle);
				SetZeroNegative(X);

			}
			break;

			case LDX_ABSY:	// 4 ~ 5 cycle
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				X = ReadByte(mem, addr, cycle);
				SetZeroNegative(X);

			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			case LDY_IM : // 2 cycle
			{
				Y = Fetch(mem, cycle);
				SetZeroNegative(Y);
			}
			break;

			case LDY_ZP: // 3 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				Y = ReadByte(mem, addr, cycle);
				SetZeroNegative(Y);
			}
			break;

			case LDY_ZPX : // 4 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				Y = ReadByte(mem, addr, cycle);
				SetZeroNegative(Y);
			}
			break;

			case LDY_ABS : // 4 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				Y = ReadByte(mem, addr, cycle);
				SetZeroNegative(Y);
			}
			break;

			case LDY_ABSX : // 4~5 cycle
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				Y = ReadByte(mem, addr, cycle);
				SetZeroNegative(Y);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			case STA_ZP	:	// 3 cycle
			{
				// Write the A register to zero page
				WORD addr = addr_mode_ZP(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_ZPX :	// 4 cycle
			{
				// Write the A register to ZP + X
				WORD addr = addr_mode_ZPX(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_ABS:	// 4 cycle
			{
				// Write the A register to a WORD address
				WORD addr = addr_mode_ABS(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_ABSX:	// 5 cycle
			{
				// Write the A register to WORD address + X
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_ABSY:	// 5 cycle
			{
				// Write the A register to WORD address + Y
				WORD addr = addr_mode_ABSY_NoPage(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_INDX:	// 6 cycle
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				WriteByte(mem, A, addr, cycle);
			}
			break;

			case STA_INDY:	// 6 cycle
			{
				// Get a WORD address from zero page and write the A register to (address + Y)
				BYTE zp = Fetch(mem, cycle);
				// the pointer wraps within page zero
				WORD addr = ReadByte(mem, zp, cycle) | (ReadByte(mem, (BYTE)(zp + 1), cycle) << 8);
				addr += Y;
				cycle--;
				WriteByte(mem, A, addr, cycle);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			case STX_ZP	:	// 3 cycle
			{
				// Write the X register to zero page
				WORD addr = addr_mode_ZP(mem, cycle);
				WriteByte(mem, X, addr, cycle);
			}
			break;

			case STX_ZPY:	// 4 cycle
			{
				// Write the X register to ZP + Y
				WORD addr = addr_mode_ZPY(mem, cycle);
				WriteByte(mem, X, addr, cycle);
			}
			break;

			case STX_ABS:	// 4 cycle
			{
				// Write the X register to a WORD address
				WORD addr = addr_mode_ABS(mem, cycle);
				WriteByte(mem, X, addr, cycle);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			case STY_ZP:	// 3 cycle
			{
				// Write the Y register to zero page
				WORD addr = addr_mode_ZP(mem, cycle);
				WriteByte(mem, Y, addr, cycle);

			}
			break;

			case STY_ZPX:	// 4 cycle
			{
				// Write the Y register to ZP + X
				WORD addr = addr_mode_ZPX(mem, cycle);
				WriteByte(mem, Y, addr, cycle);
			}
			break;

			case STY_ABS:	// 4 cycle
			{
				// Write the Y register to a WORD address
				WORD addr = addr_mode_ABS(mem, cycle);
				WriteByte(mem, Y, addr, cycle);
			}
			break;



			////////////////////////////////////////////////////////////////////////////// JUMP

			case JSR : // 6 cycle
			{
				// The JSR instruction pushes the address (minus one) of the return 
				// point on to the stack and then sets the program counter to the target memory address.
//   				WORD sr_addr = FetchWord(mem, cycle);
// 				// Push PC-1 onto the stack
// 				PushStackWord(mem, PC - 1, cycle);
// 				PC = sr_addr;
// 				cycle--;

				BYTE lo = ReadByte(mem, PC, cycle);
				PC++;
				WORD address = lo | (ReadByte(mem, PC, cycle) << 8);

				PushStackWord(mem, PC, cycle);
				PC = address;
				cycle--;
			}
			break;

			case JMP_ABS :	// 3 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				PC = addr;
			}
			break;

			case JMP_IND :	// 5 cycle (6 on the 65C02)
			{
				WORD ptr = FetchWord(mem, cycle);
				// NMOS bug: a pointer at $xxFF takes its high byte from $xx00,
				// not from the next page. The 65C02 fixed it.
				WORD hiAddr = cmos ? (WORD)(ptr + 1)
				                   : (WORD)((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
				PC = ReadByte(mem, ptr, cycle) | (ReadByte(mem, hiAddr, cycle) << 8);
				if (cmos)
					cycle--;
			}
			break;

			case RTS :	// 6 cycle
			{
				WORD addr = PopStackWord(mem, cycle);
				PC = addr + 1;
				cycle -= 2;
			}
			break;

			//////////////////////////////////////////////////////////////////////////////	STACK

			// Transfer (Stack Pointer) to X
			case TSX :	// 2 cycle
			{
				// Stack pointer -> X register
				X = SP;
				cycle--;
				// Z / N flag
				SetZeroNegative(X);
			}
			break;

			// Transfer X to (Stack Pointer)
			case TXS :	// 2 cycle
			{
				// X register -> SP
				SP = X;
				cycle--;
			}
			break;

			// Pushes a copy of the accumulator on to the stack.
			case PHA :	// 3 cycle
			{
				// Push the A register onto the stack
				PushStackByte(mem, A, cycle);
			}
			break;

			// Pulls an 8 bit value from the stack and into the accumulator. 
			// The zero and negative flags are set as appropriate.
			case PLA :	// 4 cycle
			{
				// Pull 8 bits off the stack --> into A
				A = PopStackByte(mem, cycle);
				cycle--;
				// Z / N flag
				SetZeroNegative(A);
			}
			break;

			// Pulls an 8 bit value from the stack and into the processor flags. 
			// The flags will take on new states as determined by the value pulled.
			case PLP :	// 4 cycle
			{
				// pop 8 bit --> PS (Flag) : the flags take their new state from the popped value
#if USEOLD
				BYTE _PS = PopStackByte(mem, cycle);
				cycle--;
				_PS &= ~(FLAG_UNUSED | FLAG_BREAK);
				PS = 0;
				PS |= _PS;
				// B and Unused are not used
				Flag.B = 0;
				Flag.Unused = 0;
#else
				_PS = PopStackByte(mem, cycle) | FLAG_UNUSED;
				//SP++;
				//PS = ReadByte(mem, 0x100 + SP, cycle) | FLAG_UNUSED;
#endif
			}
			break;

			// Pushes a copy of the status flags on to the stack.
			case PHP :	// 3 cycle
			{
				// PS -> Push onto the stack
				BYTE __PS = _PS | FLAG_BREAK;
				PushStackByte(mem, __PS, cycle);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			case AND_IM :	// 2 cycle
			{
				A &= Fetch(mem, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_ZP:	// 3 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				A &= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_ZPX:	// 4 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				A &= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_ABS:	// 4cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				A &= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_ABSX:	// 4 cycle / +1 cycle when a page boundary is crossed
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				A &= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_ABSY:	// 4 cycle / +1 cycle when a page boundary is crossed
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				A &= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case AND_INDX:	// 6 cycle
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				A &= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			case AND_INDY:	// 5 cycle / +1 cycle when a page boundary is crossed
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				A &= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;


			case ORA_IM :
			{
				A |= Fetch(mem, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				A |= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				A |= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				A |= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_ABSX:
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				A |= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_ABSY:
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				A |= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case ORA_INDX:
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				A |= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			case ORA_INDY:
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				A |= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;


			case EOR_IM:
			{
				A ^= Fetch(mem, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_ABSX:
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_ABSY:
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);
				SetZeroNegative(A);
			}
			break;

			case EOR_INDX:
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			case EOR_INDY:
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				A ^= ReadByte(mem, addr, cycle);

				SetZeroNegative(A);
			}
			break;

			// Bit Test Zero page
			case BIT_ZP:
			{
				// AND-test the value read from ZP against A and set the flags / Set if the result if the AND is zero
				// The N flag is bit 7, Set to bit 7 of the memory value
				// The V flag is bit 6, Set to bit 6 of the memory value
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE R = ReadByte(mem, addr, cycle);

				Flag.Z = !(A & R);	
				Flag.N = (R & FLAG_NEGATIVE) != 0;
				Flag.V = (R & FLAG_OVERFLOW) != 0;
			}
			break;

			case BIT_ABS :
			{
				// AND-test the value read from ZP against A and set the flags
				// The N flag is bit 7 / the V flag is bit 6
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE R = ReadByte(mem, addr, cycle);

				Flag.Z = !(A & R);
				Flag.N = (R & FLAG_NEGATIVE) != 0;
				Flag.V = (R & FLAG_OVERFLOW) != 0;

			}
			break;

			////////////////////////////////////////////////////////////////////////////// Register Transfer

			case TAX :
			{
				// Transfer Accumulator to X
				X = A;
				cycle--;
				SetZeroNegative(X);
			}
			break;

			case TAY:
			{
				//Transfer Accumulator to Y
				Y = A;
				cycle--;
				SetZeroNegative(Y);
			}
			break;

			case TXA:
			{
				// Transfer X to Accumulator
				A = X;
				cycle--;
				SetZeroNegative(A);
			}
			break;

			case TYA:
			{
				// Transfer Y to Accumulator
				A = Y;
				cycle--;
				SetZeroNegative(A);
			}
			break;


			////////////////////////////////////////////////////////////////////////////// increase / decrease

			case INX :	// 2 cycle
			{
				// Increment X Register / X,Z,N = X+1
				X++;
				cycle--;
				Flag.Z = (X == 0);
				Flag.N = (X & FLAG_NEGATIVE) != 0;
			}
			break;

			case INY :	// 2 cycle
			{
				// Increment Y Register / Y,Z,N = Y+1
				Y++;
				cycle--;
				Flag.Z = (Y == 0);
				Flag.N = (Y & FLAG_NEGATIVE) != 0;
			}
			break;
			case DEX:	// 2 cycle
			{
				// Decrease X Register / X,Z,N = X+1
				X--;
				cycle--;
				Flag.Z = (X == 0);
				Flag.N = (X & FLAG_NEGATIVE) != 0;

			}
			break;

			case DEY:	// 2 cycle
			{
				// Decrement Y Register / Y,Z,N = Y+1
				Y--;
				cycle--;
				Flag.Z = (Y == 0);
				Flag.N = (Y & FLAG_NEGATIVE) != 0;

			}
			break;

			case INC_ZP:
			{
				// Increment Memory by One / M + 1 -> M
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v++;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);
			}
			break;
			case INC_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v++;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);
			}
			break;
			case INC_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v++;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);				
			}
			break;
			case INC_ABSX:
			{
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v++;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);				
			}
			break;
			case DEC_ZP:// 5 cycle
			{
				// Decrement Memory by One / M - 1 -> M
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v--;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);

			}
			break;
			case DEC_ZPX: // 6 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v--;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);
			}
			break;
			case DEC_ABS:	// 6 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v--;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);
			}
			break;

			case DEC_ABSX:	// 7 cycle
			{
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				v--;
				cycle--;
				WriteByte(mem, v, addr, cycle);
				SetZeroNegative(v);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////	Arithmetic

			// Add with Carry
			// This instruction adds the contents of a memory location to the accumulator together with 
			// the carry bit.If overflow occurs the carry bit is set, this enables multiple byte addition to be performed.
			case ADC_IM	:	// 2 cycle
			{
				// A + M + C -> A, C
				BYTE v = Fetch(mem, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_ZP:	// 3 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_ZPX:	// 4 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_ABS:	// 4 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_ABSX:	// 4~5 cycle
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_ABSY:	// 4~5 cycle
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_INDX:	// 6 cycle
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			case ADC_INDY:	// 5~6 cycle
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ADC(v);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			// SBC - Subtract with Carry
			// A, Z, C, N = A - M - (1 - C)
			case SBC_IM	:	// 2 cycle
			{
				BYTE v = Fetch(mem, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_ZP:	// 3 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_ABSX:
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_ABSY:
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_INDX:
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			case SBC_INDY:
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_SBC(v);
			}
			break;

			////////////////////////////////////////////////////////////////////////////// Compare

			// Z,C,N = A-M
			// This instruction compares the contents of the accumulator with another memory held 
			// valueand sets the zeroand carry flags as appropriate.
			case CMP_IM:
			{
				BYTE v = Fetch(mem, cycle);
				Execute_CMP(v);
			}
			break;

			case CMP_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;
			
			case CMP_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;

			case CMP_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;

			case CMP_ABSX:
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;

			case CMP_ABSY:
			{
				WORD addr = addr_mode_ABSY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;

			case CMP_INDX:
			{
				WORD addr = addr_mode_INDX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}

			break;

			case CMP_INDY:
			{
				WORD addr = addr_mode_INDY(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CMP(v);
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			// Compare X Register
			// Z, C, N = X - M
			// This instruction compares the contents of the X register with another memory
			// held value and sets the zero and carry flags as appropriate.
			case CPX_IM	:
			{
				BYTE v = Fetch(mem, cycle);
				Execute_CPX(v);
			}
			break;

			case CPX_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CPX(v);
			}
			break;

			case CPX_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CPX(v);
			}
			break;

			case CPY_IM:
			{
				BYTE v = Fetch(mem, cycle);
				Execute_CPY(v);
			}
			break;

			case CPY_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CPY(v);
			}
			break;

			case CPY_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_CPY(v);
			}
			break;

			////////////////////////////////////////////////////////////////////////////// SHIFT

			// Arithmetic Shift Left
			case ASL : // 2 cycle
			{
				// A,Z,C,N = M*2 or M,Z,C,N = M*2
				// The carry bit has to be computed first, because if the value being shifted is negative a left shift raises the carry
				Execute_ASL(A, cycle);
			}
			break;

			case ASL_ZP: // 5 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ASL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;

			case ASL_ZPX: // 6 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ASL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;

			case ASL_ABS: // 6 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ASL(v, cycle);
				WriteByte(mem, v, addr, cycle);

			}
			break;

			case ASL_ABSX: // 7 cycle
			{
				WORD addr = addr_mode_ABSX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ASL(v, cycle);
				WriteByte(mem, v, addr, cycle);

			}
			break;

			// Logical Shift Right : A,C,Z,N = A/2 or M,C,Z,N = M/2
			// Each of the bits in A or M is shift one place to the right. 
			// The bit that was in bit 0 is shifted into the carry flag. Bit 7 is set to zero.
			// Carry Flag :	Set to contents of old bit 0
			case LSR:	// 2 cycle
			{
				Execute_LSR(A, cycle);
			}
			break;
			case LSR_ZP:	// 5 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_LSR(v, cycle);
				WriteByte(mem, v, addr, cycle);
}
			break;
			case LSR_ZPX:	// 6 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_LSR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case LSR_ABS:	// 6 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_LSR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case LSR_ABSX:	// 7 cycle
			{
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_LSR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;

			// Move each of the bits in either A or M one place to the left. 
			// Bit 0 is filled with the current value of the carry flag whilst 
			// the old bit 7 becomes the new carry flag value.
			case ROL:	// 2 cycle
			{
				Execute_ROL(A, cycle);
			}
			break;
			case ROL_ZP:	// 5 cycle
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROL_ZPX:	// 6 cycle
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROL_ABS:	// 6 cycle
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROL_ABSX:	// 7 cycle
			{
				// This one is ABS, no page crossing
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROL(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;

			// Rotate Right
			// Move each of the bits in either A or M one place to the right.
			// Bit 7 is filled with the current value of the carry flag whilst 
			// the old bit 0 becomes the new carry flag value.
			case ROR:
			{
				Execute_ROR(A, cycle);
			}
			break;
			case ROR_ZP:
			{
				WORD addr = addr_mode_ZP(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROR_ZPX:
			{
				WORD addr = addr_mode_ZPX(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROR_ABS:
			{
				WORD addr = addr_mode_ABS(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;
			case ROR_ABSX:
			{
				WORD addr = addr_mode_ABSX_NoPage(mem, cycle);
				BYTE v = ReadByte(mem, addr, cycle);
				Execute_ROR(v, cycle);
				WriteByte(mem, v, addr, cycle);
			}
			break;


			////////////////////////////////////////////////////////////////////////////// Branches

			// Branch if carry flag clear
			// If the carry flag is clear then add the relative displacement to the program 
			// counter to cause a branch to a new location.
			case BCC :
			{
				Execute_BRANCH(Flag.C, false, mem, cycle);
/*
				BYTE offset = Fetch(mem, cycle);
				if (!Flag.C)
				{
					// Crossing a page boundary adds a cycle
					BYTE lo = PC & 0x00FF;
					WORD t = lo + (char)offset;
					if (t > 0xFF) cycle--;

					PC += (char)offset;
					cycle--;
				}*/
			}
			break;

			// Branch if Carry Set
			// If the carry flag is set then add the relative displacement to the program 
			// counter to cause a branch to a new location.
			// The opposite of BCC
			case BCS: // 2 ~ 4 cycle
			{
				Execute_BRANCH(Flag.C, true, mem, cycle);
			}
			break;

			// Branch if Equal
			// If the zero flag is set then add the relative displacement to the program counter 
			// to cause a branch to a new location.
			// 2 (+1 if branch succeeds +2 if to a new page)
			case BEQ:	// 2 cycle + 1 cycle if zero + 1 cycle if a page boundary is crossed
			{
				Execute_BRANCH(Flag.Z, true, mem, cycle);
			}
			break;

			// If the zero flag is clear then add the relative displacement 
			// to the program counter to cause a branch to a new location.
			// The opposite of BEQ
			case BNE:
			{
				Execute_BRANCH(Flag.Z, false, mem, cycle);
			}
			break;

			// Branch if negative flag set
			case BMI:
			{
				Execute_BRANCH(Flag.N, true, mem, cycle);
			}
			break;

			// Branch if negative flag clear
			case BPL:
			{
				Execute_BRANCH(Flag.N, false, mem, cycle);
			}
			break;

			// Branch if overflow flag clear
			case BVC:
			{
				Execute_BRANCH(Flag.V, false, mem, cycle);
			}
			break;

			// Branch if overflow flag set
			case BVS:
			{
				Execute_BRANCH(Flag.V, true, mem, cycle);
			}
			break;

			////////////////////////////////////////////////////////////////////////////// Status Flag Changes

			// Clear carry flag
			case CLC :	// 2 cycle
			{
				Flag.C = 0;
				cycle--;
			}
			break;

			// Clear Decimal Mode
			case CLD:	// 2 cycle
			{
				Flag.D = 0;
				cycle--;
			}
			break;

			// Clear Interrupt Disable
			case CLI:	// 2 cycle
			{
				Flag.I = 0;
				cycle--;
			}
			break;

			// Clear Overflow Flag
			case CLV:	// 2 cycle
			{
				Flag.V = 0;
				cycle--;
			}
			break;

			// Set carry flag
			case SEC:	// 2 cycle
			{
				Flag.C = 1;
				cycle--;
			}
			break;

			// Set decimal mode flag
			case SED:	// 2 cycle
			{
				Flag.D = 1;
				cycle--;
			}
			break;

			// Set interrupt disable flag
			case SEI:	// 2 cycle
			{
				Flag.I = 1;
				cycle--;
			}
			break;

			//////////////////////////////////////////////////////////////////////////////

			// The BRK instruction forces the generation of an interrupt request.
			// The program counter and processor status are pushed on the stack, then the IRQ interrupt vector at $FFFE/F is
			// loaded into the PC and the break flag in the status is set to 1.
			case BRK :	// 7 cycle
			{
#if USEOLD
				// PC Push
				// BRK is said to advance the PC by 2 rather than 1, so PC+1 is pushed
				// https://www.c64-wiki.com/wiki/BRK
				PushStackWord(mem, PC+1, cycle);

				// SP Push
				BYTE _PS = PS | FLAG_BREAK | FLAG_UNUSED;
				PushStackByte(mem, _PS, cycle);

				WORD interruptVector = ReadWord(mem, 0xFFFE, cycle);
				PC = interruptVector;
				Flag.B = 1;
				Flag.I = 1;
#else
				PC++;
				WriteByte(mem, ((PC) >> 8) & 0xFF, 0x100 + SP, cycle);
				SP--;
				WriteByte(mem, PC & 0xFF, 0x100 + SP, cycle);
				SP--;
				WriteByte(mem, _PS | FLAG_BREAK, 0x100 + SP, cycle);
				SP--;
				Flag.I = 1;
				if (cmos)
					Flag.D = 0;       // only the 65C02 clears decimal mode on BRK
				PC = ReadByte(mem, 0xFFFE, cycle) | ReadByte(mem, 0xFFFF, cycle) << 8;
#endif
				printf("BREAK!! : %x\n", PC);
			}
			break;

			// Return from Interrupt
			// The RTI instruction is used at the end of an interrupt handler.
			// It pulls the processor flags from the stack, followed by the program counter.
			case RTI :	// 6 cycle
			{
				//BYTE PS = PopStackByte(mem, cycle);
				//PC = PopStackWord(mem, cycle);

				SP++;
				_PS = ReadByte(mem, 0x100 + SP, cycle);
				SP++;
				PC = ReadByte(mem, 0x100 + SP, cycle);
				SP++;
				PC |= ReadByte(mem, 0x100 + SP, cycle) << 8;
				cycle -= 5;
			}
			break;

			case APPLE_NOP :
				cycle--;
			break;

			//////////////////////////////////////////////////////////////////////////////

			default:
				// every 65C02-only opcode is undocumented on the NMOS part
				if (cmos && ExecuteCmos(inst, mem, cycle))
					break;
				printf("Unknown instruction : %x\n", inst);
				//throw -1;
				break;
		}
	}

	// emulated time, once per call: see CurrentTick()
	tick += _cycle - cycle;
	runCycle = NULL;
	return 0;//CyclesRequested - cycle;
}

void CPU::LoadToRegister(Memory& mem, Cycles& cycle, BYTE &reg)
{
	reg = Fetch(mem, cycle);
	SetZeroNegative(reg);
}

// Load the value at ZP into a register
void CPU::LoadToRegisterFromZP(Memory& mem, Cycles& cycle, BYTE& reg)
{
	BYTE zpa = Fetch(mem, cycle);
	reg = ReadByte(mem, zpa, cycle);
	SetZeroNegative(reg);
}

////////////////////////////////////////////////////////////////////////////// memory addressing mode

// ZeroPage
WORD  CPU::addr_mode_ZP(Memory &mem, Cycles& cycle)
{
	BYTE address = Fetch(mem, cycle);
	return address;
}

// Zero page + X
WORD CPU::addr_mode_ZPX(Memory& mem, Cycles& cycle)
{
	BYTE address = Fetch(mem, cycle) + X;
	cycle--;
	return address;
}

// Zero page + X
WORD CPU::addr_mode_ZPY(Memory& mem, Cycles& cycle)
{
	BYTE address = Fetch(mem, cycle) + Y;
	cycle--;
	return address;
}

// ABS
WORD CPU::addr_mode_ABS(Memory& mem, Cycles& cycle)
{
	WORD address = FetchWord(mem, cycle);
	return address;
}

// ABS + X
WORD CPU::addr_mode_ABSX(Memory& mem, Cycles& cycle)
{
#if 1
	BYTE lo = Fetch(mem, cycle);
	BYTE hi = Fetch(mem, cycle);
	WORD t = lo + X;
	if (t & 0xFF00) cycle--;
	WORD address = (lo | (hi << 8)) + X;
	return address;

#else
	WORD address = Fetch(mem, cycle);
	if ((address + X) & 0xFF00)
		cycle--;
	address |= Fetch(mem, cycle) << 8;
	address += X;
	return address;
#endif

}

// ABS + X : page crossing ignored (the hardware is simply built this way)
WORD CPU::addr_mode_ABSX_NoPage(Memory& mem, Cycles& cycle)
{
	WORD address = FetchWord(mem, cycle);
	address += X;
	cycle--;
	return address;
}

// ABS + Y
WORD CPU::addr_mode_ABSY(Memory& mem, Cycles& cycle)
{
	BYTE lo = Fetch(mem, cycle);
	BYTE hi = Fetch(mem, cycle);

	WORD t = lo + Y;
	if (t > 0xFF) cycle--;
	WORD address = (lo | (hi << 8)) + Y;
	return address;
}

WORD CPU::addr_mode_ABSY_NoPage(Memory& mem, Cycles& cycle)
{
	WORD address = FetchWord(mem, cycle);
	address += Y;
	cycle--;
	return address;
}

WORD CPU::addr_mode_INDX(Memory& mem, Cycles& cycle)
{
	BYTE t = Fetch(mem, cycle);
	BYTE inx = t + X;
	cycle--;
	// the pointer wraps within page zero: ($FF,X) with X=0 reads $FF and $00
	WORD address = ReadByte(mem, inx, cycle) | (ReadByte(mem, (BYTE)(inx + 1), cycle) << 8);
	return address;
}


WORD CPU::addr_mode_INDY(Memory& mem, Cycles& cycle)
{
#if 1
	// Read a word from zero page, add the Y register, and load the byte at that address into A
	// If the address being read crosses a page, one cycle is deducted
	BYTE addr = Fetch(mem, cycle);
	BYTE lo = ReadByte(mem, addr, cycle);
	BYTE hi = ReadByte(mem, (BYTE)(addr + 1), cycle);   // wraps within page zero

	WORD t = lo + Y;
	if (t > 0xFF) cycle--;	// page boundary crossed

	WORD index_addr = ((hi << 8) | lo) + Y;
	return index_addr;
#else
	BYTE addr = Fetch(mem, cycle);
	WORD EAddr = ReadWord(mem, addr, cycle);
	WORD EAddrY = EAddr + Y;
	const bool CrossedPageBoundary = (EAddr ^ EAddrY) >> 8;
	if (CrossedPageBoundary)
	{
		cycle--;
	}
	return EAddrY;
#endif
}

// Add with carry. Binary mode is exact on both CPUs. Decimal mode follows
// Bruce Clark's "Decimal Mode" (6502.org): A and C are right for valid BCD
// on both parts. The NMOS 6502 leaves N/V describing an intermediate value
// and Z the binary sum; the 65C02 makes N and Z reflect the BCD result.
void CPU::Execute_ADC(BYTE v)
{
	WORD bin = A + v + Flag.C;
	if (!Flag.D)
	{
		Flag.V = ((A ^ bin) & (v ^ bin) & 0x80) != 0;
		Flag.C = bin > 0xFF;
		A = bin & 0xFF;
		SetZeroNegative(A);
		return;
	}

	int lo = (A & 0x0F) + (v & 0x0F) + Flag.C;
	if (lo >= 0x0A)
		lo = ((lo + 0x06) & 0x0F) + 0x10;
	int r = (A & 0xF0) + (v & 0xF0) + lo;
	Flag.V = ((A ^ r) & (v ^ r) & 0x80) != 0;
	Flag.N = (r & 0x80) != 0;
	if (r >= 0xA0)
		r += 0x60;
	Flag.C = r >= 0x100;
	A = r & 0xFF;
	if (cmos)
		SetZeroNegative(A);
	else
		Flag.Z = (bin & 0xFF) == 0;
}

// Subtract with borrow. C and V come from the binary difference on both
// CPUs. It used to be ADC of the complement, which is only right in binary
// mode: decimal SBC gave wrong BCD results. See Execute_ADC for the
// decimal-mode flag differences between the NMOS 6502 and the 65C02.
void CPU::Execute_SBC(BYTE v)
{
	int carryIn = Flag.C;                // read before C is overwritten below
	BYTE nv = ~v;
	WORD bin = A + nv + carryIn;
	Flag.V = ((A ^ bin) & (nv ^ bin) & 0x80) != 0;
	Flag.C = bin > 0xFF;
	if (!Flag.D)
	{
		A = bin & 0xFF;
		SetZeroNegative(A);
		return;
	}

	int lo = (A & 0x0F) - (v & 0x0F) + carryIn - 1;
	if (lo < 0)
		lo = ((lo - 0x06) & 0x0F) - 0x10;
	int r = (A & 0xF0) - (v & 0xF0) + lo;
	if (r < 0)
		r -= 0x60;
	A = r & 0xFF;
	if (cmos)
		SetZeroNegative(A);
	else
	{
		Flag.N = (bin & 0x80) != 0;
		Flag.Z = (bin & 0xFF) == 0;
	}
}

void CPU::Execute_CMP(BYTE v)
{
	//WORD t = A - v;
	// 	Flag.N = (t & FLAG_NEGATIVE) > 0;	// Set if bit 7 of the result is set
	// 	Flag.Z = A == v;					// Set if A = M
	// 	Flag.C = A >= v;					// Set if A >= M

	Flag.Z = ((A - v) & 0xFF) == 0;
	Flag.N = ((A - v) & FLAG_NEGATIVE) != 0;
	Flag.C = (A >= v) != 0;
}

void CPU::Execute_CPX(BYTE v)
{
	//WORD t = X - v;
	// 	Flag.N = (t & FLAG_NEGATIVE) > 0;	// Set if bit 7 of the result is set
	// 	Flag.Z = X == v;					// Set if X = M
	// 	Flag.C = X >= v;					// Set if X >= M

	Flag.Z = ((X - v) & 0xFF) == 0;
	Flag.N = ((X - v) & FLAG_NEGATIVE) != 0;
	Flag.C = (X >= v) != 0;
}

void CPU::Execute_CPY(BYTE v)
{
	//WORD t = Y - v;
	// 	Flag.N = (t & FLAG_NEGATIVE) > 0;	// Set if bit 7 of the result is set
	// 	Flag.Z = Y == v;					// Set if Y = M
	// 	Flag.C = Y >= v;					// Set if Y >= M

	Flag.Z = ((Y - v) & 0xFF) == 0;
	Flag.N = ((Y - v) & FLAG_NEGATIVE) != 0;
	Flag.C = (Y >= v) != 0;
}

void CPU::Execute_ASL(BYTE &v, Cycles& cycle)
{
	Flag.C = (v & FLAG_NEGATIVE) > 0;
	v = v << 1;
	cycle--;
	SetZeroNegative(v);
}

void CPU::Execute_LSR(BYTE& v, Cycles& cycle)
{
	Flag.C = (v & 0x01);
	v = v >> 1;
	cycle--;
	SetZeroNegative(v);
}

/*
			   +------------------------------+
			   |         M or A               |
			   |   +-+-+-+-+-+-+-+-+    +-+   |
  Operation:   +-< |7|6|5|4|3|2|1|0| <- |C| <-+         N Z C I D V
				   +-+-+-+-+-+-+-+-+    +-+             / / / _ _ _
*/
void CPU::Execute_ROL(BYTE& v, Cycles& cycle)
{
	// Fill bit 0 after the shift with the previous carry flag value
	BYTE oldcarry = Flag.C ? 0x01 : 0x00;
	Flag.C = (v & FLAG_NEGATIVE) > 0;
	v <<= 1;
	v |= oldcarry;
	cycle--;
	SetZeroNegative(v);
}

/*
			   +------------------------------+
			   |                              |
			   |   +-+    +-+-+-+-+-+-+-+-+   |
  Operation:   +-> |C| -> |7|6|5|4|3|2|1|0| >-+         N Z C I D V
				   +-+    +-+-+-+-+-+-+-+-+             / / / _ _ _
*/
void CPU::Execute_ROR(BYTE& v, Cycles& cycle)
{
	// Is the lowest bit 1? -> it becomes the next carry bit
	BYTE oldcarry = (v & FLAG_CARRY) > 0;
	v = v >> 1;
	// If the previous carry was 1, fill in NEGATIVE
	v |= (Flag.C ? FLAG_NEGATIVE : 0);
	cycle--;
	Flag.C = oldcarry;
	SetZeroNegative(v);
}


void CPU::Execute_BRANCH(bool v, bool condition, Memory &mem, Cycles& cycle)
{
	SBYTE offset = (SBYTE)Fetch(mem, cycle);
	if (v == condition)
	{
#if !USEOLD
		// Crossing a page boundary adds a cycle
		BYTE lo = PC & 0x00FF;
		WORD t = lo + (SBYTE)offset;
		if (t > 0xFF) cycle--;

		PC += (SBYTE)offset;
		cycle--;
#else
		if (offset & FLAG_NEGATIVE)
			offset |= 0xFF00;  // jump backward
		if (((PC & 0xFF) + offset) & 0xFF00)  // page crossing
			cycle--;
		PC += offset;
		cycle--;
#endif
	}
}


void CPU::Dump(FILE* fp)
{
	fwrite(&tick, 1, sizeof(long long), fp);

	fwrite(&A, 1, sizeof(BYTE), fp);
	fwrite(&X, 1, sizeof(BYTE), fp);
	fwrite(&Y, 1, sizeof(BYTE), fp);
	fwrite(&SP, 1, sizeof(BYTE), fp);
	fwrite(&_PS, 1, sizeof(BYTE), fp);
	fwrite(&PC, 1, sizeof(WORD), fp);
}

void CPU::LoadDump(FILE* fp)
{
	fread(&tick, 1, sizeof(long long), fp);

	fread(&A, 1, sizeof(BYTE), fp);
	fread(&X, 1, sizeof(BYTE), fp);
	fread(&Y, 1, sizeof(BYTE), fp);
	fread(&SP, 1, sizeof(BYTE), fp);
	fread(&_PS, 1, sizeof(BYTE), fp);
	fread(&PC, 1, sizeof(WORD), fp);
}
