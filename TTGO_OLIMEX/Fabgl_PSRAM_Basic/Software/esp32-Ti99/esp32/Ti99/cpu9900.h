// Classic99 for ESP32 - TMS9900 CPU class
// Derived from Classic99 console/cpu9900.h (C) 2012 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
//
// Differences from the original: no F18A GPU subclass, no debug opcodes, and the
// 65536-entry opcode pointer table is replaced by opIndex[in>>4] -> opFns[] (the
// low 4 bits of every TMS9900 opcode are operand bits, so 4096 entries decode fully).

#pragma once
#include "ti_types.h"
#include "bus.h"

class CPU9900;
typedef void (CPU9900::*CPU990Fctn)(void);
#define CALL_MEMBER_FN(object, ptr) ((object)->*(ptr))

#define MAX_OPFNS 80

class CPU9900 {
public:
	Word PC;
	Word WP;
	Word X_flag;
	Word ST;
	Word in,D,S,Td,Ts,B;
	int nCycleCount;
	Byte nPostInc[2];
	const char *pType;

	uint8_t opIndex[4096];
	CPU990Fctn opFns[MAX_OPFNS];
	int opCount;

	int idling;
	int halted;
	int nReturnAddress;
	bool enableDebug;

	CPU9900();
	void reset();

	// the 9900 only does word access; bytes are selected from the word (as in Classic99)
	inline Byte RCPUBYTE(Word src) { Word v = romword(src); return (src & 1) ? (v & 0xff) : (v >> 8); }
	inline void WCPUBYTE(Word dest, Byte c) {
		Word v = romword(dest, ACCESS_RMW);
		wrword(dest, (dest & 1) ? (Word)((v & 0xff00) | c) : (Word)((v & 0x00ff) | (c << 8)));
	}
	inline Word ROMWORD(Word src, READACCESSTYPE rmw=ACCESS_READ) { return romword(src, rmw); }
	inline void WRWORD(Word dest, Word val) { wrword(dest, val); }
	Word GetSafeWord(int x, int bank);
	Byte GetSafeByte(int x, int bank);

	void TriggerInterrupt(Word vector, Byte level);

	void fixS();
	void fixD();
	void parity(Byte x);

	void StartIdle();
	void StopIdle();
	int  GetIdle();
	void StartHalt(int source);
	void StopHalt(int source);
	int  GetHalt();
	void SetReturnAddress(Word x);
	int GetReturnAddress();
	void ResetCycleCount();
	void AddCycleCount(int val);
	int  GetCycleCount();
	void SetCycleCount(int x);
	Word GetPC();
	void SetPC(Word x);
	Word GetST();
	void SetST(Word x);
	Word GetWP();
	void SetWP(Word x);
	Word GetX();
	void SetX(Word x);
	Word ExecuteOpcode(bool nopFrame);

	void op_a(); void op_ab(); void op_abs(); void op_ai(); void op_dec(); void op_dect();
	void op_div(); void op_inc(); void op_inct(); void op_mpy(); void op_neg(); void op_s();
	void op_sb(); void op_b(); void op_bl(); void op_blwp(); void op_jeq(); void op_jgt();
	void op_jhe(); void op_jh(); void op_jl(); void op_jle(); void op_jlt(); void op_jmp();
	void op_jnc(); void op_jne(); void op_jno(); void op_jop(); void op_joc(); void op_rtwp();
	void op_x(); void op_xop(); void op_c(); void op_cb(); void op_ci(); void op_coc();
	void op_czc(); void op_ldcr(); void op_sbo(); void op_sbz(); void op_stcr(); void op_tb();
	void op_ckof(); void op_ckon(); void op_idle(); void op_rset(); void op_lrex();
	void op_li(); void op_limi(); void op_lwpi(); void op_mov(); void op_movb(); void op_stst();
	void op_stwp(); void op_swpb(); void op_andi(); void op_ori(); void op_xor(); void op_inv();
	void op_clr(); void op_seto(); void op_soc(); void op_socb(); void op_szc(); void op_szcb();
	void op_sra(); void op_srl(); void op_sla(); void op_src();
	void op_bad();

	void buildcpu();
	void setOp(Word in, CPU990Fctn fn);
	void opcode0(Word in);
	void opcode02(Word in);
	void opcode03(Word in);
	void opcode04(Word in);
	void opcode05(Word in);
	void opcode06(Word in);
	void opcode07(Word in);
	void opcode1(Word in);
	void opcode2(Word in);
	void opcode3(Word in);
};

// replaces Classic99's 64K WStatusLookup table: LGT/AGT/EQ plus the C and OV
// bits that INC/NEG-style ops mask in
static inline Word wstatus(Word i) {
	Word r = 0;
	if (i) r |= BIT_LGT;
	if (i && i < 0x8000) r |= BIT_AGT;
	if (!i) r |= BIT_EQ | BIT_C;
	if (i == 0x8000) r |= BIT_OV;
	return r;
}

extern CPU9900 *pCPU;
