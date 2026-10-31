/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : speed_meter.cpp
 *  Module : Emulated CPU speed in real time (cycles per wall-clock second)
 * ============================================================
*/

#include "speed_meter.h"
#include "machine.h"
#include <esp_timer.h>

#define SPEED_WINDOW_US  1000000

static int64_t  s_last_us;          // previous frame's call (0 = after a gap)
static uint64_t s_win_us;
static uint64_t s_win_cycles;
static uint32_t s_win_frames;
static uint64_t s_win_work_us;
static uint32_t s_win_work_max;

// Written on core 1, read by the debug server on core 0. Fields are 32-bit,
// so a read during an update can mix two windows but never tears a value.
static volatile SpeedStats s_stats;

static uint8_t guest_rate(const Machine* m) {
    if (g_machine_type == 4) return m->gime.R1 ? 1 : 0;
    return (m->sam.reg >> SAM_REG_R0) & 3;
}

static float guest_target_mhz(uint8_t rate) {
    // CoCo 2 rate 1 is address-dependent (ROM fast, RAM slow): report slow.
    bool fast = (g_machine_type == 4) ? rate != 0 : rate >= 2;
    return (fast ? SPEED_HZ_FAST : SPEED_HZ_SLOW) / 1e6f;
}

void speed_meter_frame(const Machine* m, uint32_t work_us) {
    int64_t now = esp_timer_get_time();
    if (s_last_us == 0) {           // first frame after boot or a gap: start timing
        s_last_us = now;
        return;
    }
    s_win_us     += (uint64_t)(now - s_last_us);
    s_last_us     = now;
    s_win_cycles += m->cycles_this_frame;
    s_win_frames++;
    s_win_work_us += work_us;
    if (work_us > s_win_work_max) s_win_work_max = work_us;

    if (s_win_us < SPEED_WINDOW_US) return;

    uint8_t rate = guest_rate(m);
    s_stats.mhz              = (float)s_win_cycles / (float)s_win_us;
    s_stats.target_mhz       = guest_target_mhz(rate);
    s_stats.fps              = (float)s_win_frames * 1e6f / (float)s_win_us;
    s_stats.frame_us_avg     = (uint32_t)(s_win_work_us / s_win_frames);
    s_stats.frame_us_max     = s_win_work_max;
    s_stats.cycles_per_frame = m->cycles_per_frame;
    s_stats.rate             = rate;
    s_stats.windows          = s_stats.windows + 1;

    s_win_us = s_win_cycles = s_win_work_us = 0;
    s_win_frames = s_win_work_max = 0;
}

void speed_meter_gap(void) {
    s_last_us = 0;
}

void speed_meter_get(SpeedStats* out) {
    out->mhz              = s_stats.mhz;
    out->target_mhz       = s_stats.target_mhz;
    out->fps              = s_stats.fps;
    out->frame_us_avg     = s_stats.frame_us_avg;
    out->frame_us_max     = s_stats.frame_us_max;
    out->cycles_per_frame = s_stats.cycles_per_frame;
    out->rate             = s_stats.rate;
    out->windows          = s_stats.windows;
}
