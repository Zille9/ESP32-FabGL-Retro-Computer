/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : debug_server.h
 *  Module : WiFi debug server (core-0 WebServer task)
 * ============================================================
 *
 * A single Arduino WebServer running in a core-0 task, serving the HTTP/JSON
 * debug API once the board has joined a network. Emulator state is reached
 * only through debug_rpc (cross-core), never touched directly.
 */

#ifndef NET_DEBUG_SERVER_H
#define NET_DEBUG_SERVER_H

#include <stdbool.h>

// Firmware + API version reported by /api/status.
#define DEBUG_API_VERSION 1

// Create the core-0 server task unless the persisted setting is Off. Routes are
// registered immediately; the WebServer starts once WiFi has an IP. Call once.
void debug_server_begin(void);

// The "Debug Server On/Off" toggle. Persisted in NVS. On starts the task if
// needed; Off stops serving the debug API at once and skips the task (and its
// internal RAM) from the next boot.
void debug_server_set_enabled(bool on);
bool debug_server_enabled(void);

#endif // NET_DEBUG_SERVER_H
