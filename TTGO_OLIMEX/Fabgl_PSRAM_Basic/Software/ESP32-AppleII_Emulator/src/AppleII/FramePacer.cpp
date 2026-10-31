/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : FramePacer.cpp
 *  Module : Frame pacing, see FramePacer.h.
 * ============================================================
*/

#include "FramePacer.h"

// gnu++11 (the ESP32 core) still wants these defined somewhere once odr-used
constexpr int32_t  FramePacer::CYCLES_PER_FRAME;
constexpr uint32_t FramePacer::FRAME_US;
constexpr int      FramePacer::MAX_FRAMES_PER_STEP;
constexpr int      FramePacer::MAX_SKIP;
constexpr uint32_t FramePacer::RESYNC_US;
constexpr uint32_t FramePacer::DISK_RENDER_US;
constexpr uint32_t FramePacer::PAUSE_US;

FramePacer::FramePacer()
	: speed(SPEED_1X), started(false), paced(false), deadline(0), lastRender(0), lastBegin(0), skipped(0)
{
}

void FramePacer::SetSpeed(Speed s)
{
	speed = (s < SPEED_COUNT) ? s : SPEED_1X;
	// a new speed starts its own schedule rather than catching up on the old one
	started = false;
}

const char* FramePacer::Label(Speed s)
{
	switch (s)
	{
		case SPEED_MAX: return "MX";
		default:        return "1X";
	}
}

FrameStep FramePacer::Begin(uint32_t now, bool diskBusy)
{
	FrameStep step;
	// A long gap since the last step (the supervisor held the loop) makes the
	// old deadline meaningless - past half the 32-bit clock it would even
	// look far ahead - so start the schedule again. Unsigned: any gap counts.
	if (started && (uint32_t)(now - lastBegin) > PAUSE_US)
		started = false;
	lastBegin = now;
	if (!started)
	{
		started = true;
		deadline = now;
		lastRender = now - DISK_RENDER_US;
		skipped = 0;
	}

	if (diskBusy || speed == SPEED_MAX)
	{
		// flat out; a paced step after this one starts its schedule from here
		paced = false;
		step.frames = MAX_FRAMES_PER_STEP;
		step.render = !diskBusy || (int32_t)(now - lastRender) >= (int32_t)DISK_RENDER_US;
		deadline = now;
	}
	else
	{
		paced = true;
		step.frames = 1;
		int32_t late = (int32_t)(now - deadline);
		if (late > (int32_t)RESYNC_US)
		{
			// too far behind to catch up: forget the backlog
			deadline = now;
			late = 0;
		}
		// more than a frame behind: skip drawing, but never for long
		step.render = !(late > (int32_t)FRAME_US && skipped < MAX_SKIP);
		skipped = step.render ? 0 : skipped + 1;
		deadline += FRAME_US;
	}

	if (step.render)
		lastRender = now;
	return step;
}

uint32_t FramePacer::End(uint32_t now) const
{
	if (!paced)
		return 0;
	int32_t wait = (int32_t)(deadline - now);
	return wait > 0 ? (uint32_t)wait : 0;
}
