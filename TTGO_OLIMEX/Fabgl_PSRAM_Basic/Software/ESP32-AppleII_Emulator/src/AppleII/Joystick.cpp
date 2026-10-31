/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Joystick.cpp
 *  Module : Apple II joystick. A mouse does not spring back like
 *           a real stick, so the position simply stays where the
 *           mouse left it until Center() (middle button, F3).
 * ============================================================
*/

#include "Joystick.h"

static const int MAX_POS = 255 * Joystick::SUBUNITS;

static int Clamp(int v)
{
	return v < 0 ? 0 : (v > MAX_POS ? MAX_POS : v);
}

void Joystick::Reset()
{
	Center();
	btn[0] = btn[1] = false;
	// long ago: every timer has already run out until the first trigger
	trigTick = -(1LL << 40);
}

void Joystick::Center()
{
	pos[0] = pos[1] = CENTER * SUBUNITS;
}

void Joystick::AddMotion(int dx, int dy)
{
	pos[0] = Clamp(pos[0] + dx * MOUSE_STEPS);
	pos[1] = Clamp(pos[1] - dy * MOUSE_STEPS);
}

int Joystick::Axis(int n) const
{
	return (n == 0 || n == 1) ? pos[n] / SUBUNITS : 0;
}

uint8_t Joystick::ReadPaddle(int n, long long tick) const
{
	return tick - trigTick < (long long)Axis(n) * PDL_CYCLES_PER_UNIT ? 0x80 : 0;
}
