/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Log.h
 *  Module : Serial debug logging macros (DEBUG_PRINT /
 *           DEBUG_PRINTLN), compiled out when debug output is
 *           disabled. Third-party file: originally by bitluni,
 *           licensed CC BY-SA 4.0 (see the notice below) - not
 *           MIT.
 * ============================================================
*/

/*
	Author: bitluni 2019
	License: 
	Creative Commons Attribution ShareAlike 4.0
	https://creativecommons.org/licenses/by-sa/4.0/
	
	For further details check out: 
		https://youtube.com/bitlunislab
		https://github.com/bitluni
		http://bitluni.net
*/
#pragma once
#include "Arduino.h"


// The Super Serial Card sends the Apple's data out of the same USB UART
// this log uses, so installing that card silences everything the firmware
// would print from then on. The boot messages are already out by then.
struct Log
{
	static bool& Muted() { static bool muted = false; return muted; }
	static bool On() { return !Muted(); }
	static void Mute() { Muted() = true; }
};

// Log lines printed while the machine is running go through these, so that
// muting reaches them. Boot-time messages may use Serial directly.
#define LOGF(...)   do { if (Log::On()) Serial.printf(__VA_ARGS__); } while (0)
#define LOGLN(a)    do { if (Log::On()) Serial.println(a); } while (0)

#define DEBUG_PRINTLN(a) LOGLN(a)
#define DEBUG_PRINT(a) do { if (Log::On()) Serial.print(a); } while (0)
#define DEBUG_PRINTLNF(a, f) do { if (Log::On()) Serial.println(a, f); } while (0)
#define DEBUG_PRINTF(a, f) do { if (Log::On()) Serial.print(a, f); } while (0)
/*
#define DEBUG_PRINTLN(a) ;
#define DEBUG_PRINT(a) ;
#define DEBUG_PRINTLNF(a, f) ;
#define DEBUG_PRINTF(a, f) ;
*/
#define ERROR(a) {Serial.println((a)); delay(3000); throw 0;};
