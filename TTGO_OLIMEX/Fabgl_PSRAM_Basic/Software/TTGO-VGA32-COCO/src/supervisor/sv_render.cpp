/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_render.cpp
 *  Module : OSD rendering engine — green phosphor UI using OSDCanvas built-in fonts
 * ============================================================
*/

/*
 * sv_render.cpp - OSD rendering engine
 *
 * Green phosphor aesthetic. Uses OSDCanvas built-in font 2 (16px).
 * Coordinates derived from DISPLAY_WIDTH/DISPLAY_HEIGHT in config.h.
 */

#include "sv_render.h"
#include <string.h>

static OSDCanvas* g_tft = nullptr;

void sv_render_init(OSDCanvas* tft) {
    g_tft = tft;
}

void sv_render_frame(const char* title, const char* footer) {
    if (!g_tft) return;

    g_tft->startWrite();

    // Fill background
    g_tft->fillRect(SV_BORDER_X, SV_BORDER_Y, SV_BORDER_W, SV_BORDER_H, SV_COLOR_BG);

    // Border
    g_tft->drawRect(SV_BORDER_X, SV_BORDER_Y, SV_BORDER_W, SV_BORDER_H, SV_COLOR_BORDER);

    // Title bar (inverse)
    g_tft->fillRect(SV_BORDER_X + 1, SV_BORDER_Y + 1, SV_BORDER_W - 2, SV_TITLE_H, SV_COLOR_TITLE_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(SV_COLOR_TITLE_TEXT, SV_COLOR_TITLE_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(title, SV_BORDER_X + SV_BORDER_W / 2, SV_BORDER_Y + 2);

    // Footer (smaller font)
    if (footer) {
        int footer_y = SV_BORDER_Y + SV_BORDER_H - SV_FOOTER_H - 1;
        g_tft->drawFastHLine(SV_BORDER_X + 1, footer_y, SV_BORDER_W - 2, SV_COLOR_DIM);
        g_tft->setTextFont(0);  // compact 6x8 — keeps long hint strings on one line
        g_tft->setTextColor(SV_COLOR_DIM, SV_COLOR_BG);
        g_tft->setTextDatum(TC_DATUM);
        g_tft->drawString(footer, SV_BORDER_X + SV_BORDER_W / 2, footer_y + 4);
    }

    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_menu_item(int index, const char* label, const char* value,
                         bool highlighted) {
    if (!g_tft) return;

    int y = SV_CONTENT_Y + index * SV_ITEM_H;
    int x = SV_CONTENT_X;

    g_tft->startWrite();

    if (highlighted) {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_BG);
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_BG);
    }

    g_tft->setTextFont(2);
    g_tft->setTextDatum(TL_DATUM);

    // Cursor glyph
    if (highlighted) {
        g_tft->drawString(">", x - 2, y + 2);
    }

    // Label
    g_tft->drawString(label, x + 10, y + 2);

    // Value (right-aligned)
    if (value) {
        g_tft->setTextDatum(TR_DATUM);
        g_tft->drawString(value, SV_VALUE_RIGHT, y + 2);
        g_tft->setTextDatum(TL_DATUM);
    }

    g_tft->endWrite();
}

void sv_render_scrollbar(int visible_start, int visible_count, int total_count, int base_row) {
    if (!g_tft || total_count <= visible_count) return;

    int sb_x = SV_BORDER_X + SV_BORDER_W - 4;
    int sb_y = SV_CONTENT_Y + base_row * SV_ITEM_H;
    int sb_h = visible_count * SV_ITEM_H;

    // Background
    g_tft->drawFastVLine(sb_x, sb_y, sb_h, SV_COLOR_BG);

    // Thumb
    int thumb_h = (visible_count * sb_h) / total_count;
    if (thumb_h < 4) thumb_h = 4;
    int thumb_y = sb_y + (visible_start * sb_h) / total_count;

    g_tft->drawFastVLine(sb_x, thumb_y, thumb_h, SV_COLOR_DIM);
}

void sv_render_status_line(const char* text, uint16_t color) {
    if (!g_tft) return;

    int y = SV_CONTENT_Y + 8 * SV_ITEM_H;
    g_tft->startWrite();
    g_tft->fillRect(SV_CONTENT_X, y, SV_CONTENT_W, SV_ITEM_H, SV_COLOR_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(color, SV_COLOR_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(text, SV_BORDER_X + SV_BORDER_W / 2, y + 2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_confirm_dialog(const char* message, bool yes_highlighted) {
    if (!g_tft) return;

    int dw = 240, dh = 80;  // 20% wider than the original 200px
    int dx = (DISPLAY_WIDTH - dw) / 2;
    int dy = (DISPLAY_HEIGHT - dh) / 2;

    g_tft->startWrite();
    g_tft->fillRect(dx, dy, dw, dh, SV_COLOR_DIALOG_BG);
    g_tft->drawRect(dx, dy, dw, dh, SV_COLOR_BORDER);

    g_tft->setTextFont(2);
    g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    g_tft->setTextDatum(TC_DATUM);

    // Render the message across up to two lines, splitting on '\n'.
    const char* nl = strchr(message, '\n');
    if (nl) {
        char line1[48];
        size_t len = (size_t)(nl - message);
        if (len >= sizeof(line1)) len = sizeof(line1) - 1;
        memcpy(line1, message, len);
        line1[len] = '\0';
        g_tft->drawString(line1, dx + dw / 2, dy + 6);
        g_tft->drawString(nl + 1, dx + dw / 2, dy + 22);
    } else {
        g_tft->drawString(message, dx + dw / 2, dy + 10);
    }

    // Yes button
    int btn_y = dy + 45;
    if (yes_highlighted) {
        g_tft->fillRect(dx + 20, btn_y, 60, 20, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    }
    g_tft->drawString("Yes", dx + 50, btn_y + 2);

    // No button
    if (!yes_highlighted) {
        g_tft->fillRect(dx + 120, btn_y, 60, 20, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    }
    g_tft->drawString("No", dx + 150, btn_y + 2);

    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_centered_item(int index, const char* text, uint16_t color) {
    if (!g_tft) return;
    int y = SV_CONTENT_Y + index * SV_ITEM_H;
    g_tft->startWrite();
    g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(color, SV_COLOR_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(text, SV_BORDER_X + SV_BORDER_W / 2, y + 2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_clear_content(void) {
    if (!g_tft) return;
    g_tft->fillRect(SV_BORDER_X + 2, SV_CONTENT_Y,
                    SV_BORDER_W - 4, SV_BORDER_H - SV_TITLE_H - SV_FOOTER_H - 6,
                    SV_COLOR_BG);
}

// ---- Joy - Mouse Sensitivity popup (shared by full draw + partial updates) ----
// A white popup window over the Settings list, in the theme colours: accent
// border and title bar, a green pad with a black cursor, accent bar segments.
static const int JOYWIN_W   = 360;
static const int JOYWIN_H   = 162;
static const int JOYPAD_W   = 192;   // pad box; ~square on 640x200 pixels
static const int JOYPAD_H   = 80;
static const int JOYPAD_CUR_W = 12;  // cursor block
static const int JOYPAD_CUR_H = 5;

static inline int joywin_x(void) { return (DISPLAY_WIDTH - JOYWIN_W) / 2; }
static inline int joywin_y(void) { return (DISPLAY_HEIGHT - JOYWIN_H) / 2; }
static inline int joypad_box_x(void) { return joywin_x() + (JOYWIN_W - JOYPAD_W) / 2; }
static inline int joypad_box_y(void) { return joywin_y() + 26; }

// Logical axis (0..63) -> pixel offset of the cursor block inside the box.
static inline int joypad_cur_dx(uint8_t v) {
    return 1 + (int)v * (JOYPAD_W - 2 - JOYPAD_CUR_W) / 63;
}
static inline int joypad_cur_dy(uint8_t v) {
    return 1 + (int)v * (JOYPAD_H - 2 - JOYPAD_CUR_H) / 63;
}

// Sensitivity bar + numeric level and the Invert-Y value. Repainted on its
// own when a value changes, so the window does not flicker.
void sv_render_joystick_values(uint8_t level, bool invert_y) {
    if (!g_tft) return;
    int left  = joywin_x() + 16;
    int right = joywin_x() + JOYWIN_W - 16;
    int row_y = joypad_box_y() + JOYPAD_H + 8;

    // Ten segment bar, filled up to `level`.
    const int SEG_W = 14, SEG_H = 10, GAP = 2;
    int bar_x = left + 104;
    for (int i = 0; i < 10; i++) {
        int x = bar_x + i * (SEG_W + GAP);
        g_tft->fillRect(x, row_y, SEG_W, SEG_H, (i < level) ? SVW_DKBLUE : SVW_WHITE);
        g_tft->drawRect(x, row_y, SEG_W, SEG_H, SVW_DKBLUE);
    }

    char text[8];
    g_tft->setTextFont(1);
    g_tft->setTextColor(SVW_DKBLUE, SVW_WHITE);
    g_tft->setTextDatum(TR_DATUM);
    snprintf(text, sizeof(text), "%2u", (unsigned)level);
    g_tft->drawString(text, right, row_y + 1);
    g_tft->drawString(invert_y ? " ON" : "OFF", right, row_y + 17);
    g_tft->setTextDatum(TL_DATUM);
}

// Full repaint of the popup: window, title, pad with the cursor at the
// current position, labels, values, key hints. Called once on open — NOT per
// frame. Afterwards the cursor moves incrementally through
// sv_render_joystick_cursor() and values through sv_render_joystick_values().
void sv_render_joystick_pad(uint8_t cursor_x, uint8_t cursor_y,
                            uint8_t level, bool invert_y) {
    if (!g_tft) return;

    int wx = joywin_x(), wy = joywin_y();
    int box_x = joypad_box_x(), box_y = joypad_box_y();

    g_tft->fillRect(wx, wy, JOYWIN_W, JOYWIN_H, SVW_WHITE);
    g_tft->drawRect(wx, wy, JOYWIN_W, JOYWIN_H, SVW_DKBLUE);
    g_tft->drawRect(wx + 1, wy + 1, JOYWIN_W - 2, JOYWIN_H - 2, SVW_DKBLUE);
    g_tft->fillRect(wx + 2, wy + 2, JOYWIN_W - 4, 18, SVW_DKBLUE);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->setTextFont(2);
    g_tft->setTextColor(SVW_WHITE, SVW_DKBLUE);
    g_tft->drawString("Joy - Mouse Sensitivity", wx + JOYWIN_W / 2, wy + 4);

    g_tft->setTextFont(0);
    g_tft->setTextColor(SVW_DKBLUE, SVW_WHITE);
    g_tft->drawString("Left/Right Sensitivity   Up/Dn Invert   ESC Save",
                      wx + JOYWIN_W / 2, wy + JOYWIN_H - 14);
    g_tft->setTextDatum(TL_DATUM);

    // Pad and the cursor block at the current position.
    g_tft->fillRect(box_x, box_y, JOYPAD_W, JOYPAD_H, SVW_GREEN);
    g_tft->drawRect(box_x, box_y, JOYPAD_W, JOYPAD_H, SVW_DKBLUE);
    g_tft->fillRect(box_x + joypad_cur_dx(cursor_x), box_y + joypad_cur_dy(cursor_y),
                    JOYPAD_CUR_W, JOYPAD_CUR_H, SVW_BLACK);

    int row_y = box_y + JOYPAD_H + 8;
    g_tft->setTextFont(1);
    g_tft->setTextColor(SVW_BLACK, SVW_WHITE);
    g_tft->drawString("Sensitivity:", wx + 16, row_y + 1);
    g_tft->drawString("Invert Y:", wx + 16, row_y + 17);

    sv_render_joystick_values(level, invert_y);
}

// Incremental cursor move: erase the cursor block at its old logical position
// and redraw it at the new one. The pad box border is never touched (the cursor
// stays >=1px inside), so no full repaint — and thus no flicker — is needed.
// Pass old_x/old_y > 63 (e.g. 0xFF) to skip the erase on the first draw.
void sv_render_joystick_cursor(uint8_t old_x, uint8_t old_y,
                               uint8_t new_x, uint8_t new_y) {
    if (!g_tft) return;
    int box_x = joypad_box_x();
    int box_y = joypad_box_y();

    if (old_x <= 63 && old_y <= 63) {
        g_tft->fillRect(box_x + joypad_cur_dx(old_x), box_y + joypad_cur_dy(old_y),
                        JOYPAD_CUR_W, JOYPAD_CUR_H, SVW_GREEN);
    }
    g_tft->fillRect(box_x + joypad_cur_dx(new_x), box_y + joypad_cur_dy(new_y),
                    JOYPAD_CUR_W, JOYPAD_CUR_H, SVW_BLACK);
}

// ============================================================
// Wide green frame — main menu and Disk Manager
// ============================================================

void sv_render_wide_frame(const char* title, const char* hint) {
    if (!g_tft) return;
    int cx = SVW_BOX_X + SVW_BOX_W / 2;
    g_tft->fillRect(SVW_BOX_X, SVW_BOX_Y, SVW_BOX_W, SVW_BOX_H, SVW_GREEN);
    g_tft->drawRect(SVW_BOX_X, SVW_BOX_Y, SVW_BOX_W, SVW_BOX_H, SVW_DKBLUE);
    g_tft->fillRect(SVW_BOX_X + 4, SVW_BOX_Y + 4, SVW_BOX_W - 8, SVW_STRIP_H, SVW_DKBLUE);
    g_tft->fillRect(SVW_BOX_X + 4, SVW_BOX_Y + SVW_BOX_H - 4 - SVW_STRIP_H, SVW_BOX_W - 8, SVW_STRIP_H, SVW_DKBLUE);

    g_tft->setTextColor(SVW_BLACK, SVW_GREEN);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->setTextFont(2);
    g_tft->drawString(title, cx, SVW_TITLE_Y);
    g_tft->setTextFont(0);
    g_tft->drawString(hint, cx, SVW_HINT_Y);
    g_tft->setTextDatum(TL_DATUM);
}

void sv_render_wide_clear(void) {
    if (g_tft) g_tft->fillRect(SVW_BOX_X, SVW_BOX_Y, SVW_BOX_W, SVW_BOX_H, OSD_BLACK);
}

void sv_render_wide_row(int y, const char* label, const char* value, bool highlighted) {
    if (!g_tft) return;
    uint16_t bg = highlighted ? SVW_DKBLUE : SVW_GREEN;
    g_tft->fillRect(SVW_LIST_X, y, SVW_LIST_W, SVW_ROW_H, bg);
    g_tft->setTextFont(2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->setTextColor(highlighted ? SVW_WHITE : SVW_BLACK, bg);
    g_tft->drawString(label, SVW_LIST_X + 56, y + 1);
    if (value) {
        g_tft->setTextColor(highlighted ? SVW_WHITE : SVW_DKBLUE, bg);
        g_tft->setTextDatum(TR_DATUM);
        g_tft->drawString(value, SVW_LIST_X + SVW_LIST_W - 12, y + 1);
        g_tft->setTextDatum(TL_DATUM);
    }
}

// ============================================================
// Popup window
// ============================================================

#define SVP_TITLE_H  18
#define SVP_X        ((DISPLAY_WIDTH - SVP_W) / 2)

int sv_render_popup(const char* title, const char* msg1, const char* msg2,
                    int count, bool draw_window) {
    const char* msgs[2] = { msg1 ? msg1 : "", msg2 ? msg2 : "" };
    int nmsg = (msgs[0][0] ? 1 : 0) + (msgs[1][0] ? 1 : 0);
    int body = SVP_TITLE_H + 8 + nmsg * 10 + (nmsg ? 4 : 0);
    int h = body + count * SVP_ROW_H + 6;
    int x = SVP_X, y = (DISPLAY_HEIGHT - h) / 2;
    if (!g_tft || !draw_window) return y + body;

    g_tft->fillRect(x, y, SVP_W, h, SVW_WHITE);
    g_tft->drawRect(x, y, SVP_W, h, SVW_DKBLUE);
    g_tft->drawRect(x + 1, y + 1, SVP_W - 2, h - 2, SVW_DKBLUE);
    g_tft->fillRect(x + 2, y + 2, SVP_W - 4, SVP_TITLE_H, SVW_DKBLUE);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->setTextFont(2);
    g_tft->setTextColor(SVW_WHITE, SVW_DKBLUE);
    g_tft->drawString(title, x + SVP_W / 2, y + 4);

    g_tft->setTextFont(1);
    g_tft->setTextColor(SVW_BLACK, SVW_WHITE);
    int my = y + SVP_TITLE_H + 8;
    for (int i = 0; i < 2; i++) {
        if (!msgs[i][0]) continue;
        g_tft->drawString(msgs[i], x + SVP_W / 2, my);
        my += 10;
    }
    g_tft->setTextDatum(TL_DATUM);
    return y + body;
}

void sv_render_popup_row(int y, const char* label, const char* value, bool highlighted) {
    if (!g_tft) return;
    int x = SVP_X;
    uint16_t bg = highlighted ? SVW_DKBLUE : SVW_WHITE;

    g_tft->fillRect(x + 4, y, SVP_W - 8, SVP_ROW_H, bg);
    g_tft->setTextFont(2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->setTextColor(highlighted ? SVW_WHITE : SVW_BLACK, bg);
    g_tft->drawString(label, x + 12, y + 1);
    if (value && value[0]) {
        g_tft->setTextColor(highlighted ? SVW_WHITE : SVW_DKBLUE, bg);
        g_tft->setTextDatum(TR_DATUM);
        g_tft->drawString(value, x + SVP_W - 12, y + 1);
        g_tft->setTextDatum(TL_DATUM);
    }
}
