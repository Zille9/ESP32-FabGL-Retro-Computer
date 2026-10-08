/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_fujinet.cpp
 *  Module : Supervisor DriveWire / FujiNet settings screen
 * ============================================================
 */

#include "sv_fujinet.h"
#include "supervisor.h"
#include "sv_menu.h"
#include "sv_render.h"

#include "../core/machine.h"
#include "../net/dw_bus.h"
#include "../net/wifi_mgr.h"
#include "../hal/hal.h"
#include "../../config.h"

extern OSDCanvas* hal_video_get_canvas(void);

/*
 * Screen, in the wide green frame:
 *   - white status panel: the bus configuration running this boot (mode and
 *     link, traffic, cartridge ROM);
 *   - a warning bar when no WiFi network is saved (the External back end
 *     needs WiFi);
 *   - rows: External Server ON/OFF, then Host / Port / HDB-DOS ROM (only
 *     while External Server is on), then Save & Restart.
 *
 * Turning External Server on opens a popup that asks for the host, port and
 * ROM variant; it refuses to turn on while the host is empty or the HDB-DOS
 * ROM file is not on the SD card, and says where to copy it. Edits are held
 * locally and only written by "Save & Restart", because the bus mode is
 * fixed at boot.
 */

#define HID_UP        0x52
#define HID_DOWN      0x51
#define HID_ENTER     0x28
#define HID_ESC       0x29
#define HID_F1        0x3A
#define HID_BACKSPACE 0x2A
#define HID_MINUS     0x2D
#define HID_PERIOD    0x37

// Rows of the screen. Host / Port / ROM are hidden while External Server is off.
enum {
    FACT_SERVER = 0, // External Server ON/OFF
    FACT_HOST,       // External server host (text entry)
    FACT_PORT,       // External server port (digit entry)
    FACT_ROM,        // HDB-DOS ROM variant: Standard / Timeout
    FACT_SAVE,       // Save & Restart
    FACT_COUNT
};

#define HOST_MAX     40
#define PORT_MAX     5

static const char* const FUJI_ACTIONS[FACT_COUNT] = {
    "External Server", "Host", "Port", "HDB-DOS ROM", "Save & Restart",
};
static const uint8_t FUJI_ICONS[FACT_COUNT] = {
    ROW_ICON_DRIVEWIRE, ROW_ICON_MACHINE, ROW_ICON_SERIAL, ROW_ICON_CHIP, ROW_ICON_SD,
};

// Layout: status panel, warning bar, rows.
#define FUJI_PANEL_Y  (SVW_BOX_Y + 32)
#define FUJI_PANEL_H  38
#define FUJI_WARN_Y   (FUJI_PANEL_Y + FUJI_PANEL_H + 2)
#define FUJI_WARN_H   10
#define FUJI_LIST_Y   (FUJI_WARN_Y + FUJI_WARN_H + 2)

// Pending (unsaved) settings.
static BusMode  p_mode;
static char     p_host[HOST_MAX + 1];
static char     p_port[PORT_MAX + 1];
static bool     p_rom_to;
static int      s_editing = -1;         // FACT_HOST / FACT_PORT while typing
static char     s_edit_backup[HOST_MAX + 1];

// Popups. POP_ENABLE is the form shown when External Server is turned on.
enum FujiPopup : uint8_t { POP_NONE, POP_ENABLE, POP_MESSAGE, POP_SAVE };
enum { EN_HOST = 0, EN_PORT, EN_ROM, EN_TURN_ON, EN_CANCEL, EN_COUNT };

static FujiPopup s_pop = POP_NONE;
static FujiPopup s_msg_return = POP_NONE;   // popup to go back to after a message
static int8_t    s_pop_sel = 0;
static int8_t    s_pop_drawn = -1;          // -1 = draw the whole window
static const char* s_msg_title = "";
static char      s_msg1[SVP_MSG_COLS + 1];
static char      s_msg2[SVP_MSG_COLS + 1];
// Values to restore if the enable form is cancelled.
static char      b_host[HOST_MAX + 1];
static char      b_port[PORT_MAX + 1];
static bool      b_rom_to;

// Redraw bookkeeping.
static const char* s_last_link = nullptr;
static int8_t s_drawn = -1;             // row drawn as selected; -1 = repaint all
static bool   s_info_dirty = false;     // link state changed: repaint the panel

void sv_fujinet_invalidate(void) {
    s_drawn = -1;
    s_pop_drawn = -1;
}

void sv_fujinet_open(Supervisor_t* sv) {
    sv->state = SV_FUJINET;
    sv->menu_cursor = 0;
    s_editing = -1;
    s_pop = POP_NONE;

    p_mode = dw_bus_mode();
    snprintf(p_host, sizeof(p_host), "%s", dw_bus_host().c_str());
    snprintf(p_port, sizeof(p_port), "%u", (unsigned)dw_bus_port());
    p_rom_to = dw_bus_rom_timeout();

    s_last_link = dw_bus_link_str();
    sv_fujinet_invalidate();
    sv->needs_redraw = true;
}

static bool pending_differs(void) {
    return p_mode != dw_bus_mode()
        || String(p_host) != dw_bus_host()
        || (uint16_t)atoi(p_port) != dw_bus_port()
        || p_rom_to != dw_bus_rom_timeout();
}

// Visible rows: Host / Port / ROM only while External Server is on.
static int visible_count(void) {
    return (p_mode == BUS_MODE_EXTERNAL) ? FACT_COUNT : 2;
}
static int visible_row(int i) {
    if (p_mode == BUS_MODE_EXTERNAL) return i;
    return (i == 0) ? FACT_SERVER : FACT_SAVE;
}

// HDB-DOS ROM file the running machine needs for the chosen variant.
static const char* rom_file(bool timeout) {
    if (g_machine_type == 4) return timeout ? ROM_BECKER_TO_COCO3_FILE : ROM_BECKER_COCO3_FILE;
    return timeout ? ROM_BECKER_TO_COCO2_FILE : ROM_BECKER_COCO2_FILE;
}

static bool rom_present(bool timeout) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%s", ROM_BASE_PATH, rom_file(timeout));
    return hal_storage_file_exists(path);
}

static void popup_open(Supervisor_t* sv, FujiPopup kind, int sel) {
    s_pop = kind;
    s_pop_sel = (int8_t)sel;
    // Popups differ in size: repaint the screen underneath first.
    sv_fujinet_invalidate();
    sv->needs_redraw = true;
}

static void popup_close(Supervisor_t* sv) {
    popup_open(sv, POP_NONE, 0);
}

static void message_open(Supervisor_t* sv, const char* title, FujiPopup return_to) {
    s_msg_title = title;
    s_msg_return = return_to;
    popup_open(sv, POP_MESSAGE, 0);
}

// External Server may only be on with a host and with the HDB-DOS ROM on the
// SD card. Explains what is missing and returns false otherwise.
static bool validate_external(Supervisor_t* sv, FujiPopup return_to) {
    if (p_host[0] == 0) {
        snprintf(s_msg1, sizeof(s_msg1), "Enter the server Host first");
        snprintf(s_msg2, sizeof(s_msg2), "(name or IP address).");
        message_open(sv, "Host missing", return_to);
        return false;
    }
    if (!rom_present(p_rom_to)) {
        snprintf(s_msg1, sizeof(s_msg1), "Copy %s to the SD card", rom_file(p_rom_to));
        snprintf(s_msg2, sizeof(s_msg2), "folder %s and try again.", ROM_BASE_PATH);
        message_open(sv, "HDB-DOS ROM not found", return_to);
        return false;
    }
    return true;
}

static void begin_edit(int field) {
    s_editing = field;
    snprintf(s_edit_backup, sizeof(s_edit_backup), "%s", field == FACT_HOST ? p_host : p_port);
}

static void cancel_edit(void) {
    if (s_editing == FACT_HOST) snprintf(p_host, sizeof(p_host), "%s", s_edit_backup);
    else if (s_editing == FACT_PORT) snprintf(p_port, sizeof(p_port), "%s", s_edit_backup);
    s_editing = -1;
}

static void end_edit(void) {
    if (s_editing == FACT_PORT) {
        long v = atol(p_port);
        if (v <= 0 || v > 65535) snprintf(p_port, sizeof(p_port), "%u", DW_DEFAULT_PORT);
    }
    s_editing = -1;
}

// The supervisor HID route delivers letters as uppercase usages; host names
// are case-insensitive, so they are stored lowercase.
static char usage_to_char(uint8_t u, bool digits_only) {
    if (u >= 0x1E && u <= 0x26) return '1' + (u - 0x1E);
    if (u == 0x27) return '0';
    if (digits_only) return 0;
    if (u >= 0x04 && u <= 0x1D) return 'a' + (u - 0x04);
    if (u == HID_PERIOD) return '.';
    if (u == HID_MINUS)  return '-';
    return 0;
}

static void edit_key(Supervisor_t* sv, uint8_t u) {
    char*  buf = (s_editing == FACT_HOST) ? p_host : p_port;
    size_t cap = (s_editing == FACT_HOST) ? HOST_MAX : PORT_MAX;
    size_t len = strlen(buf);

    if (u == HID_ENTER)      { end_edit(); }
    else if (u == HID_ESC)   { cancel_edit(); }
    else if (u == HID_BACKSPACE) { if (len > 0) buf[len - 1] = 0; }
    else {
        char c = usage_to_char(u, s_editing == FACT_PORT);
        if (c && len < cap) { buf[len] = c; buf[len + 1] = 0; }
    }
    sv->needs_redraw = true;
}

// Turning External Server on: ask for host, port and ROM in a popup form.
static void enable_form_open(Supervisor_t* sv) {
    snprintf(b_host, sizeof(b_host), "%s", p_host);
    snprintf(b_port, sizeof(b_port), "%s", p_port);
    b_rom_to = p_rom_to;
    popup_open(sv, POP_ENABLE, EN_HOST);
}

static void enable_form_cancel(Supervisor_t* sv) {
    snprintf(p_host, sizeof(p_host), "%s", b_host);
    snprintf(p_port, sizeof(p_port), "%s", b_port);
    p_rom_to = b_rom_to;
    popup_close(sv);
}

static void popup_accept(Supervisor_t* sv) {
    switch (s_pop) {
        case POP_ENABLE:
            switch (s_pop_sel) {
                case EN_HOST: begin_edit(FACT_HOST); break;
                case EN_PORT: begin_edit(FACT_PORT); break;
                case EN_ROM:  p_rom_to = !p_rom_to;  break;
                case EN_TURN_ON:
                    if (validate_external(sv, POP_ENABLE)) {
                        p_mode = BUS_MODE_EXTERNAL;
                        sv->menu_cursor = 0;
                        popup_close(sv);
                    }
                    return;
                default:
                    enable_form_cancel(sv);
                    return;
            }
            sv->needs_redraw = true;
            break;

        case POP_MESSAGE:
            popup_open(sv, s_msg_return, s_msg_return == POP_ENABLE ? EN_TURN_ON : 0);
            break;

        case POP_SAVE:
            if (s_pop_sel == 1) {
                dw_bus_save_config(p_mode, String(p_host), (uint16_t)atoi(p_port), p_rom_to);
                supervisor_save_and_restart();   // never returns
            }
            popup_close(sv);
            break;

        default:
            break;
    }
}

static void popup_on_key(Supervisor_t* sv, uint8_t hid_usage) {
    int count = (s_pop == POP_ENABLE) ? EN_COUNT : (s_pop == POP_MESSAGE) ? 1 : 2;
    switch (hid_usage) {
        case HID_UP:
            if (s_pop_sel > 0) { s_pop_sel--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (s_pop_sel < count - 1) { s_pop_sel++; sv->needs_redraw = true; }
            break;
        case HID_ENTER:
            popup_accept(sv);
            break;
        case HID_ESC:
            if (s_pop == POP_ENABLE)       enable_form_cancel(sv);
            else if (s_pop == POP_MESSAGE) popup_accept(sv);
            else                           popup_close(sv);
            break;
    }
}

static void fujinet_execute(Supervisor_t* sv, int action) {
    switch (action) {
        case FACT_SERVER:
            if (p_mode == BUS_MODE_EXTERNAL) {
                // Off needs no questions; the server rows disappear.
                p_mode = BUS_MODE_OFF;
                sv->menu_cursor = 0;
                sv_fujinet_invalidate();
            } else {
                enable_form_open(sv);
            }
            break;
        case FACT_HOST:
        case FACT_PORT: begin_edit(action);    break;
        case FACT_ROM:  p_rom_to = !p_rom_to;  break;
        case FACT_SAVE:
            if (p_mode == BUS_MODE_EXTERNAL && !validate_external(sv, POP_NONE)) return;
            popup_open(sv, POP_SAVE, 0);
            return;
    }
    sv->needs_redraw = true;
}

void sv_fujinet_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    if (hid_usage == HID_F1) {      // closes the whole supervisor, discarding edits
        if (s_editing >= 0) cancel_edit();
        s_pop = POP_NONE;
        supervisor_toggle();
        return;
    }
    if (s_editing >= 0) {
        edit_key(sv, hid_usage);
        return;
    }
    if (s_pop != POP_NONE) {
        popup_on_key(sv, hid_usage);
        return;
    }

    switch (hid_usage) {
        case HID_UP:
            if (sv->menu_cursor > 0) { sv->menu_cursor--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (sv->menu_cursor < visible_count() - 1) { sv->menu_cursor++; sv->needs_redraw = true; }
            break;
        case HID_ENTER:
            fujinet_execute(sv, visible_row(sv->menu_cursor));
            break;
        case HID_ESC:   // discard pending edits
            sv->state = SV_SETTINGS;
            sv->menu_cursor = SV_SET_DRIVEWIRE;
            sv->needs_redraw = true;
            break;
    }
}

void sv_fujinet_tick(Supervisor_t* sv) {
    const char* link = dw_bus_link_str();
    if (link != s_last_link) {
        s_last_link = link;
        s_info_dirty = true;
        sv->needs_redraw = true;
    }
}

// ============================================================
// Drawing
// ============================================================

// White panel: the configuration running this boot (not the pending edits).
static void draw_status_panel(OSDCanvas* tft) {
    int x = SVW_LIST_X, y = FUJI_PANEL_Y, w = SVW_LIST_W;
    bool on = (dw_bus_mode() != BUS_MODE_OFF);

    tft->fillRect(x, y, w, FUJI_PANEL_H, SVW_WHITE);
    tft->drawRect(x, y, w, FUJI_PANEL_H, SVW_DKBLUE);

    // 64x28 diskette on a wire: coloured when the bus is on, grey when off
    tft->fillRect(x + 16, y + 5, 40, 28, on ? SVW_RED : SVW_GRAY);
    tft->fillRect(x + 22, y + 7, 28, 8, SVW_WHITE);
    tft->fillEllipse(x + 36, y + 24, 14, 6, on ? SVW_BLACK : SVW_DKGRAY);
    tft->fillRect(x + 56, y + 17, 26, 3, on ? SVW_DKBLUE : SVW_GRAY);

    char traffic[32];
    snprintf(traffic, sizeof(traffic), "%lu in / %lu out",
             (unsigned long)becker_bytes_to_coco(), (unsigned long)becker_bytes_from_coco());
    int idx = (g_machine_type == 4) ? 1 : 0;
    const char* rom = g_cart_rom_fallback[idx] ? "HDB-DOS MISSING"
                    : (g_cart_rom_loaded[idx] ? g_cart_rom_loaded[idx] : "none");
    char bus[40];
    snprintf(bus, sizeof(bus), "%s  %s", dw_bus_mode_str(dw_bus_mode()),
             on ? dw_bus_link_str() : "");

    const char* labels[3] = { "Bus", "Traffic", "ROM" };
    const char* values[3] = { bus, traffic, rom };
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    for (int i = 0; i < 3; i++) {
        int ty = y + 4 + i * 11;
        char padded[41];
        snprintf(padded, sizeof(padded), "%-40.40s", values[i]);
        tft->setTextColor(SVW_BLACK, SVW_WHITE);
        tft->drawString(labels[i], x + 104, ty);
        tft->setTextColor((i == 2 && g_cart_rom_fallback[idx]) ? SV_COLOR_WARN : SVW_DKBLUE, SVW_WHITE);
        tft->drawString(padded, x + 176, ty);
        DEBUG_PRINTF("drivewire: %s %s", labels[i], values[i]);
    }
}

// Orange bar when no WiFi network is saved: the external server needs WiFi.
static void draw_wifi_warning(OSDCanvas* tft) {
    if (wifi_mgr_has_creds()) return;
    static const char msg[] = "! WiFi is not configured: set it up in Settings > WiFi / Debug first";
    tft->fillRect(SVW_LIST_X, FUJI_WARN_Y, SVW_LIST_W, FUJI_WARN_H, SVW_ORANGE);
    tft->setTextFont(0);
    tft->setTextDatum(TC_DATUM);
    tft->setTextColor(SVW_BLACK, SVW_ORANGE);
    tft->drawString(msg, SVW_BOX_X + SVW_BOX_W / 2, FUJI_WARN_Y + 1);
    tft->setTextDatum(TL_DATUM);
    DEBUG_PRINTF("drivewire: %s", msg);
}

// Value shown for a setting, with an underscore cursor while it is typed.
static const char* field_value(int field, char* buf, size_t size) {
    switch (field) {
        case FACT_SERVER: return (p_mode == BUS_MODE_EXTERNAL) ? "ON" : "OFF";
        case FACT_HOST:
        case FACT_PORT: {
            const char* src = (field == FACT_HOST) ? p_host : p_port;
            if (s_editing == field) {
                // keep the tail visible when the host is long
                size_t len = strlen(src), keep = 27;
                snprintf(buf, size, "%s_", len > keep ? src + len - keep : src);
                return buf;
            }
            if (!src[0]) return "-";
            snprintf(buf, size, "%.28s", src);
            return buf;
        }
        case FACT_ROM:  return p_rom_to ? "Timeout" : "Standard";
        case FACT_SAVE: return pending_differs() ? "*" : nullptr;
    }
    return nullptr;
}

static void draw_row(int i, bool highlighted) {
    int field = visible_row(i);
    char buf[HOST_MAX + 2];
    const char* value = field_value(field, buf, sizeof(buf));
    int y = FUJI_LIST_Y + i * SVW_ROW_H;
    sv_render_wide_row(y, FUJI_ACTIONS[field], value, highlighted);
    sv_menu_draw_row_icon(FUJI_ICONS[field], SVW_ICON_X, y + 2,
                          highlighted ? SVW_DKBLUE : SVW_GREEN,
                          highlighted ? SVW_WHITE : SVW_BLACK);
    if (highlighted) DEBUG_PRINTF("menu: DriveWire > %s %s", FUJI_ACTIONS[field], value ? value : "");
}

static void draw_popup(void) {
    static const char* const en_labels[EN_COUNT] = {
        "Host", "Port", "HDB-DOS ROM", "Turn On", "Cancel"
    };
    static const int en_fields[EN_COUNT] = { FACT_HOST, FACT_PORT, FACT_ROM, -1, -1 };

    const char* title = "Save and restart?";
    const char* msg1 = "Save DriveWire settings and restart?";
    const char* msg2 = NULL;
    int count = 2;
    if (s_pop == POP_ENABLE) {
        title = "External Server";
        msg1 = "Server the emulator will connect to.";
        msg2 = "ENTER edits a field; then choose Turn On.";
        count = EN_COUNT;
    } else if (s_pop == POP_MESSAGE) {
        title = s_msg_title;
        msg1 = s_msg1;
        msg2 = s_msg2;
        count = 1;
    }

    bool all = (s_pop_drawn < 0);
    int ry = sv_render_popup(title, msg1, msg2, count, all);
    if (all) DEBUG_PRINTF("popup: %s: %s %s", title, msg1, msg2 ? msg2 : "");
    for (int i = 0; i < count; i++) {
        if (!all && i != s_pop_sel && i != s_pop_drawn) continue;
        char buf[HOST_MAX + 2];
        const char* label = (s_pop == POP_ENABLE) ? en_labels[i]
                          : (s_pop == POP_MESSAGE) ? "OK" : (i == 0 ? "No" : "Yes");
        const char* value = (s_pop == POP_ENABLE && en_fields[i] >= 0)
                          ? field_value(en_fields[i], buf, sizeof(buf)) : NULL;
        sv_render_popup_row(ry + i * SVP_ROW_H, label, value, i == s_pop_sel);
        if (i == s_pop_sel) DEBUG_PRINTF("popup: %s > %s %s", title, label, value ? value : "");
    }
    s_pop_drawn = s_pop_sel;
}

void sv_fujinet_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;
    int sel = sv->menu_cursor;
    if (sel >= visible_count()) sel = sv->menu_cursor = 0;

    if (s_drawn < 0) {
        // Whole screen: frame, status panel, warning, every row.
        sv_render_wide_frame("DriveWire / FujiNet",
                             "Up/Dn   ENTER Select / Edit   ESC Back   F3 Exit");
        draw_status_panel(tft);
        draw_wifi_warning(tft);
        for (int i = 0; i < visible_count(); i++) draw_row(i, i == sel);
        s_info_dirty = false;
    } else if (s_pop == POP_NONE) {
        if (s_info_dirty) {
            draw_status_panel(tft);     // link state changed
            s_info_dirty = false;
        }
        // The row left behind, the selected row (its value may have just
        // changed or be mid-edit) and Save & Restart (its "*" marker).
        if (s_drawn != sel) draw_row(s_drawn, false);
        draw_row(sel, true);
        int save = visible_count() - 1;
        if (save != sel) draw_row(save, false);
    }
    s_drawn = (int8_t)sel;

    if (s_pop != POP_NONE) draw_popup();
}
