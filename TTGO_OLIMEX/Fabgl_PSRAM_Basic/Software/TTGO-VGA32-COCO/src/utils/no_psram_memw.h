/*
 * no_psram_memw.h — drop the PSRAM cache-bug workaround from one source file.
 *
 * ESP32 silicon before rev 3 can return stale data when a store to external
 * RAM (PSRAM) is closely followed by a load from it. The board is built with
 * -mfix-esp32-psram-cache-issue (strategy "memw"), which makes the compiler
 * emit a `memw` after stores in ALL code, because it cannot tell where a
 * pointer leads. The emulator's hot paths pay for that heavily: ~40% of every
 * CoCo 3 frame (see docs/performance.md, OPT-M1).
 *
 * GCC for Xtensa has no per-function switch for the fix, so a file that
 * includes this header defines an empty assembler macro named `memw`; the
 * compiler's inserted `memw`s then assemble to nothing in that file only.
 *
 * RULE: include this ONLY in a file whose stores all go to internal RAM —
 * globals/statics, the stack, and structs embedded in the global `Machine`.
 * Loads from PSRAM are fine (the compiler adds nothing for loads either).
 * Remember that malloc() of more than 4 KB may return PSRAM
 * (CONFIG_SPIRAM_USE_MALLOC). If such a file ever needs a store that can
 * land in PSRAM, follow it with PSRAM_STORE_BARRIER().
 *
 * Build with -DPSRAM_MEMW_KEEP to leave the workaround fully in place.
 */
#ifndef NO_PSRAM_MEMW_H
#define NO_PSRAM_MEMW_H

#if defined(__XTENSA__) && !defined(PSRAM_MEMW_KEEP)
__asm__(".macro memw\n.endm");
// A real `memw` (opcode 0x0020C0), immune to the macro above.
#define PSRAM_STORE_BARRIER() __asm__ volatile(".byte 0xc0, 0x20, 0x00")
#else
#define PSRAM_STORE_BARRIER() ((void)0)
#endif

#endif
