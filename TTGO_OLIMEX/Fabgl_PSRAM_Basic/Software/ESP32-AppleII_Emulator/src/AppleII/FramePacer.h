/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : FramePacer.h
 *  Module : Frame pacing. The Apple II draws 60.05 frames a second,
 *           17030 CPU cycles each (65 cycles x 262 lines). At 1X the
 *           loop runs one frame's cycles per 16652 us and waits out
 *           the rest; MAX runs four and never waits.
 *           While the disk motor runs it never waits either, and
 *           draws only every DISK_RENDER_US, so loading stays fast.
 *           No Arduino dependency: tests/host/pacer_test.cpp.
 * ============================================================
*/

#ifndef FRAME_PACER_H
#define FRAME_PACER_H

#include <cstdint>

struct FrameStep
{
	int  frames;    // emulated frames to run now, CYCLES_PER_FRAME cycles each
	bool render;    // draw once they have run
};

class FramePacer
{
public:
	enum Speed : uint8_t { SPEED_1X = 0, SPEED_MAX = 1, SPEED_COUNT = 2 };

	static constexpr int32_t  CYCLES_PER_FRAME    = 17030;
	static constexpr uint32_t FRAME_US            = 16652;       // 17030 cycles at 1.020484 MHz
	static constexpr int      MAX_FRAMES_PER_STEP = 4;           // MAX speed and fast disk
	static constexpr int      MAX_SKIP            = 3;           // when late, draw at least every 4th frame
	static constexpr uint32_t RESYNC_US           = 8 * FRAME_US; // further behind: drop the backlog
	static constexpr uint32_t DISK_RENDER_US      = 100000;      // drawing rate while the disk runs
	static constexpr uint32_t PAUSE_US            = 1000000;     // no step for this long: start afresh

	FramePacer();

	void  SetSpeed(Speed s);
	Speed GetSpeed() const { return speed; }
	static const char* Label(Speed s);

	// Before emulating. nowUs is a free-running microsecond clock that may
	// wrap; only differences are used.
	FrameStep Begin(uint32_t nowUs, bool diskBusy);
	// After emulating and drawing: microseconds to wait before the next Begin.
	uint32_t End(uint32_t nowUs) const;

private:
	Speed    speed;
	bool     started;      // false: the next Begin starts the schedule afresh
	bool     paced;        // the current step waits for its deadline
	uint32_t deadline;     // when the next step is due
	uint32_t lastRender;
	uint32_t lastBegin;    // when Begin last ran, to notice a long pause
	int      skipped;      // frames not drawn in a row
};

#endif
