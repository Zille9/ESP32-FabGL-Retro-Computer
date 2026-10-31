/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : layout_test.cpp
 *  Module : Host-side keyboard layout test. Runs the layout tables
 *           from src/Tools/KeyboardLayouts.cpp through the same
 *           lookup rules FabGL's Keyboard applies, then through
 *           FabGL's own virtualKeyToASCII, so a table typo shows up
 *           on the Pi instead of on the keyboard.
 *           Driven by tests/host/run-layout-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "kbdlayouts.h"
#include "codepages.h"
#include "KeyboardLayouts.h"

using namespace fabgl;

static int failures = 0;

static void fail(const std::string& what)
{
	printf("  FAIL %s\n", what.c_str());
	failures++;
}

//////////////////////////////////////////////////////////////////////////////// FabGL's lookup rules

// Sizes of the fixed arrays in KeyboardLayout. FabGL walks these until it
// meets an empty entry, but its own US table fills scancodeToVK exactly, so
// that loop runs past the end; every scan here is bounded by the array.
static const int SCANCODE_MAX = sizeof(((KeyboardLayout*)0)->scancodeToVK) / sizeof(VirtualKeyDef);
static const int EXSCANCODE_MAX = sizeof(((KeyboardLayout*)0)->exScancodeToVK) / sizeof(VirtualKeyDef);
static const int ALTERNATE_MAX = sizeof(((KeyboardLayout*)0)->alternateVK) / sizeof(AltVirtualKeyDef);
static const int DEADKEY_MAX = sizeof(((KeyboardLayout*)0)->deadKeysVK) / sizeof(VirtualKey);

// Keyboard::scancodeToVK: the layout's own table first, then the layout it
// inherits from. A zero scancode ends the table.
static VirtualKey ScancodeToVK(uint8_t scancode, KeyboardLayout const* layout)
{
	for (int i = 0; i < SCANCODE_MAX && layout->scancodeToVK[i].scancode; i++)
		if (layout->scancodeToVK[i].scancode == scancode)
			return layout->scancodeToVK[i].virtualKey;
	return layout->inherited ? ScancodeToVK(scancode, layout->inherited) : VK_NONE;
}

struct Mods
{
	bool ctrl = false, lalt = false, ralt = false, shift = false;
};

// Keyboard::VKtoAlternateVK, for the key-down case: every modifier has to
// match exactly, the layout's own table wins over the inherited one, and a
// key with no entry keeps the virtual key it already had.
static VirtualKey AlternateVK(VirtualKey in, Mods m, KeyboardLayout const* layout)
{
	for (int i = 0; i < ALTERNATE_MAX && layout->alternateVK[i].reqVirtualKey != VK_NONE; i++)
	{
		AltVirtualKeyDef const& def = layout->alternateVK[i];
		if (def.reqVirtualKey == in && def.ctrl == m.ctrl && def.lalt == m.lalt &&
		    def.ralt == m.ralt && def.shift == m.shift)
			return def.virtualKey;
	}
	return layout->inherited ? AlternateVK(in, m, layout->inherited) : in;
}

static bool IsDeadKey(VirtualKey vk, KeyboardLayout const* layout)
{
	for (int i = 0; i < DEADKEY_MAX && layout->deadKeysVK[i] != VK_NONE; i++)
		if (layout->deadKeysVK[i] == vk)
			return true;
	return false;
}

// What the emulator would receive for this key: the ASCII of the resulting
// virtual key, or -1 when the key makes no character. Apple2Device keeps
// 0x01-0x7F and drops everything else, so that is what we measure.
static int AsciiFor(KeyboardLayout const* layout, uint8_t scancode, Mods m)
{
	VirtualKey vk = ScancodeToVK(scancode, layout);
	if (vk == VK_NONE)
		return -1;
	vk = AlternateVK(vk, m, layout);
	if (IsDeadKey(vk, layout))
		return -1;                       // a dead key composes, it does not type

	VirtualKeyItem item;
	memset(&item, 0, sizeof(item));
	item.vk    = vk;
	item.down  = true;
	item.CTRL  = m.ctrl;
	item.LALT  = m.lalt;
	item.RALT  = m.ralt;
	item.SHIFT = m.shift;
	int ascii = virtualKeyToASCII(item, &CodePage437);
	return (ascii > 0 && ascii < 0x80) ? ascii : -1;
}

//////////////////////////////////////////////////////////////////////////////// checks

// No scancode may appear twice: the first entry would mask the second.
static void CheckNoDuplicateScancodes(const char* name, KeyboardLayout const* layout)
{
	char msg[120];
	std::set<uint8_t> seen;
	for (int i = 0; i < SCANCODE_MAX && layout->scancodeToVK[i].scancode; i++)
		if (!seen.insert(layout->scancodeToVK[i].scancode).second)
		{
			snprintf(msg, sizeof(msg), "%s: scancode 0x%02X listed twice",
			         name, layout->scancodeToVK[i].scancode);
			fail(msg);
		}

	std::set<uint8_t> seenEx;
	for (int i = 0; i < EXSCANCODE_MAX && layout->exScancodeToVK[i].scancode; i++)
		if (!seenEx.insert(layout->exScancodeToVK[i].scancode).second)
		{
			snprintf(msg, sizeof(msg), "%s: extended scancode 0x%02X listed twice",
			         name, layout->exScancodeToVK[i].scancode);
			fail(msg);
		}
}

// An alternateVK entry that repeats a key plus modifier combination is dead
// code, and usually means one of the two was meant to be something else.
static void CheckNoDuplicateAlternates(const char* name, KeyboardLayout const* layout)
{
	std::set<std::string> seen;
	for (int i = 0; i < ALTERNATE_MAX && layout->alternateVK[i].reqVirtualKey != VK_NONE; i++)
	{
		AltVirtualKeyDef const& def = layout->alternateVK[i];
		std::string key = std::to_string((int)def.reqVirtualKey) + ":" +
		                  std::to_string(def.ctrl) + std::to_string(def.lalt) +
		                  std::to_string(def.ralt) + std::to_string(def.shift);
		if (!seen.insert(key).second)
			fail(std::string(name) + ": duplicate alternate for virtual key " +
			     std::to_string((int)def.reqVirtualKey));
	}
}

// Every character the Apple II can accept has to be typeable somehow, with
// no modifier, Shift, AltGr, or Shift+AltGr. This is what catches a wrong
// scancode: the key still works, but some character is no longer reachable.
static void CheckEveryAsciiReachable(const char* name, KeyboardLayout const* layout)
{
	static const Mods COMBOS[] = {
		{ false, false, false, false },
		{ false, false, false, true  },   // Shift
		{ false, false, true,  false },   // AltGr
		{ false, false, true,  true  },   // Shift + AltGr
	};

	std::set<int> reachable;
	for (int scancode = 1; scancode < 0x100; scancode++)
		for (const Mods& m : COMBOS)
		{
			int ascii = AsciiFor(layout, (uint8_t)scancode, m);
			if (ascii > 0)
				reachable.insert(ascii);
		}

	for (int c = 0x20; c <= 0x7E; c++)
		if (!reachable.count(c))
			fail(std::string(name) + ": cannot type '" + std::string(1, (char)c) + "'");
}

// Spot checks: the characters Apple software needs most, at the position
// they occupy on the real keyboard. Derived from the X11 xkb tables.
struct KeyCheck
{
	uint8_t     scancode;
	Mods        mods;
	int         expected;
	const char* what;
};

static void CheckKeys(const char* name, KeyboardLayout const* layout,
                      const KeyCheck* checks, int count)
{
	for (int i = 0; i < count; i++)
	{
		int got = AsciiFor(layout, checks[i].scancode, checks[i].mods);
		if (got != checks[i].expected)
		{
			char msg[160];
			snprintf(msg, sizeof(msg), "%s: %s (scancode 0x%02X) gave %s, expected '%c'",
			         name, checks[i].what, checks[i].scancode,
			         got > 0 ? std::string(1, (char)got).c_str() : "nothing",
			         checks[i].expected);
			fail(msg);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////// main

int main()
{
	const Mods PLAIN, SHIFT { false, false, false, true }, ALTGR { false, false, true, false };

	// The US layout is FabGL's, not ours; running the same checks over it
	// proves the harness agrees with a layout known to be correct.
	printf("US (FabGL, reference)\n");
	CheckNoDuplicateScancodes("US", &USLayout);
	CheckEveryAsciiReachable("US", &USLayout);

	printf("Latin American Spanish\n");
	CheckNoDuplicateScancodes("LatAm", &LatinAmericanLayout);
	CheckNoDuplicateAlternates("LatAm", &LatinAmericanLayout);
	CheckEveryAsciiReachable("LatAm", &LatinAmericanLayout);
	static const KeyCheck LATAM[] = {
		{ 0x3D, SHIFT, '/',  "Shift+7" },
		{ 0x3E, SHIFT, '(',  "Shift+8" },
		{ 0x46, SHIFT, ')',  "Shift+9" },
		{ 0x45, SHIFT, '=',  "Shift+0" },
		{ 0x1E, SHIFT, '"',  "Shift+2" },
		{ 0x1E, ALTGR, '@',  "AltGr+2" },
		{ 0x26, SHIFT, '#',  "Shift+3" },
		{ 0x36, SHIFT, '&',  "Shift+6" },
		{ 0x4E, PLAIN, '\'', "the apostrophe key" },
		{ 0x4E, SHIFT, '?',  "Shift+apostrophe" },
		{ 0x4E, ALTGR, '\\', "AltGr+apostrophe" },
		{ 0x0E, PLAIN, '|',  "the key left of 1" },
		{ 0x4C, ALTGR, '~',  "AltGr+N-tilde" },
		{ 0x52, PLAIN, '{',  "the brace key" },
		{ 0x52, SHIFT, '[',  "Shift+brace" },
		{ 0x52, ALTGR, '^',  "AltGr+brace" },
		{ 0x5D, PLAIN, '}',  "the key left of Enter" },
		{ 0x5D, SHIFT, ']',  "Shift+that key" },
		{ 0x5D, ALTGR, '`',  "AltGr+that key" },
		{ 0x5B, PLAIN, '+',  "the plus key" },
		{ 0x5B, SHIFT, '*',  "Shift+plus" },
		{ 0x41, SHIFT, ';',  "Shift+comma" },
		{ 0x49, SHIFT, ':',  "Shift+period" },
		{ 0x4A, PLAIN, '-',  "the minus key" },
		{ 0x4A, SHIFT, '_',  "Shift+minus" },
		{ 0x61, PLAIN, '<',  "the key right of left Shift" },
		{ 0x61, ALTGR, '\\', "AltGr+that key" },
	};
	CheckKeys("LatAm", &LatinAmericanLayout, LATAM, sizeof(LATAM) / sizeof(LATAM[0]));

	printf("Brazilian ABNT2\n");
	CheckNoDuplicateScancodes("ABNT2", &BrazilianLayout);
	CheckNoDuplicateAlternates("ABNT2", &BrazilianLayout);
	CheckEveryAsciiReachable("ABNT2", &BrazilianLayout);
	static const KeyCheck ABNT2[] = {
		{ 0x51, PLAIN, '/',  "the key left of right Shift" },
		{ 0x51, SHIFT, '?',  "Shift+that key" },
		{ 0x4A, PLAIN, ';',  "the semicolon key" },
		{ 0x4A, SHIFT, ':',  "Shift+semicolon" },
		{ 0x0E, PLAIN, '\'', "the key left of 1" },
		{ 0x0E, SHIFT, '"',  "Shift+that key" },
		{ 0x1E, SHIFT, '@',  "Shift+2" },
		{ 0x5B, PLAIN, '[',  "the bracket key" },
		{ 0x5B, SHIFT, '{',  "Shift+bracket" },
		{ 0x5D, PLAIN, ']',  "the key left of Enter" },
		{ 0x5D, SHIFT, '}',  "Shift+that key" },
		{ 0x52, PLAIN, '~',  "the tilde key" },
		{ 0x52, SHIFT, '^',  "Shift+tilde" },
		{ 0x61, PLAIN, '\\', "the key right of left Shift" },
		{ 0x61, SHIFT, '|',  "Shift+that key" },
		{ 0x55, PLAIN, '=',  "the equals key" },
		{ 0x55, SHIFT, '+',  "Shift+equals" },
		{ 0x54, SHIFT, '`',  "Shift+acute" },
	};
	CheckKeys("ABNT2", &BrazilianLayout, ABNT2, sizeof(ABNT2) / sizeof(ABNT2[0]));

	// The pickable list has to line up with the layouts themselves.
	printf("Layout profile table\n");
	for (int i = 0; i < KEYBOARD_LAYOUT_COUNT; i++)
	{
		const KeyboardLayoutProfile* p = GetKeyboardLayoutProfile((uint8_t)i);
		if (!p || !p->layout || !p->name)
			fail("profile " + std::to_string(i) + " is incomplete");
		else if (strlen(p->name) > 20)
			fail(std::string("profile name too wide for the menu: ") + p->name);
	}
	if (GetKeyboardLayoutProfile(KEYBOARD_LAYOUT_COUNT) != GetKeyboardLayoutProfile(0))
		fail("an unknown layout id should fall back to the first profile");

	if (failures == 0)
		printf("PASS keyboard layouts\n");
	else
		printf("FAIL keyboard layouts: %d problem(s)\n", failures);
	return failures ? 1 : 0;
}
