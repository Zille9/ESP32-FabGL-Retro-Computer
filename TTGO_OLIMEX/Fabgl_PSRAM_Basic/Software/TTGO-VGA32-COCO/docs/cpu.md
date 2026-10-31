# CPU — MC6809 and HD6309

The emulator has two CPUs built from one interpreter: the Motorola **MC6809**
the CoCo shipped with, and the Hitachi **HD6309** many owners fitted in its
place. Both derive from XRoar's CPU cores by Ciaran Anscomb.

**Choosing the CPU:** F3 → Setup → Machine → CPU, a picker listing 6809 and 6309 with the running one marked `current`. The choice is stored in
NVS (`"sv"` / `cpu_variant`) and applied at boot, so changing it restarts the
emulator. `CPU_VARIANT` in `config.h` is only the default for a board with no
stored value. The debug API can switch it too: `POST /api/machine` with
`cpu=6309` (see `wifi-debug.md`).

> The HD6309 has been verified on the host against XRoar (see *Tests*). It has
> not yet been tested on the board.

## Source files

| File | Role |
|---|---|
| `mc6809.h` | `MC6809` state struct (used by both CPUs), flag and vector constants, public API |
| `mc6809_core_impl.h` | The interpreter body: helpers, addressing, opcode switches, interrupts, run loop. Not a normal header — each CPU file includes it once |
| `mc6809.cpp` | MC6809 build (`CPU_HD6309 0`) → `mc6809_run()`. Also `mc6809_nmi/firq/irq()` |
| `hd6309.cpp` | HD6309 build (`CPU_HD6309 1`) → `hd6309_run()` |
| `hd6309_ops.h` | Everything only a 6309 has: extra instructions, TFR/EXG rules, traps, native-mode tables |
| `mc6809_opcodes.h` | Reference cycle table and opcode constants (used by tests, not by the interpreter) |

## Public API

| Function | Purpose |
|---|---|
| `mc6809_init(cpu)` | Zero the state; the machine then sets `read`, `write`, the page tables and `variant` |
| `mc6809_reset(cpu)` | Load PC from `$FFFE`, mask interrupts, clear DP; on a 6309 also back to emulation mode |
| `mc6809_run(cpu, budget)` / `hd6309_run(cpu, budget)` | Run instructions until `budget` cycles are used; returns cycles consumed |
| `mc6809_run_variant(cpu, budget)` | Calls the one matching `cpu->variant`. The machine uses this once per scanline |
| `mc6809_nmi/firq/irq(cpu, active)` | Drive the interrupt lines (shared by both CPUs) |

## How the two CPUs are built

There is one interpreter source, `mc6809_core_impl.h`, compiled twice: `mc6809.cpp` includes it with `CPU_HD6309 0` and `hd6309.cpp` with `CPU_HD6309 1`. The 6309-only paths sit behind `#if CPU_HD6309`, so a CPU fix is made once and reaches both.

`mc6809_run_variant()` picks one per scanline from `MC6809.variant`, so neither build pays for the other. `mc6809_init()`, `mc6809_reset()` and the `mc6809_nmi/firq/irq()` functions serve both.

The MC6809 build did not change: apart from `mc6809_init` and `mc6809_reset` (which now clear the new state), every section of `mc6809.cpp.o` is byte-identical to the object from before the 6309 work.

---

## MC6809

Full emulation of the Motorola MC6809E, the CoCo's original processor.
Undefined opcodes run as a 2-cycle NOP.

### Registers

| Register | Width | Description |
|----------|-------|-------------|
| `pc` | 16-bit | Program counter |
| `d` | 16-bit | Accumulator D (A = high byte, B = low byte) |
| `x`, `y` | 16-bit | Index registers |
| `s` | 16-bit | Hardware stack pointer |
| `u` | 16-bit | User stack pointer |
| `dp` | 8-bit | Direct page register |
| `cc` | 8-bit | Condition codes (E, F, H, I, N, Z, V, C) |

### Condition Code Flags

| Flag | Bit | Description |
|------|-----|-------------|
| E | 7 | Entire state saved (1 = full push on interrupt, 0 = partial/FIRQ) |
| F | 6 | FIRQ mask (1 = FIRQ disabled) |
| H | 5 | Half carry (bit 3 carry, used by DAA) |
| I | 4 | IRQ mask (1 = IRQ disabled) |
| N | 3 | Negative (MSB of result) |
| Z | 2 | Zero (result == 0) |
| V | 1 | Overflow (signed arithmetic) |
| C | 0 | Carry / borrow |

### Execution Loop (`mc6809_run`)

```
mc6809_run(cpu, budget):
  cycles = 0
  while cycles < budget:
    if cpu->halted:
      cycles = budget; break          // FDC HALT burns budget
    check_interrupts(cpu)             // NMI > FIRQ > IRQ priority
    if cpu->wait_for_interrupt:
      if SYNC and (irq_pending or firq_pending):
        wait_for_interrupt = false    // SYNC ends on any active line
      else:
        cycles = budget; break        // CWAI/SYNC idles
    execute_one(cpu)                  // Fetch-decode-execute
  return cycles
```

- **HALT support**: When `cpu->halted` is true (set by the FDC's DRQ/HALT mechanism), the CPU burns its entire budget doing nothing — essential for disk I/O synchronization.
- **CWAI/SYNC**: `wait_for_interrupt` flag causes the CPU to idle until an interrupt arrives. The run loop decides when the wait ends, so code that drives the lines only sets `irq_pending` / `firq_pending`. SYNC ends as soon as a line is active, masked or not, including a line that was already active when SYNC executed; a masked line makes it continue with the next instruction. CWAI ends only when an unmasked interrupt is taken.

### Interrupt Handling

Three interrupt types, checked before each instruction in priority order:

**NMI** (`mc6809_nmi(cpu, active)`)
- **Edge-triggered**: Latches `nmi_pending` on inactive→active transition of `nmi_line`. After servicing, `nmi_line` is cleared — a new edge is required for the next NMI.
- **Non-maskable**: Cannot be disabled via CC flags.
- **nmi_armed gate**: NMI is ignored until the first LDS instruction executes (per MC6809 spec). This prevents spurious NMI during reset when S is uninitialized.
- **Stack push**: Full state (E=1) — CC, A, B, DP, X, Y, U, PC → 12 bytes on S stack (19 cycles). If `cwai_state` is true, push is skipped (7 cycles).
- **Masks**: Sets both I and F flags.
- **Vector**: $FFFC.
- **Used by**: FDC INTRQ for disk transfer completion.

**FIRQ** (`mc6809_firq(cpu, active)`)
- **Level-triggered**: `firq_pending` mirrors the pin state.
- **Masked by**: F flag in CC.
- **Fast**: Pushes only CC and PC (E=0) — 3 bytes (10 cycles). Exception: if `cwai_state` is true, the full state was already pushed with E=1 by CWAI.
- **Masks**: Sets both I and F flags.
- **Vector**: $FFF6.
- **Used by**: Cartridge interrupt (PIA1 IRQA/IRQB).

**IRQ** (`mc6809_irq(cpu, active)`)
- **Level-triggered**: `irq_pending` mirrors the pin state.
- **Masked by**: I flag in CC.
- **Stack push**: Full state (E=1) — 12 bytes (19 cycles), or 7 cycles if CWAI.
- **Masks**: Sets I flag only (F unchanged).
- **Vector**: $FFF8.
- **Used by**: 60Hz vsync timer (PIA0 CB1) and keyboard.

**CWAI and SYNC operations:**
- **CWAI** ($3C): ANDs an immediate byte with CC (clearing mask bits to allow interrupt), sets E=1, pre-pushes entire state to S stack, then enters `wait_for_interrupt`. When an interrupt arrives, the handler skips the redundant push — only vectoring and masking are needed (7 cycles instead
of 19).
- **SYNC** ($13): Enters `wait_for_interrupt` without pushing state. The interrupt that wakes SYNC causes a normal push-and-vector sequence.

### Interrupt Vector Table

| Vector | Address | Use |
|--------|---------|-----|
| RESET | $FFFE | Power-on / reset |
| NMI | $FFFC | FDC disk transfer |
| SWI | $FFFA | Software interrupt |
| IRQ | $FFF8 | 60Hz timer, keyboard |
| FIRQ | $FFF6 | Cartridge |
| SWI2 | $FFF4 | (unused on CoCo) |
| SWI3 | $FFF2 | (unused on CoCo) |

### Opcode Coverage

All documented MC6809 opcodes are implemented across three pages:

- **Page 1** (no prefix): 8-bit ALU (ADD, ADC, SUB, SBC, AND, OR, EOR, CMP, TST, NEG, COM, CLR, INC, DEC, LSR, LSL/ASL, ASR, ROR, ROL), loads/stores (LD, ST for A, B, D, X, Y, S, U), branches (BRA, BEQ, BNE, BCC, BCS, BPL, BMI, BVS, BVC, BGE, BGT, BLE, BLT, BHI, BLS, BSR), stack ops (PSHS, P
ULS, PSHU, PULU), LEA (LEAX, LEAY, LEAS, LEAU), TFR, EXG, MUL, DAA, SEX, ABX, NOP, SYNC, CWAI, SWI, RTI, RTS
- **Page 2** (prefix `$10`): 16-bit comparisons (CMPD, CMPY), long branches (LBRA, LBSR, LBcc), LDY/STY, LDS/STS, SWI2
- **Page 3** (prefix `$11`): CMPU, CMPS, SWI3

### Addressing Modes

All MC6809 addressing modes are implemented:

| Mode | Syntax | Example | Notes |
|------|--------|---------|-------|
| Inherent | — | CLRA | No operand |
| Immediate 8 | #nn | LDA #$42 | |
| Immediate 16 | #nnnn | LDD #$1234 | |
| Direct | dp:nn | LDA $30 | DP register provides high byte |
| Extended | nnnn | LDA $1234 | Full 16-bit address |
| Indexed | various | LDA ,X | Complex postbyte decoding (see below) |
| Relative 8 | offset | BNE loop | Signed 8-bit (-128..+127) |
| Relative 16 | offset | LBNE loop | Signed 16-bit |

**Indexed sub-modes** (decoded from postbyte):
- Constant offset: 5-bit signed, 8-bit signed, 16-bit signed
- Register offset: A,R / B,R / D,R
- Auto-increment: ,R+ / ,R++ (post-increment by 1 or 2)
- Auto-decrement: ,-R / ,--R (pre-decrement by 1 or 2)
- Zero offset: ,R
- PC-relative: 8-bit or 16-bit offset from PC
- Indirect: [any of the above] — adds an extra memory read for the effective address
- Extended indirect: [nnnn]

### Performance Optimizations

- **Branchless flag computation**: ALU helpers (`op_add8`, `op_sub8`, `op_add16`, `op_sub16`, `update_nz8`, `update_nz16`) use a compute-and-mask
pattern — flags are accumulated into a local variable `f` and written to `cpu->cc` in a single masked OR. The `CC_PUT` macro uses a branchless ternary that compiles to Xtensa MOVNEZ. This optimization improved performance from ~23.5 to ~25–27 fps.
- **Direct RAM access (OPT-M2)**: for addresses below `fast_limit`, `mem_read`/`mem_write` use the machine's 8 KB page tables (`rd_page`/`wr_page`) and skip the read/write callbacks. The CoCo 3 tables belong to the GIME (`fast_limit` = $FE00); the CoCo 2 tables are built by `update_fast_pages_coco2()` in `machine.cpp` (`fast_limit` = $FF00: RAM below $8000, ROM pages read-only, everything RAM in SAM all-RAM mode).
- **No PSRAM store workaround in the core**: both CPU files include `utils/no_psram_memw.h`; see `performance.md`.
- **Inline memory helpers**: `mem_read`, `mem_write`, `fetch8`, `fetch16`, push/pull helpers are all `static inline` to eliminate function call overhead in the hot instruction loop.
- **D register as single uint16_t**: A and B are stored as the high and low bytes of a single `uint16_t d`, accessed via `GET_A()` / `GET_B()` macros and `SET_A()` / `SET_B()`. This makes 16-bit D operations (ADDD, SUBD, LDD, STD) naturally efficient.
- **No IRAM_ATTR**: Testing showed that placing CPU functions in IRAM actually hurt performance on ESP32-S3 (flash cache is faster than IRAM for large code).

---

## HD6309

The HD6309 runs all MC6809 code and adds registers, instructions and a faster
*native* mode. It starts in *emulation* mode, where timing and interrupt
stacking match the MC6809.

### Extra registers

| Register | Width | Description |
|----------|-------|-------------|
| `w` | 16-bit | Accumulator W (E = high byte, F = low byte) |
| Q | 32-bit | D:W as one register (LDQ, STQ, DIVQ, MULD) |
| `v` | 16-bit | Value register; only TFR/EXG and the register-to-register group reach it. Survives reset |
| `md` | 8-bit | Mode/error: bit 0 native mode, bit 1 FIRQ stacks everything, bit 6 illegal-instruction trap taken, bit 7 divide-by-zero trap taken |
| zero | — | TFR/EXG codes `$C` and `$D` read as 0 |

### What is implemented

- Registers E, F, W (= E:F), V, MD, Q (= D:W) and the zero register.
- All 6309 instructions: OIM/AIM/EIM/TIM, SEXW, LDQ/STQ, the register-to-register group (ADDR…CMPR), PSHSW/PULSW/PSHUW/PULUW, D/W/E/F inherent and arithmetic forms, the bit group (BAND…STBT), TFM, BITMD/LDMD, MULD, DIVD, DIVQ.
- Indexed modes `,W` `n,W` `,W++` `,--W` (and indirect), and `E,R` `F,R` `W,R`.
- Native mode (MD bit 0): shorter cycle counts, E and F stacked on interrupts and pulled by RTI. MD bit 1 makes FIRQ stack the whole state.
- Traps through `$FFF0`: illegal instruction (MD bit 6) and divide by zero (MD bit 7). Illegal 6809 opcodes trap in emulation mode too, as on the chip; the MC6809 build still treats them as a 2-cycle NOP.
- Reset returns to emulation mode; W and V keep their values.

### Native-mode timing

Every instruction adds its emulation-mode cycle count. In native mode the saving is subtracted afterwards from per-opcode tables at the end of `hd6309_ops.h` (plus a small table for indexed modes in `addr_indexed`). This is why the shared 6809 cases carry no native-mode code.

### TFM

TFM moves one byte per pass and rewinds PC to the start of the instruction while W ≠ 0. A pending interrupt is therefore taken between bytes and RTI resumes the transfer. `tfm_busy` / `tfm_pc` mark a transfer in progress so only the first pass pays the 6-cycle setup.

### Reference and known differences

Behaviour follows XRoar's `hd6309.c`. `tools/cpu_test` compares the two instruction by instruction (see below). The differences are deliberate:

| Case | Here | XRoar |
|---|---|---|
| TSTA/TSTB/TSTE/TSTF cycles | documented 2/1 and 3/2 | one more |
| OIM/AIM/EIM/TIM cycles | documented 6 / 7+ / 7, same in native | one fewer in native and indexed (marked unverified in XRoar) |
| MULD, high word zero | sets Z | sets N |
| TFM with an illegal register | traps and sets MD bit 6 | traps without setting it |
| M latch (DP as source of a 16-bit ADDR-group op) | reads as 0 | modelled |
| DAA, SEX: V flag | cleared (shared with the MC6809 build) | left / derived |

---

## Tests

`tools/cpu_test/run.sh` builds both CPU cores on the host and runs:

1. Hand-written vectors for the 6309 additions, interrupts, FIRQ mode, traps and TFM resume.
2. A check that legal MC6809 code behaves and times the same on the HD6309 build in emulation mode.
3. With `XROAR_SRC=<xroar tree>`: random instructions in both modes compared against XRoar for registers, memory and cycles.
