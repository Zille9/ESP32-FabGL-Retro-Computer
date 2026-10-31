/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_cart.cpp
 *  Module : Supervisor Load Cart screen
 * ============================================================
 */

#include "sv_cart.h"
#include "supervisor.h"
#include "sv_menu.h"
#include "sv_render.h"
#include "sv_filebrowser.h"

#include "../core/machine.h"
#include "../core/orch90.h"
#include "../hal/hal.h"
#include "../utils/debug.h"
#include "../../config.h"
#include <SD.h>
#include <string.h>
#include <esp_heap_caps.h>

// External canvas instance from hal_video.cpp
extern OSDCanvas* hal_video_get_canvas(void);

// HID usage codes
#define HID_UP      0x52
#define HID_DOWN    0x51
#define HID_ENTER   0x28
#define HID_ESC     0x29
#define HID_PGUP    0x4B
#define HID_PGDN    0x4E
#define HID_HOME    0x4A
#define HID_END     0x4D

// Folder the screen opens in the first time; the SD root if it is missing.
#define CART_DIR        "/carts"

// Layout inside the wide green frame: two status lines, then the list.
#define CART_STATUS_Y   (SVW_BOX_Y + 34)
#define CART_FOLDER_Y   (SVW_BOX_Y + 44)
#define CART_LIST_Y     (SVW_BOX_Y + 56)
#define CART_ROWS       ((SVW_HINT_Y - 4 - CART_LIST_Y) / SVW_ROW_H)
#define CART_SB_X       (SVW_LIST_X + SVW_LIST_W - 4)
#define CART_LABEL_COLS 50
#define CART_INFO_COLS  66

enum CartPopup : uint8_t { P_NONE, P_RESTART, P_MSG };

static char    s_path[256] = "";        // folder on screen; kept between visits
static int16_t s_count = 0;             // entries in sv->file_entries
static bool    s_has_unload = false;    // row 0 is "Unload cartridge"
static int16_t s_cursor = 0;
static int16_t s_scroll = 0;
// Cursor / scroll last drawn; s_drawn_cursor -1 = draw the whole screen.
static int16_t s_drawn_cursor = -1;
static int16_t s_drawn_scroll = 0;

static CartPopup s_pop = P_NONE;
static int8_t    s_pop_sel = 0;         // 0 = No, 1 = Yes
static int8_t    s_pop_drawn = -1;      // row drawn selected; -1 = draw the window
static char      s_target[256] = "";    // cartridge the popup is about; "" = unload
static char      s_pop_name[SVP_MSG_COLS + 1] = "";
static bool      s_pop_coco3_only = false;   // refused ROM would load on the CoCo 3

void sv_cart_invalidate(void) {
    s_drawn_cursor = -1;
    s_pop_drawn = -1;
}

static const char* base_name(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool is_cart_file(const char* name) {
    const char* ext = strrchr(name, '.');
    return ext && (strcasecmp(ext, ".ccc") == 0 || strcasecmp(ext, ".rom") == 0);
}

static bool dir_exists(const char* path) {
    File dir = SD.open(path);
    bool ok = dir && dir.isDirectory();
    if (dir) dir.close();
    return ok;
}

static int total_rows(void) {
    return s_count + (s_has_unload ? 1 : 0);
}

static void keep_cursor_visible(void) {
    int total = total_rows();
    if (s_cursor >= total) s_cursor = total - 1;
    if (s_cursor < 0) s_cursor = 0;
    if (s_cursor < s_scroll) s_scroll = s_cursor;
    if (s_cursor >= s_scroll + CART_ROWS) s_scroll = s_cursor - CART_ROWS + 1;
    if (s_scroll < 0) s_scroll = 0;
}

// The Unload row is there while a cartridge is loaded or waiting for a restart.
static void refresh_unload_row(void) {
    bool has = supervisor_cart_rom_saved()[0] || supervisor_cart_rom_boot()[0];
    if (has != s_has_unload) {
        s_cursor += has ? 1 : -1;       // stay on the same entry
        s_has_unload = has;
    }
    keep_cursor_visible();
}

// Scan s_path, keeping only folders and cartridge files.
static void scan_folder(Supervisor_t* sv) {
    s_count = 0;
    s_cursor = 0;
    s_scroll = 0;
    sv_cart_invalidate();
    sv->needs_redraw = true;

    if (!sv->file_entries) {
        size_t bytes = SV_FB_MAX_ENTRIES * sizeof(SV_FileEntry);
        sv->file_entries = (SV_FileEntry*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!sv->file_entries) {
            DEBUG_PRINT("Cart: failed to allocate file entries");
            return;
        }
    }

    int found = sv_fb_scan_directory(s_path, sv->file_entries, SV_FB_MAX_ENTRIES);
    for (int i = 0; i < found; i++) {
        SV_FileEntry* e = &sv->file_entries[i];
        if (!e->is_dir && !is_cart_file(e->name)) continue;
        if (s_count != i) sv->file_entries[s_count] = *e;
        s_count++;
    }
    sv_fb_sort_entries(sv->file_entries, s_count);
    DEBUG_PRINTF("Cart: %s -> %d entries", s_path, (int)s_count);
}

void sv_cart_open(Supervisor_t* sv) {
    sv->state = SV_CART;
    s_pop = P_NONE;
    if (!s_path[0] || !dir_exists(s_path)) {
        strcpy(s_path, dir_exists(CART_DIR) ? CART_DIR : "/");
    }
    s_has_unload = false;
    scan_folder(sv);
    refresh_unload_row();
    s_cursor = 0;
    s_scroll = 0;
}

static void enter_folder(Supervisor_t* sv, const char* name) {
    if (strcmp(name, "..") == 0) {
        char* slash = strrchr(s_path, '/');
        if (slash == s_path) s_path[1] = '\0';
        else if (slash)      *slash = '\0';
    } else {
        size_t len = strlen(s_path);
        if (len + 1 + strlen(name) >= sizeof(s_path)) return;
        snprintf(s_path + len, sizeof(s_path) - len, "%s%s", (len > 1) ? "/" : "", name);
    }
    bool had_unload = s_has_unload;
    scan_folder(sv);
    s_has_unload = had_unload;
    keep_cursor_visible();
}

static void popup_open(Supervisor_t* sv, CartPopup kind) {
    s_pop = kind;
    s_pop_sel = 0;
    sv_cart_invalidate();       // closing repaints the screen underneath
    sv->needs_redraw = true;
}

// Store `target` ("" = unload) as the cartridge for the next boot. Any other
// cartridge takes the slot from Orchestra-90, so its DAC ports go off with it;
// going back to what this boot loaded keeps the boot's Orchestra-90 state.
static void apply_target(Supervisor_t* sv, const char* target, bool restart) {
    bool same_as_boot = (strcmp(target, supervisor_cart_rom_boot()) == 0);
    supervisor_save_cart_rom(target);
    supervisor_save_orch90(same_as_boot ? orch90_enabled() : false);
    DEBUG_PRINTF("Cart: %s%s", target[0] ? target : "(unloaded)", restart ? ", restarting" : "");
    if (restart) supervisor_save_and_restart();     // never returns
    refresh_unload_row();
    popup_open(sv, P_NONE);
}

// Ask about the restart, unless the choice is what is already running.
static void choose_target(Supervisor_t* sv, const char* target) {
    strncpy(s_target, target, sizeof(s_target) - 1);
    s_target[sizeof(s_target) - 1] = '\0';
    snprintf(s_pop_name, sizeof(s_pop_name), "%s", base_name(s_target));
    if (strcmp(s_target, supervisor_cart_rom_boot()) == 0) {
        apply_target(sv, s_target, false);
        return;
    }
    popup_open(sv, P_RESTART);
}

static void select_row(Supervisor_t* sv) {
    int idx = s_cursor;
    if (s_has_unload) {
        if (idx == 0) {
            choose_target(sv, "");
            return;
        }
        idx--;
    }
    if (idx < 0 || idx >= s_count) return;

    SV_FileEntry* e = &sv->file_entries[idx];
    if (e->is_dir) {
        enter_folder(sv, e->name);
        return;
    }
    // The CoCo 3 takes bank-switched ROMs; the CoCo 2 only a single 16K bank.
    s_pop_coco3_only = (e->size > CART_BANK_SIZE && e->size <= CART_ROM_MAX_SIZE);
    if (e->size == 0 || e->size > ((g_machine_type == 4) ? CART_ROM_MAX_SIZE : CART_BANK_SIZE)) {
        snprintf(s_pop_name, sizeof(s_pop_name), "%s", e->name);
        popup_open(sv, P_MSG);
        return;
    }
    char path[sizeof(s_path) + sizeof(e->name) + 1];
    snprintf(path, sizeof(path), "%s%s%s", s_path, (s_path[1] ? "/" : ""), e->name);
    if (strlen(path) >= sizeof(s_target)) return;
    choose_target(sv, path);
}

void sv_cart_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    if (s_pop != P_NONE) {
        bool confirm = (s_pop == P_RESTART);
        switch (hid_usage) {
            case HID_UP:
            case HID_DOWN:
                if (confirm) {
                    s_pop_sel = (hid_usage == HID_DOWN) ? 1 : 0;
                    sv->needs_redraw = true;
                }
                break;
            case HID_ENTER:
                if (confirm) {
                    apply_target(sv, s_target, s_pop_sel == 1);
                    break;
                }
                // fall through: OK
            case HID_ESC:
                popup_open(sv, P_NONE);
                break;
        }
        return;
    }

    switch (hid_usage) {
        case HID_UP:    s_cursor--; break;
        case HID_DOWN:  s_cursor++; break;
        case HID_PGUP:  s_cursor -= CART_ROWS; break;
        case HID_PGDN:  s_cursor += CART_ROWS; break;
        case HID_HOME:  s_cursor = 0; break;
        case HID_END:   s_cursor = total_rows() - 1; break;
        case HID_ENTER: select_row(sv); return;
        case HID_ESC:   sv_menu_return(sv); return;
        default:        return;
    }
    keep_cursor_visible();
    sv->needs_redraw = true;
}

// ============================================================
// Drawing
// ============================================================

// Two lines above the list: the cartridge (with "after restart" while the
// change is pending) and the folder on screen.
static void draw_status(OSDCanvas* tft) {
    const char* saved = supervisor_cart_rom_saved();
    const char* boot  = supervisor_cart_rom_boot();
    int idx = (g_machine_type == 4) ? 1 : 0;
    const char* note = strcmp(saved, boot) != 0              ? " (after restart)"
                     : (saved[0] && g_cart_rom_fallback[idx]) ? " (file missing)"
                                                              : "";
    char line[CART_INFO_COLS + 1];
    snprintf(line, sizeof(line), "Cartridge: %.36s%s", saved[0] ? base_name(saved) : "none", note);

    tft->fillRect(SVW_LIST_X, CART_STATUS_Y, SVW_LIST_W, 20, SVW_GREEN);
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(SVW_DKBLUE, SVW_GREEN);
    tft->drawString(line, SVW_LIST_X + 8, CART_STATUS_Y);
    snprintf(line, sizeof(line), "Folder: %.56s", s_path);
    tft->setTextColor(SVW_BLACK, SVW_GREEN);
    tft->drawString(line, SVW_LIST_X + 8, CART_FOLDER_Y);
    DEBUG_PRINTF("cart: %s%s, folder %s", saved[0] ? saved : "none", note, s_path);
}

static void draw_row(OSDCanvas* tft, Supervisor_t* sv, int row) {
    int idx = s_scroll + row;
    int y = CART_LIST_Y + row * SVW_ROW_H;
    bool hl = (idx == s_cursor);
    uint16_t bg = hl ? SVW_DKBLUE : SVW_GREEN;
    uint16_t fg = hl ? SVW_WHITE : SVW_BLACK;

    if (idx >= total_rows()) {
        tft->fillRect(SVW_LIST_X, y, SVW_LIST_W, SVW_ROW_H, SVW_GREEN);
        return;
    }
    if (s_has_unload && idx == 0) {
        sv_render_wide_row(y, "Unload cartridge", NULL, hl);
        sv_menu_draw_row_icon(ROW_ICON_CROSS, SVW_ICON_X, y + 2, bg, fg);
        if (hl) DEBUG_PRINT("cart: > Unload cartridge");
        return;
    }

    SV_FileEntry* e = &sv->file_entries[idx - (s_has_unload ? 1 : 0)];
    char label[CART_LABEL_COLS + 1], value[12];
    snprintf(label, sizeof(label), "%s", e->name);
    if (e->is_dir) snprintf(value, sizeof(value), "<DIR>");
    else           snprintf(value, sizeof(value), "%luK", (unsigned long)((e->size + 1023) / 1024));
    sv_render_wide_row(y, label, value, hl);
    sv_menu_draw_row_icon(e->is_dir ? ROW_ICON_FOLDER : ROW_ICON_CART, SVW_ICON_X, y + 2, bg, fg);
    if (hl) DEBUG_PRINTF("cart: > %s %s", label, value);
}

// Thin scrollbar on the right edge of the rows, only when the list is longer
// than the page. Rows paint over it, so it is redrawn after them.
static void draw_scrollbar(OSDCanvas* tft) {
    int total = total_rows();
    if (total <= CART_ROWS) return;
    int h = CART_ROWS * SVW_ROW_H;
    int len = h * CART_ROWS / total;
    if (len < 8) len = 8;
    int pos = (h - len) * s_scroll / (total - CART_ROWS);
    tft->fillRect(CART_SB_X, CART_LIST_Y, 4, h, SVW_GREEN);
    tft->fillRect(CART_SB_X, CART_LIST_Y + pos, 4, len, SVW_DKBLUE);
}

static void draw_list(OSDCanvas* tft, Supervisor_t* sv) {
    for (int r = 0; r < CART_ROWS; r++) draw_row(tft, sv, r);
    if (total_rows() == 0) {
        tft->setTextFont(1);
        tft->setTextDatum(TL_DATUM);
        tft->setTextColor(SVW_DIM, SVW_GREEN);
        tft->drawString(sv->file_entries ? "(no .ccc or .rom files here)" : "(out of memory)",
                        SVW_LIST_X + 8, CART_LIST_Y + 4);
    }
    draw_scrollbar(tft);
}

static void draw_popup(void) {
    const char* title;
    const char* msg1;
    const char* msg2;
    if (s_pop == P_MSG) {
        title = "Cartridge not supported";
        msg1  = s_pop_name;
        msg2  = s_pop_coco3_only ? "ROMs over 16K need the CoCo 3 machine."
                                 : "Only ROMs of up to 128K can be loaded.";
    } else if (s_target[0]) {
        title = "Load cartridge";
        msg1  = s_pop_name;
        msg2  = "Restart the CoCo now to start it?";
    } else {
        title = "Unload cartridge";
        msg1  = "The cartridge is removed after a restart.";
        msg2  = "Restart the CoCo now?";
    }

    bool confirm = (s_pop == P_RESTART);
    int count = confirm ? 2 : 1;
    bool all = (s_pop_drawn < 0);
    int ry = sv_render_popup(title, msg1, msg2, count, all);
    if (all) DEBUG_PRINTF("popup: %s: %s %s", title, msg1, msg2);
    for (int i = 0; i < count; i++) {
        if (!all && i != s_pop_sel && i != s_pop_drawn) continue;
        const char* label = !confirm ? "OK" : (i == 0) ? "No" : "Yes";
        const char* value = !confirm ? NULL : (i == 0) ? "on next restart" : "restart now";
        sv_render_popup_row(ry + i * SVP_ROW_H, label, value, i == s_pop_sel);
        if (i == s_pop_sel) DEBUG_PRINTF("popup: %s > %s", title, label);
    }
    s_pop_drawn = s_pop_sel;
}

void sv_cart_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    // First pass draws everything; afterwards the whole list when it scrolled,
    // else only the two rows whose selection changed.
    if (s_drawn_cursor < 0) {
        sv_render_wide_frame("Load Cart", "Up/Dn   ENTER Select   ESC Back   F3 Exit");
        draw_status(tft);
        draw_list(tft, sv);
    } else if (s_pop == P_NONE) {
        if (s_drawn_scroll != s_scroll) {
            draw_list(tft, sv);
        } else if (s_drawn_cursor != s_cursor) {
            draw_row(tft, sv, s_drawn_cursor - s_scroll);
            draw_row(tft, sv, s_cursor - s_scroll);
            draw_scrollbar(tft);
        }
    }
    s_drawn_cursor = s_cursor;
    s_drawn_scroll = s_scroll;

    if (s_pop != P_NONE) draw_popup();
}
