/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 * ============================================================
 *  File   : sv_wifi.h
 *  Module : Supervisor "WiFi / Debug" status + control screen
 * ============================================================
 *
 * Status/control for WiFi and the debug server. Shows STA state, SSID, IP and
 * the debug-server on/off state. Actions: Config WiFi (scan, pick a network,
 * type the password on the keyboard), Read WiFi from SD Card (cocowifi.cfg),
 * Connect (saved), Stop/Disconnect, Forget Credentials, Debug Server On/Off.
 */
#ifndef SV_WIFI_H
#define SV_WIFI_H

#include <stdint.h>

typedef struct Supervisor_t Supervisor_t;

// Open the screen (sets state, resets cursor).
// Force the next render to repaint the whole WiFi / Debug screen.
void sv_wifi_invalidate(void);
void sv_wifi_open(Supervisor_t* sv);

// HID key handler (Up/Down move, ENTER execute, ESC back to Settings).
void sv_wifi_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);

// Per-frame: refresh when the WiFi state or IP changes (so CONNECTING ->
// CONNECTED is reflected without a keypress).
void sv_wifi_tick(Supervisor_t* sv);

// Render the screen.
void sv_wifi_render(Supervisor_t* sv);

// Text-entry path for the network-name and password popups. The normal
// supervisor key route translates VirtualKeys to HID usages and drops case
// and symbols, so hal_keyboard's process_vk() calls this instead while one of
// those popups is up, passing FabGL's layout-resolved ASCII.
bool sv_wifi_wants_text(void);
void sv_wifi_on_text_key(int16_t vk, uint8_t ascii, bool pressed);

#endif // SV_WIFI_H
