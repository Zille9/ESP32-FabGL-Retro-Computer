/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleFont.h
 *  Module : Apple II character generator (interface). 1-bit glyph
 *           rows and the RenderFont entry point.
 * ============================================================
*/

#ifndef APPLE2_FONT_H
#define APPLE2_FONT_H

#include "Predef.h"

class VGA;

class AppleFont
{
private:
	// One byte per glyph row, bit 6 = leftmost of the 7 pixels. 256 glyphs
	// for the IIe character ROM; the built-in ][+ font fills the first 128.
	unsigned char glyphs[256][FONT_Y];
	unsigned char* read_bmp_memory(char* buffer, int* _w, int* _h);
public:
	AppleFont();
	~AppleFont();

	void Create();
	// Replaces the glyphs with a IIe character ROM set: 256 glyphs of 8
	// bytes, bit 0 = leftmost dot, a dot lit where its bit is clear.
	void LoadCharRom(const unsigned char* rom);
	// Draws straight into the VGA framebuffer. There is no intermediate
	// backbuffer: the framebuffer itself persists between frames, which is
	// what lets the callers' dirty-cell caches skip unchanged glyphs.
	// fg/bg are palette indices and inv swaps them. Each font pixel is scaleX
	// framebuffer pixels wide: 2 for 40-column text on the 640-wide screen.
	void RenderFont(VGA *vga, int fontnum, int x, int y, bool inv,
	                int fg, int bg, int scaleX = 2);

};



#endif
