/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : SupervisorLogic.h
 *  Module : The supervisor menu's logic that needs no hardware:
 *           the keys it understands, movement through the tile
 *           grid and the lists, decoding of terminal key
 *           sequences from the serial port, and fitting of file
 *           names. Header-only and free of Arduino and FabGL so
 *           tests/host/supervisor_test.cpp can run it on the Pi.
 * ============================================================
*/

#ifndef SUPERVISOR_LOGIC_H
#define SUPERVISOR_LOGIC_H

#include <stdint.h>
#include <string.h>

// the keys the menu acts on, whichever device they came from
enum SupKey
{
	K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESC, K_TAB,
	K_PGUP, K_PGDN, K_HOME, K_END, K_F1
};

// Tile index after an arrow. Unchanged when the move leaves the grid or
// lands on a cell past count, the empty cells at the end of the last row.
inline int GridMove(int sel, int dx, int dy, int cols, int count)
{
	int col = sel % cols + dx;
	int row = sel / cols + dy;
	if (col < 0 || col >= cols || row < 0)
		return sel;
	int next = row * cols + col;
	return next < count ? next : sel;
}

// Applies a key to a list cursor: keeps sel in 0..count-1 and top such that
// sel is visible. Returns true when top changed, i.e. the list has to be
// repainted rather than just the two rows the cursor left and reached.
inline bool ListMove(SupKey key, int count, int visible, int& sel, int& top)
{
	int oldTop = top;
	if (count <= 0)
	{
		sel = top = 0;
		return false;
	}
	switch (key)
	{
		case K_UP:   sel--; break;
		case K_DOWN: sel++; break;
		case K_PGUP: sel -= visible; break;
		case K_PGDN: sel += visible; break;
		case K_HOME: sel = 0; break;
		case K_END:  sel = count - 1; break;
		default: break;
	}
	if (sel < 0) sel = 0;
	if (sel > count - 1) sel = count - 1;
	if (sel < top) top = sel;
	if (sel >= top + visible) top = sel - visible + 1;
	return top != oldTop;
}

// Terminal key sequences, a byte at a time: arrows, Home/End, PgUp/PgDn and
// F1 as a VT terminal sends them, plus CR and Tab. ESC starts every sequence,
// so a lone ESC is only known to be the key once nothing has followed it.
class SerialKeyDecoder
{
public:
	SerialKeyDecoder() : state(IDLE), arg(0), digits(0) {}

	// K_NONE while a sequence is incomplete, and for anything not understood
	SupKey Feed(uint8_t c)
	{
		switch (state)
		{
			case IDLE:
				if (c == 0x1B) { state = ESC; return K_NONE; }
				if (c == '\r') return K_ENTER;
				if (c == '\t') return K_TAB;
				return K_NONE;
			case ESC:
				if (c == '[') { state = CSI; arg = digits = 0; return K_NONE; }
				if (c == 'O') { state = SS3; return K_NONE; }
				state = IDLE;
				return Feed(c);                  // ESC, then an ordinary key
			case SS3:
				state = IDLE;
				return c == 'P' ? K_F1 : Letter(c);
			case CSI:
				if (c >= '0' && c <= '9' && digits < 3)
				{
					arg = arg * 10 + (c - '0');
					digits++;
					return K_NONE;
				}
				state = IDLE;
				if (c == '~')
				{
					switch (arg)
					{
						case 1: case 7: return K_HOME;
						case 4: case 8: return K_END;
						case 5:  return K_PGUP;
						case 6:  return K_PGDN;
						case 11: return K_F1;
						default: return K_NONE;
					}
				}
				if (digits == 0)
					return Letter(c);
				// a sequence this menu has no use for: drop it
				return (c >= '0' && c <= '9') ? K_NONE : Feed(c);
		}
		return K_NONE;
	}

	// Call when no byte is waiting. Once the last byte is 30 ms old a lone
	// ESC is the key, reported once, and a sequence cut short is dropped
	// rather than left to swallow whatever comes next.
	SupKey Idle(uint32_t msSinceByte)
	{
		if (state == IDLE || msSinceByte < 30)
			return K_NONE;
		bool esc = (state == ESC);
		state = IDLE;
		return esc ? K_ESC : K_NONE;
	}

private:
	enum { IDLE, ESC, CSI, SS3 } state;
	int arg, digits;

	static SupKey Letter(uint8_t c)
	{
		switch (c)
		{
			case 'A': return K_UP;
			case 'B': return K_DOWN;
			case 'C': return K_RIGHT;
			case 'D': return K_LEFT;
			case 'H': return K_HOME;
			case 'F': return K_END;
			default:  return K_NONE;
		}
	}
};

// Copy of text cut to room characters, ending in '~' when it did not fit.
// out holds room + 1 bytes.
inline void FitName(char* out, int room, const char* text)
{
	int len = (int)strlen(text);
	if (len > room)
	{
		memcpy(out, text, room - 1);
		out[room - 1] = '~';
		out[room] = '\0';
	}
	else
		memcpy(out, text, len + 1);
}

// A path's file name, without directory or extension, cut to room characters
// with '~' last when it did not fit. out holds room + 1 bytes.
inline void BaseTitle(const char* path, char* out, int room)
{
	const char* base = strrchr(path, '/');
	base = base ? base + 1 : path;
	const char* dot = strrchr(base, '.');
	int len = (dot && dot != base) ? (int)(dot - base) : (int)strlen(base);
	int n = len < room ? len : room;
	memcpy(out, base, n);
	out[n] = '\0';
	if (len > room && room > 0)
		out[room - 1] = '~';
}

// A path's file name, without directory or extension, on two lines of width
// characters; the second ends in '~' when the name is longer than both.
// Each line holds width + 1 bytes.
inline void SplitName(const char* path, int width, char* line1, char* line2)
{
	const char* base = strrchr(path, '/');
	base = base ? base + 1 : path;
	const char* dot = strrchr(base, '.');
	int len = (dot && dot != base) ? (int)(dot - base) : (int)strlen(base);

	int n1 = len < width ? len : width;
	memcpy(line1, base, n1);
	line1[n1] = '\0';

	int rest = len - n1;
	int n2 = rest < width ? rest : width;
	memcpy(line2, base + n1, n2);
	line2[n2] = '\0';
	if (rest > width)
		line2[width - 1] = '~';
}

#endif
