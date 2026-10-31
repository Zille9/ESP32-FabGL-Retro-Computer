/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_render.h
 *  Module : OSD rendering engine interface
 * ============================================================
*/

/*
 * sv_render.h - OSD rendering engine for supervisor
 *
 * CoCo green phosphor aesthetic. Renders into the VGA framebuffer via OSDCanvas.
 */

#ifndef SV_RENDER_H
#define SV_RENDER_H

#include <stdint.h>
#include "../../config.h"
#include "../hal/osd_canvas.h"

// Color theme — blue
#define SV_COLOR_BG          0x0000  // black
#define SV_COLOR_TEXT         0xFFE0  //Yellow 
#define SV_COLOR_HIGHLIGHT   0x5DDF  // Bright blue-white bg
#define SV_COLOR_HL_TEXT     0xFFE0  // Yellow
#define SV_COLOR_DIM         0xFFFF  // white
#define SV_COLOR_BORDER      0x5DDF  // Bright blue-white
#define SV_COLOR_TITLE_BG    0x5DDF
#define SV_COLOR_TITLE_TEXT  0xFFFF // whithe 
#define SV_COLOR_DIR         0xFFE0  // Yellow
#define SV_COLOR_DISK        0x07FF  // Cyan
#define SV_COLOR_WARN        0xF800  // Red
#define SV_COLOR_DIALOG_BG   0x0000  // Black

// Layout constants — centered on display, derived from config.h
#include "../../config.h"

// OSD area matches the 256x192 VDG active area, centered in the 640x200 VGA frame.
#define SV_BORDER_W     256
#define SV_BORDER_H     192
#define SV_ITEM_H       18
#define SV_BORDER_X     ((DISPLAY_WIDTH - SV_BORDER_W) / 2)
#define SV_BORDER_Y     ((DISPLAY_HEIGHT - SV_BORDER_H) / 2)
#define SV_TITLE_H      18
#define SV_FOOTER_H     16
#define SV_CONTENT_X    (SV_BORDER_X + 8)
#define SV_CONTENT_Y    (SV_BORDER_Y + SV_TITLE_H + 4)
#define SV_CONTENT_W    (SV_BORDER_W - 16)
#define SV_VALUE_RIGHT  (SV_BORDER_X + SV_BORDER_W - 8)

// Wide green frame (main menu, Disk Manager) — CoCo boot-screen theme: green
// box, black text, dark blue accent. Wider than the submenu frame because
// 640x200 pixels are ~2.4x taller than wide.
#define SVW_GREEN    0x07E0
#define SVW_DIM      0x02A0  // dark green: dimmed text on the green box
#define SVW_BLACK    0x0000
#define SVW_DKBLUE   0x0015
#define SVW_WHITE    0xFFFF
#define SVW_GRAY     0xAD55
#define SVW_DKGRAY   0x52AA
#define SVW_RED      0xFAAA
#define SVW_ORANGE   0xFD40
#define SVW_LEAF     0x0540
#define SVW_TEAL     0x054A
#define SVW_PURPLE   0xAAB5

#define SVW_BOX_W    560
#define SVW_BOX_H    SV_BORDER_H
#define SVW_BOX_X    ((DISPLAY_WIDTH - SVW_BOX_W) / 2)
#define SVW_BOX_Y    SV_BORDER_Y
#define SVW_STRIP_H  6
#define SVW_TITLE_Y  (SVW_BOX_Y + 14)
#define SVW_HINT_Y   (SVW_BOX_Y + SVW_BOX_H - 26)

void sv_render_init(OSDCanvas* canvas);

// Green box, accent strips top and bottom, black title and key hints.
void sv_render_wide_frame(const char* title, const char* hint);
// Blank the wide box (before a narrow submenu draws, or on close).
void sv_render_wide_clear(void);

// List rows inside the wide frame: 16-pixel rows, a 32x12 icon slot, label on
// the left, value right-aligned in the accent colour. The selected row is a
// full-width accent bar with white text. The caller draws the icon afterwards
// at (SVW_ICON_X, y + 2), where y is the row top passed here.
#define SVW_ROW_H    16
#define SVW_LIST_X   (SVW_BOX_X + 8)
#define SVW_LIST_W   (SVW_BOX_W - 16)
#define SVW_LIST_Y   (SVW_BOX_Y + 36)
#define SVW_LIST_ROWS ((SVW_HINT_Y - 4 - SVW_LIST_Y) / SVW_ROW_H)
#define SVW_ICON_X   (SVW_LIST_X + 12)
void sv_render_wide_row(int y, const char* label, const char* value, bool highlighted);

// Popup: a small white window centred over the current screen, double accent
// border, accent title bar with white text, up to two message lines (NULL or
// "" to skip), then `count` option rows. sv_render_popup() returns the y of
// the first option row; with draw_window false it only computes that y, for
// repainting single rows. The caller draws the rows and repaints the screen
// underneath when the popup closes.
#define SVP_W        360
#define SVP_ROW_H    16
#define SVP_MSG_COLS 40
int  sv_render_popup(const char* title, const char* msg1, const char* msg2,
                     int count, bool draw_window);
void sv_render_popup_row(int y, const char* label, const char* value, bool highlighted);
// Same popup at another width, for messages longer than SVP_MSG_COLS.
int  sv_render_popup_w(int w, const char* title, const char* msg1, const char* msg2,
                       int count, bool draw_window);
void sv_render_popup_row_w(int w, int y, const char* label, const char* value, bool highlighted);

void sv_render_frame(const char* title, const char* footer);
void sv_render_menu_item(int index, const char* label, const char* value,
                         bool highlighted);
void sv_render_scrollbar(int visible_start, int visible_count, int total_count, int base_row = 0);

void sv_render_status_line(const char* text, uint16_t color);
void sv_render_confirm_dialog(const char* message, bool yes_highlighted);
void sv_render_clear_content(void);
void sv_render_centered_item(int index, const char* text, uint16_t color);

// Joy - Mouse Sensitivity popup, drawn over the Settings list in the theme
// colours. sv_render_joystick_pad draws the whole window: title, pad with a
// live cursor at (cursor_x, cursor_y) in 0..63 logical space, the sensitivity
// bar (level 1..10), the Invert-Y value and the key hints.
void sv_render_joystick_pad(uint8_t cursor_x, uint8_t cursor_y,
                            uint8_t level, bool invert_y);

// Repaints only the sensitivity bar, level and Invert-Y value.
void sv_render_joystick_values(uint8_t level, bool invert_y);

// Incremental cursor move for the pad: erases the cursor at its old logical
// position (0..63) and redraws it at the new one, without repainting the
// window (avoids flicker). Pass old coords > 63 to skip the erase on the
// first draw.
void sv_render_joystick_cursor(uint8_t old_x, uint8_t old_y,
                               uint8_t new_x, uint8_t new_y);

#endif // SV_RENDER_H
