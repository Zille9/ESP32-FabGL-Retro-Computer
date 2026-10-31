/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : speed_meter.h
 *  Module : Emulated CPU speed in real time (cycles per wall-clock second)
 * ============================================================
*/

#ifndef SPEED_METER_H
#define SPEED_METER_H

#include <stdint.h>

struct Machine;

// Real-hardware CPU clocks: 14.31818 MHz master / 16 (slow) or / 8 (fast).
#define SPEED_HZ_SLOW   894886.25f
#define SPEED_HZ_FAST   1789772.5f

// Snapshot published once per ~1 s window of emulated frames.
typedef struct {
    float    mhz;               // emulated cycles / wall-clock us
    float    target_mhz;        // what real hardware would run at the guest's rate setting
    float    fps;               // emulated frames per wall-clock second
    uint32_t frame_us_avg;      // emulation work per frame (no pace wait)
    uint32_t frame_us_max;
    uint32_t cycles_per_frame;  // the emulator's current per-frame cycle budget
    uint8_t  rate;              // CoCo 3: GIME R1 (0/1). CoCo 2: SAM R1:R0 (0-3)
    uint32_t windows;           // published windows since boot (0 = no data yet)
} SpeedStats;

// After each emulated frame: cycles it ran and the work time it took.
void speed_meter_frame(const struct Machine* m, uint32_t work_us);

// Emulation stopped this loop pass (OSD, debugger pause): the wall time until
// the next frame is not counted.
void speed_meter_gap(void);

void speed_meter_get(SpeedStats* out);

#endif // SPEED_METER_H
