/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 * ============================================================
 *  File   : sv_wifi.cpp
 *  Module : Supervisor "WiFi / Debug" status + control screen
 * ============================================================
 *
 * Views, all inside the SV_WIFI state:
 *   main      status panel (State / SSID / IP) and the action rows;
 *   scanning  "Config WiFi" window while the blocking network scan runs;
 *   networks  the scanned networks plus "Other network...";
 *   SSID / password entry  popups typed on the PS/2 keyboard. These take
 *             keys through sv_wifi_on_text_key(), because the supervisor's
 *             HID route cannot carry lower case or symbols.
 *
 * "Read WiFi from SD Card" loads /cocowifi.cfg:
 *     [WiFi]
 *     enabled=1
 *     SSID=MyNetwork
 *     passphrase=secret
 */
#include "sv_wifi.h"
#include "supervisor.h"
#include "sv_menu.h"
#include "sv_render.h"
#include "../net/wifi_mgr.h"
#include "../net/debug_server.h"
#include "fabgl.h"
#include <SD.h>
#include <esp_heap_caps.h>

extern OSDCanvas* hal_video_get_canvas(void);

#define HID_UP    0x52
#define HID_DOWN  0x51
#define HID_ENTER 0x28
#define HID_ESC   0x29
#define HID_F1    0x3A
#define HID_R     0x15

#define WIFI_CFG_FILE  "/cocowifi.cfg"
#define SSID_MAX       32
#define PASS_MAX       63
#define PASS_MIN       8        // WPA passphrase minimum
#define NETS_MAX       24

// Action rows (navigable). The status panel above is not selectable.
enum {
    WACT_CONFIG = 0,   // Config WiFi: scan, pick a network, type the password
    WACT_SDFILE,       // Read credentials from cocowifi.cfg on the SD card
    WACT_CONNECT,      // Connect using saved credentials (STA)
    WACT_STOP,         // Stop / disconnect
    WACT_FORGET,       // Forget saved credentials
    WACT_SERVER,       // Toggle debug server on/off
    WACT_COUNT
};

static const char* const WIFI_ACTIONS[WACT_COUNT] = {
    "Config WiFi",
    "Read WiFi from SD Card",
    "Connect (saved)",
    "Stop / Disconnect",
    "Forget Credentials",
    "Debug Server",
};

static const uint8_t WIFI_ICONS[WACT_COUNT] = {
    ROW_ICON_WIFI, ROW_ICON_SD, ROW_ICON_CHECK, ROW_ICON_STOP, ROW_ICON_CROSS, ROW_ICON_SPIDER,
};

// Main view layout in the wide green frame: a white status panel, then the rows.
#define WIFI_PANEL_Y   (SVW_BOX_Y + 31)
#define WIFI_PANEL_H   34
#define WIFI_LIST_Y    (WIFI_PANEL_Y + WIFI_PANEL_H + 3)

enum WifiView : uint8_t { V_MAIN, V_SCANNING, V_NETS, V_SSID, V_PASSWORD };
static WifiView s_view = V_MAIN;

// Redraw bookkeeping so tick() only repaints on a real change.
static WifiMgrState s_last_state = WIFI_MGR_OFF;
static String       s_last_ip;
// Row last drawn as selected; -1 = nothing on screen, draw the whole screen.
static int8_t s_drawn = -1;
// WiFi state changed: repaint the status panel and every row value.
static bool   s_info_dirty = false;

// Popups over the main view: forget confirmation (No / Yes, defaults to No)
// and a message with a single OK row.
enum WifiPopup : uint8_t { P_NONE, P_FORGET, P_MESSAGE };
static WifiPopup s_pop = P_NONE;
static int8_t s_pop_sel = 0;
static int8_t s_pop_drawn = -1;
static const char* s_msg_title = "";
static char   s_msg1[SVP_MSG_COLS + 1];
static char   s_msg2[SVP_MSG_COLS + 1];

// Scanned networks (PSRAM), strongest first, one entry per SSID.
struct WifiNet {
    char   ssid[SSID_MAX + 1];
    int8_t rssi;
    bool   secure;
};
static WifiNet* s_nets = nullptr;
static int8_t   s_net_count = 0;
static int8_t   s_net_sel = 0;
static int8_t   s_net_top = 0;
static int8_t   s_net_drawn = -1;       // -1 = repaint the whole list
static int8_t   s_net_drawn_top = 0;

// Text entry (network name, then password).
static char   s_ssid[SSID_MAX + 1];
static bool   s_ssid_secure = true;     // false: picked an open network
static bool   s_ssid_typed = false;     // SSID came from "Other network..."
static char   s_text[PASS_MAX + 1];
static bool   s_text_drawn = false;     // false = draw the whole popup
static bool   s_pass_short = false;     // show the "too short" message

void sv_wifi_invalidate(void) {
    s_drawn = -1;
    s_pop_drawn = -1;
    s_net_drawn = -1;
    s_text_drawn = false;
}

static void set_view(Supervisor_t* sv, WifiView view) {
    s_view = view;
    sv_wifi_invalidate();
    sv->needs_redraw = true;
}

void sv_wifi_open(Supervisor_t* sv) {
    sv->state = SV_WIFI;
    sv->menu_cursor = 0;
    s_last_state = wifi_mgr_state();
    s_last_ip    = wifi_mgr_ip();
    s_pop = P_NONE;
    set_view(sv, V_MAIN);
}

static void popup_open(Supervisor_t* sv, WifiPopup kind) {
    s_pop = kind;
    s_pop_sel = 0;
    sv_wifi_invalidate();   // popups differ in size: repaint underneath first
    sv->needs_redraw = true;
}

static void message_open(Supervisor_t* sv, const char* title) {
    s_msg_title = title;
    popup_open(sv, P_MESSAGE);
}

// ============================================================
// cocowifi.cfg
// ============================================================

// Load the [WiFi] section of /cocowifi.cfg: SSID and passphrase are saved to
// NVS; enabled=1 (the default) also connects, enabled=0 leaves WiFi off.
static void load_cfg_file(Supervisor_t* sv) {
    File f = SD.open(WIFI_CFG_FILE, FILE_READ);
    if (!f) {
        snprintf(s_msg1, sizeof(s_msg1), "cocowifi.cfg was not found.");
        snprintf(s_msg2, sizeof(s_msg2), "Put it in the root of the SD card.");
        message_open(sv, "File not found");
        return;
    }

    String ssid, pass;
    bool enabled = true;
    bool in_wifi = true;        // keys before any [section] count too
    for (int lines = 0; f.available() && lines < 64; lines++) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[') {
            in_wifi = line.equalsIgnoreCase("[wifi]");
            continue;
        }
        int eq = line.indexOf('=');
        if (!in_wifi || eq <= 0) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();
        if (key.equalsIgnoreCase("ssid"))            ssid = val;
        else if (key.equalsIgnoreCase("passphrase")) pass = val;
        else if (key.equalsIgnoreCase("enabled"))    enabled = (val.toInt() != 0);
    }
    f.close();

    if (ssid.length() == 0) {
        snprintf(s_msg1, sizeof(s_msg1), "cocowifi.cfg has no network name.");
        snprintf(s_msg2, sizeof(s_msg2), "Add an SSID= line under [WiFi].");
        message_open(sv, "No SSID in file");
        return;
    }
    if (ssid.length() > SSID_MAX || pass.length() > PASS_MAX ||
        (pass.length() > 0 && pass.length() < PASS_MIN)) {
        snprintf(s_msg1, sizeof(s_msg1), "SSID: up to 32 characters.");
        snprintf(s_msg2, sizeof(s_msg2), "passphrase: 8 to 63, or empty.");
        message_open(sv, "Invalid cocowifi.cfg");
        return;
    }

    snprintf(s_msg1, sizeof(s_msg1), "Network: %.30s", ssid.c_str());
    if (enabled) {
        wifi_mgr_connect(ssid.c_str(), pass.c_str());       // saves, then connects
        snprintf(s_msg2, sizeof(s_msg2), "Saved. Connecting...");
    } else {
        wifi_mgr_stop();
        wifi_mgr_save_creds(ssid.c_str(), pass.c_str(), false);
        snprintf(s_msg2, sizeof(s_msg2), "Saved. WiFi left off (enabled=0).");
    }
    s_info_dirty = true;
    message_open(sv, "WiFi loaded from SD card");
}

// ============================================================
// Config WiFi: scan, network list, text entry
// ============================================================

// Blocking scan (a few seconds), then keep one entry per SSID, strongest first.
static void scan_networks(void) {
    s_net_count = 0;
    s_net_sel = 0;
    s_net_top = 0;
    if (!s_nets) {
        s_nets = (WifiNet*)heap_caps_malloc(NETS_MAX * sizeof(WifiNet), MALLOC_CAP_SPIRAM);
        if (!s_nets) {
            DEBUG_PRINT("wifi: failed to allocate the network list");
            return;
        }
    }

    int n = wifi_mgr_scan();    // the WiFi library returns them strongest first
    for (int i = 0; i < n && s_net_count < NETS_MAX; i++) {
        String ssid = wifi_mgr_scan_ssid(i);
        if (ssid.length() == 0 || ssid.length() > SSID_MAX) continue;   // hidden
        bool seen = false;
        for (int k = 0; k < s_net_count; k++) {
            if (ssid == s_nets[k].ssid) { seen = true; break; }
        }
        if (seen) continue;
        WifiNet* net = &s_nets[s_net_count++];
        snprintf(net->ssid, sizeof(net->ssid), "%s", ssid.c_str());
        int rssi = wifi_mgr_scan_rssi(i);
        net->rssi = (int8_t)(rssi < -127 ? -127 : rssi);
        net->secure = wifi_mgr_scan_secure(i);
    }
    wifi_mgr_scan_free();
}

// List rows: the networks, then "Other network...".
static inline int net_rows(void) { return s_net_count + 1; }

static void text_begin(Supervisor_t* sv, WifiView view) {
    s_text[0] = '\0';
    s_pass_short = false;
    set_view(sv, view);
}

static void net_select(Supervisor_t* sv) {
    if (s_net_sel >= s_net_count) {             // Other network...
        s_ssid_typed = true;
        s_ssid_secure = true;
        text_begin(sv, V_SSID);
        return;
    }
    const WifiNet* net = &s_nets[s_net_sel];
    snprintf(s_ssid, sizeof(s_ssid), "%s", net->ssid);
    s_ssid_typed = false;
    s_ssid_secure = net->secure;
    if (!net->secure) {
        wifi_mgr_connect(s_ssid, "");           // open network: no password
        s_info_dirty = true;
        set_view(sv, V_MAIN);
        return;
    }
    text_begin(sv, V_PASSWORD);
}

static void nets_on_key(Supervisor_t* sv, uint8_t hid_usage) {
    switch (hid_usage) {
        case HID_UP:
            if (s_net_sel > 0) { s_net_sel--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (s_net_sel < net_rows() - 1) { s_net_sel++; sv->needs_redraw = true; }
            break;
        case HID_ENTER:
            net_select(sv);
            break;
        case HID_R:
            set_view(sv, V_SCANNING);
            break;
        case HID_ESC:
            set_view(sv, V_MAIN);
            break;
    }
    if (s_net_sel < s_net_top) s_net_top = s_net_sel;
    if (s_net_sel >= s_net_top + SVW_LIST_ROWS) s_net_top = s_net_sel - SVW_LIST_ROWS + 1;
}

bool sv_wifi_wants_text(void) {
    return supervisor_get()->state == SV_WIFI && (s_view == V_SSID || s_view == V_PASSWORD);
}

void sv_wifi_on_text_key(int16_t vk, uint8_t ascii, bool pressed) {
    Supervisor_t* sv = supervisor_get();
    if (!pressed) return;
    size_t max = (s_view == V_SSID) ? SSID_MAX : PASS_MAX;
    size_t len = strlen(s_text);

    switch ((fabgl::VirtualKey)vk) {
        case fabgl::VK_ESCAPE:
            // back one step: password -> typed name, otherwise the list
            if (s_view == V_PASSWORD && s_ssid_typed) {
                snprintf(s_text, sizeof(s_text), "%s", s_ssid);
                s_pass_short = false;
                set_view(sv, V_SSID);
            } else {
                set_view(sv, V_NETS);
            }
            return;

        case fabgl::VK_RETURN:
        case fabgl::VK_KP_ENTER:
            if (s_view == V_SSID) {
                if (len == 0) return;
                snprintf(s_ssid, sizeof(s_ssid), "%s", s_text);
                text_begin(sv, V_PASSWORD);
                return;
            }
            // A typed-in network may be open (empty password); a secured one
            // needs a full-length passphrase.
            if ((len == 0 && !s_ssid_typed) || (len > 0 && len < PASS_MIN)) {
                s_pass_short = true;
                s_text_drawn = false;
                sv->needs_redraw = true;
                return;
            }
            wifi_mgr_connect(s_ssid, s_text);   // saves the credentials, then connects
            s_text[0] = '\0';
            s_info_dirty = true;
            set_view(sv, V_MAIN);
            return;

        case fabgl::VK_BACKSPACE:
            if (len > 0) s_text[len - 1] = '\0';
            sv->needs_redraw = true;
            return;

        default:
            break;
    }
    if (ascii >= 0x20 && ascii < 0x7F && len < max) {
        s_text[len] = (char)ascii;
        s_text[len + 1] = '\0';
        sv->needs_redraw = true;
    }
}

// ============================================================
// Main view keys
// ============================================================

static void wifi_execute(Supervisor_t* sv, int action) {
    switch (action) {
        case WACT_CONFIG:  set_view(sv, V_SCANNING);                             return;
        case WACT_SDFILE:  load_cfg_file(sv);                                    return;
        case WACT_CONNECT: wifi_mgr_connect_saved();                             break;
        case WACT_STOP:    wifi_mgr_stop();                                      break;
        case WACT_FORGET:
            // Irreversible (erases the saved SSID/password) and one row above
            // Debug Server: confirm first, defaulting to No.
            popup_open(sv, P_FORGET);
            return;
        case WACT_SERVER:  debug_server_set_enabled(!debug_server_enabled());    break;
    }
    s_info_dirty = true;    // row values ("none", ON/OFF) may have changed
    sv->needs_redraw = true;
}

void sv_wifi_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    if (hid_usage == HID_F1) {
        s_pop = P_NONE;
        s_view = V_MAIN;
        supervisor_toggle();
        return;
    }
    if (s_view == V_NETS) {
        nets_on_key(sv, hid_usage);
        return;
    }
    if (s_view != V_MAIN) return;       // scanning; text views use the raw route

    if (s_pop != P_NONE) {
        bool confirm = (s_pop == P_FORGET);
        switch (hid_usage) {
            case HID_UP:   if (s_pop_sel > 0) { s_pop_sel--; sv->needs_redraw = true; } break;
            case HID_DOWN: if (confirm && s_pop_sel < 1) { s_pop_sel++; sv->needs_redraw = true; } break;
            case HID_ENTER:
                if (confirm && s_pop_sel == 1) wifi_mgr_forget();
                // fall through: close the popup
            case HID_ESC:
                s_info_dirty = true;
                popup_open(sv, P_NONE);     // repaints the screen underneath
                break;
        }
        return;
    }

    switch (hid_usage) {
        case HID_UP:
            if (sv->menu_cursor > 0) { sv->menu_cursor--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (sv->menu_cursor < WACT_COUNT - 1) { sv->menu_cursor++; sv->needs_redraw = true; }
            break;
        case HID_ENTER:
            wifi_execute(sv, sv->menu_cursor);
            break;
        case HID_ESC:
            sv->state = SV_SETTINGS;
            sv->menu_cursor = SV_SET_WIFI;
            sv->needs_redraw = true;
            break;
    }
}

void sv_wifi_tick(Supervisor_t* sv) {
    WifiMgrState st = wifi_mgr_state();
    String ip = wifi_mgr_ip();
    if (st != s_last_state || ip != s_last_ip) {
        s_last_state = st;
        s_last_ip = ip;
        s_info_dirty = true;
        if (s_view == V_MAIN) sv->needs_redraw = true;
    }
}

// ============================================================
// Drawing
// ============================================================

// White panel: large signal bars (accent when connected, grey otherwise)
// and the State / SSID / IP lines.
static void draw_status_panel(OSDCanvas* tft) {
    int x = SVW_LIST_X, y = WIFI_PANEL_Y, w = SVW_LIST_W;
    bool up = (wifi_mgr_state() == WIFI_MGR_STA_RUNNING);

    tft->fillRect(x, y, w, WIFI_PANEL_H, SVW_WHITE);
    tft->drawRect(x, y, w, WIFI_PANEL_H, SVW_DKBLUE);
    for (int b = 0; b < 4; b++) {           // 64x28 signal bars
        int h = 7 + b * 7;
        tft->fillRect(x + 16 + b * 16, y + 3 + 28 - h, 12, h, up ? SVW_DKBLUE : SVW_GRAY);
    }

    String ssid = wifi_mgr_ssid();
    if (ssid.length() == 0) ssid = "-";
    const char* labels[3] = { "State", "SSID", "IP" };
    String values[3] = { wifi_mgr_state_str(), ssid, wifi_mgr_ip() };
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    for (int i = 0; i < 3; i++) {
        int ty = y + 3 + i * 10;
        tft->setTextColor(SVW_BLACK, SVW_WHITE);
        tft->drawString(labels[i], x + 104, ty);
        tft->setTextColor(SVW_DKBLUE, SVW_WHITE);
        tft->drawString(values[i].substring(0, 40).c_str(), x + 160, ty);
        DEBUG_PRINTF("wifi: %s %s", labels[i], values[i].c_str());
    }
}

static void draw_action_row(int i, bool highlighted) {
    const char* value = nullptr;
    if (i == WACT_CONNECT && !wifi_mgr_has_creds()) value = "none";
    else if (i == WACT_SERVER)                      value = debug_server_enabled() ? "ON" : "OFF";

    int y = WIFI_LIST_Y + i * SVW_ROW_H;
    sv_render_wide_row(y, WIFI_ACTIONS[i], value, highlighted);
    sv_menu_draw_row_icon(WIFI_ICONS[i], SVW_ICON_X, y + 2,
                          highlighted ? SVW_DKBLUE : SVW_GREEN,
                          highlighted ? SVW_WHITE : SVW_BLACK);
    if (highlighted) DEBUG_PRINTF("menu: WiFi / Debug > %s %s", WIFI_ACTIONS[i], value ? value : "");
}

static void draw_main_popup(void) {
    bool confirm = (s_pop == P_FORGET);
    bool all = (s_pop_drawn < 0);
    int count = confirm ? 2 : 1;
    const char* title = confirm ? "Forget credentials?" : s_msg_title;
    int ry = sv_render_popup(title,
                             confirm ? "Forget the saved WiFi network?" : s_msg1,
                             confirm ? NULL : s_msg2, count, all);
    if (all && !confirm) DEBUG_PRINTF("popup: %s: %s %s", title, s_msg1, s_msg2);
    for (int i = 0; i < count; i++) {
        if (!all && i != s_pop_sel && i != s_pop_drawn) continue;
        const char* label = !confirm ? "OK" : (i == 0 ? "No" : "Yes");
        sv_render_popup_row(ry + i * SVP_ROW_H, label, NULL, i == s_pop_sel);
        if (i == s_pop_sel) DEBUG_PRINTF("popup: %s > %s", title, label);
    }
    s_pop_drawn = s_pop_sel;
}

static void render_main(Supervisor_t* sv, OSDCanvas* tft) {
    int sel = sv->menu_cursor;

    if (s_drawn < 0) {
        // Whole screen: frame, status panel, every action row.
        sv_render_wide_frame("WiFi / Debug", "Up/Dn   ENTER Select   ESC Back   F3 Exit");
        draw_status_panel(tft);
        for (int i = 0; i < WACT_COUNT; i++) draw_action_row(i, i == sel);
        s_info_dirty = false;
    } else if (s_pop == P_NONE) {
        if (s_info_dirty) {
            // WiFi state changed: panel and row values, no frame repaint.
            draw_status_panel(tft);
            for (int i = 0; i < WACT_COUNT; i++) draw_action_row(i, i == sel);
            s_info_dirty = false;
        } else if (s_drawn != sel) {
            draw_action_row(s_drawn, false);
            draw_action_row(sel, true);
        }
    }
    s_drawn = (int8_t)sel;

    if (s_pop != P_NONE) draw_main_popup();
}

// 32x12 signal-strength bars: one to four filled, by RSSI.
static void draw_signal(OSDCanvas* tft, int x, int y, int rssi, uint16_t fg) {
    int bars = (rssi > -55) ? 4 : (rssi > -67) ? 3 : (rssi > -78) ? 2 : 1;
    for (int b = 0; b < 4; b++) {
        int h = 3 + b * 3;
        if (b < bars) tft->fillRect(x + 4 + b * 6, y + 12 - h, 4, h, fg);
        else          tft->fillRect(x + 4 + b * 6, y + 11, 4, 1, fg);
    }
}

static void draw_net_row(OSDCanvas* tft, int idx, bool highlighted) {
    int y = SVW_LIST_Y + (idx - s_net_top) * SVW_ROW_H;
    uint16_t fg = highlighted ? SVW_WHITE : SVW_BLACK;
    if (idx >= s_net_count) {
        sv_render_wide_row(y, "Other network...", NULL, highlighted);
        sv_menu_draw_row_icon(ROW_ICON_KEYBOARD, SVW_ICON_X, y + 2,
                              highlighted ? SVW_DKBLUE : SVW_GREEN, fg);
        if (highlighted) DEBUG_PRINT("wifi: net > Other network...");
        return;
    }
    const WifiNet* net = &s_nets[idx];
    sv_render_wide_row(y, net->ssid, net->secure ? "secured" : "open", highlighted);
    draw_signal(tft, SVW_ICON_X, y + 2, net->rssi, fg);
    if (highlighted) DEBUG_PRINTF("wifi: net > %s %d dBm %s", net->ssid, net->rssi,
                                  net->secure ? "secured" : "open");
}

// Thin scrollbar on the right, only when the list is longer than the page.
static void draw_net_scrollbar(OSDCanvas* tft) {
    int total = net_rows();
    if (total <= SVW_LIST_ROWS) return;
    int x = SVW_LIST_X + SVW_LIST_W - 4;
    int h = SVW_LIST_ROWS * SVW_ROW_H;
    int len = h * SVW_LIST_ROWS / total;
    if (len < 8) len = 8;
    int pos = (h - len) * s_net_top / (total - SVW_LIST_ROWS);
    tft->fillRect(x, SVW_LIST_Y, 3, h, SVW_GREEN);
    tft->fillRect(x, SVW_LIST_Y + pos, 3, len, SVW_DKBLUE);
}

static void render_nets(OSDCanvas* tft) {
    int total = net_rows();
    if (s_net_drawn < 0 || s_net_drawn_top != s_net_top) {
        if (s_net_drawn < 0) {
            sv_render_wide_frame("Config WiFi",
                                 "Up/Dn   ENTER Select   R Rescan   ESC Back   F3 Exit");
            if (s_net_count == 0) {
                tft->setTextFont(1);
                tft->setTextDatum(TC_DATUM);
                tft->setTextColor(SVW_DIM, SVW_GREEN);
                tft->drawString("(no networks found - press R to scan again)",
                                SVW_BOX_X + SVW_BOX_W / 2, SVW_LIST_Y + SVW_ROW_H + 8);
                tft->setTextDatum(TL_DATUM);
            }
        }
        for (int r = 0; r < SVW_LIST_ROWS && s_net_top + r < total; r++) {
            draw_net_row(tft, s_net_top + r, s_net_top + r == s_net_sel);
        }
    } else if (s_net_drawn != s_net_sel) {
        draw_net_row(tft, s_net_drawn, false);
        draw_net_row(tft, s_net_sel, true);
    }
    draw_net_scrollbar(tft);
    s_net_drawn = s_net_sel;
    s_net_drawn_top = s_net_top;
}

// Popup over the network list with one input field; the typed text is shown
// as entered, its tail kept visible when it is longer than the field.
static void render_text_popup(void) {
    bool pass = (s_view == V_PASSWORD);
    char msg1[SVP_MSG_COLS + 1];
    const char* msg2;
    if (pass) {
        snprintf(msg1, sizeof(msg1), "Network: %.30s", s_ssid);
        msg2 = s_pass_short ? "A password needs at least 8 characters."
             : s_ssid_typed ? "ENTER connects (empty = open network)."
                            : "ENTER connects, ESC goes back.";
    } else {
        snprintf(msg1, sizeof(msg1), "Type the network name (SSID).");
        msg2 = "ENTER continues, ESC goes back.";
    }
    const char* title = pass ? "WiFi password" : "Network name";
    int ry = sv_render_popup(title, msg1, msg2, 1, !s_text_drawn);
    if (!s_text_drawn) DEBUG_PRINTF("popup: %s: %s %s", title, msg1, msg2);
    s_text_drawn = true;

    char field[44];
    size_t len = strlen(s_text), keep = 39;
    snprintf(field, sizeof(field), "%s_", len > keep ? s_text + len - keep : s_text);
    sv_render_popup_row(ry, field, NULL, true);
}

void sv_wifi_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    switch (s_view) {
        case V_MAIN:
            render_main(sv, tft);
            break;

        case V_SCANNING:
            // Draw the wait message FIRST (the framebuffer is scanned out
            // continuously, so it is on screen at once), then block on the scan.
            sv_render_wide_frame("Config WiFi", "Scanning...");
            tft->setTextFont(2);
            tft->setTextDatum(TC_DATUM);
            tft->setTextColor(SVW_BLACK, SVW_GREEN);
            tft->drawString("Scanning for WiFi networks...", SVW_BOX_X + SVW_BOX_W / 2, SVW_BOX_Y + 84);
            tft->setTextDatum(TL_DATUM);
            scan_networks();
            set_view(sv, V_NETS);
            break;

        case V_NETS:
            render_nets(tft);
            break;

        case V_SSID:
        case V_PASSWORD:
            if (!s_text_drawn) {
                s_net_drawn = -1;       // the list underneath, then the popup
                render_nets(tft);
            }
            render_text_popup();
            break;
    }
}
