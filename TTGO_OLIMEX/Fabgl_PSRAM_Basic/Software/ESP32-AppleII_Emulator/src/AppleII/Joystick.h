/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Joystick.h
 *  Module : Apple II joystick (interface). Two axes built up from
 *           PS/2 mouse motion, two pushbuttons, and the 558 paddle
 *           timers read at $C064-$C067 and triggered at $C070.
 *           No FabGL or Arduino types, so it is tested on the host
 *           (tests/host/run-joystick-tests.sh).
 * ============================================================
*/

#pragma once

#include <cstdint>

class Joystick
{
public:
	// Emulated cycles per paddle unit: the ROM's PREAD loop counts one unit
	// every 11 cycles, so a paddle at 255 times out after about 2.8 ms.
	static const int PDL_CYCLES_PER_UNIT = 11;
	// Fixed-point steps per Apple unit, and steps added per mouse count:
	// 2 of 4 makes it one Apple unit for two counts of mouse travel.
	static const int SUBUNITS = 4;
	static const int MOUSE_STEPS = 2;
	static const int CENTER = 127;

	Joystick() { Reset(); }

	void Reset();                                   // centred, buttons up, no timer running
	void Center();                                  // both axes back to 127
	// Mouse delta as FabGL reports it: dy is positive when moving up, the
	// Apple's Y grows downward.
	void AddMotion(int dx, int dy);
	void SetButtons(bool b0, bool b1) { btn[0] = b0; btn[1] = b1; }

	void Trigger(long long tick) { trigTick = tick; }    // $C070
	uint8_t ReadPaddle(int n, long long tick) const;     // $C064+n: bit 7 while timing
	bool Button(int n) const { return n >= 0 && n < 2 && btn[n]; }
	int Axis(int n) const;                               // 0..255; paddles 2, 3 read 0

private:
	int pos[2];                                     // in SUBUNITS
	bool btn[2];
	long long trigTick;
};
