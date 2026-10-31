/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : supervisor_test.cpp
 *  Module : Host-side test of the supervisor's menu logic in
 *           src/Supervisor/SupervisorLogic.h: tile grid and list
 *           movement, the serial key decoder and name fitting.
 *           Driven by tests/host/run-supervisor-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "SupervisorLogic.h"

static int failures = 0;

static void check(bool ok, const char* what)
{
	if (!ok)
	{
		printf("  FAIL %s\n", what);
		failures++;
	}
}

static void TestGrid()
{
	// 5 tiles in 3 columns, cell 5 is empty
	check(GridMove(0, 1, 0, 3, 5) == 1, "right");
	check(GridMove(2, 1, 0, 3, 5) == 2, "right edge stays");
	check(GridMove(0, -1, 0, 3, 5) == 0, "left edge stays");
	check(GridMove(1, 0, 1, 3, 5) == 4, "down");
	check(GridMove(2, 0, 1, 3, 5) == 2, "down into the empty cell stays");
	check(GridMove(4, 1, 0, 3, 5) == 4, "right into the empty cell stays");
	check(GridMove(4, 0, -1, 3, 5) == 1, "up");
}

static void TestList()
{
	int sel = 0, top = 0;
	check(!ListMove(K_DOWN, 0, 10, sel, top) && sel == 0 && top == 0, "empty list");
	check(!ListMove(K_PGDN, 1, 10, sel, top) && sel == 0, "one row, PgDn");
	sel = 9; top = 0;
	check(ListMove(K_DOWN, 30, 10, sel, top) && sel == 10 && top == 1, "scrolls down");
	check(ListMove(K_END, 30, 10, sel, top) && sel == 29 && top == 20, "End");
	check(ListMove(K_PGUP, 30, 10, sel, top) && sel == 19 && top == 19, "PgUp");
	check(ListMove(K_HOME, 30, 10, sel, top) && sel == 0 && top == 0, "Home");
	check(!ListMove(K_UP, 30, 10, sel, top) && sel == 0, "Up at the top");
}

static void TestSerialKeys()
{
	SerialKeyDecoder d;
	check(d.Feed('\r') == K_ENTER, "CR");
	check(d.Feed('\n') == K_NONE, "LF ignored");
	check(d.Feed('\t') == K_TAB, "Tab");
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Feed('A') == K_UP, "CSI A");
	check(d.Feed(0x1B) == K_NONE && d.Feed('O') == K_NONE && d.Feed('P') == K_F1, "SS3 P");
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Feed('5') == K_NONE
	      && d.Feed('~') == K_PGUP, "CSI 5~");
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Feed('1') == K_NONE
	      && d.Feed('1') == K_NONE && d.Feed('~') == K_F1, "CSI 11~");
	check(d.Feed(0x1B) == K_NONE && d.Idle(10) == K_NONE && d.Idle(30) == K_ESC, "lone ESC");
	check(d.Idle(100) == K_NONE, "ESC reported once");
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Feed('Z') == K_NONE
	      && d.Feed('\r') == K_ENTER, "unknown sequence dropped, decoder idle");
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Feed('9') == K_NONE
	      && d.Feed('9') == K_NONE && d.Feed('9') == K_NONE && d.Feed('9') == K_NONE
	      && d.Feed('\t') == K_TAB, "over-long sequence dropped");
	check(d.Feed('x') == K_NONE && d.Feed(0xFF) == K_NONE, "noise ignored");
	// a sequence cut short must not swallow the next key
	check(d.Feed(0x1B) == K_NONE && d.Feed('[') == K_NONE && d.Idle(10) == K_NONE
	      && d.Idle(30) == K_NONE && d.Feed('\r') == K_ENTER, "half CSI times out");
	check(d.Feed(0x1B) == K_NONE && d.Feed('O') == K_NONE && d.Idle(30) == K_NONE
	      && d.Feed('P') == K_NONE, "half SS3 times out: a late P is not F1");
}

static void TestNames()
{
	char a[32], b[32];
	FitName(a, 8, "KARATEKA");
	check(!strcmp(a, "KARATEKA"), "fits");
	FitName(a, 8, "KARATEKA2");
	check(!strcmp(a, "KARATEK~"), "cut");
	SplitName("/games/Karateka.nib", 10, a, b);
	check(!strcmp(a, "Karateka") && !b[0], "one line, no extension");
	SplitName("/Ultima IV Disk 1.dsk", 10, a, b);
	check(!strcmp(a, "Ultima IV ") && !strcmp(b, "Disk 1"), "two lines");
	SplitName("ABCDEFGHIJKLMNOPQRSTUVWXYZ.po", 10, a, b);
	check(!strcmp(a, "ABCDEFGHIJ") && !strcmp(b, "KLMNOPQRS~"), "two lines, cut");
	SplitName("noext", 10, a, b);
	check(!strcmp(a, "noext"), "no extension");
	SplitName("", 10, a, b);
	check(!a[0] && !b[0], "empty path");

	// one line: the canary after the buffer must survive a long name
	char t[12];
	memset(t, '#', sizeof(t));
	BaseTitle("/games/Prince of Persia (1989) Side A.dsk", t, 8);
	check(!strcmp(t, "Prince ~"), "title cut to its room");
	check(t[9] == '#' && t[10] == '#' && t[11] == '#', "title stays inside its buffer");
	BaseTitle("/AppleII/dkk.nib", t, 8);
	check(!strcmp(t, "dkk"), "short title, no directory or extension");
	BaseTitle("", t, 8);
	check(!t[0], "empty title");
}

int main()
{
	printf("supervisor logic\n");
	TestGrid();
	TestList();
	TestSerialKeys();
	TestNames();
	printf(failures ? "%d FAILED\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
