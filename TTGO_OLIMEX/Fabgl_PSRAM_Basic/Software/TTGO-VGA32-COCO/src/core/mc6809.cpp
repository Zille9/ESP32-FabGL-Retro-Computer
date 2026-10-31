#pragma GCC optimize("O2", "jump-tables")
/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : mc6809.cpp
 *  Module : Motorola MC6809 CPU emulation — full opcode set with accurate cycle counts
 * ============================================================
*/

// Stores in this file go to internal RAM only (see the header for the rule).
#include "../utils/no_psram_memw.h"
#include "mc6809.h"

#define CPU_HD6309 0
#define CPU_RUN_NAME mc6809_run
#include "mc6809_core_impl.h"

void mc6809_nmi(MC6809* cpu, bool active) {
    if (active && !cpu->nmi_line) {
        // Edge-triggered: latch on inactive→active transition only
        cpu->nmi_pending = true;
    }
    cpu->nmi_line = active;
}

void mc6809_firq(MC6809* cpu, bool active) {
    cpu->firq_pending = active;
}

void mc6809_irq(MC6809* cpu, bool active) {
    cpu->irq_pending = active;
}
