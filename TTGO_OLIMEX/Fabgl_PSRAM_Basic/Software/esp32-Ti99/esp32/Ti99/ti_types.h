// Classic99 for ESP32 - shared types and constants
// Derived from Classic99 (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence in console/Tiemul.cpp. Not for distribution without the author's permission.

#pragma once
#include <stdint.h>

typedef uint8_t  Byte;
typedef uint16_t Word;
typedef uint32_t DWord;

#define CLOCK_MHZ        3000000
#define HZ60             60
#define DEFAULT_60HZ_CPF (CLOCK_MHZ / HZ60)
#define SCANLINES        262

#define TI_ROOT      "/SD/ti99"
#define TI_ROM_DIR   TI_ROOT "/rom"
#define TI_CART_DIR  TI_ROOT "/carts"
#define TI_DISK_DIR  TI_ROOT "/disks"

// Diagnostic output on the serial port, switched from Setup > Debug log (or the
// "debug on|off" serial command). Replies to serial commands are always printed.
extern bool debugLog;
#define DBG(...) do { if (debugLog) Serial.printf(__VA_ARGS__); } while (0)
