/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : KeyboardLayouts.h
 *  Module : PS/2 keyboard layouts beyond the US one FabGL defaults
 *           to, and the pickable list the supervisor shows. Each
 *           layout inherits FabGL's USLayout and lists only the keys
 *           that sit somewhere else, the way FabGL's own national
 *           layouts do.
 *
 *           The Apple II character set has no accented letters, so a
 *           layout cannot make them print: what it fixes is where the
 *           punctuation Apple software needs actually lives on the
 *           keyboard. Keys that would only make a character the Apple
 *           lacks produce nothing, which Apple2Device already handles
 *           by keeping ASCII 0x01-0x7F and dropping the rest.
 * ============================================================
*/

#pragma once

#include <stdint.h>
#include <fabgl.h>

// Saved in NVS, so the numbers are part of the settings format: append
// new layouts, never renumber the existing ones.
enum KeyboardLayoutId
{
	KEYBOARD_LAYOUT_US    = 0,
	KEYBOARD_LAYOUT_LATAM = 1,
	KEYBOARD_LAYOUT_ABNT2 = 2,
	KEYBOARD_LAYOUT_COUNT
};

struct KeyboardLayoutProfile
{
	uint8_t                       id;
	const char*                   name;     // as the supervisor lists it
	const fabgl::KeyboardLayout*  layout;
};

// Spanish (Latin American): the ISO keyboard sold across Latin America,
// with N-tilde right of L and the accent dead key right of P.
extern const fabgl::KeyboardLayout LatinAmericanLayout;

// Portuguese (Brazil), ABNT2: adds the C-cedilla key and the extra key
// left of the right Shift, which carries the slash and question mark.
extern const fabgl::KeyboardLayout BrazilianLayout;

// Never null: an id that is not known falls back to the US layout, so a
// settings value written by a later firmware still boots to a usable
// keyboard.
const KeyboardLayoutProfile* GetKeyboardLayoutProfile(uint8_t id);

// Which layout the keyboard is using now, for the supervisor to show and
// preselect. Kept here rather than read back from FabGL, which only hands
// out the layout pointer.
uint8_t CurrentKeyboardLayoutId();
void    SetCurrentKeyboardLayoutId(uint8_t id);

#ifdef ARDUINO
// Firmware only: the host layout test compiles these tables without the
// rest of FabGL, where fabgl::Keyboard does not exist. Applies at once -
// FabGL translates each key as it arrives - so no restart is needed.
inline const KeyboardLayoutProfile* ApplyKeyboardLayout(uint8_t id, fabgl::Keyboard* keyboard)
{
	const KeyboardLayoutProfile* profile = GetKeyboardLayoutProfile(id);
	if (keyboard)
		keyboard->setLayout(profile->layout);
	SetCurrentKeyboardLayoutId(profile->id);
	return profile;
}
#endif
