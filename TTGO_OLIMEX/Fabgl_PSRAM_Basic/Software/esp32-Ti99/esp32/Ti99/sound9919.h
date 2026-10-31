// Classic99 for ESP32 - TMS9919 / SN76494 sound chip
// Derived from Classic99 console/sound.cpp and Tiemul.cpp wsndbyte
// (C) 2009-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.

#pragma once
#include "ti_types.h"

void soundInit();               // creates the FabGL SoundGenerator and starts playback
void soundReset();              // all channels silent
void soundWrite(Byte c);        // CPU write to >8400
void soundMute(bool mute);
