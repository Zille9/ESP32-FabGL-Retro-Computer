/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : wifi_mgr.h
 *  Module : WiFi state machine (STA station, network scan, credentials)
 * ============================================================
 *
 * Flow: the user picks a network in the supervisor's Config WiFi window and
 * types the password on the PS/2 keyboard, or loads both from cocowifi.cfg on
 * the SD card; credentials are saved to NVS, the device joins as a station
 * and the debug API listens on the home-LAN IP. The board never opens an
 * access point of its own.
 *
 * Credentials and the auto-connect flag live in NVS namespace "sv":
 *   wifi_ssid, wifi_pass, wifi_auto.
 */

#ifndef NET_WIFI_MGR_H
#define NET_WIFI_MGR_H

#include <Arduino.h>

typedef enum {
    WIFI_MGR_OFF,         // radio idle
    WIFI_MGR_CONNECTING,  // STA association in progress
    WIFI_MGR_STA_RUNNING, // STA connected, IP assigned
    WIFI_MGR_FAILED,      // STA connect failed / timed out
} WifiMgrState;

void          wifi_mgr_init(void);          // load creds, WiFi.setSleep(false)
WifiMgrState  wifi_mgr_state(void);
const char*   wifi_mgr_state_str(void);
void          wifi_mgr_tick(void);          // advance CONNECTING; call periodically

// Scanning (Config WiFi window). wifi_mgr_scan() blocks for a few seconds and
// powers the radio up if it was off; the results stay readable until
// wifi_mgr_scan_free(), which also powers the radio back down if the scan
// turned it on.
int           wifi_mgr_scan(void);          // returns network count
int           wifi_mgr_scan_count(void);
String        wifi_mgr_scan_ssid(int i);
int           wifi_mgr_scan_rssi(int i);
bool          wifi_mgr_scan_secure(int i);
void          wifi_mgr_scan_free(void);

// Station (STA) mode
void          wifi_mgr_connect(const char* ssid, const char* pass);  // saves creds, begins
void          wifi_mgr_connect_saved(void);  // auto-connect with stored creds
// Store credentials without connecting (auto-connect follows `autoconnect`).
void          wifi_mgr_save_creds(const char* ssid, const char* pass, bool autoconnect);
void          wifi_mgr_stop(void);           // disconnect, radio off -> OFF

// Status getters
String        wifi_mgr_ip(void);
String        wifi_mgr_ssid(void);

// Credentials / settings (NVS "sv")
bool          wifi_mgr_has_creds(void);
void          wifi_mgr_forget(void);
bool          wifi_mgr_autoconnect(void);
void          wifi_mgr_set_autoconnect(bool on);

#endif // NET_WIFI_MGR_H
