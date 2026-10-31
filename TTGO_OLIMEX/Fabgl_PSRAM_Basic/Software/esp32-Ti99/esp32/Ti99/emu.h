// Classic99 for ESP32 - emulation task (replaces Classic99's emulti/do1 loop)
#pragma once
#include "ti_types.h"

void emuInit();                 // build CPU and reset the machine
void emuReset();                // cold reset (console power cycle)
bool emuStart();                // start the emulation task on core 1; false if out of memory
void emuPause(bool pause);      // pauses at the next frame boundary; returns once paused
bool emuIsPaused();
