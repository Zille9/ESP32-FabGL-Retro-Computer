/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : hd6309_ops.h
 *  Module : Hitachi HD6309 additions — extra instructions, traps, native-mode timing
 * ============================================================
*/

// Included by mc6809_core_impl.h in the HD6309 build only, after the shared
// helpers (mem_*, op_*, addr_*) and before the opcode switches.
//
// Instruction behaviour follows XRoar's hd6309.c (C. Anscomb), including the
// hardware findings it credits to David Banks [hoglet67]. Cycle counts were
// cross-checked against VCC's hd6309.cpp; where the two disagree XRoar wins,
// except OIM/AIM/EIM/TIM, which use the documented counts because XRoar
// marks its own as unverified.
//
// Every instruction here adds its EMULATION-mode cycle count. Native mode is
// applied afterwards by subtracting the per-opcode saving in the tables at
// the end of this file, so the shared 6809 cases need no native-mode edits.
//
// Not modelled: the internal M latch (only visible as the low byte when DP is
// the source of a 16-bit register-to-register operation; reads as 0 here).

// ============================================================
// ALU helpers
// ============================================================

// a + b + carry: N, Z, V, C
static inline uint16_t h_add16c(MC6809* cpu, uint16_t a, uint16_t b, uint8_t carry) {
    uint32_t result = (uint32_t)a + (uint32_t)b + carry;
    uint16_t r16 = (uint16_t)result;

    uint8_t f = 0;
    if (r16 & 0x8000)                   f |= MC6809_FLAG_N;
    if (r16 == 0)                       f |= MC6809_FLAG_Z;
    if ((a ^ r16) & (b ^ r16) & 0x8000) f |= MC6809_FLAG_V;
    if (result & 0x10000)               f |= MC6809_FLAG_C;
    cpu->cc = (cpu->cc & ~(MC6809_FLAG_N | MC6809_FLAG_Z
                         | MC6809_FLAG_V | MC6809_FLAG_C)) | f;
    return r16;
}

// a - b - carry: N, Z, V, C
static inline uint16_t h_sub16c(MC6809* cpu, uint16_t a, uint16_t b, uint8_t carry) {
    uint32_t result = (uint32_t)a - (uint32_t)b - carry;
    uint16_t r16 = (uint16_t)result;

    uint8_t f = 0;
    if (r16 & 0x8000)                  f |= MC6809_FLAG_N;
    if (r16 == 0)                      f |= MC6809_FLAG_Z;
    if ((a ^ b) & (a ^ r16) & 0x8000) f |= MC6809_FLAG_V;
    if (result & 0x10000)              f |= MC6809_FLAG_C;
    cpu->cc = (cpu->cc & ~(MC6809_FLAG_N | MC6809_FLAG_Z
                         | MC6809_FLAG_V | MC6809_FLAG_C)) | f;
    return r16;
}

// AND/OR/EOR/LD/ST result: N, Z, V=0
static inline uint8_t h_logic8(MC6809* cpu, uint8_t r) {
    update_nz8(cpu, r);
    CC_CLR(MC6809_FLAG_V);
    return r;
}

static inline uint16_t h_logic16(MC6809* cpu, uint16_t r) {
    update_nz16(cpu, r);
    CC_CLR(MC6809_FLAG_V);
    return r;
}

// Inherent-style operation selected by the opcode's low nibble, 8-bit.
// Only COM/DEC/INC/TST/CLR exist for E and F.
static uint8_t h_unary8(MC6809* cpu, uint8_t sub, uint8_t v) {
    switch (sub) {
        case 0x3: // COM
            v = ~v;
            update_nz8(cpu, v);
            CC_CLR(MC6809_FLAG_V);
            CC_SET(MC6809_FLAG_C);
            return v;
        case 0xA: // DEC
            CC_PUT(MC6809_FLAG_V, v == 0x80);
            v--;
            update_nz8(cpu, v);
            return v;
        case 0xC: // INC
            CC_PUT(MC6809_FLAG_V, v == 0x7F);
            v++;
            update_nz8(cpu, v);
            return v;
        case 0xD: // TST
            update_nz8(cpu, v);
            CC_CLR(MC6809_FLAG_V);
            return v;
        default:  // 0xF CLR
            CC_CLR(MC6809_FLAG_N | MC6809_FLAG_V | MC6809_FLAG_C);
            CC_SET(MC6809_FLAG_Z);
            return 0;
    }
}

// Same, 16-bit (D and W).
static uint16_t h_unary16(MC6809* cpu, uint8_t sub, uint16_t v) {
    switch (sub) {
        case 0x0: // NEG
            return op_sub16(cpu, 0, v);
        case 0x3: // COM
            v = ~v;
            update_nz16(cpu, v);
            CC_CLR(MC6809_FLAG_V);
            CC_SET(MC6809_FLAG_C);
            return v;
        case 0x4: // LSR
            CC_PUT(MC6809_FLAG_C, v & 0x0001);
            v >>= 1;
            CC_CLR(MC6809_FLAG_N);
            CC_PUT(MC6809_FLAG_Z, v == 0);
            return v;
        case 0x6: { // ROR
            uint16_t c = CC_TST(MC6809_FLAG_C) ? 0x8000 : 0;
            CC_PUT(MC6809_FLAG_C, v & 0x0001);
            v = (v >> 1) | c;
            update_nz16(cpu, v);
            return v;
        }
        case 0x7: // ASR
            CC_PUT(MC6809_FLAG_C, v & 0x0001);
            v = (v >> 1) | (v & 0x8000);
            update_nz16(cpu, v);
            return v;
        case 0x8: // ASL/LSL
            CC_PUT(MC6809_FLAG_C, v & 0x8000);
            CC_PUT(MC6809_FLAG_V, (v ^ (v << 1)) & 0x8000);
            v <<= 1;
            update_nz16(cpu, v);
            return v;
        case 0x9: { // ROL
            uint16_t c = CC_TST(MC6809_FLAG_C) ? 1 : 0;
            CC_PUT(MC6809_FLAG_C, v & 0x8000);
            CC_PUT(MC6809_FLAG_V, (v ^ (v << 1)) & 0x8000);
            v = (v << 1) | c;
            update_nz16(cpu, v);
            return v;
        }
        case 0xA: // DEC
            CC_PUT(MC6809_FLAG_V, v == 0x8000);
            v--;
            update_nz16(cpu, v);
            return v;
        case 0xC: // INC
            CC_PUT(MC6809_FLAG_V, v == 0x7FFF);
            v++;
            update_nz16(cpu, v);
            return v;
        case 0xD: // TST
            update_nz16(cpu, v);
            CC_CLR(MC6809_FLAG_V);
            return v;
        default:  // 0xF CLR
            CC_CLR(MC6809_FLAG_N | MC6809_FLAG_V | MC6809_FLAG_C);
            CC_SET(MC6809_FLAG_Z);
            return 0;
    }
}

// ============================================================
// Operand access by addressing mode (opcode bits 5-4)
//   0 = immediate, 1 = direct, 2 = indexed, 3 = extended
// ============================================================
#define H_MODE(op)   (((op) >> 4) & 0x03)

static inline uint16_t h_ea(MC6809* cpu, uint8_t opcode) {
    switch (H_MODE(opcode)) {
        case 1:  return addr_direct(cpu);
        case 2:  return addr_indexed(cpu);
        default: return addr_extended(cpu);
    }
}

static inline uint8_t h_rd8(MC6809* cpu, uint8_t opcode) {
    if (H_MODE(opcode) == 0) return fetch8(cpu);
    return mem_read(cpu, h_ea(cpu, opcode));
}

static inline uint16_t h_rd16(MC6809* cpu, uint8_t opcode) {
    if (H_MODE(opcode) == 0) return fetch16(cpu);
    return mem_read16(cpu, h_ea(cpu, opcode));
}

// ============================================================
// Illegal-instruction / divide-by-zero trap
// ============================================================
// The caller has already added the cycles spent before the trap.
static void hd6309_trap(MC6809* cpu, uint8_t md_bit) {
    cpu->md |= md_bit;
    cpu->tfm_busy = false;
    CC_SET(MC6809_FLAG_E);
    push16s(cpu, cpu->pc);
    push16s(cpu, cpu->u);
    push16s(cpu, cpu->y);
    push16s(cpu, cpu->x);
    push8s(cpu, cpu->dp);
    HD6309_PUSH_EF(cpu);
    push8s(cpu, GET_B());
    push8s(cpu, GET_A());
    push8s(cpu, cpu->cc);
    CC_SET(MC6809_FLAG_I | MC6809_FLAG_F);
    cpu->pc = mem_read16(cpu, HD6309_VEC_TRAP);
    cpu->cycles += 17;
}

// ============================================================
// TFR/EXG register access (6309 rules)
// ============================================================
// An 8-bit source is presented on both halves of the 16-bit bus; an 8-bit
// destination takes the half it is wired to (A, DP, E: high; B, CC, F: low).
// Codes $C and $D are the zero register.

static uint16_t h_tfr_read(MC6809* cpu, uint8_t code) {
    switch (code & 0x0F) {
        case 0x0: return cpu->d;
        case 0x1: return cpu->x;
        case 0x2: return cpu->y;
        case 0x3: return cpu->u;
        case 0x4: return cpu->s;
        case 0x5: return cpu->pc;
        case 0x6: return cpu->w;
        case 0x7: return cpu->v;
        case 0x8: return (uint16_t)(GET_A() * 0x0101);
        case 0x9: return (uint16_t)(GET_B() * 0x0101);
        case 0xA: return (uint16_t)(cpu->cc * 0x0101);
        case 0xB: return (uint16_t)(cpu->dp * 0x0101);
        case 0xE: return (uint16_t)(GET_E() * 0x0101);
        case 0xF: return (uint16_t)(GET_F() * 0x0101);
        default:  return 0;
    }
}

static void h_tfr_write(MC6809* cpu, uint8_t code, uint16_t val) {
    switch (code & 0x0F) {
        case 0x0: cpu->d  = val; break;
        case 0x1: cpu->x  = val; break;
        case 0x2: cpu->y  = val; break;
        case 0x3: cpu->u  = val; break;
        case 0x4: cpu->s  = val; cpu->nmi_armed = true; break;
        case 0x5: cpu->pc = val; break;
        case 0x6: cpu->w  = val; break;
        case 0x7: cpu->v  = val; break;
        case 0x8: SET_A(val >> 8); break;
        case 0x9: SET_B(val); break;
        case 0xA: cpu->cc = (uint8_t)val; break;
        case 0xB: cpu->dp = (uint8_t)(val >> 8); break;
        case 0xE: SET_E(val >> 8); break;
        case 0xF: SET_F(val); break;
        default:  break;
    }
}

// ============================================================
// Register-to-register arithmetic: ADDR ADCR SUBR SBCR ANDR ORR EORR CMPR
// ============================================================
// Postbyte: source in the high nibble, destination in the low nibble. The
// destination's width selects an 8- or 16-bit operation.
static void h_reg_reg(MC6809* cpu, uint8_t op) {
    uint8_t postbyte = fetch8(cpu);
    uint8_t src = postbyte >> 4;
    uint8_t dst = postbyte & 0x0F;
    uint8_t carry = CC_TST(MC6809_FLAG_C) ? 1 : 0;

    if (!(dst & 0x08)) {
        // 16-bit destination
        uint16_t a = h_tfr_read(cpu, dst);
        uint16_t b;
        switch (src) {
            case 0x8: case 0x9: b = cpu->d; break;            // A, B -> D
            case 0xE: case 0xF: b = cpu->w; break;            // E, F -> W
            case 0xA: b = cpu->cc; break;
            case 0xB: b = (uint16_t)cpu->dp << 8; break;      // low byte is the M latch
            default:  b = h_tfr_read(cpu, src); break;        // 16-bit regs, zero reg
        }
        uint16_t r = a;
        switch (op & 0x07) {
            case 0: r = op_add16(cpu, a, b); break;
            case 1: r = h_add16c(cpu, a, b, carry); break;
            case 2: r = op_sub16(cpu, a, b); break;
            case 3: r = h_sub16c(cpu, a, b, carry); break;
            case 4: r = h_logic16(cpu, a & b); break;
            case 5: r = h_logic16(cpu, a | b); break;
            case 6: r = h_logic16(cpu, a ^ b); break;
            case 7: op_sub16(cpu, a, b); break;               // CMPR: flags only
        }
        switch (dst) {
            case 0x0: cpu->d  = r; break;
            case 0x1: cpu->x  = r; break;
            case 0x2: cpu->y  = r; break;
            case 0x3: cpu->u  = r; break;
            case 0x4: cpu->s  = r; break;
            case 0x5: cpu->pc = r; break;
            case 0x6: cpu->w  = r; break;
            case 0x7: cpu->v  = r; break;
        }
    } else {
        // 8-bit destination; a 16-bit source contributes its low byte
        uint8_t a, b;
        switch (dst) {
            case 0x8: a = GET_A(); break;
            case 0x9: a = GET_B(); break;
            case 0xA: a = cpu->cc; break;
            case 0xB: a = cpu->dp; break;
            case 0xE: a = GET_E(); break;
            case 0xF: a = GET_F(); break;
            default:  a = 0; break;
        }
        switch (src) {
            case 0x8: b = GET_A(); break;
            case 0x9: b = GET_B(); break;
            case 0xA: b = cpu->cc; break;
            case 0xB: b = cpu->dp; break;
            case 0xE: b = GET_E(); break;
            case 0xF: b = GET_F(); break;
            default:  b = (uint8_t)h_tfr_read(cpu, src); break;
        }
        uint8_t r = a;
        switch (op & 0x07) {
            case 0: r = op_add8(cpu, a, b, 0); break;
            case 1: r = op_add8(cpu, a, b, carry); break;
            case 2: r = op_sub8(cpu, a, b, 0); break;
            case 3: r = op_sub8(cpu, a, b, carry); break;
            case 4: r = h_logic8(cpu, a & b); break;
            case 5: r = h_logic8(cpu, a | b); break;
            case 6: r = h_logic8(cpu, a ^ b); break;
            case 7: op_sub8(cpu, a, b, 0); break;             // CMPR: flags only
        }
        switch (dst) {
            case 0x8: SET_A(r); break;
            case 0x9: SET_B(r); break;
            case 0xA: cpu->cc = r; break;
            case 0xB: cpu->dp = r; break;
            case 0xE: SET_E(r); break;
            case 0xF: SET_F(r); break;
        }
    }
    cpu->cycles += 4;
}

// ============================================================
// Direct-page bit operations: BAND BIAND BOR BIOR BEOR BIEOR LDBT STBT
// ============================================================
// Postbyte: bits 7-6 register (0 = CC, 1 = A, 2 = B), bits 5-3 source bit,
// bits 2-0 destination bit.
static void h_bit_op(MC6809* cpu, uint8_t op) {
    uint8_t postbyte = fetch8(cpu);
    uint16_t ea = addr_direct(cpu);
    uint8_t mem = mem_read(cpu, ea);
    uint8_t dst_bit = postbyte & 7;
    uint8_t src_bit = (postbyte >> 3) & 7;
    uint8_t reg_code = postbyte >> 6;
    uint8_t dst_mask = (uint8_t)(1 << dst_bit);

    uint8_t reg;
    switch (reg_code) {
        case 0:  reg = cpu->cc; break;
        case 1:  reg = GET_A(); break;
        case 2:  reg = GET_B(); break;
        default: reg = 0; break;      // register 3 is undefined; it does not trap
    }

    uint8_t out;
    switch (op & 7) {
        case 0:  out = (mem >> src_bit) & (reg >> dst_bit); break;             // BAND
        case 1:  out = ((uint8_t)~mem >> src_bit) & (reg >> dst_bit); break;   // BIAND
        case 2:  out = (mem >> src_bit) | (reg >> dst_bit); break;             // BOR
        case 3:  out = ((uint8_t)~mem >> src_bit) | (reg >> dst_bit); break;   // BIOR
        case 4:  out = (mem >> src_bit) ^ (reg >> dst_bit); break;             // BEOR
        case 5:  out = ((uint8_t)~mem >> src_bit) ^ (reg >> dst_bit); break;   // BIEOR
        case 6:  out = mem >> src_bit; break;                                  // LDBT
        default: out = reg >> src_bit; break;                                  // STBT
    }
    out &= 1;

    if ((op & 7) == 7) {
        mem_write(cpu, ea, (uint8_t)((mem & ~dst_mask) | (out << dst_bit)));
        cpu->cycles += 8;
        return;
    }
    uint8_t r = (uint8_t)((reg & ~dst_mask) | (out << dst_bit));
    switch (reg_code) {
        case 1:  SET_A(r); break;
        case 2:  SET_B(r); break;
        default: cpu->cc = (uint8_t)((cpu->cc & ~dst_mask) | (out << dst_bit)); break;
    }
    cpu->cycles += 7;
}

// ============================================================
// TFM — block transfer, W bytes
// ============================================================
// Runs one byte per pass and rewinds PC to the instruction while W != 0, so a
// pending interrupt is taken between bytes and RTI resumes the transfer, as
// on the real chip. The first pass pays the 6-cycle setup and clears Z.
// `at` is the address of the instruction's first prefix byte.
static void h_tfm(MC6809* cpu, uint8_t op, uint16_t at) {
    uint8_t postbyte = fetch8(cpu);
    uint8_t sc = postbyte >> 4;
    uint8_t dc = postbyte & 0x0F;

    if (!(cpu->tfm_busy && cpu->tfm_pc == at)) {
        CC_CLR(MC6809_FLAG_Z);
        cpu->cycles += 6;
        cpu->tfm_busy = true;
        cpu->tfm_pc = at;
    } else {
        // A resumed pass re-read any repeated prefix bytes; they only cost
        // cycles the first time.
        cpu->cycles -= (uint16_t)(cpu->pc - at) - 3;
    }
    if (sc > 4 || dc > 4) {                // only D, X, Y, U, S
        hd6309_trap(cpu, HD6309_MD_IL);    // (XRoar traps here without setting MD.IL)
        return;
    }

    uint16_t* regs[5] = { &cpu->d, &cpu->x, &cpu->y, &cpu->u, &cpu->s };
    uint16_t* src = regs[sc];
    uint16_t* dst = regs[dc];

    if (cpu->w != 0) {
        mem_write(cpu, *dst, mem_read(cpu, *src));
        switch (op & 3) {
            case 0: (*src)++; (*dst)++; break;   // TFM r0+,r1+
            case 1: (*src)--; (*dst)--; break;   // TFM r0-,r1-
            case 2: (*src)++;           break;   // TFM r0+,r1
            case 3:           (*dst)++; break;   // TFM r0,r1+
        }
        cpu->w--;
        cpu->cycles += 3;
        if (cpu->w != 0) {
            cpu->pc = at;
            return;
        }
    }
    CC_SET(MC6809_FLAG_Z);
    cpu->tfm_busy = false;
}

// ============================================================
// DIVD, DIVQ, MULD
// ============================================================
// Cycle counts vary with the operand signs and with how early the divider
// gives up; `base` is the emulation-mode count for a full division.

static void h_divd(MC6809* cpu, uint8_t opcode) {
    static const uint8_t base_cycles[4] = { 25, 27, 27, 28 };
    uint8_t  base = base_cycles[H_MODE(opcode)];
    uint8_t  divisor = h_rd8(cpu, opcode);
    uint16_t dividend = cpu->d;

    if (divisor == 0) {
        CC_CLR(MC6809_FLAG_N | MC6809_FLAG_V);
        CC_SET(MC6809_FLAG_Z);
        cpu->cycles += base - 19;
        hd6309_trap(cpu, HD6309_MD_D0);
        return;
    }

    bool nsign = false, vsign = false;
    if (dividend & 0x8000) {
        dividend = (uint16_t)(0 - dividend);
        cpu->d = dividend;        // the negation shows in D even if the divide aborts
        nsign = true;
        base++;
    }
    if (divisor & 0x80) {
        divisor = (uint8_t)(0 - divisor);
        vsign = true;
        base++;
    }

    uint16_t quotient  = dividend / divisor;
    uint8_t  remainder = (uint8_t)(dividend % divisor);

    CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z | MC6809_FLAG_V | MC6809_FLAG_C);
    if (quotient >> 8) {
        // Range overflow: registers keep the (possibly negated) dividend
        CC_SET(MC6809_FLAG_V);
        if (nsign) CC_SET(MC6809_FLAG_N);
        cpu->cycles += base - 13;
        return;
    }

    if (nsign) remainder = (uint8_t)(0 - remainder);
    if (nsign != vsign) {
        uint16_t negated = (uint16_t)(0 - quotient);
        if ((negated & 0x80) && !(quotient & 0x80)) quotient = negated;
    }

    SET_A(remainder);
    SET_B(quotient);
    uint8_t b = GET_B();
    if (b & 1) CC_SET(MC6809_FLAG_C);
    if ((((quotient >> 15) ^ (b >> 7)) & 1) == 0) {
        update_nz8(cpu, b);
        cpu->cycles += base;
    } else {
        // Two's-complement overflow: the result is still stored
        CC_SET(MC6809_FLAG_N | MC6809_FLAG_V);
        cpu->cycles += base - 1;
    }
}

static void h_divq(MC6809* cpu, uint8_t opcode) {
    static const uint8_t base_cycles[4] = { 34, 36, 36, 37 };
    uint8_t  base = base_cycles[H_MODE(opcode)];
    uint16_t divisor = h_rd16(cpu, opcode);
    uint32_t dividend = ((uint32_t)cpu->d << 16) | cpu->w;

    if (divisor == 0) {
        CC_CLR(MC6809_FLAG_N | MC6809_FLAG_V);
        CC_SET(MC6809_FLAG_Z);
        cpu->cycles += base - 27;
        hd6309_trap(cpu, HD6309_MD_D0);
        return;
    }

    bool nsign = false, vsign = false;
    if (dividend & 0x80000000u) {
        dividend = 0u - dividend;
        cpu->d = (uint16_t)(dividend >> 16);
        cpu->w = (uint16_t)dividend;
        nsign = true;
        base++;
    }
    if (divisor & 0x8000) {
        divisor = (uint16_t)(0 - divisor);
        vsign = true;
        base++;
    }

    uint32_t quotient  = dividend / divisor;
    uint16_t remainder = (uint16_t)(dividend % divisor);

    CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z | MC6809_FLAG_V | MC6809_FLAG_C);
    if (quotient >> 16) {
        CC_SET(MC6809_FLAG_V);
        if (nsign) CC_SET(MC6809_FLAG_N);
        cpu->cycles += base - 21;
        return;
    }

    if (nsign) remainder = (uint16_t)(0 - remainder);
    if (nsign != vsign) {
        uint32_t negated = 0u - quotient;
        if ((negated & 0x8000) && !(quotient & 0x8000)) quotient = negated;
    }

    cpu->d = remainder;
    cpu->w = (uint16_t)quotient;
    if (cpu->w & 1) CC_SET(MC6809_FLAG_C);
    if ((((quotient >> 31) ^ (cpu->w >> 15)) & 1) == 0) {
        update_nz16(cpu, cpu->w);
    } else {
        CC_SET(MC6809_FLAG_N | MC6809_FLAG_V);
    }
    cpu->cycles += base;
}

static void h_muld(MC6809* cpu, uint8_t opcode) {
    static const uint8_t base_cycles[4] = { 28, 30, 30, 31 };
    int16_t b = (int16_t)h_rd16(cpu, opcode);
    int32_t result = (int32_t)(int16_t)cpu->d * (int32_t)b;
    cpu->d = (uint16_t)((uint32_t)result >> 16);
    cpu->w = (uint16_t)result;
    // N and Z come from D alone; W is ignored. (XRoar sets N where this sets
    // Z for D == 0, which reads as a typo next to its LDQ code.)
    CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z);
    if (cpu->d & 0x8000) CC_SET(MC6809_FLAG_N);
    if (cpu->d == 0)     CC_SET(MC6809_FLAG_Z);
    cpu->cycles += base_cycles[H_MODE(opcode)];
}

// ============================================================
// 6309-only opcodes, page 1. Returns false if the opcode is illegal.
// ============================================================
static bool hd6309_exec_page1(MC6809* cpu, uint8_t opcode) {
    switch (opcode) {
        // ---- OIM / AIM / EIM / TIM: memory with immediate ----
        case 0x01: case 0x02: case 0x05: case 0x0B:
        case 0x61: case 0x62: case 0x65: case 0x6B:
        case 0x71: case 0x72: case 0x75: case 0x7B: {
            uint8_t imm = fetch8(cpu);
            uint16_t ea;
            switch (opcode >> 4) {
                case 0x0: ea = addr_direct(cpu);   cpu->cycles += 6; break;
                case 0x6: ea = addr_indexed(cpu);  cpu->cycles += 7; break;
                default:  ea = addr_extended(cpu); cpu->cycles += 7; break;
            }
            uint8_t val = mem_read(cpu, ea);
            switch (opcode & 0x0F) {
                case 0x1: val |= imm; break;   // OIM
                case 0x5: val ^= imm; break;   // EIM
                default:  val &= imm; break;   // AIM, TIM
            }
            h_logic8(cpu, val);
            if ((opcode & 0x0F) != 0x0B) mem_write(cpu, ea, val);   // TIM only tests
            return true;
        }

        case 0x14: { // SEXW (sign-extend W into D)
            cpu->d = (cpu->w & 0x8000) ? 0xFFFF : 0x0000;
            CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z);
            if (cpu->d & 0x8000) CC_SET(MC6809_FLAG_N);
            if (cpu->d == 0 && cpu->w == 0) CC_SET(MC6809_FLAG_Z);
            cpu->cycles += 4;
            return true;
        }

        case 0xCD: { // LDQ #imm32 — V is left alone; Z looks at D only
            cpu->d = fetch16(cpu);
            cpu->w = fetch16(cpu);
            CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z);
            if (cpu->d & 0x8000) CC_SET(MC6809_FLAG_N);
            if (cpu->d == 0)     CC_SET(MC6809_FLAG_Z);
            cpu->cycles += 5;
            return true;
        }

        default:
            return false;
    }
}

// ============================================================
// 6309-only opcodes, page 2 ($10 prefix)
// ============================================================
static bool hd6309_exec_page2(MC6809* cpu, uint8_t opcode) {
    static const uint8_t arith16_cycles[4] = { 5, 7, 7, 8 };
    static const uint8_t ldst16_cycles[4]  = { 4, 6, 6, 7 };

    switch (opcode) {
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x36: case 0x37:
            h_reg_reg(cpu, opcode);
            return true;

        case 0x38: // PSHSW
            push8s(cpu, GET_F());
            push8s(cpu, GET_E());
            cpu->cycles += 6;
            return true;
        case 0x39: // PULSW
            SET_E(pull8s(cpu));
            SET_F(pull8s(cpu));
            cpu->cycles += 6;
            return true;
        case 0x3A: // PSHUW
            push8u(cpu, GET_F());
            push8u(cpu, GET_E());
            cpu->cycles += 6;
            return true;
        case 0x3B: // PULUW
            SET_E(pull8u(cpu));
            SET_F(pull8u(cpu));
            cpu->cycles += 6;
            return true;

        // ---- NEGD COMD LSRD RORD ASRD ASLD ROLD DECD INCD TSTD CLRD ----
        case 0x40: case 0x43: case 0x44: case 0x46: case 0x47: case 0x48:
        case 0x49: case 0x4A: case 0x4C: case 0x4D: case 0x4F:
            cpu->d = h_unary16(cpu, opcode & 0x0F, cpu->d);
            cpu->cycles += 3;
            return true;

        // ---- COMW LSRW RORW ROLW DECW INCW TSTW CLRW (no NEGW/ASRW/ASLW) ----
        case 0x53: case 0x54: case 0x56: case 0x59:
        case 0x5A: case 0x5C: case 0x5D: case 0x5F:
            cpu->w = h_unary16(cpu, opcode & 0x0F, cpu->w);
            cpu->cycles += 3;
            return true;

        // ---- SUBW / CMPW / ADDW ----
        case 0x80: case 0x90: case 0xA0: case 0xB0:
        case 0x81: case 0x91: case 0xA1: case 0xB1:
        case 0x8B: case 0x9B: case 0xAB: case 0xBB: {
            uint16_t val = h_rd16(cpu, opcode);
            switch (opcode & 0x0F) {
                case 0x0: cpu->w = op_sub16(cpu, cpu->w, val); break;
                case 0x1: op_sub16(cpu, cpu->w, val); break;
                default:  cpu->w = op_add16(cpu, cpu->w, val); break;
            }
            cpu->cycles += arith16_cycles[H_MODE(opcode)];
            return true;
        }

        // ---- SBCD / ANDD / BITD / EORD / ADCD / ORD ----
        case 0x82: case 0x92: case 0xA2: case 0xB2:
        case 0x84: case 0x94: case 0xA4: case 0xB4:
        case 0x85: case 0x95: case 0xA5: case 0xB5:
        case 0x88: case 0x98: case 0xA8: case 0xB8:
        case 0x89: case 0x99: case 0xA9: case 0xB9:
        case 0x8A: case 0x9A: case 0xAA: case 0xBA: {
            uint16_t val = h_rd16(cpu, opcode);
            uint8_t carry = CC_TST(MC6809_FLAG_C) ? 1 : 0;
            switch (opcode & 0x0F) {
                case 0x2: cpu->d = h_sub16c(cpu, cpu->d, val, carry); break;
                case 0x4: cpu->d = h_logic16(cpu, cpu->d & val); break;
                case 0x5: h_logic16(cpu, cpu->d & val); break;
                case 0x8: cpu->d = h_logic16(cpu, cpu->d ^ val); break;
                case 0x9: cpu->d = h_add16c(cpu, cpu->d, val, carry); break;
                default:  cpu->d = h_logic16(cpu, cpu->d | val); break;
            }
            cpu->cycles += arith16_cycles[H_MODE(opcode)];
            return true;
        }

        // ---- LDW ----
        case 0x86: case 0x96: case 0xA6: case 0xB6:
            cpu->w = h_logic16(cpu, h_rd16(cpu, opcode));
            cpu->cycles += ldst16_cycles[H_MODE(opcode)];
            return true;

        // ---- STW ----
        case 0x97: case 0xA7: case 0xB7: {
            uint16_t ea = h_ea(cpu, opcode);
            mem_write16(cpu, ea, h_logic16(cpu, cpu->w));
            cpu->cycles += ldst16_cycles[H_MODE(opcode)];
            return true;
        }

        // ---- LDQ / STQ (memory): N from D, Z from all 32 bits ----
        case 0xDC: case 0xEC: case 0xFC:
        case 0xDD: case 0xED: case 0xFD: {
            uint16_t ea = h_ea(cpu, opcode);
            if (opcode & 0x01) {
                mem_write16(cpu, ea, cpu->d);
                mem_write16(cpu, ea + 2, cpu->w);
            } else {
                cpu->d = mem_read16(cpu, ea);
                cpu->w = mem_read16(cpu, ea + 2);
            }
            CC_CLR(MC6809_FLAG_N | MC6809_FLAG_Z | MC6809_FLAG_V);
            if (cpu->d & 0x8000) CC_SET(MC6809_FLAG_N);
            if (cpu->d == 0 && cpu->w == 0) CC_SET(MC6809_FLAG_Z);
            cpu->cycles += (H_MODE(opcode) == 3) ? 9 : 8;
            return true;
        }

        default:
            return false;
    }
}

// ============================================================
// 6309-only opcodes, page 3 ($11 prefix)
// ============================================================
static bool hd6309_exec_page3(MC6809* cpu, uint8_t opcode, uint16_t insn_pc) {
    static const uint8_t arith8_cycles[4] = { 3, 5, 5, 6 };

    switch (opcode) {
        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x36: case 0x37:
            h_bit_op(cpu, opcode);
            return true;

        case 0x38: case 0x39: case 0x3A: case 0x3B:
            h_tfm(cpu, opcode, insn_pc);
            return true;

        case 0x3C: { // BITMD #imm — tests then clears the two trap flags
            uint8_t bits = fetch8(cpu) & (HD6309_MD_D0 | HD6309_MD_IL);
            CC_PUT(MC6809_FLAG_Z, (cpu->md & bits) == 0);
            cpu->md &= ~bits;
            cpu->cycles += 4;
            return true;
        }

        case 0x3D: { // LDMD #imm — only the two mode bits are writable
            uint8_t bits = fetch8(cpu) & (HD6309_MD_FM | HD6309_MD_NM);
            cpu->md = (cpu->md & ~(HD6309_MD_FM | HD6309_MD_NM)) | bits;
            cpu->cycles += 5;
            return true;
        }

        // ---- COME DECE INCE TSTE CLRE ----
        case 0x43: case 0x4A: case 0x4C: case 0x4D: case 0x4F:
            SET_E(h_unary8(cpu, opcode & 0x0F, GET_E()));
            cpu->cycles += 3;
            return true;

        // ---- COMF DECF INCF TSTF CLRF ----
        case 0x53: case 0x5A: case 0x5C: case 0x5D: case 0x5F:
            SET_F(h_unary8(cpu, opcode & 0x0F, GET_F()));
            cpu->cycles += 3;
            return true;

        // ---- SUBE CMPE LDE ADDE / SUBF CMPF LDF ADDF ----
        case 0x80: case 0x81: case 0x86: case 0x8B:
        case 0x90: case 0x91: case 0x96: case 0x9B:
        case 0xA0: case 0xA1: case 0xA6: case 0xAB:
        case 0xB0: case 0xB1: case 0xB6: case 0xBB:
        case 0xC0: case 0xC1: case 0xC6: case 0xCB:
        case 0xD0: case 0xD1: case 0xD6: case 0xDB:
        case 0xE0: case 0xE1: case 0xE6: case 0xEB:
        case 0xF0: case 0xF1: case 0xF6: case 0xFB: {
            uint8_t val = h_rd8(cpu, opcode);
            uint8_t reg = (opcode & 0x40) ? GET_F() : GET_E();
            switch (opcode & 0x0F) {
                case 0x0: reg = op_sub8(cpu, reg, val, 0); break;
                case 0x1: op_sub8(cpu, reg, val, 0); break;
                case 0x6: reg = h_logic8(cpu, val); break;
                default:  reg = op_add8(cpu, reg, val, 0); break;
            }
            if (opcode & 0x40) SET_F(reg); else SET_E(reg);
            cpu->cycles += arith8_cycles[H_MODE(opcode)];
            return true;
        }

        // ---- STE / STF ----
        case 0x97: case 0xA7: case 0xB7:
        case 0xD7: case 0xE7: case 0xF7: {
            uint16_t ea = h_ea(cpu, opcode);
            mem_write(cpu, ea, h_logic8(cpu, (opcode & 0x40) ? GET_F() : GET_E()));
            cpu->cycles += arith8_cycles[H_MODE(opcode)];
            return true;
        }

        case 0x8D: case 0x9D: case 0xAD: case 0xBD:
            h_divd(cpu, opcode);
            return true;
        case 0x8E: case 0x9E: case 0xAE: case 0xBE:
            h_divq(cpu, opcode);
            return true;
        case 0x8F: case 0x9F: case 0xAF: case 0xBF:
            h_muld(cpu, opcode);
            return true;

        default:
            return false;
    }
}

// ============================================================
// Native-mode cycle savings (emulation count minus native count)
// ============================================================
// Indexed-mode savings are separate (see addr_indexed). Instructions whose
// native timing depends on what they do — long branches, interrupt stacking,
// RTI — are handled where they execute and are 0 here.

static const uint8_t hd6309_native_save_p1[256] = {
//  x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF
     1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 2, 1, 1, // 0x
     0, 0, 1, 1, 0, 0, 1, 2, 0, 1, 1, 0, 1, 1, 3, 2, // 1x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
     0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 2, 0, 0, 1, 0, 0, // 3x
     1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, // 4x
     1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, // 5x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, // 6x
     1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 2, 1, 1, // 7x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, // 8x
     1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, // 9x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // Ax
     1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, // Bx
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Cx
     1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Dx
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
     1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Fx
};

static const uint8_t hd6309_native_save_p2[256] = {
//  x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3x
     1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, // 4x
     0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, // 5x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 7x
     1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, // 8x
     2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 0, 1, 1, // 9x
     1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, // Ax
     2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 0, 1, 1, // Bx
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Cx
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, // Dx
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, // Fx
};

static const uint8_t hd6309_native_save_p3[256] = {
//  x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
     1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 3x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, // 4x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, // 5x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 7x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // 8x
     1, 1, 0, 2, 0, 0, 1, 1, 0, 0, 0, 1, 2, 1, 1, 1, // 9x
     0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // Ax
     1, 1, 0, 2, 0, 0, 1, 1, 0, 0, 0, 1, 2, 1, 1, 1, // Bx
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Cx
     1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, // Dx
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
     1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, // Fx
};
