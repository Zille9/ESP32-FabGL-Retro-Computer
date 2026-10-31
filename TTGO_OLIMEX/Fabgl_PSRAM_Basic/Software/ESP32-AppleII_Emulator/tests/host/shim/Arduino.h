/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Arduino.h (host shim)
 *  Module : Stand-in for the Arduino core so the emulation core's
 *           headers compile under g++ on the host. Provides only
 *           what those headers touch: Serial logging and delay().
 *           Never part of the firmware build.

 *           Serial also stands in for the UART the Super Serial Card
 *           talks to: what the firmware writes lands in HostSerialTx(),
 *           what a test puts in HostSerialRx() is read back, and
 *           millis() is whatever HostMillis() was set to.
 * ============================================================
*/

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <deque>
#include <string>

// What the code under test has written to the UART
inline std::string& HostSerialTx() { static std::string tx; return tx; }
// What it will read from the UART, oldest first
inline std::deque<int>& HostSerialRx() { static std::deque<int> rx; return rx; }
// The clock millis() returns; tests move it by hand
inline unsigned long& HostMillis() { static unsigned long ms = 0; return ms; }

struct HostSerial
{
	void print(const char* s) { fputs(s, stdout); }
	void println(const char* s = "") { puts(s); }
	void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)))
	{
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}

	size_t write(unsigned char b) { HostSerialTx().push_back((char)b); return 1; }
	int available() { return (int)HostSerialRx().size(); }
	int read()
	{
		if (HostSerialRx().empty())
			return -1;
		int c = HostSerialRx().front();
		HostSerialRx().pop_front();
		return c;
	}
	void begin(unsigned long) {}
	void setRxBufferSize(size_t) {}
};

static HostSerial Serial;

inline void delay(unsigned long) {}

inline unsigned long millis() { return HostMillis(); }

// ESP32 placement attribute (code in internal RAM); meaningless on the host
#define IRAM_ATTR
