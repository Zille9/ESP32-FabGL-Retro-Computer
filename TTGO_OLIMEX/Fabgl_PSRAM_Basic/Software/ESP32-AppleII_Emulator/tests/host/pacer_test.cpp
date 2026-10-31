/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : pacer_test.cpp
 *  Module : Host-side FramePacer test: real-time pacing at 1X,
 *           flat out at MAX and while the disk runs, bounded
 *           frame skipping when overloaded, and the clock wrap.
 *           Driven by tests/host/run-pacer-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>

#include "FramePacer.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

struct Result
{
	uint32_t now;          // the clock after the last step
	int drawn;             // steps that drew
	int longestSkip;       // longest run of steps that did not
	int emulated;          // emulated frames run
};

// The emulation loop with a fake clock: each step takes workUs, then waits
// as End() says.
static Result Simulate(FramePacer& p, uint32_t start, int steps, uint32_t workUs, bool disk = false)
{
	Result r = { start, 0, 0, 0 };
	int skip = 0;
	for (int i = 0; i < steps; i++)
	{
		FrameStep s = p.Begin(r.now, disk);
		r.emulated += s.frames;
		r.now += workUs;
		if (s.render)
		{
			r.drawn++;
			skip = 0;
		}
		else if (++skip > r.longestSkip)
			r.longestSkip = skip;
		r.now += p.End(r.now);
	}
	return r;
}

static void TestLabels()
{
	expect(std::string(FramePacer::Label(FramePacer::SPEED_1X)) == "1X", "label 1X");
	expect(std::string(FramePacer::Label(FramePacer::SPEED_MAX)) == "MX", "label MX");
	FramePacer p;
	expect(p.GetSpeed() == FramePacer::SPEED_1X, "starts at 1X");
	p.SetSpeed((FramePacer::Speed)7);
	expect(p.GetSpeed() == FramePacer::SPEED_1X, "an unknown speed falls back to 1X");
}

static void TestRealTime()
{
	FramePacer p;
	Result r = Simulate(p, 1000, 600, 5000);
	uint32_t elapsed = r.now - 1000;
	expect(elapsed >= 599u * FramePacer::FRAME_US && elapsed <= 601u * FramePacer::FRAME_US,
	       "1X: 600 frames take 10 s");
	expect(r.drawn == 600, "1X: every frame drawn when there is time");
	expect(r.emulated == 600, "1X: one emulated frame per step");
}

static void TestOverload()
{
	// every step takes 25 ms, longer than a frame
	FramePacer p;
	Result r = Simulate(p, 0, 400, 25000);
	expect(r.longestSkip <= FramePacer::MAX_SKIP, "overload: at most MAX_SKIP frames skipped in a row");
	expect(r.drawn >= 400 / (FramePacer::MAX_SKIP + 1), "overload: at least one frame in MAX_SKIP+1 drawn");
	expect(r.now == 400u * 25000u, "overload: never waits while behind");
}

static void TestMaxSpeed()
{
	FramePacer p;
	p.SetSpeed(FramePacer::SPEED_MAX);
	Result r = Simulate(p, 0, 10, 3000);
	expect(r.emulated == 10 * FramePacer::MAX_FRAMES_PER_STEP, "MAX: four frames a step");
	expect(r.drawn == 10, "MAX: draws every step");
	expect(r.now == 30000, "MAX: never waits");
}

static void TestDiskBusy()
{
	// motor on at 1X: flat out, drawing about every DISK_RENDER_US
	FramePacer p;
	Result r = Simulate(p, 0, 100, 10000, true);
	expect(r.now == 1000000, "disk: never waits");
	expect(r.drawn >= 9 && r.drawn <= 11, "disk: draws about ten times a second");
	expect(r.emulated == 100 * FramePacer::MAX_FRAMES_PER_STEP, "disk: four frames a step");

	// motor off again: back to real time without a catch-up burst
	FrameStep s = p.Begin(r.now, false);
	expect(s.frames == 1 && s.render, "after disk: one frame, drawn");
	uint32_t wait = p.End(r.now + 2000);
	expect(wait > 0 && wait <= FramePacer::FRAME_US, "after disk: waits at most one frame");
}

static void TestClockWrap()
{
	FramePacer p;
	uint32_t start = 0xFFFFFFFFu - 5u * FramePacer::FRAME_US;
	Result r = Simulate(p, start, 20, 4000);
	expect(r.drawn == 20, "wrap: every frame drawn across the clock wrap");
	expect((uint32_t)(r.now - start) == 20u * FramePacer::FRAME_US, "wrap: 20 frames still take 20 frame times");
}

static void TestSpeedChangeRestarts()
{
	FramePacer p;
	p.SetSpeed(FramePacer::SPEED_MAX);
	Simulate(p, 0, 10, 4000);
	p.SetSpeed(FramePacer::SPEED_1X);                         // MAX -> 1X
	uint32_t late = 10u * 4000u + 50000;                      // arrives 50 ms later
	FrameStep s = p.Begin(late, false);
	expect(s.render && s.frames == 1, "speed change: first step drawn, one frame");
	expect(p.End(late + 4000) == FramePacer::FRAME_US - 4000, "speed change: the schedule restarts from now");
}

static void TestLongPause()
{
	// the supervisor held the loop for 40 minutes: past half the 32-bit
	// clock, where a stale deadline looks like it is still far ahead
	const uint32_t gap = 40u * 60u * 1000000u;
	FramePacer p;
	Result r = Simulate(p, 0, 10, 4000);
	FrameStep s = p.Begin(r.now + gap, false);
	expect(s.render && s.frames == 1, "long pause: next step drawn");
	uint32_t wait = p.End(r.now + gap + 4000);
	expect(wait <= FramePacer::FRAME_US, "long pause: waits at most one frame, not half an hour");

	// same with the disk running: drawing resumes straight away
	FramePacer d;
	Simulate(d, 0, 10, 10000, true);
	FrameStep ds = d.Begin(100000u + gap, true);
	expect(ds.render, "long pause, disk busy: next step drawn");
}

int main()
{
	TestLabels();
	TestRealTime();
	TestOverload();
	TestMaxSpeed();
	TestDiskBusy();
	TestClockWrap();
	TestSpeedChangeRestarts();
	TestLongPause();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
