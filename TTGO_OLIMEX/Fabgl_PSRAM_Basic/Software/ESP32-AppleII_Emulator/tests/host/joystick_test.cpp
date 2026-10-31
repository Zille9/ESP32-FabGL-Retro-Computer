/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : joystick_test.cpp
 *  Module : Host-side joystick test. Mouse motion to axis values,
 *           clamping, buttons, and the paddle timer bit that PREAD
 *           counts from $C070 to $C064-$C067.
 *           Driven by tests/host/run-joystick-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>

#include "Joystick.h"

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

static void expectEq(long long got, long long want, const std::string& what)
{
	expect(got == want, what + " (got " + std::to_string(got) + ", want " + std::to_string(want) + ")");
}

static void TestReset()
{
	Joystick j;
	expectEq(j.Axis(0), 127, "reset: X centred");
	expectEq(j.Axis(1), 127, "reset: Y centred");
	expect(!j.Button(0) && !j.Button(1), "reset: buttons up");
	expectEq(j.ReadPaddle(0, 0), 0, "reset: no timer running before the first trigger");
}

static void TestMotion()
{
	Joystick j;
	j.AddMotion(2, 0);                     // two counts right: one unit
	expectEq(j.Axis(0), 128, "right 2 counts");
	j.AddMotion(0, 2);                     // mouse up: Apple Y goes down
	expectEq(j.Axis(1), 126, "up 2 counts, Y inverted");

	j.AddMotion(10000, -10000);
	expectEq(j.Axis(0), 255, "X clamps at 255");
	expectEq(j.Axis(1), 255, "Y clamps at 255");
	j.AddMotion(-10000, 10000);
	expectEq(j.Axis(0), 0, "X clamps at 0");
	expectEq(j.Axis(1), 0, "Y clamps at 0");

	j.Center();
	expectEq(j.Axis(0), 127, "Center X");
	expectEq(j.Axis(1), 127, "Center Y");

	expectEq(j.Axis(2), 0, "paddle 2 has no axis");
	expectEq(j.Axis(3), 0, "paddle 3 has no axis");
}

static void TestButtons()
{
	Joystick j;
	j.SetButtons(true, false);
	expect(j.Button(0) && !j.Button(1), "button 0 only");
	j.SetButtons(false, true);
	expect(!j.Button(0) && j.Button(1), "button 1 only");
	expect(!j.Button(2) && !j.Button(-1), "out of range buttons are up");
	j.Center();
	expect(j.Button(1), "Center keeps buttons");
}

static void TestPaddleTimer()
{
	const long long t0 = 1000000;
	Joystick j;

	// centred: 127 units
	j.Trigger(t0);
	long long span = 127LL * Joystick::PDL_CYCLES_PER_UNIT;
	expectEq(j.ReadPaddle(0, t0), 0x80, "centre: timing right after trigger");
	expectEq(j.ReadPaddle(0, t0 + span - 1), 0x80, "centre: still timing one cycle before");
	expectEq(j.ReadPaddle(0, t0 + span), 0, "centre: done at 127*11");

	// full scale
	j.AddMotion(10000, -10000);
	j.Trigger(t0);
	span = 255LL * Joystick::PDL_CYCLES_PER_UNIT;
	expectEq(j.ReadPaddle(1, t0 + span - 1), 0x80, "255: still timing one cycle before");
	expectEq(j.ReadPaddle(1, t0 + span), 0, "255: done at 255*11");

	// zero: never set
	j.AddMotion(-10000, 10000);
	j.Trigger(t0);
	expectEq(j.ReadPaddle(0, t0), 0, "0: done immediately");

	// paddles 2 and 3 are not connected
	j.Center();
	j.Trigger(t0);
	expectEq(j.ReadPaddle(2, t0), 0, "paddle 2 reads 0");
	expectEq(j.ReadPaddle(3, t0), 0, "paddle 3 reads 0");

	// a new trigger restarts the count
	j.Trigger(t0 + 5000);
	expectEq(j.ReadPaddle(0, t0 + 5000 + 100), 0x80, "retrigger restarts timing");
}

// What the ROM's PREAD would count: loop while bit 7 is set, 11 cycles a pass
static void TestPreadCount()
{
	Joystick j;
	j.AddMotion(-40, 60);                  // X 107, Y 97
	for (int n = 0; n < 2; n++)
	{
		long long tick = 5000;
		j.Trigger(tick);
		int count = 0;
		while (count < 255 && (j.ReadPaddle(n, tick) & 0x80))
		{
			tick += Joystick::PDL_CYCLES_PER_UNIT;
			count++;
		}
		expectEq(count, j.Axis(n), "PREAD count equals axis " + std::to_string(n));
	}
}

int main()
{
	printf("==> Joystick tests\n");
	TestReset();
	TestMotion();
	TestButtons();
	TestPaddleTimer();
	TestPreadCount();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
