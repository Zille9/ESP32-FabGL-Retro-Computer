/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : wifi_mgr.cpp
 *  Module : WiFi state machine (STA station, network scan, credentials)
 * ============================================================
 */

#include "wifi_mgr.h"

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <Preferences.h>

#include "../utils/debug.h"

#define WIFI_CONNECT_TIMEOUT_MS 20000

static WifiMgrState s_state    = WIFI_MGR_OFF;
static uint32_t     s_connect_start = 0;
static int          s_scan_count    = 0;
static String       s_ssid;            // current/last STA SSID

// --- NVS helpers (namespace "sv", shared with the supervisor) ---

static String nvs_get_str(const char* key) {
    Preferences p;
    p.begin("sv", true);
    String v = p.getString(key, "");
    p.end();
    return v;
}

static void nvs_put_str(const char* key, const String& val) {
    Preferences p;
    p.begin("sv", false);
    p.putString(key, val);
    p.end();
}

static void nvs_remove(const char* key) {
    Preferences p;
    p.begin("sv", false);
    p.remove(key);
    p.end();
}

// --- lifecycle ---

void wifi_mgr_init(void) {
    WiFi.setSleep(false);          // keep latency low for the debug server
    s_ssid = nvs_get_str("wifi_ssid");
    s_state = WIFI_MGR_OFF;
}

WifiMgrState wifi_mgr_state(void) { return s_state; }

const char* wifi_mgr_state_str(void) {
    switch (s_state) {
        case WIFI_MGR_OFF:         return "Off";
        case WIFI_MGR_CONNECTING:  return "Connecting";
        case WIFI_MGR_STA_RUNNING: return "Connected";
        case WIFI_MGR_FAILED:      return "Failed";
    }
    return "?";
}

void wifi_mgr_tick(void) {
    // One-shot RAM report 10 s after connecting: readable over serial even when
    // the debug server (and so /api/status) is switched off.
    static uint32_t s_connected_at = 0;
    static bool     s_reported = false;
    if (s_state == WIFI_MGR_STA_RUNNING && !s_reported && s_connected_at &&
        millis() - s_connected_at > 10000) {
        s_reported = true;
        DEBUG_PRINTF("wifi_mgr: RAM 10 s after connect: int8_free %u, int8_min %u, int8_largest %u",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (s_state != WIFI_MGR_CONNECTING) return;
    if (WiFi.status() == WL_CONNECTED) {
        s_state = WIFI_MGR_STA_RUNNING;
        s_connected_at = millis() | 1;
        DEBUG_PRINTF("wifi_mgr: STA connected, IP %s", WiFi.localIP().toString().c_str());
    } else if (millis() - s_connect_start > WIFI_CONNECT_TIMEOUT_MS) {
        s_state = WIFI_MGR_FAILED;
        DEBUG_PRINT("wifi_mgr: STA connect timed out");
    }
}

// --- scanning ---

// A scan needs the radio in station mode. Remember whether the scan had to
// power it up, so wifi_mgr_scan_free() can power it back down.
static bool s_scan_powered_radio = false;

int wifi_mgr_scan(void) {
    // A pending or failed connection keeps retrying and makes the scan fail;
    // the user is about to pick a network anyway, so drop it.
    if (s_state == WIFI_MGR_CONNECTING || s_state == WIFI_MGR_FAILED) {
        WiFi.disconnect(false);
        s_state = WIFI_MGR_OFF;
        s_scan_powered_radio = true;
    }
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        delay(50);
        s_scan_powered_radio = true;
    }
    s_scan_count = WiFi.scanNetworks();
    if (s_scan_count < 0) s_scan_count = 0;
    DEBUG_PRINTF("wifi_mgr: scan found %d networks", s_scan_count);
    return s_scan_count;
}

int    wifi_mgr_scan_count(void)      { return s_scan_count; }
String wifi_mgr_scan_ssid(int i)      { return WiFi.SSID(i); }
int    wifi_mgr_scan_rssi(int i)      { return WiFi.RSSI(i); }
bool   wifi_mgr_scan_secure(int i)    { return WiFi.encryptionType(i) != WIFI_AUTH_OPEN; }

void wifi_mgr_scan_free(void) {
    WiFi.scanDelete();
    s_scan_count = 0;
    if (s_scan_powered_radio && (s_state == WIFI_MGR_OFF || s_state == WIFI_MGR_FAILED)) {
        WiFi.mode(WIFI_OFF);
    }
    s_scan_powered_radio = false;
}

// --- station ---

static void begin_sta(const String& ssid, const String& pass) {
    // Reconnect cleanly from ANY prior state, including OFF (after
    // Stop/Disconnect, where the radio was powered down). Force STA mode and
    // let the mode change settle before begin() — coming straight out of
    // WIFI_OFF, an immediate begin() can be dropped.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    delay(50);
    WiFi.begin(ssid.c_str(), pass.c_str());
    s_ssid = ssid;
    s_connect_start = millis();
    s_state = WIFI_MGR_CONNECTING;
    s_scan_powered_radio = false;   // the radio now belongs to the connection
    DEBUG_PRINTF("wifi_mgr: connecting to '%s'", ssid.c_str());
}

void wifi_mgr_connect(const char* ssid, const char* pass) {
    nvs_put_str("wifi_ssid", ssid);
    nvs_put_str("wifi_pass", pass);
    wifi_mgr_set_autoconnect(true);
    begin_sta(String(ssid), String(pass));
}

void wifi_mgr_save_creds(const char* ssid, const char* pass, bool autoconnect) {
    nvs_put_str("wifi_ssid", ssid);
    nvs_put_str("wifi_pass", pass);
    wifi_mgr_set_autoconnect(autoconnect);
    s_ssid = ssid;
    DEBUG_PRINTF("wifi_mgr: credentials saved for '%s' (auto-connect %s)",
                 ssid, autoconnect ? "on" : "off");
}

void wifi_mgr_connect_saved(void) {
    String ssid = nvs_get_str("wifi_ssid");
    String pass = nvs_get_str("wifi_pass");
    if (ssid.length() == 0) {
        DEBUG_PRINT("wifi_mgr: no saved credentials");
        s_state = WIFI_MGR_OFF;
        return;
    }
    begin_sta(ssid, pass);
}

void wifi_mgr_stop(void) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    s_state = WIFI_MGR_OFF;
    DEBUG_PRINT("wifi_mgr: stopped");
}

// --- status ---

String wifi_mgr_ip(void) {
    if (s_state == WIFI_MGR_STA_RUNNING) return WiFi.localIP().toString();
    return String("0.0.0.0");
}

String wifi_mgr_ssid(void) { return s_ssid; }

// --- credentials / settings ---

bool wifi_mgr_has_creds(void) {
    return nvs_get_str("wifi_ssid").length() > 0;
}

void wifi_mgr_forget(void) {
    nvs_remove("wifi_ssid");
    nvs_remove("wifi_pass");
    wifi_mgr_set_autoconnect(false);
    s_ssid = "";
    DEBUG_PRINT("wifi_mgr: credentials forgotten");
}

bool wifi_mgr_autoconnect(void) {
    Preferences p;
    p.begin("sv", true);
    bool v = p.getBool("wifi_auto", false);
    p.end();
    return v;
}

void wifi_mgr_set_autoconnect(bool on) {
    Preferences p;
    p.begin("sv", false);
    p.putBool("wifi_auto", on);
    p.end();
}
