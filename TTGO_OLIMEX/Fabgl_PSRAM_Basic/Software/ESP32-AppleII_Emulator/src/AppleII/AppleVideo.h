/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleVideo.h
 *  Module : Apple II video (interface). Render caches, the FPS
 *           overlay and the Render entry point.
 * ============================================================
*/

#ifndef APPLE_VIDEO_H
#define APPLE_VIDEO_H

#include "Predef.h"
#include "AppleFont.h"

class Memory;
class Apple2Device;
class VGA;

// Reads the video mode from Apple2Device (the soft switches live there) and
// the screen from Memory, and paints only the cells that changed since the
// last frame into the persistent VGA framebuffer.
class AppleVideo
{
public:
	AppleVideo();

	void Create();
	void Reset();
	// IIe character generator: the first 2K of the video ROM. Until one is
	// loaded, text uses the built-in ][+ font and its inverse/flash rules.
	void LoadCharRom(const BYTE* rom);
	void Render(Memory& mem, const Apple2Device& dev, int frame, VGA* vga);

	// Everything repaints on the next frame, palette and border included:
	// for when something else (the supervisor) has drawn over the screen.
	void InvalidateRenderCache();
	// marks only the cells under the FPS overlay dirty, so the emulator
	// repaints them instead of trusting a cache the overlay has scribbled on
	void InvalidateFpsOverlayRegion();

private:
	// Set for the duration of Render(); the helpers below write straight
	// into the VGA framebuffer, so there is no backbuffer.
	VGA* vga;
	AppleFont font;
	bool fullRepaint;
	bool charRom;
	int  lastMode;              // video switches the caches were drawn under

	int LoResCache[24][40];
	// text cells already drawn: glyph | 0x100 when drawn inverse, -1 = dirty.
	// 80 wide for the IIe's 80-column mode.
	int TextCache[24][80];
	int HiResCache[192][40];

	void InvalidateCells();
	void RenderText(Memory& mem, const Apple2Device& dev, int page, int firstLine, int frame, bool col80);
	void RenderLores(Memory& mem, int page, int lines);
	void RenderHires(Memory& mem, int page, int lines);
	void RenderDoubleHires(Memory& mem, int page, int lines);
	void RenderFpsOverlay(int fps, const char* speed);
};

#endif
