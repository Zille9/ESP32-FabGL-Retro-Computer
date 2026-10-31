/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : SupervisorUI.cpp
 *  Module : Look and feel of the supervisor menu. Draws with
 *           FabGL's Canvas over the paused Apple picture, in a
 *           palette of its own that stays close to the Apple's: a
 *           blue box with the six Apple logo stripes top and
 *           bottom, white tiles and popups, a navy selection bar.
 *           Reads the PS/2 keyboard and, with the debug setting
 *           on, terminal keys from the serial port, and traces
 *           each selection there.
 * ============================================================
*/

#include <Arduino.h>
#include <stdarg.h>
#include "SupervisorUI.h"
#include "../Tools/Log.h"
#include "../VGA/VGA.h"

extern fabgl::Canvas Canvas;

namespace SupUI
{

bool debugOn = false;
bool serialKeys = true;
bool closeAll = false;

static fabgl::Keyboard* kbd = nullptr;

//////////////////////////////////////////////////////////////////////////
// Keys and trace

// The palette while the menu is open. It starts from the Apple's lores
// colours (applePalette in AppleVideo.cpp), so the paused picture around the
// box stays recognisable, and changes nine entries: 1, 3, 6, 9, 12 and 13
// become the Apple logo's stripe colours, 7 the navy accent, and 4 and 10 a
// pale aqua and a second white. AppleVideo loads its own palette again when
// the menu closes (InvalidateRenderCache).
//
// The last three are where they are because of how FabGL resolves a Canvas
// colour: it takes the first entry of the same hue whose saturation and
// brightness are within a wide margin. With black first, any grey lighter
// than (137,130,122) would resolve to black and white to that grey, so there
// is one grey and a white ahead of the pink; a pale blue would resolve to
// the navy, so the pale colour is an aqua, ahead of the Apple's own.
static const uint8_t menuPalette[16][3] =
{
	{   0,   0,   0 }, { 227,  27,  35 }, {  28, 116, 205 }, { 158,  40, 181 },
	{ 159, 210, 213 }, { 137, 130, 122 }, {   0, 160, 223 }, {   0,   0, 170 },
	{ 151,  88,  34 }, { 242, 101,  34 }, { 255, 255, 255 }, { 255, 206, 240 },
	{ 107, 182,  74 }, { 253, 185,  36 }, { 159, 210, 213 }, { 255, 255, 255 }
};

static void LoadPalette()
{
	for (int i = 0; i < 16; i++)
		DisplayController.setPaletteItem(i, fabgl::RGB888(menuPalette[i][0], menuPalette[i][1], menuPalette[i][2]));
	// Canvas colours go through FabGL's own lookup, see VGA.h
	DisplayController.refreshColorLookup();

	// every theme colour has to land on an entry that displays as itself
	static const fabgl::RGB888 theme[] =
	{
		T_INK, T_BG, T_TEXT, T_ACCENT, T_DIM, T_DIM,
		T_GREEN, T_YELLOW, T_ORANGE, T_RED, T_VIOLET, T_BLUE
	};
	bool ok = true;
	for (unsigned t = 0; t < sizeof(theme) / sizeof(theme[0]); t++)
	{
		int i = DisplayController.paletteIndexOf(theme[t]);
		fabgl::RGB888 shown(menuPalette[i][0], menuPalette[i][1], menuPalette[i][2]);
		if (fabgl::RGB888toPackedRGB222(shown) != fabgl::RGB888toPackedRGB222(theme[t]))
		{
			Trace("menu: palette BAD: theme colour %u resolves to entry %d\n", t, i);
			ok = false;
		}
	}
	if (ok)
		Trace("menu: palette ok\n");
}

// Reads the framebuffer back: the palette index of the box background and of
// each stripe. With the lookup right these are 2 and 12 13 9 1 3 6.
void TracePixels()
{
	if (!debugOn)
		return;
	int y = BOX_Y + 5;
	int w = (BOX_W - 16) / 6;
	int idx[7];
	for (int i = 0; i < 7; i++)
	{
		int x = (i == 0) ? BOX_X + 4 : BOX_X + 8 + (i - 1) * w + w / 2;
		int yy = (i == 0) ? BOX_Y + 20 : y;
		uint8_t b = DisplayController.getScanline(yy)[x >> 1];
		idx[i] = (x & 1) ? (b & 0x0F) : (b >> 4);
	}
	Trace("menu: pixels bg=%d stripes=%d %d %d %d %d %d\n",
	      idx[0], idx[1], idx[2], idx[3], idx[4], idx[5], idx[6]);
}

void Begin(fabgl::Keyboard* keyboard)
{
	kbd = keyboard;
	closeAll = false;
	LoadPalette();
	if (kbd)
		kbd->emptyVirtualKeyQueue();
}

static SerialKeyDecoder decoder;
static uint32_t lastByte = 0;

// Bytes that reached the port while nobody was reading it are not keys.
void FlushSerialKeys()
{
	while (Serial.available())
		Serial.read();
	decoder = SerialKeyDecoder();
}

SupKey WaitKey()
{
	for (;;)
	{
		fabgl::VirtualKeyItem item;
		if (kbd && kbd->getNextVirtualKey(&item, 10) && item.down)
		{
			switch (item.vk)
			{
				case fabgl::VK_UP:       return K_UP;
				case fabgl::VK_DOWN:     return K_DOWN;
				case fabgl::VK_LEFT:     return K_LEFT;
				case fabgl::VK_RIGHT:    return K_RIGHT;
				case fabgl::VK_RETURN:
				case fabgl::VK_KP_ENTER: return K_ENTER;
				case fabgl::VK_ESCAPE:   return K_ESC;
				case fabgl::VK_TAB:      return K_TAB;
				case fabgl::VK_PAGEUP:   return K_PGUP;
				case fabgl::VK_PAGEDOWN: return K_PGDN;
				case fabgl::VK_HOME:     return K_HOME;
				case fabgl::VK_END:      return K_END;
				case fabgl::VK_F1:       return K_F1;
				default: break;
			}
		}
		if (!kbd)
			delay(10);
		if (debugOn && serialKeys)
		{
			SupKey k = K_NONE;
			while (k == K_NONE && Serial.available())
			{
				k = decoder.Feed((uint8_t)Serial.read());
				lastByte = millis();
			}
			if (k == K_NONE)
				k = decoder.Idle(millis() - lastByte);
			if (k != K_NONE)
				return k;
		}
	}
}

void Trace(const char* fmt, ...)
{
	if (!debugOn)
		return;
	char line[112];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	LOGF("%s", line);
}

//////////////////////////////////////////////////////////////////////////
// Drawing

bool Same(fabgl::RGB888 a, fabgl::RGB888 b)
{
	return a.R == b.R && a.G == b.G && a.B == b.B;
}

void FillBox(fabgl::Canvas& cv, fabgl::RGB888 c, int x1, int y1, int x2, int y2)
{
	cv.setBrushColor(c);
	cv.fillRectangle(x1, y1, x2, y2);
}

void Text(fabgl::Canvas& cv, const fabgl::FontInfo* font, int x, int y, const char* s)
{
	cv.selectFont(font);
	cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true).DoubleWidth(1));
	cv.drawText(x, y, s);
}

int TextW(const fabgl::FontInfo* font, const char* s)
{
	return (int)strlen(s) * font->width * 2;
}

void TextCentered(fabgl::Canvas& cv, const fabgl::FontInfo* font, int x, int w, int y, const char* s)
{
	Text(cv, font, x + (w - TextW(font, s)) / 2, y, s);
}

// the six bands of the Apple logo
static void DrawStripes(fabgl::Canvas& cv, int y)
{
	static const fabgl::RGB888 band[6] = { T_GREEN, T_YELLOW, T_ORANGE, T_RED, T_VIOLET, T_BLUE };
	int x0 = BOX_X + 8, w = (BOX_W - 16) / 6;
	for (int i = 0; i < 6; i++)
		FillBox(cv, band[i], x0 + i * w, y, x0 + (i + 1) * w - 1, y + STRIPE_H - 1);
}

// blue box, stripes top and bottom, white title, key hints
void DrawFrame(fabgl::Canvas& cv, const char* title, const char* hint)
{
	FillBox(cv, T_BG, BOX_X, BOX_Y, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
	cv.setPenColor(T_ACCENT);
	cv.drawRectangle(BOX_X, BOX_Y, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
	cv.drawRectangle(BOX_X + 1, BOX_Y, BOX_X + BOX_W - 2, BOX_Y + BOX_H - 1);
	DrawStripes(cv, BOX_Y + 3);
	DrawStripes(cv, BOX_Y + BOX_H - 3 - STRIPE_H);

	cv.setBrushColor(T_BG);
	cv.setPenColor(T_TEXT);
	TextCentered(cv, &fabgl::FONT_8x14, BOX_X, BOX_W, TITLE_Y, title);
	cv.setPenColor(T_DIM);
	TextCentered(cv, &fabgl::FONT_6x8, BOX_X, BOX_W, HINT_Y, hint);
}

// One line: label on the left starting at x+textOff, value on the right. The
// highlighted line is an accent bar with white text. bg is the box's blue in
// a list, with white text and yellow values, and white in a popup, with black
// text and accent values.
void DrawRowAt(fabgl::Canvas& cv, int x, int y, int w, int textOff, bool compact,
               const Row& r, bool hl, fabgl::RGB888 bg)
{
	int h = compact ? LIST_ROW_H : MENU_ROW_H;
	const fabgl::FontInfo* font = compact ? &fabgl::FONT_8x8 : &fabgl::FONT_8x14;
	int cols = (w - textOff - 8) / 16;
	if (cols > 31) cols = 31;

	bool onWhite = Same(bg, T_TEXT);
	fabgl::RGB888 fg = onWhite ? T_INK : T_TEXT;
	fabgl::RGB888 valueFg = onWhite ? T_ACCENT : T_YELLOW;
	if (hl)
	{
		bg = T_ACCENT;
		fg = valueFg = T_TEXT;
	}
	else if (r.dim)
		fg = onWhite ? T_GREY : T_DIM;

	FillBox(cv, bg, x, y, x + w - 1, y + h - 1);
	int ty = y + (h - font->height) / 2;

	// Both in full when they fit. When they do not, the value keeps up to 16
	// characters and the label what is left; either is cut with a '~'.
	int llen = strlen(r.label);
	int vlen = strlen(r.value);
	if (vlen && llen + 1 + vlen > cols)
	{
		int keep = cols - 1 - llen;
		if (keep < 16) keep = 16;
		if (keep > cols - 2) keep = cols - 2;
		if (vlen > keep) vlen = keep;
	}
	int lroom = cols - (vlen ? vlen + 1 : 0);

	char text[32];
	cv.setBrushColor(bg);
	cv.setPenColor(fg);
	if (lroom > 0)
	{
		FitName(text, lroom, r.label);
		Text(cv, font, x + textOff, ty, text);
	}
	if (vlen > 0)
	{
		FitName(text, vlen, r.value);
		cv.setPenColor(valueFg);
		Text(cv, font, x + textOff + (cols - vlen) * 16, ty, text);
	}
}

void DrawScrollbar(fabgl::Canvas& cv, int y, int h, int top, int visible, int count)
{
	int x = BOX_X + BOX_W - 16;
	FillBox(cv, T_BG, x, y, x + 5, y + h - 1);
	if (count <= visible)
		return;
	int len = h * visible / count;
	if (len < 6) len = 6;
	int pos = (h - len) * top / (count - visible);
	FillBox(cv, T_ACCENT, x + 2, y, x + 3, y + h - 1);
	FillBox(cv, T_TEXT, x, y + pos, x + 5, y + pos + len - 1);
}

//////////////////////////////////////////////////////////////////////////
// Icons

// rectangle on an icon grid whose cells are two pixels wide
static void IBox(fabgl::Canvas& cv, fabgl::RGB888 c, int ox, int oy, int x1, int y1, int x2, int y2)
{
	FillBox(cv, c, ox + x1 * 2, oy + y1, ox + x2 * 2 + 1, oy + y2);
}

// A round disc h lines tall: pixels are 2.4 times taller than wide.
static void IDisc(fabgl::Canvas& cv, fabgl::RGB888 c, int cx, int cy, int h)
{
	cv.setBrushColor(c);
	cv.fillEllipse(cx, cy, h * 12 / 5, h);
}

void DrawSmallIcon(fabgl::Canvas& cv, int icon, int x, int y,
                   fabgl::RGB888 fg, fabgl::RGB888 bg, bool grey)
{
	switch (icon)
	{
		case ICON_DISK:         // 5.25" diskette: jacket, label, hub, head slot
			IBox(cv, grey ? T_DIM : T_INK, x, y, 3, 0, 12, 11);
			IBox(cv, grey ? T_GREY : T_YELLOW, x, y, 5, 1, 10, 3);
			IBox(cv, T_TEXT, x, y, 7, 6, 8, 7);
			IBox(cv, T_TEXT, x, y, 7, 9, 8, 10);
			break;
		case ICON_HDD:          // drive box: case, front panel, activity light
			IBox(cv, grey ? T_DIM : T_GREY, x, y, 1, 2, 14, 10);
			IBox(cv, grey ? T_GREY : T_INK, x, y, 1, 7, 14, 10);
			if (!grey)
				IBox(cv, T_GREEN, x, y, 11, 8, 13, 9);
			break;
		case ICON_MACHINE:      // computer: monitor with a green screen on its case
			IBox(cv, T_GREY, x, y, 3, 0, 12, 6);
			IBox(cv, T_GREEN, x, y, 4, 1, 11, 5);
			IBox(cv, T_DIM, x, y, 1, 7, 14, 11);
			IBox(cv, T_GREY, x, y, 3, 9, 12, 9);
			break;
		case ICON_KEYBOARD:     // keyboard: case, two rows of keys, space bar
			IBox(cv, T_GREY, x, y, 0, 1, 15, 10);
			for (int r = 0; r < 2; r++)
				for (int k = 0; k < 5; k++)
					IBox(cv, T_TEXT, x, y, 2 + k * 3 - r, 3 + r * 2, 3 + k * 3 - r, 3 + r * 2);
			IBox(cv, T_TEXT, x, y, 4, 8, 11, 8);
			break;
		case ICON_SERIAL:       // serial plug: shell, pins, cable
			IBox(cv, T_DIM, x, y, 2, 2, 13, 8);
			for (int p = 0; p < 4; p++)
				IBox(cv, T_INK, x, y, 4 + p * 2, 4, 4 + p * 2, 6);
			IBox(cv, fg, x, y, 7, 9, 8, 11);
			break;
		case ICON_CAPTURE:      // printed page
			IBox(cv, T_DIM, x, y, 4, 0, 11, 11);
			IBox(cv, T_TEXT, x, y, 5, 1, 10, 10);
			for (int l = 0; l < 3; l++)
				IBox(cv, T_GREY, x, y, 6, 3 + l * 3, 9, 3 + l * 3);
			break;
		case ICON_SPEED:        // gauge: half dial and a needle
			IDisc(cv, T_YELLOW, x + 16, y + 7, 10);
			IBox(cv, bg, x, y, 0, 8, 15, 12);
			cv.setPenColor(T_RED);
			cv.drawLine(x + 16, y + 7, x + 24, y + 2);
			cv.drawLine(x + 17, y + 7, x + 25, y + 2);
			break;
		case ICON_SPIDER:       // spider: body, head, four legs each side
		{
			static const int8_t legs[4][4] = { { 6, 5, 1, 2 }, { 5, 6, 0, 6 }, { 5, 8, 0, 9 }, { 6, 9, 2, 11 } };
			cv.setPenColor(fg);
			for (int l = 0; l < 4; l++)
			{
				cv.drawLine(x + legs[l][0] * 2, y + legs[l][1], x + legs[l][2] * 2, y + legs[l][3]);
				cv.drawLine(x + 31 - legs[l][0] * 2, y + legs[l][1], x + 31 - legs[l][2] * 2, y + legs[l][3]);
			}
			IBox(cv, fg, x, y, 5, 4, 10, 10);
			IBox(cv, fg, x, y, 6, 1, 9, 3);
			IBox(cv, T_RED, x, y, 6, 2, 6, 2);
			IBox(cv, T_RED, x, y, 9, 2, 9, 2);
			break;
		}
		default:
			break;
	}
}

void DrawBigIcon(fabgl::Canvas& cv, int icon, int x, int y,
                 fabgl::RGB888 fg, fabgl::RGB888 bg, bool grey)
{
	switch (icon)
	{
		case ICON_DISK:         // 5.25" diskette: jacket, label, hub ring, head slot, notch
			IBox(cv, grey ? T_DIM : T_INK, x, y, 4, 0, 27, 25);
			IBox(cv, grey ? T_GREY : T_YELLOW, x, y, 7, 2, 24, 7);
			IBox(cv, grey ? T_DIM : T_ORANGE, x, y, 7, 2, 24, 3);
			IDisc(cv, T_TEXT, x + 32, y + 14, 8);
			IDisc(cv, grey ? T_DIM : T_INK, x + 32, y + 14, 3);
			IBox(cv, T_TEXT, x, y, 15, 20, 16, 24);
			IBox(cv, bg, x, y, 27, 5, 27, 7);
			break;
		case ICON_HDD:
			IBox(cv, grey ? T_DIM : T_GREY, x, y, 2, 5, 29, 21);
			IBox(cv, grey ? T_GREY : T_INK, x, y, 2, 16, 29, 21);
			for (int v = 0; v < 2; v++)
				IBox(cv, grey ? T_GREY : T_DIM, x, y, 5, 8 + v * 3, 20, 8 + v * 3);
			if (!grey)
				IBox(cv, T_GREEN, x, y, 23, 18, 26, 19);
			break;
		case ICON_SETUP:        // three sliders
		{
			static const uint8_t knob[3] = { 7, 19, 12 };
			for (int r = 0; r < 3; r++)
			{
				int yy = 5 + r * 8;
				IBox(cv, fg, x, y, 3, yy, 28, yy + 1);
				IBox(cv, T_GREEN, x, y, knob[r], yy - 3, knob[r] + 4, yy + 4);
			}
			break;
		}
		case ICON_RESET:        // power symbol: ring open at the top, bar through the gap
			IDisc(cv, T_RED, x + 32, y + 14, 22);
			IDisc(cv, bg, x + 32, y + 14, 14);
			IBox(cv, bg, x, y, 11, 0, 20, 10);
			IBox(cv, T_RED, x, y, 14, 0, 17, 13);
			break;
		case ICON_ABOUT:        // "i" in a disc
			IDisc(cv, T_ORANGE, x + 32, y + 13, 24);
			IBox(cv, T_TEXT, x, y, 14, 5, 17, 7);
			IBox(cv, T_TEXT, x, y, 14, 10, 17, 20);
			break;
		case ICON_RESUME:       // play triangle
			for (int r = -11; r <= 11; r++)
			{
				int len = (11 - abs(r)) * 4;
				FillBox(cv, T_GREEN, x + 12, y + 13 + r, x + 12 + len, y + 13 + r);
			}
			break;
		default:
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Popup

#define POPUP_TITLE_H 18

static void TraceRow(const char* what, const char* title, const Row& row)
{
	Trace("%s: %s > %s%s%s\n", what, title, row.label, row.value[0] ? " " : "", row.value);
}

int Popup(const char* title, const char* msg1, const char* msg2, int count, RowFn rowFn, int selected)
{
	fabgl::Canvas& cv = Canvas;
	const char* msgs[2] = { msg1 ? msg1 : "", msg2 ? msg2 : "" };
	int nmsg = (msgs[0][0] ? 1 : 0) + (msgs[1][0] ? 1 : 0);
	int h = POPUP_TITLE_H + 8 + nmsg * 10 + (nmsg ? 4 : 0) + count * MENU_ROW_H + 6;
	int x = (SUP_SCREEN_W - POPUP_W) / 2, y = (SUP_SCREEN_H - h) / 2;

	FillBox(cv, T_TEXT, x, y, x + POPUP_W - 1, y + h - 1);
	cv.setPenColor(T_ACCENT);
	// a vertical line is one pixel and a horizontal one a whole scanline:
	// four columns each side match the two rows top and bottom
	for (int i = 0; i < 4; i++)
		cv.drawRectangle(x + i, y + i / 2, x + POPUP_W - 1 - i, y + h - 1 - i / 2);
	FillBox(cv, T_ACCENT, x + 4, y + 2, x + POPUP_W - 5, y + POPUP_TITLE_H + 1);
	cv.setBrushColor(T_ACCENT);
	cv.setPenColor(T_TEXT);
	TextCentered(cv, &fabgl::FONT_8x14, x, POPUP_W, y + 4, title);

	int ry = y + POPUP_TITLE_H + 8;
	cv.setBrushColor(T_TEXT);
	cv.setPenColor(T_INK);
	for (int i = 0; i < 2; i++)
	{
		if (!msgs[i][0])
			continue;
		char text[28];
		FitName(text, 27, msgs[i]);
		TextCentered(cv, &fabgl::FONT_8x8, x, POPUP_W, ry, text);
		ry += 10;
		Trace("popup: %s: %s\n", title, text);
	}
	if (nmsg)
		ry += 4;

	if (selected < 0 || selected >= count)
		selected = 0;
	int drawn = -1;
	for (;;)
	{
		Row row;
		for (int i = 0; i < count; i++)
		{
			if (drawn >= 0 && i != drawn && i != selected)
				continue;
			row.label[0] = row.value[0] = 0;
			row.dim = false;
			rowFn(i, row);
			DrawRowAt(cv, x + 8, ry + i * MENU_ROW_H, POPUP_W - 16, 16, false, row, i == selected, T_TEXT);
			if (i == selected)
				TraceRow("popup", title, row);
		}
		drawn = selected;
		cv.waitCompletion();

		switch (WaitKey())
		{
			case K_UP:    if (selected > 0) selected--; break;
			case K_DOWN:  if (selected < count - 1) selected++; break;
			case K_ENTER: return selected;
			case K_ESC:   return -1;
			case K_F1:    closeAll = true; return -1;
			default: break;
		}
	}
}

static void ConfirmRow(int idx, Row& row)
{
	strcpy(row.label, idx == 0 ? "No" : "Yes");
}

bool Confirm(const char* title, const char* msg1, const char* msg2)
{
	return Popup(title, msg1, msg2, 2, ConfirmRow, 0) == 1;
}

static void NoticeRow(int, Row& row)
{
	strcpy(row.label, "OK");
}

void Notice(const char* title, const char* msg1, const char* msg2)
{
	Popup(title, msg1, msg2, 1, NoticeRow, 0);
}

//////////////////////////////////////////////////////////////////////////
// List

int RunList(const char* title, int count, RowFn rowFn, IconFn iconFn, int selected,
            HeaderFn header, int listY)
{
	fabgl::Canvas& cv = Canvas;
	int visible = (HINT_Y - 4 - listY) / MENU_ROW_H;
	int y0 = listY;
	if (!header && count < visible)
		y0 += (visible - count) / 2 * MENU_ROW_H;
	if (selected < 0 || selected >= count)
		selected = 0;
	int top = 0;
	ListMove(K_NONE, count, visible, selected, top);

	DrawFrame(cv, title, "Up/Dn   ENTER Select   ESC Back   F1 Exit");
	if (header)
		header(cv);

	int drawn = -1;                      // row to repaint with the selected one, -1 = all
	for (;;)
	{
		Row row;
		for (int r = 0; r < visible && top + r < count; r++)
		{
			int idx = top + r;
			if (drawn >= 0 && idx != drawn && idx != selected)
				continue;
			bool hl = (idx == selected);
			int y = y0 + r * MENU_ROW_H;
			row.label[0] = row.value[0] = 0;
			row.dim = false;
			rowFn(idx, row);
			DrawRowAt(cv, LIST_X, y, LIST_W, 56, false, row, hl, T_BG);
			DrawSmallIcon(cv, iconFn(idx), LIST_X + 12, y + 2, T_TEXT, hl ? T_ACCENT : T_BG);
			if (hl)
				TraceRow("menu", title, row);
		}
		if (drawn < 0)
			DrawScrollbar(cv, listY, visible * MENU_ROW_H, top, visible, count);
		cv.waitCompletion();

		SupKey key = WaitKey();
		switch (key)
		{
			case K_ENTER: return count > 0 ? selected : -1;
			case K_ESC:   return -1;
			case K_F1:    closeAll = true; return -1;
			default:
				drawn = selected;
				if (ListMove(key, count, visible, selected, top))
					drawn = -1;
				break;
		}
	}
}

}
