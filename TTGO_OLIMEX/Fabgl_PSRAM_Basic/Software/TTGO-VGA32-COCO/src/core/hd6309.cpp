#pragma GCC optimize("O2", "jump-tables")
/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : hd6309.cpp
 *  Module : Hitachi HD6309 CPU emulation — the 6809 interpreter rebuilt with the 6309 additions
 * ============================================================
*/

// Stores in this file go to internal RAM only (see the header for the rule).
#include "../utils/no_psram_memw.h"
#include "mc6809.h"

// The interpreter body is shared with mc6809.cpp; this build turns on the
// 6309 paths (hd6309_ops.h). mc6809_init/reset and the interrupt-line
// functions live in mc6809.cpp and serve both CPUs.
#define CPU_HD6309 1
#define CPU_RUN_NAME hd6309_run
#include "mc6809_core_impl.h"
