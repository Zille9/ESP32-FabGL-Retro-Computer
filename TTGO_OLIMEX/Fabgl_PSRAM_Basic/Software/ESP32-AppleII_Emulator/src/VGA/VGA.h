/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : VGA.h
 *  Module : Hardware seam between the emulator and FabGL. Thin
 *           wrapper over the global fabgl::VGA16Controller: a
 *           640x200 framebuffer of 4-bit palette indices, two
 *           pixels per byte with the even x in the high nibble,
 *           and the 16-entry palette that maps them to colours.
 * ============================================================
*/

#ifndef VGA_H
#define VGA_H

#include "fabgl.h"
#include <esp_heap_caps.h>

// VGA16Controller with its framebuffer pinned to internal RAM. Stock FabGL
// already puts it there, but installs patched to use PSRAM exist (this
// project's build machine has one). For the emulator PSRAM is the wrong
// place: the ISR streams the framebuffer over the same bus the flash-resident
// interpreter uses, and that cost ~20% of emulation speed. Overriding the
// allocation makes the firmware behave the same on either library.
class AppleVGAController : public fabgl::VGA16Controller
{
protected:
	void allocateViewPort() override
	{
		// 4 bits per pixel: a row is half the width in bytes
		VGABaseController::allocateViewPort(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, getViewPortWidth() / 2);
		for (int i = 0; i < VGA16_LinesCount; ++i)
			m_lines[i] = (uint8_t*) heap_caps_malloc(getViewPortWidth(), MALLOC_CAP_DMA);
	}

public:
	// FabGL resolves the colours drawn through fabgl::Canvas to palette
	// indices with a table it builds once, from its own default palette;
	// setPaletteItem() leaves it alone. Whoever loads a palette and then
	// draws with Canvas rebuilds the table here.
	void refreshColorLookup() { updateRGB2PaletteLUT(); }

	// the index a Canvas colour resolves to, for checking a palette
	int paletteIndexOf(fabgl::RGB888 const & color) { return RGB888toPaletteIndex(color); }
};

extern AppleVGAController DisplayController;

#define VGA_WIDTH   640
#define VGA_HEIGHT  200

// One palette entry, 8 bits per channel; the DAC keeps the top two bits.
struct VGAColor
{
	uint8_t r, g, b;
};

class VGA
{
public:
	fabgl::Canvas *canvas;

public:
	VGA() : canvas(nullptr) {}
	~VGA() {}

	void setCanvas(fabgl::Canvas *c) {
		canvas = c;
	}

	bool init() { return true; }
	bool start() { return true; }
	bool show() { return true; }

	// All 16 entries. The framebuffer holds indices, so everything already
	// on screen recolours from the next scanline on.
	void setPalette(const VGAColor pal[16])
	{
		for (int i = 0; i < 16; ++i)
			DisplayController.setPaletteItem(i, fabgl::RGB888(pal[i].r, pal[i].g, pal[i].b));
	}

	// Packed scanline: byte x/2 holds pixel x (high nibble) and x+1 (low).
	// Callers drawing many pixels fetch this once per row.
	uint8_t *row(int y)
	{
		return DisplayController.getScanline(y);
	}

	// Two identical pixels at an even x: one whole byte, no masking. Content
	// drawn at double width (40-column text, lores, hires) uses only this.
	static inline void pair(uint8_t *row, int x, int idx)
	{
		row[x >> 1] = (idx << 4) | idx;
	}

	static inline void put(uint8_t *row, int x, int idx)
	{
		uint8_t &b = row[x >> 1];
		b = (x & 1) ? ((b & 0xF0) | idx) : ((b & 0x0F) | (idx << 4));
	}

	void dot(int x, int y, int idx)
	{
		if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT)
			put(row(y), x, idx);
	}

	void fillRect(int x, int y, int w, int h, int idx)
	{
		if (x < 0) { w += x; x = 0; }
		if (y < 0) { h += y; y = 0; }
		if (x + w > VGA_WIDTH)  w = VGA_WIDTH - x;
		if (y + h > VGA_HEIGHT) h = VGA_HEIGHT - y;
		if (w <= 0 || h <= 0)
			return;
		for (int yy = y; yy < y + h; ++yy) {
			uint8_t *r = row(yy);
			int xx = x, end = x + w;
			// odd edges take half a byte; the middle is whole bytes
			if (xx & 1)
				put(r, xx++, idx);
			if ((end & 1) && end > xx)
				put(r, --end, idx);
			if (end > xx)
				memset(r + (xx >> 1), (idx << 4) | idx, (end - xx) >> 1);
		}
	}

	void clear(int idx = 0)
	{
		for (int y = 0; y < VGA_HEIGHT; ++y)
			memset(row(y), (idx << 4) | idx, VGA_WIDTH / 2);
	}
};

#endif // VGA_H
