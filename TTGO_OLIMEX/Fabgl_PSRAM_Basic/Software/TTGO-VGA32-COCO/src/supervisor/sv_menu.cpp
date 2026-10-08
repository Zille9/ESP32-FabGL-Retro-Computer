/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_menu.cpp
 *  Module : Supervisor OSD main menu — disk/drive options, reset, and settings
 * ============================================================
*/

/*
 * sv_menu.cpp - Main menu for supervisor OSD
 */

#include "sv_menu.h"
#include "supervisor.h"
#include "sv_filebrowser.h"
#include "sv_render.h"
#include "sv_debug.h"
#include "sv_keymap.h"
#include "sv_joystick.h"
#include "sv_wifi.h"
#include "sv_fujinet.h"
#include "../net/dw_bus.h"
#include "../net/wifi_mgr.h"
#include "../hal/hal.h"
#include "../../config.h"

// External canvas instance from hal_video.cpp
extern OSDCanvas* hal_video_get_canvas(void);

// HID usage codes
#define HID_UP    0x52
#define HID_DOWN  0x51
#define HID_LEFT  0x50
#define HID_RIGHT 0x4F
#define HID_ENTER 0x28
#define HID_ESC   0x29
#define HID_F1    0x3A

// ============================================================
// Main menu — 3x2 grid of icon tiles
// ============================================================
//
// Drawn in the wide green frame (sv_render_wide_frame). 640x200 pixels are
// ~2.4x taller than wide, so tiles and icons are stretched horizontally to
// look square.

#define TILE_W       168
#define TILE_H       58
#define TILE_PITCH_X 180
#define TILE_PITCH_Y 66
#define TILE_X0      (SVW_BOX_X + (SVW_BOX_W - (2 * TILE_PITCH_X + TILE_W)) / 2)
#define TILE_Y0      (SVW_BOX_Y + 36)
#define TILE_COLS    3
#define ICON_W       64      // 32 units, each 2 pixels wide
#define ICON_H       28

struct SV_Tile {
    const char*   label;
    SV_MenuAction action;
};

static const SV_Tile tiles[] = {
    { "Disks",  SV_ACT_MOUNT_DISK },
    { "Setup",  SV_ACT_SETTINGS   },
    { "Reset",  SV_ACT_RESET      },
    { "Debug",  SV_ACT_DEBUG      },
    { "About",  SV_ACT_ABOUT      },
    { "Resume", SV_ACT_RESUME     },
};

static const int TILE_COUNT = sizeof(tiles) / sizeof(tiles[0]);

// Tile last drawn as selected; -1 = nothing on screen, draw the whole menu.
static int8_t s_tile_drawn = -1;

void sv_menu_init(Supervisor_t* sv) {
    sv->menu_cursor = 0;
    sv->menu_scroll_offset = 0;
    sv->menu_item_count = TILE_COUNT;
}

void sv_menu_invalidate(void) {
    s_tile_drawn = -1;
}

// Machine-select submenu (2 options: CoCo 2 / CoCo 3).
static const char* const MACHINE_LABELS[2] = { MACHINE_NAME_COCO2, MACHINE_NAME_COCO3 };
static const uint8_t      MACHINE_VALUES[2] = { 3, 4 };
// Stashed during the confirm dialog so the callback knows which target to set.
static uint8_t pending_machine_type = 0;
// Saved main-menu cursor/count so ESC from the submenu restores the caller.
static int8_t saved_main_menu_cursor = 0;
static uint8_t saved_main_menu_count = 0;

static void restore_main_menu(Supervisor_t* sv) {
    sv->state = SV_MAIN_MENU;
    sv->menu_cursor = saved_main_menu_cursor;
    sv->menu_item_count = saved_main_menu_count;
    sv->needs_redraw = true;
}

static void execute_action(Supervisor_t* sv, SV_MenuAction action) {
    switch (action) {
        case SV_ACT_MOUNT_DISK:
            sv->target_drive = 0;
            sv->prev_state = sv->state;
            sv->state = SV_FILE_BROWSER;
            sv_filebrowser_open(sv, sv->current_path, sv->target_drive);
            break;

        case SV_ACT_SETTINGS:
            // Save main-menu state so ESC restores it.
            saved_main_menu_cursor = sv->menu_cursor;
            saved_main_menu_count  = sv->menu_item_count;
            sv->prev_state = sv->state;
            sv->state = SV_SETTINGS;
            sv->menu_cursor = 0;
            sv->needs_redraw = true;
            break;

        case SV_ACT_RESET:
            sv->prev_state = sv->state;
            sv->state = SV_CONFIRM_DIALOG;
            sv->confirm_message = "Reset machine?";
            sv->confirm_yes_selected = false;
            sv->confirm_callback = [](bool accepted, void* ctx) {
                Supervisor_t* s = (Supervisor_t*)ctx;
                if (accepted && s->machine) {
                    // Flush dirty disk caches to SD before reset — a reset
                    // wipes FDC state and would otherwise lose pending writes.
                    sv_disk_flush_all(&s->machine->fdc);
                    machine_reset(s->machine);
                }
                // Close through the toggle so the main-menu box is cleared.
                supervisor_toggle();
            };
            sv->confirm_context = sv;
            sv->needs_redraw = true;
            break;

        case SV_ACT_DEBUG:
            // Save main-menu state so ESC from the submenu restores it.
            saved_main_menu_cursor = sv->menu_cursor;
            saved_main_menu_count  = sv->menu_item_count;
            sv->prev_state = sv->state;
            sv->state = SV_DEBUG_MENU;
            sv->menu_cursor = 0;
            sv->menu_item_count = SV_DBG_PAGE_COUNT;
            sv->needs_redraw = true;
            break;

        case SV_ACT_ABOUT:
            sv->prev_state = sv->state;
            sv->state = SV_ABOUT;
            sv->needs_redraw = true;
            break;

        case SV_ACT_RESUME:
            supervisor_toggle();
            break;

        default:
            break;
    }
}

void sv_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    int sel = sv->menu_cursor;
    switch (hid_usage) {
        case HID_LEFT:  if (sel % TILE_COLS > 0) sel--; break;
        case HID_RIGHT: if (sel % TILE_COLS < TILE_COLS - 1 && sel < TILE_COUNT - 1) sel++; break;
        case HID_UP:    if (sel >= TILE_COLS) sel -= TILE_COLS; break;
        case HID_DOWN:  if (sel + TILE_COLS < TILE_COUNT) sel += TILE_COLS; break;

        case HID_ENTER:
            DEBUG_PRINTF("menu: SUPERVISOR > %s (open)", tiles[sel].label);
            execute_action(sv, tiles[sel].action);
            return;

        case HID_ESC:
        case HID_F1:
            supervisor_toggle();
            return;
    }
    if (sel != sv->menu_cursor) {
        sv->menu_cursor = sel;
        sv->needs_redraw = true;
    }
}

// One icon rectangle in 32x28 unit coordinates (inclusive corners).
static inline void icon_box(OSDCanvas* tft, int x, int y, int ux1, int uy1,
                            int ux2, int uy2, uint16_t color) {
    tft->fillRect(x + ux1 * 2, y + uy1, (ux2 - ux1 + 1) * 2, uy2 - uy1 + 1, color);
}

// 64x28 icons drawn from rectangles, ellipses and lines; bg/fg are the tile's
// own colours so each icon reads on both the white and the selected tile.
static void draw_icon(OSDCanvas* tft, SV_MenuAction action, int x, int y,
                      uint16_t bg, uint16_t fg) {
    switch (action) {
        case SV_ACT_MOUNT_DISK:   // 5.25" diskette: jacket, label, hub, head slot
            icon_box(tft, x, y, 4, 1, 27, 26, SVW_DKGRAY);
            icon_box(tft, x, y, 7, 3, 20, 7, SVW_WHITE);
            icon_box(tft, x, y, 7, 3, 20, 4, SVW_RED);
            icon_box(tft, x, y, 26, 6, 27, 8, bg);
            tft->fillEllipse(x + 32, y + 14, 22, 10, SVW_GRAY);
            tft->fillEllipse(x + 32, y + 14, 10, 4, SVW_BLACK);
            icon_box(tft, x, y, 15, 20, 16, 24, SVW_BLACK);
            break;

        case SV_ACT_SETTINGS: {   // three sliders
            static const uint8_t knob[3] = { 7, 19, 12 };
            for (int r = 0; r < 3; r++) {
                int uy = 5 + r * 8;
                icon_box(tft, x, y, 3, uy, 28, uy + 1, fg);
                icon_box(tft, x, y, knob[r], uy - 3, knob[r] + 5, uy + 4, SVW_LEAF);
            }
            break;
        }

        case SV_ACT_RESET:        // power symbol: ring open at the top, bar through the gap
            tft->fillEllipse(x + 32, y + 15, 52, 23, SVW_ORANGE);
            tft->fillEllipse(x + 32, y + 15, 36, 15, bg);
            icon_box(tft, x, y, 11, 1, 20, 11, bg);
            icon_box(tft, x, y, 14, 1, 17, 14, SVW_ORANGE);
            break;

        case SV_ACT_DEBUG: {      // ladybug: legs, antennae, head, red shell with spots
            static const int8_t legs[3][4] = { { 14, 11, 4, 6 }, { 11, 17, 1, 17 }, { 14, 23, 4, 27 } };
            for (int l = 0; l < 3; l++) {
                for (int t = 0; t < 2; t++) {       // two pixels thick
                    tft->drawLine(x + legs[l][0] + t, y + legs[l][1], x + legs[l][2] + t, y + legs[l][3], fg);
                    tft->drawLine(x + 63 - legs[l][0] - t, y + legs[l][1], x + 63 - legs[l][2] - t, y + legs[l][3], fg);
                }
            }
            for (int t = 0; t < 2; t++) {
                tft->drawLine(x + 27 + t, y + 4, x + 21 + t, y, fg);
                tft->drawLine(x + 36 - t, y + 4, x + 42 - t, y, fg);
            }
            tft->fillEllipse(x + 32, y + 6, 20, 9, fg);
            tft->fillRect(x + 27, y + 4, 2, 2, bg);
            tft->fillRect(x + 35, y + 4, 2, 2, bg);
            tft->fillEllipse(x + 32, y + 17, 44, 20, 0xF800);
            tft->fillRect(x + 31, y + 8, 2, 19, SVW_BLACK);
            static const int8_t spots[6][2] = { { 21, 13 }, { 43, 13 }, { 17, 19 }, { 47, 19 }, { 25, 23 }, { 39, 23 } };
            for (int i = 0; i < 6; i++) {
                tft->fillEllipse(x + spots[i][0], y + spots[i][1], 8, 4, SVW_BLACK);
            }
            break;
        }

        case SV_ACT_ABOUT:        // "i" in a disc
            tft->fillEllipse(x + 32, y + 14, 58, 27, SVW_TEAL);
            icon_box(tft, x, y, 15, 5, 16, 7, SVW_WHITE);
            icon_box(tft, x, y, 15, 10, 16, 22, SVW_WHITE);
            break;

        case SV_ACT_RESUME:       // play triangle
            for (int r = -12; r <= 12; r++) {
                int len = (12 - abs(r)) * 20 / 12;
                icon_box(tft, x, y, 7, 14 + r, 7 + len, 14 + r, SVW_PURPLE);
            }
            break;

        default:
            break;
    }
}

static void draw_tile(OSDCanvas* tft, int idx, bool selected) {
    int x = TILE_X0 + (idx % TILE_COLS) * TILE_PITCH_X;
    int y = TILE_Y0 + (idx / TILE_COLS) * TILE_PITCH_Y;
    uint16_t bg = selected ? SVW_DKBLUE : SVW_WHITE;
    uint16_t fg = selected ? SVW_WHITE : SVW_BLACK;
    uint16_t edge = selected ? SVW_BLACK : SVW_DKBLUE;

    tft->fillRect(x, y, TILE_W, TILE_H, bg);
    tft->drawRect(x, y, TILE_W, TILE_H, edge);
    if (selected) tft->drawRect(x + 1, y + 1, TILE_W - 2, TILE_H - 2, edge);

    draw_icon(tft, tiles[idx].action, x + (TILE_W - ICON_W) / 2, y + 6, bg, fg);

    tft->setTextFont(2);
    tft->setTextColor(fg, bg);
    tft->setTextDatum(TC_DATUM);
    tft->drawString(tiles[idx].label, x + TILE_W / 2, y + 39);
    tft->setTextDatum(TL_DATUM);
}

void sv_menu_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    int sel = sv->menu_cursor;
    if (sel < 0 || sel >= TILE_COUNT) sel = sv->menu_cursor = 0;

    // First pass draws everything; afterwards only the two tiles that changed.
    if (s_tile_drawn < 0) {
        sv_render_wide_frame("COCO SUPERVISOR", "Arrows   ENTER Select   ESC/F3 Exit");
        for (int i = 0; i < TILE_COUNT; i++) draw_tile(tft, i, i == sel);
    } else if (s_tile_drawn != sel) {
        draw_tile(tft, s_tile_drawn, false);
        draw_tile(tft, sel, true);
    }
    s_tile_drawn = sel;
    DEBUG_PRINTF("menu: SUPERVISOR > %s", tiles[sel].label);
}

// ============================================================
// Machine select — popup over the Settings list
// ============================================================
//
// Two steps in the same window: pick CoCo 2 / CoCo 3, then confirm the
// restart (No / Yes, defaults to No). Accepting persists the choice in NVS
// and restarts; ESC goes back one step.

static bool   s_machine_confirming = false;
// Row last drawn as selected; -1 = draw the whole window.
static int8_t s_machine_drawn = -1;

void sv_machine_select_invalidate(void) {
    s_machine_drawn = -1;
}

// Opened from the Settings "Machine" row; ESC and a no-op select return there.
static void restore_settings(Supervisor_t* sv) {
    sv->state = SV_SETTINGS;
    sv->menu_cursor = SV_SET_MACHINE;
    sv->needs_redraw = true;
}

static void machine_select_open(Supervisor_t* sv) {
    sv->prev_state = sv->state;
    sv->state = SV_MACHINE_SELECT;
    // Seed cursor on the currently-active machine so ENTER on it is a no-op.
    sv->menu_cursor = (g_machine_type == 4) ? 1 : 0;
    s_machine_confirming = false;
    s_machine_drawn = -1;
    sv->needs_redraw = true;
}

void sv_machine_select_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    switch (hid_usage) {
        case HID_UP:
            if (sv->menu_cursor > 0) {
                sv->menu_cursor--;
                sv->needs_redraw = true;
            }
            break;

        case HID_DOWN:
            if (sv->menu_cursor < 1) {
                sv->menu_cursor++;
                sv->needs_redraw = true;
            }
            break;

        case HID_ENTER:
            if (s_machine_confirming) {
                if (sv->menu_cursor == 1) {
                    // Persists selection in NVS and calls esp_restart() — no return.
                    supervisor_set_machine_type(pending_machine_type);
                }
                restore_settings(sv);   // No
                break;
            }
            if (MACHINE_VALUES[sv->menu_cursor] == g_machine_type) {
                // Already active — go back to Settings without restarting.
                restore_settings(sv);
                break;
            }
            pending_machine_type = MACHINE_VALUES[sv->menu_cursor];
            s_machine_confirming = true;
            sv->menu_cursor = 0;        // No
            s_machine_drawn = -1;
            sv->needs_redraw = true;
            break;

        case HID_ESC:
            if (s_machine_confirming) {
                // Back to the machine list, on the machine that was picked.
                s_machine_confirming = false;
                sv->menu_cursor = (pending_machine_type == 4) ? 1 : 0;
                s_machine_drawn = -1;
                sv->needs_redraw = true;
            } else {
                restore_settings(sv);
            }
            break;

        case HID_F1:
            supervisor_toggle();
            break;
    }
}

static void draw_machine_row(int y, int row, bool highlighted) {
    const char* label;
    const char* value = NULL;
    if (s_machine_confirming) {
        label = (row == 0) ? "No" : "Yes";
    } else {
        label = MACHINE_LABELS[row];
        if (MACHINE_VALUES[row] == g_machine_type) value = "(current)";
    }
    sv_render_popup_row(y, label, value, highlighted);
    if (highlighted) DEBUG_PRINTF("popup: Machine > %s %s", label, value ? value : "");
}

void sv_machine_select_render(Supervisor_t* sv) {
    // Both steps carry one message line, so the window keeps its size.
    const char* title = s_machine_confirming ? "Restart emulator?" : "Select Machine";
    const char* msg   = !s_machine_confirming ? "Changing machine restarts the emulator."
                      : (pending_machine_type == 4) ? "Switch to CoCo 3 and restart?"
                                                    : "Switch to CoCo 2 and restart?";
    int sel = sv->menu_cursor;
    bool all = (s_machine_drawn < 0);
    int ry = sv_render_popup(title, msg, NULL, 2, all);

    if (all) {
        for (int i = 0; i < 2; i++) draw_machine_row(ry + i * SVP_ROW_H, i, i == sel);
    } else if (s_machine_drawn != sel) {
        draw_machine_row(ry + s_machine_drawn * SVP_ROW_H, s_machine_drawn, false);
        draw_machine_row(ry + sel * SVP_ROW_H, sel, true);
    }
    s_machine_drawn = sel;
}

// ============================================================
// Row icons — shared by the Settings, Keyboard and Debug lists
// ============================================================

// 32x12 row icons on a 16x12 unit grid (inclusive corners, 2-pixel units).
static inline void row_icon_box(OSDCanvas* tft, int x, int y, int ux1, int uy1,
                                int ux2, int uy2, uint16_t color) {
    tft->fillRect(x + ux1 * 2, y + uy1, (ux2 - ux1 + 1) * 2, uy2 - uy1 + 1, color);
}

// bg/fg are the row's own colours so each icon reads on both the green box
// and the accent-colour selection bar.
static void draw_row_icon(OSDCanvas* tft, int icon, int x, int y, uint16_t bg, uint16_t fg) {
    switch (icon) {
        case ROW_ICON_CHECK:        // tick mark
            for (int t = 0; t < 3; t++) {
                tft->drawLine(x + 7 + t, y + 6, x + 12 + t, y + 10, fg);
                tft->drawLine(x + 12 + t, y + 10, x + 23 + t, y + 1, fg);
            }
            break;

        case ROW_ICON_STOP:         // stop square
            row_icon_box(tft, x, y, 5, 1, 10, 10, SVW_RED);
            break;

        case ROW_ICON_CROSS:        // X
            for (int t = 0; t < 3; t++) {
                tft->drawLine(x + 9 + t, y + 1, x + 20 + t, y + 10, SVW_RED);
                tft->drawLine(x + 20 + t, y + 1, x + 9 + t, y + 10, SVW_RED);
            }
            break;

        case ROW_ICON_MACHINE:      // computer: case, keyboard area, red badge
            row_icon_box(tft, x, y, 1, 2, 14, 10, SVW_GRAY);
            row_icon_box(tft, x, y, 3, 5, 12, 8, SVW_DKGRAY);
            row_icon_box(tft, x, y, 3, 3, 5, 3, SVW_RED);
            break;

        case ROW_ICON_SERIAL:       // RS-232: D-sub connector with two rows of pins
            row_icon_box(tft, x, y, 0, 1, 15, 4, SVW_GRAY);
            row_icon_box(tft, x, y, 1, 5, 14, 7, SVW_GRAY);
            row_icon_box(tft, x, y, 2, 8, 13, 10, SVW_GRAY);
            for (int p = 0; p < 5; p++) row_icon_box(tft, x, y, 3 + p * 2, 3, 3 + p * 2, 4, SVW_BLACK);
            for (int p = 0; p < 4; p++) row_icon_box(tft, x, y, 4 + p * 2, 7, 4 + p * 2, 8, SVW_BLACK);
            break;

        case ROW_ICON_KEYBOARD:     // keyboard: case, two rows of keys, space bar
            row_icon_box(tft, x, y, 0, 1, 15, 10, SVW_DKGRAY);
            for (int r = 0; r < 2; r++) {
                for (int k = 0; k < 6; k++) {
                    row_icon_box(tft, x, y, 2 + k * 2, 3 + r * 2, 2 + k * 2, 3 + r * 2, SVW_WHITE);
                }
            }
            row_icon_box(tft, x, y, 4, 8, 11, 8, SVW_WHITE);
            break;

        case ROW_ICON_JOYSTICK:     // joystick: base, stick, red ball, fire button
            row_icon_box(tft, x, y, 2, 8, 13, 11, SVW_DKGRAY);
            row_icon_box(tft, x, y, 7, 3, 8, 8, fg);
            tft->fillEllipse(x + 16, y + 2, 10, 5, SVW_RED);
            row_icon_box(tft, x, y, 11, 7, 12, 7, SVW_RED);
            break;

        case ROW_ICON_WIFI:         // signal bars, rising left to right
            for (int b = 0; b < 4; b++) {
                row_icon_box(tft, x, y, 2 + b * 3, 9 - b * 3, 3 + b * 3, 11, fg);
            }
            break;

        case ROW_ICON_DRIVEWIRE:    // diskette on a wire
            row_icon_box(tft, x, y, 0, 0, 9, 11, SVW_RED);
            row_icon_box(tft, x, y, 2, 1, 7, 3, SVW_WHITE);
            row_icon_box(tft, x, y, 4, 7, 5, 9, SVW_BLACK);
            row_icon_box(tft, x, y, 10, 5, 15, 6, fg);
            break;

        case ROW_ICON_LANGUAGE:     // flag on a pole
            row_icon_box(tft, x, y, 3, 0, 3, 11, fg);
            row_icon_box(tft, x, y, 4, 1, 13, 7, SVW_WHITE);
            row_icon_box(tft, x, y, 4, 3, 13, 5, SVW_RED);
            break;

        case ROW_ICON_KEYCAP:       // single key: skirt and top
            row_icon_box(tft, x, y, 3, 0, 12, 11, SVW_DKGRAY);
            row_icon_box(tft, x, y, 5, 1, 10, 7, SVW_WHITE);
            break;

        case ROW_ICON_CHIP:         // chip with pins top and bottom
            for (int p = 0; p < 4; p++) {
                row_icon_box(tft, x, y, 4 + p * 2, 0, 4 + p * 2, 11, fg);
            }
            row_icon_box(tft, x, y, 2, 2, 13, 9, SVW_DKGRAY);
            row_icon_box(tft, x, y, 3, 4, 3, 7, SVW_WHITE);
            break;

        case ROW_ICON_PAGE:         // page with lines of text
            row_icon_box(tft, x, y, 2, 0, 13, 11, SVW_WHITE);
            for (int l = 0; l < 4; l++) {
                row_icon_box(tft, x, y, 4, 2 + l * 2, (l == 3) ? 8 : 11, 2 + l * 2, SVW_DKGRAY);
            }
            break;

        case ROW_ICON_SD:           // SD card: body with a cut corner and contacts
            row_icon_box(tft, x, y, 3, 0, 12, 11, SVW_ORANGE);
            row_icon_box(tft, x, y, 11, 0, 12, 1, bg);
            for (int c = 0; c < 3; c++) row_icon_box(tft, x, y, 4 + c * 2, 1, 4 + c * 2, 4, SVW_WHITE);
            break;

        case ROW_ICON_SPIDER: {     // spider: four legs each side, body, head, red eyes
            static const int8_t legs[4][4] = { { 6, 5, 1, 2 }, { 5, 6, 0, 6 }, { 5, 8, 0, 9 }, { 6, 9, 2, 11 } };
            for (int l = 0; l < 4; l++) {
                int lx1 = x + legs[l][0] * 2, lx2 = x + legs[l][2] * 2;
                int rx1 = x + 31 - legs[l][0] * 2, rx2 = x + 31 - legs[l][2] * 2;
                for (int t = 0; t < 2; t++) {       // two pixels thick
                    tft->drawLine(lx1 + t, y + legs[l][1], lx2 + t, y + legs[l][3], fg);
                    tft->drawLine(rx1 - t, y + legs[l][1], rx2 - t, y + legs[l][3], fg);
                }
            }
            row_icon_box(tft, x, y, 5, 4, 10, 10, fg);
            row_icon_box(tft, x, y, 6, 1, 9, 3, fg);
            row_icon_box(tft, x, y, 6, 2, 6, 2, SVW_RED);
            row_icon_box(tft, x, y, 9, 2, 9, 2, SVW_RED);
            break;
        }
    }
}

void sv_menu_draw_row_icon(int icon, int x, int y, uint16_t bg, uint16_t fg) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (tft) draw_row_icon(tft, icon, x, y, bg, fg);
}

// ============================================================
// Settings submenu and its Keyboard submenu
// ============================================================
//
// Both are icon lists in the wide green frame.
//
// Settings rows (SV_SettingsRow):
//   Machine      opens the machine-select submenu (CoCo 2 / CoCo 3)
//   RS-232 Pak   toggle; owns UART0 while on:
//                  ON  -> SERIAL_MODE_RS232  (forces the debug Echo Log off)
//                  OFF -> SERIAL_MODE_OFF
//                The Echo Log toggle that shares the port is in the Debug submenu.
//   Keyboard     opens the Keyboard submenu
//   Joy - Mouse Sensitivity, WiFi / Debug, DriveWire   open their own screens
//
// Keyboard rows (SV_KeyboardRow):
//   Keyboard Language   cycles the PS/2 layout (US English / Spanish Latam);
//                       applied live via FabGL setLayout(), persisted in NVS
//   Key Mapper          opens the Key Mapper screens (sv_keymap.cpp)

struct SV_ListRow {
    const char* label;
    uint8_t     icon;
};

static const SV_ListRow SETTINGS_ROWS[SV_SET_COUNT] = {
    { "Machine",                 ROW_ICON_MACHINE   },
    { "RS-232 Pak",              ROW_ICON_SERIAL    },
    { "Keyboard",                ROW_ICON_KEYBOARD  },
    { "Joy - Mouse Sensitivity", ROW_ICON_JOYSTICK  },
    { "WiFi / Debug",            ROW_ICON_WIFI      },
    { "DriveWire",               ROW_ICON_DRIVEWIRE },
};

static const SV_ListRow KEYBOARD_ROWS[SV_KBD_COUNT] = {
    { "Keyboard Language", ROW_ICON_LANGUAGE },
    { "Key Mapper",        ROW_ICON_KEYCAP   },
};

// Row last drawn as selected in whichever of the two lists is on screen;
// -1 = nothing on screen, draw the whole menu.
static int8_t s_settings_drawn = -1;

void sv_settings_invalidate(void) {
    s_settings_drawn = -1;
}

static void settings_activate(Supervisor_t* sv, int row) {
    switch (row) {
        case SV_SET_MACHINE:
            machine_select_open(sv);
            break;

        case SV_SET_RS232: {
            SerialPortMode next = (g_serial_mode == SERIAL_MODE_RS232) ? SERIAL_MODE_OFF
                                                                        : SERIAL_MODE_RS232;
            serial_mode_apply(next);
            supervisor_save_serial_mode(next);
            sv->needs_redraw = true;
            break;
        }

        case SV_SET_KEYBOARD:
            sv->prev_state = sv->state;
            sv->state = SV_KEYBOARD_MENU;
            sv->menu_cursor = 0;
            sv->needs_redraw = true;
            break;

        case SV_SET_JOYSTICK:  sv_joystick_open(sv); break;
        case SV_SET_WIFI:      sv_wifi_open(sv);     break;
        case SV_SET_DRIVEWIRE: sv_fujinet_open(sv);  break;
    }
}

static void keyboard_activate(Supervisor_t* sv, int row) {
    if (row == SV_KBD_MAPPER) {
        sv_keymap_open(sv);
        return;
    }
    // Keyboard Language — takes effect immediately, no reboot
    KbdLayout next = (g_kbd_layout == KBD_LAYOUT_US) ? KBD_LAYOUT_ES_LATAM
                                                     : KBD_LAYOUT_US;
    hal_keyboard_set_layout(next);
    supervisor_save_kbd_layout(next);
    sv->needs_redraw = true;
}

// Up/Down within a list of `count` rows.
static void list_move_cursor(Supervisor_t* sv, uint8_t hid_usage, int count) {
    if (hid_usage == HID_UP && sv->menu_cursor > 0) {
        sv->menu_cursor--;
        sv->needs_redraw = true;
    } else if (hid_usage == HID_DOWN && sv->menu_cursor < count - 1) {
        sv->menu_cursor++;
        sv->needs_redraw = true;
    }
}

void sv_settings_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    switch (hid_usage) {
        case HID_UP:
        case HID_DOWN:  list_move_cursor(sv, hid_usage, SV_SET_COUNT); break;
        case HID_ENTER: settings_activate(sv, sv->menu_cursor); break;
        case HID_ESC:   restore_main_menu(sv); break;
        case HID_F1:    supervisor_toggle(); break;
    }
}

void sv_keyboard_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    switch (hid_usage) {
        case HID_UP:
        case HID_DOWN:  list_move_cursor(sv, hid_usage, SV_KBD_COUNT); break;
        case HID_ENTER: keyboard_activate(sv, sv->menu_cursor); break;
        case HID_ESC:
            sv->state = SV_SETTINGS;
            sv->menu_cursor = SV_SET_KEYBOARD;
            sv->needs_redraw = true;
            break;
        case HID_F1:    supervisor_toggle(); break;
    }
}

// Current value shown right-aligned on a Settings row, or NULL.
static const char* settings_value(int row, char* buf, size_t size) {
    switch (row) {
        // Runtime-active machine, not the compile-time default.
        case SV_SET_MACHINE:   return (g_machine_type == 4) ? MACHINE_NAME_COCO3 : MACHINE_NAME_COCO2;
        case SV_SET_RS232:     return (g_serial_mode == SERIAL_MODE_RS232) ? "ON" : "OFF";
        case SV_SET_JOYSTICK:
            snprintf(buf, size, "%u", (unsigned)hal_joystick_get_sensitivity());
            return buf;
        case SV_SET_WIFI:      return wifi_mgr_state_str();
        case SV_SET_DRIVEWIRE: return dw_bus_mode_str(dw_bus_mode());
        default:               return NULL;
    }
}

// Shared renderer: first pass draws everything; afterwards the row left
// behind and the selected row (whose value may have just been toggled).
static void render_icon_list(Supervisor_t* sv, const char* title, const SV_ListRow* rows,
                             int count, bool keyboard_menu) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    int sel = sv->menu_cursor;
    if (sel < 0 || sel >= count) sel = sv->menu_cursor = 0;
    int y0 = SVW_LIST_Y + (SVW_LIST_ROWS - count) / 2 * SVW_ROW_H;
    bool all = (s_settings_drawn < 0);

    if (all) sv_render_wide_frame(title, "Up/Dn   ENTER Select   ESC Back   F3 Exit");
    for (int i = 0; i < count; i++) {
        if (!all && i != sel && i != s_settings_drawn) continue;
        bool hl = (i == sel);
        int y = y0 + i * SVW_ROW_H;
        char buf[12];
        const char* value = !keyboard_menu ? settings_value(i, buf, sizeof(buf))
                          : (i == SV_KBD_LANGUAGE) ? hal_keyboard_layout_name(g_kbd_layout)
                                                   : NULL;
        sv_render_wide_row(y, rows[i].label, value, hl);
        draw_row_icon(tft, rows[i].icon, SVW_ICON_X, y + 2,
                      hl ? SVW_DKBLUE : SVW_GREEN, hl ? SVW_WHITE : SVW_BLACK);
        if (hl) DEBUG_PRINTF("menu: %s > %s %s", title, rows[i].label, value ? value : "");
    }
    s_settings_drawn = sel;
}

void sv_settings_render(Supervisor_t* sv) {
    render_icon_list(sv, "Settings", SETTINGS_ROWS, SV_SET_COUNT, false);
}

void sv_keyboard_menu_render(Supervisor_t* sv) {
    render_icon_list(sv, "Keyboard", KEYBOARD_ROWS, SV_KBD_COUNT, true);
}

// ============================================================
// Debug submenu — pages, RAM dump, Echo Log toggle
// ============================================================
//
// Drawn in the wide green frame as an icon list. Rows 0..2 open the debug
// pages, the next row opens the "Dump RAM to SD" filename screen, and the
// last toggles the Echo Log: debug output on the serial port
// (SERIAL_MODE_DEBUG). It shares UART0 with the RS-232 Pak in Settings, so
// turning it on forces the Pak off.

static const char* const DEBUG_PAGE_LABELS[SV_DBG_PAGE_COUNT] = {
    "CPU / GIME Status",
    "Memory Hex Dump",
    "RS-232 Pak",
};

#define DEBUG_ROW_DUMP     SV_DBG_PAGE_COUNT
#define DEBUG_ROW_ECHO     (SV_DBG_PAGE_COUNT + 1)
#define DEBUG_MENU_COUNT   (SV_DBG_PAGE_COUNT + 2)
#define DEBUG_LIST_Y       (SVW_LIST_Y + (SVW_LIST_ROWS - DEBUG_MENU_COUNT) / 2 * SVW_ROW_H)

// Row last drawn as selected; -1 = nothing on screen, draw the whole menu.
static int8_t s_debug_drawn = -1;

void sv_debug_menu_invalidate(void) {
    s_debug_drawn = -1;
}

void sv_debug_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    switch (hid_usage) {
        case HID_UP:
            if (sv->menu_cursor > 0) {
                sv->menu_cursor--;
                sv->needs_redraw = true;
            }
            break;

        case HID_DOWN:
            if (sv->menu_cursor < DEBUG_MENU_COUNT - 1) {
                sv->menu_cursor++;
                sv->needs_redraw = true;
            }
            break;

        case HID_ENTER:
            if (sv->menu_cursor < SV_DBG_PAGE_COUNT) {
                sv_debug_set_page((SV_DebugPage)sv->menu_cursor);
                sv->prev_state = sv->state;
                sv->state = SV_DEBUG_DUMP;
                sv->needs_redraw = true;
            } else if (sv->menu_cursor == DEBUG_ROW_DUMP) {
                // Action row — open the "Dump RAM to SD" filename screen.
                sv_debug_begin_dump(sv);
            } else {
                // Echo Log — takes effect immediately, persisted in NVS.
                SerialPortMode next = (g_serial_mode == SERIAL_MODE_DEBUG) ? SERIAL_MODE_OFF
                                                                           : SERIAL_MODE_DEBUG;
                serial_mode_apply(next);
                supervisor_save_serial_mode(next);
                sv->needs_redraw = true;
            }
            break;

        case HID_ESC:
            restore_main_menu(sv);
            break;

        case HID_F1:
            supervisor_toggle();
            break;
    }
}

static void draw_debug_row(OSDCanvas* tft, int row, bool highlighted) {
    int y = DEBUG_LIST_Y + row * SVW_ROW_H;
    const char* label = (row < SV_DBG_PAGE_COUNT) ? DEBUG_PAGE_LABELS[row]
                      : (row == DEBUG_ROW_DUMP)   ? "Dump RAM to SD"
                                                  : "Echo Log";
    const char* value = (row != DEBUG_ROW_ECHO) ? NULL
                      : (g_serial_mode == SERIAL_MODE_DEBUG) ? "ON" : "OFF";

    sv_render_wide_row(y, label, value, highlighted);
    static const uint8_t icons[DEBUG_MENU_COUNT] = {
        ROW_ICON_CHIP, ROW_ICON_PAGE, ROW_ICON_SERIAL, ROW_ICON_SD, ROW_ICON_SPIDER
    };
    draw_row_icon(tft, icons[row], SVW_ICON_X, y + 2,
                    highlighted ? SVW_DKBLUE : SVW_GREEN,
                    highlighted ? SVW_WHITE : SVW_BLACK);
    if (highlighted) DEBUG_PRINTF("menu: Debug > %s %s", label, value ? value : "");
}

void sv_debug_menu_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    int sel = sv->menu_cursor;
    if (sel < 0 || sel >= DEBUG_MENU_COUNT) sel = sv->menu_cursor = 0;

    // First pass draws everything; afterwards the row left behind and the
    // selected row (whose value may have just been toggled).
    if (s_debug_drawn < 0) {
        sv_render_wide_frame("Debug", "Up/Dn   ENTER Select   ESC Back   F3 Exit");
        for (int i = 0; i < DEBUG_MENU_COUNT; i++) draw_debug_row(tft, i, i == sel);
    } else {
        if (s_debug_drawn != sel) draw_debug_row(tft, s_debug_drawn, false);
        draw_debug_row(tft, sel, true);
    }
    s_debug_drawn = sel;
}
