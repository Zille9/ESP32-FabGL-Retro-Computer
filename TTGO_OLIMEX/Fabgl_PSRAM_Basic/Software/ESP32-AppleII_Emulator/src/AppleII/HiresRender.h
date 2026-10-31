/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HiresRender.h
 *  Module : Hires dots to framebuffer bytes through a lookup table.
 *           A dot's colour depends on the dot, the dot to its left,
 *           its column parity and its byte's bit 7; the table holds,
 *           for every byte, both parities and both left neighbours,
 *           the 7 framebuffer bytes that byte draws. No Arduino
 *           dependency: tests/host/hires_test.cpp.
 * ============================================================
*/

#ifndef HIRES_RENDER_H
#define HIRES_RENDER_H

#include <cstdint>

namespace HiresRender
{
	// Palette index for (odd dot * 8 + colour set * 4 + dot * 2 + left dot):
	// black, green, violet, white / black, orange, blue, white, and the
	// same with green/violet and orange/blue swapped on odd dots.
	extern const uint8_t COLOR[16];

	// Fills the table (8 KB). Call once before DrawCell.
	void Init();

	// Draws the cell of screen bytes b0 (left) and b1, whose first dot is on
	// an even column, into out[0..13], one framebuffer byte per dot. pbit is
	// the last dot of the cell to the left (0 at the screen edge). Returns
	// this cell's last dot: the next cell's pbit.
	uint8_t DrawCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out);

	// One screen line: the 40 bytes at src into out[0..279], redrawing only
	// cells whose bytes or left dot changed. cache[col] (even col) remembers
	// what each cell was drawn from; set it to -1 to force a redraw. The left
	// dot is read from memory, so a cell never depends on another cell's
	// cache being right.
	void DrawLine(const uint8_t* src, int* cache, uint8_t* out);
}

#endif
