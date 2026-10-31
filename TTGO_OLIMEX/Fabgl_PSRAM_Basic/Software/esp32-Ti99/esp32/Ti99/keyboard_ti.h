// Classic99 for ESP32 - PS/2 keyboard to TI-99/4A key matrix
#pragma once
#include "fabgl.h"

// feed one FabGL virtual key event; returns false if the key is reserved for the emulator (F12)
bool tiKeyEvent(const fabgl::VirtualKeyItem &item);
void tiKeyReleaseAll();

// true: right Alt is AltGr for the keyboard layout; false: both Alt keys are FCTN
void tiKeySetAltGr(bool on);

// queue text to be typed into the TI ('|' = Enter, '~' = FCTN-9 BACK); tiTypeFrame runs once per frame
void tiType(const char *text);
void tiTypeFrame();
