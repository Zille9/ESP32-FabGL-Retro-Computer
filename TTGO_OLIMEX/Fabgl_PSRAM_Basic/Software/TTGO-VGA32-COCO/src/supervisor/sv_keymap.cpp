/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_keymap.cpp
 *  Module : Key Mapper UI — remap physical keys to CoCo characters
 * ============================================================
*/

/*
 * Screens (reached from Settings -> Keyboard -> Key Mapper):
 *   SV_KEYMAP_LIST    — a picture of the running machine's keyboard (CoCo 2
 *                       or CoCo 3 layout) in the wide green frame. The arrow
 *                       keys move over it; ENTER remaps the selected key.
 *                       Keys carrying two remappable characters (":" / "*")
 *                       ask which one first. T opens the test screen, C
 *                       clears every mapping. Choice and confirmation popups
 *                       are a sub-state of this screen.
 *   SV_KEYMAP_CAPTURE — popup that waits for the next raw keypress and binds
 *                       it after a confirmation (DEL clears, ESC cancels).
 *   SV_KEYMAP_TEST    — shows what each pressed key would type (nothing is
 *                       injected into the CoCo).
 *
 * Only the characters in hal_keyboard's remap table can be remapped (symbols,
 * arrows and, on a CoCo 3, ALT/CTRL/CLEAR/F1/F2); the other keys are drawn
 * cream with grey legends. Bindings persist via supervisor_save_keymap() (NVS "sv"/"keymap") on
 * every change.
 */

#include "sv_keymap.h"
#include "sv_menu.h"
#include "sv_render.h"
#include "../hal/hal.h"
#include "fabgl.h"
#include <esp_heap_caps.h>
#include "../utils/debug.h"

extern OSDCanvas* hal_video_get_canvas(void);

// HID usage codes (keyboard screen comes through the normal supervisor route)
#define HID_UP    0x52
#define HID_DOWN  0x51
#define HID_LEFT  0x50
#define HID_RIGHT 0x4F
#define HID_ENTER 0x28
#define HID_ESC   0x29
#define HID_F1    0x3A
#define HID_C     0x06
#define HID_T     0x17

// ============================================================
// Keyboard picture
// ============================================================
//
// Key style and the CoCo 2 geometry follow the CoCo2Keyboard on-screen
// keyboard (CoCo2-CYD project): silver case, white caps with a shadow edge,
// red BREAK, triangle arrow glyphs, the shifted character small above the
// main one, and every row flush left. Its 320-pixel layout is stretched 1.5x
// horizontally for the 640x200 screen.

#define KB_KEY_H    17
#define KB_PITCH_Y  19
#define KB_ROWS     5
#define KB_Y        (SVW_BOX_Y + 34)
#define KB_INFO_Y   (KB_Y + KB_ROWS * KB_PITCH_Y + 8)
#define KB_MAX_KEYS 60

#define KB_CASE     SVW_GRAY    // silver-gray case body
#define KB_CAP      SVW_WHITE   // remappable key cap
#define KB_CAP_FIX  0xFFF5      // cream: key with nothing to remap
#define KB_BREAK    0xF800      // red BREAK key

enum { KB_GLYPH_NONE, KB_GLYPH_UP, KB_GLYPH_DOWN, KB_GLYPH_LEFT, KB_GLYPH_RIGHT };

// One key cap: legends, position inside the keyboard, and up to two entries
// of hal_keyboard's remap table (-1 = none), compacted into km[].
struct KbKey {
    char    label[6];       // main legend
    char    shift;          // shifted character drawn above it, 0 = none
    uint8_t glyph;          // arrow drawn instead of the label
    bool    red;            // BREAK
    bool    main_map;       // the main legend is remappable
    bool    shift_map;      // the shifted character is remappable
    int16_t x;
    int16_t w;
    uint8_t row;
    int8_t  km[2];
};

static KbKey*  s_keys = nullptr;       // PSRAM, allocated on first use
static uint8_t s_key_count = 0;
static int16_t s_kb_width = 0;
static bool    s_kb_coco3 = false;
// Layout coordinates are scaled by s_kb_scale / 20 into pixels: 30 = the
// reference CoCo 2 layout stretched 1.5x, 22 = the CoCo 3 layout at 1.1x.
static uint8_t s_kb_scale = 20;

static inline int kb_px(int v) { return v * s_kb_scale / 20; }

static KbKey* kb_add(const char* label, int x, int row, int w,
                     int km_main = -1, int km_shift = -1, char shift = 0) {
    if (s_key_count >= KB_MAX_KEYS) return &s_keys[KB_MAX_KEYS - 1];
    // CoCo 3-only entries are not remappable on a CoCo 2.
    int limit = s_kb_coco3 ? KM_COUNT : KM_COCO2_COUNT;
    if (km_main >= limit)  km_main = -1;
    if (km_shift >= limit) km_shift = -1;

    KbKey* k = &s_keys[s_key_count++];
    snprintf(k->label, sizeof(k->label), "%s", label);
    k->shift = shift;
    k->glyph = KB_GLYPH_NONE;
    k->red = false;
    k->main_map = (km_main >= 0);
    k->shift_map = (km_shift >= 0);
    k->x = (int16_t)kb_px(x);
    k->row = (uint8_t)row;
    k->w = (int16_t)(kb_px(x + w) - kb_px(x));
    k->km[0] = (int8_t)(km_main >= 0 ? km_main : km_shift);
    k->km[1] = (int8_t)(km_main >= 0 ? km_shift : -1);
    if (k->x + k->w > s_kb_width) s_kb_width = (int16_t)(k->x + k->w);
    return k;
}

static void kb_add_arrow(uint8_t glyph, int x, int row, int w) {
    static const char* const name[] = { "", "UP", "DOWN", "LEFT", "RIGHT" };
    static const int8_t km[] = { -1, 22, 23, 24, 25 };
    kb_add(name[glyph], x, row, w, km[glyph])->glyph = glyph;
}

// A run of single-character keys (letters), none remappable.
static void kb_add_run(const char* chars, int x, int row, int pitch, int w) {
    for (; *chars; chars++, x += pitch) {
        char label[2] = { *chars, '\0' };
        kb_add(label, x, row, w);
    }
}

// Number row: the digit is fixed, the shifted symbol is remappable;
// ":" / "*" and "-" / "=" are remappable both ways.
static void kb_add_number_row(int x, int pitch, int w) {
    static const char main_ch[] = "1234567890:-";
    static const char shift_ch[] = "!\"#$%&'()\0*=";
    static const int8_t km_main[12]  = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 9, 11 };
    static const int8_t km_shift[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, 10, 12 };
    for (int i = 0; i < 12; i++, x += pitch) {
        char label[2] = { main_ch[i], '\0' };
        kb_add(label, x, 0, w, km_main[i], km_shift[i], shift_ch[i]);
    }
}

// Lay out the running machine's keyboard. Remap-table indices: symbols 0-21,
// UP 22, DOWN 23, LEFT 24, RIGHT 25, ALT 26, CTRL 27, CLEAR 28, F1 29, F2 30.
static bool kb_build(void) {
    s_kb_coco3 = (g_machine_type == 4);
    s_key_count = 0;
    s_kb_width = 0;
    if (!s_keys) {
        s_keys = (KbKey*)heap_caps_malloc(KB_MAX_KEYS * sizeof(KbKey), MALLOC_CAP_SPIRAM);
        if (!s_keys) {
            DEBUG_PRINT("keymap: failed to allocate the keyboard layout");
            return false;
        }
    }

    if (s_kb_coco3) {
        // CoCo 3: 57 keys, measured from a photo of the real keyboard.
        // Staggered rows; ALT/CTRL left of Q/A; BREAK top right; the arrows
        // in a diamond down the right edge (UP, LEFT+RIGHT, DOWN) with F1/F2
        // below them beside the space bar. 30 units per key step.
        s_kb_scale = 22;
        kb_add_number_row(8, 30, 28);
        kb_add("BRK", 400, 0, 36)->red = true;
        kb_add("ALT", 0, 1, 28, 26);
        kb_add_run("QWERTYUIOP", 30, 1, 30, 28);
        kb_add("@", 330, 1, 28, 13);
        kb_add("CLR", 362, 1, 28, 28);
        kb_add_arrow(KB_GLYPH_UP, 393, 1, 28);
        kb_add("CTRL", 8, 2, 28, 27);
        kb_add_run("ASDFGHJKL", 38, 2, 30, 28);
        kb_add(";", 308, 2, 28, 15, 14, '+');
        kb_add("ENTER", 338, 2, 36);
        kb_add_arrow(KB_GLYPH_LEFT, 378, 2, 28);
        kb_add_arrow(KB_GLYPH_RIGHT, 408, 2, 28);
        kb_add("SHIFT", 16, 3, 34);
        kb_add_run("ZXCVBNM", 52, 3, 30, 28);
        kb_add(",", 262, 3, 28, 18, 16, '<');
        kb_add(".", 292, 3, 28, 19, 17, '>');
        kb_add("/", 322, 3, 28, 21, 20, '?');
        kb_add("SHIFT", 354, 3, 34);
        kb_add_arrow(KB_GLYPH_DOWN, 393, 3, 28);
        kb_add("SPACE", 80, 4, 238);
        kb_add("F1", 374, 4, 28, 29);
        kb_add("F2", 408, 4, 28, 30);
    } else {
        // CoCo 1/2: 53 keys in the reference layout (units of its 320-pixel
        // screen, 1-unit gaps, every row 307 wide): UP/DOWN at the left of
        // the Q and A rows, LEFT/RIGHT after @, wide ENTER then CLEAR.
        s_kb_scale = 30;
        kb_add_number_row(0, 23, 22);
        kb_add("BRK", 276, 0, 31)->red = true;
        kb_add_arrow(KB_GLYPH_UP, 0, 1, 21);
        kb_add_run("QWERTYUIOP", 22, 1, 22, 21);
        kb_add("@", 242, 1, 21, 13);
        kb_add_arrow(KB_GLYPH_LEFT, 264, 1, 21);
        kb_add_arrow(KB_GLYPH_RIGHT, 286, 1, 21);
        kb_add_arrow(KB_GLYPH_DOWN, 0, 2, 21);
        kb_add_run("ASDFGHJKL", 22, 2, 22, 21);
        kb_add(";", 220, 2, 21, 15, 14, '+');
        kb_add("ENTER", 242, 2, 36);
        kb_add("CLR", 279, 2, 28, 28);
        kb_add("SHIFT", 0, 3, 43);
        kb_add_run("ZXCVBNM", 44, 3, 22, 21);
        kb_add(",", 198, 3, 21, 18, 16, '<');
        kb_add(".", 220, 3, 21, 19, 17, '>');
        kb_add("/", 242, 3, 21, 21, 20, '?');
        kb_add("SHIFT", 264, 3, 43);
        kb_add("SPACE", 58, 4, 190);
    }
    return true;
}

static inline int kb_origin_x(void) {
    return SVW_BOX_X + (SVW_BOX_W - s_kb_width) / 2;
}

// ============================================================
// Screen state
// ============================================================

static int8_t  km_sel = 0;              // selected key on the keyboard picture
static int8_t  km_drawn = -1;           // key drawn as selected; -1 = repaint all
static uint8_t km_capture_idx = 0;      // remap-table entry being captured
static int16_t km_pending_vk = -1;      // key awaiting user confirmation
// Last two test-screen results, newest in [1].
static char km_test_line[2][36];
static bool km_test_frame_drawn = false;

// Popups over the keyboard: which character of a two-character key, and the
// No / Yes confirmations (both default to No).
enum KmPopup : uint8_t { KM_POP_NONE, KM_POP_CHOOSE, KM_POP_CONFIRM_MAP, KM_POP_CONFIRM_CLEAR };
static KmPopup km_pop = KM_POP_NONE;
static int8_t  km_pop_sel = 0;
static int8_t  km_pop_drawn = -1;       // -1 = draw the whole window
static char    km_pop_msg[SVP_MSG_COLS + 1];

void sv_keymap_invalidate(void) {
    km_drawn = -1;
    km_pop_drawn = -1;
    km_test_frame_drawn = false;
}

// FabGL 1.0.9 bug guard: virtualKeyToString()'s internal VKTOSTR[] table has
// only 222 entries while the VirtualKey enum has 250 — keys added later
// (VK_TILDE_n = 223 for ñ, accented vowels, ...) index past the table and
// dereferencing the garbage pointer crashes with LoadProhibited.
#define FABGL_VK_NAME_COUNT 222

// FabGL key name without the "VK_" prefix, e.g. "HASH", "F7"; keys beyond
// the library's name table render as "KEY <n>".
static const char* vk_short_name(int16_t vk) {
    static char numbuf[12];
    if (vk <= 0 || vk >= FABGL_VK_NAME_COUNT) {
        snprintf(numbuf, sizeof(numbuf), "KEY %d", vk);
        return numbuf;
    }
    const char* n = fabgl::Keyboard::virtualKeyToString((fabgl::VirtualKey)vk);
    return (n && n[0] == 'V' && n[1] == 'K' && n[2] == '_') ? n + 3 : (n ? n : "?");
}

// Name of the physical key bound to a remap-table entry, or "default".
static const char* km_binding_name(int idx) {
    int16_t vk = hal_keyboard_remap_table()[idx];
    return (vk > 0) ? vk_short_name(vk) : "default";
}

void sv_keymap_open(Supervisor_t* sv) {
    if (!kb_build()) return;    // out of memory: stay on the Keyboard menu
    // Start on the first remappable key.
    km_sel = 0;
    for (int i = 0; i < s_key_count; i++) {
        if (s_keys[i].km[0] >= 0) { km_sel = (int8_t)i; break; }
    }
    km_pop = KM_POP_NONE;
    sv_keymap_invalidate();
    sv->prev_state = sv->state;
    sv->state = SV_KEYMAP_LIST;
    sv->needs_redraw = true;
}

static void km_back_to_settings(Supervisor_t* sv) {
    sv->state = SV_KEYBOARD_MENU;
    sv->menu_cursor = SV_KBD_MAPPER;
    sv->needs_redraw = true;
}

static void km_popup_open(Supervisor_t* sv, KmPopup kind, int sel) {
    km_pop = kind;
    km_pop_sel = (int8_t)sel;
    km_pop_drawn = -1;
    sv->needs_redraw = true;
}

// Closing a popup repaints the keyboard underneath.
static void km_popup_close(Supervisor_t* sv) {
    km_pop = KM_POP_NONE;
    sv_keymap_invalidate();
    sv->needs_redraw = true;
}

static void km_start_capture(Supervisor_t* sv, int idx) {
    km_capture_idx = (uint8_t)idx;
    km_pop = KM_POP_NONE;
    sv->state = SV_KEYMAP_CAPTURE;
    sv->needs_redraw = true;
}

// Move to the nearest key in the row above/below, or left/right in the row.
static void kb_move(Supervisor_t* sv, int dx, int dy) {
    const KbKey* cur = &s_keys[km_sel];
    int centre = cur->x + cur->w / 2;
    int best = -1, best_dist = 0;

    for (int i = 0; i < s_key_count; i++) {
        const KbKey* k = &s_keys[i];
        int kc = k->x + k->w / 2;
        int dist;
        if (dy != 0) {
            if ((int)k->row != (int)cur->row + dy) continue;
            dist = abs(kc - centre);
        } else {
            if (k->row != cur->row || (kc - centre) * dx <= 0) continue;
            dist = abs(kc - centre);
        }
        if (best < 0 || dist < best_dist) { best = i; best_dist = dist; }
    }
    if (best >= 0) {
        km_sel = (int8_t)best;
        sv->needs_redraw = true;
    }
}

static void km_popup_on_key(Supervisor_t* sv, uint8_t hid_usage) {
    switch (hid_usage) {
        case HID_UP:
            if (km_pop_sel > 0) { km_pop_sel--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (km_pop_sel < 1) { km_pop_sel++; sv->needs_redraw = true; }
            break;
        case HID_ESC:
            km_popup_close(sv);
            break;
        case HID_ENTER:
            if (km_pop == KM_POP_CHOOSE) {
                km_start_capture(sv, s_keys[km_sel].km[km_pop_sel]);
                break;
            }
            if (km_pop_sel == 1) {      // Yes
                if (km_pop == KM_POP_CONFIRM_MAP) {
                    // Applied live — no machine reset needed.
                    hal_keyboard_remap_set(km_capture_idx, km_pending_vk);
                } else {
                    hal_keyboard_remap_clear_all();
                }
                supervisor_save_keymap();
            }
            km_popup_close(sv);
            break;
    }
}

void sv_keymap_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    // F1 closes the whole supervisor from any level.
    if (hid_usage == HID_F1) {
        km_pop = KM_POP_NONE;
        supervisor_toggle();
        return;
    }
    if (km_pop != KM_POP_NONE) {
        km_popup_on_key(sv, hid_usage);
        return;
    }

    switch (hid_usage) {
        case HID_UP:    kb_move(sv, 0, -1); break;
        case HID_DOWN:  kb_move(sv, 0, 1);  break;
        case HID_LEFT:  kb_move(sv, -1, 0); break;
        case HID_RIGHT: kb_move(sv, 1, 0);  break;

        case HID_ENTER: {
            const KbKey* k = &s_keys[km_sel];
            if (k->km[0] < 0) break;                    // not remappable
            if (k->km[1] < 0) km_start_capture(sv, k->km[0]);
            else              km_popup_open(sv, KM_POP_CHOOSE, 0);
            break;
        }

        case HID_T:
            km_test_line[0][0] = '\0';
            km_test_line[1][0] = '\0';
            sv->state = SV_KEYMAP_TEST;
            sv->needs_redraw = true;
            break;

        case HID_C:
            snprintf(km_pop_msg, sizeof(km_pop_msg), "Clear all custom key mappings?");
            km_popup_open(sv, KM_POP_CONFIRM_CLEAR, 0);
            break;

        case HID_ESC:
            km_back_to_settings(sv);
            break;
    }
}

// ============================================================
// Raw-VK path — capture and test screens
// ============================================================

bool sv_keymap_wants_raw_vk(void) {
    SV_State st = supervisor_get()->state;
    return st == SV_KEYMAP_CAPTURE || st == SV_KEYMAP_TEST;
}

// Keys that may not become custom bindings: SHIFT is needed to form CoCo
// combos, and F3-F6 are host hotkeys consumed before the mapping tables.
static bool km_vk_bindable(int16_t vk) {
    switch ((fabgl::VirtualKey)vk) {
        case fabgl::VK_LSHIFT:
        case fabgl::VK_RSHIFT:
        case fabgl::VK_F3:
        case fabgl::VK_F4:
        case fabgl::VK_F5:
        case fabgl::VK_F6:
            return false;
        default:
            return vk > 0;
    }
}

void sv_keymap_on_raw_vk(int16_t vk, bool pressed) {
    Supervisor_t* sv = supervisor_get();
    if (!pressed) return;

    if (sv->state == SV_KEYMAP_CAPTURE) {
        if (vk == (int16_t)fabgl::VK_ESCAPE) {
            sv->state = SV_KEYMAP_LIST;
            sv->needs_redraw = true;
            return;
        }
        if (vk == (int16_t)fabgl::VK_DELETE || vk == (int16_t)fabgl::VK_KP_DELETE) {
            hal_keyboard_remap_set(km_capture_idx, -1);
            supervisor_save_keymap();
            sv->state = SV_KEYMAP_LIST;
            sv->needs_redraw = true;
            return;
        }
        if (!km_vk_bindable(vk)) return;  // ignore SHIFT / host hotkeys
        // Ask for confirmation before applying. The popup runs through the
        // normal HID key route (wants_raw_vk is false for SV_KEYMAP_LIST).
        km_pending_vk = vk;
        snprintf(km_pop_msg, sizeof(km_pop_msg), "Map  %s  to key  %s ?",
                 hal_keyboard_remap_label(km_capture_idx), vk_short_name(vk));
        sv->state = SV_KEYMAP_LIST;
        km_popup_open(sv, KM_POP_CONFIRM_MAP, 0);
        return;
    }

    // SV_KEYMAP_TEST
    if (vk == (int16_t)fabgl::VK_ESCAPE) {
        sv->state = SV_KEYMAP_LIST;
        sv->needs_redraw = true;
        return;
    }
    char desc[24];
    hal_keyboard_describe_vk(vk, desc, sizeof(desc));
    memcpy(km_test_line[0], km_test_line[1], sizeof(km_test_line[0]));
    snprintf(km_test_line[1], sizeof(km_test_line[1]), "%s -> %s",
             vk_short_name(vk), desc);
    sv->needs_redraw = true;
}

// ============================================================
// Renderers
// ============================================================

// A key has a custom binding when any of its remappable characters has one.
static bool kb_key_is_custom(const KbKey* k) {
    for (int i = 0; i < 2; i++) {
        if (k->km[i] >= 0 && hal_keyboard_remap_table()[k->km[i]] > 0) return true;
    }
    return false;
}

// Triangle arrow glyph centred on (cx, cy), built from stacked rectangles.
static void kb_draw_arrow(OSDCanvas* tft, uint8_t glyph, int cx, int cy, uint16_t color) {
    if (glyph == KB_GLYPH_UP || glyph == KB_GLYPH_DOWN) {
        for (int r = 0; r < 6; r++) {       // 6 rows, 2..22 pixels wide
            int half = (glyph == KB_GLYPH_UP) ? 1 + r * 2 : 11 - r * 2;
            tft->fillRect(cx - half, cy - 3 + r, half * 2, 1, color);
        }
    } else {
        for (int r = -4; r <= 4; r++) {     // 9 rows, tip pointing sideways
            int len = (4 - abs(r)) * 4 + 2;
            int x = (glyph == KB_GLYPH_LEFT) ? cx + 8 - len : cx - 8;
            tft->fillRect(x, cy + r, len, 1, color);
        }
    }
}

// Key caps: white = remappable, cream = fixed, orange = has a custom
// binding, red = BREAK, accent with white legends = selected. A legend is
// black when that character can be remapped and grey when it cannot.
static void kb_draw_key(OSDCanvas* tft, int idx, bool selected) {
    const KbKey* k = &s_keys[idx];
    bool remappable = (k->km[0] >= 0);
    uint16_t bg = selected ? SVW_DKBLUE
                : k->red ? KB_BREAK
                : kb_key_is_custom(k) ? SVW_ORANGE
                : remappable ? KB_CAP : KB_CAP_FIX;
    bool light = selected || k->red;        // white legends on a dark cap
    uint16_t fg_main  = light ? SVW_WHITE : (k->main_map ? SVW_BLACK : SVW_DKGRAY);
    uint16_t fg_shift = light ? SVW_WHITE : (k->shift_map ? SVW_BLACK : SVW_DKGRAY);
    int x = kb_origin_x() + k->x;
    int y = KB_Y + k->row * KB_PITCH_Y;
    int cx = x + k->w / 2;

    tft->fillRect(x, y, k->w, KB_KEY_H, bg);
    // shadow edge along the bottom and right, as on the reference keyboard
    tft->drawFastHLine(x, y + KB_KEY_H - 1, k->w, SVW_DKGRAY);
    tft->drawFastVLine(x + k->w - 1, y, KB_KEY_H, SVW_DKGRAY);

    if (k->glyph != KB_GLYPH_NONE) {
        kb_draw_arrow(tft, k->glyph, cx, y + 8, fg_main);
        return;
    }
    tft->setTextFont(0);
    tft->setTextDatum(TC_DATUM);
    if (k->shift) {
        // shifted character on top, main character below
        char sh[2] = { k->shift, '\0' };
        tft->setTextColor(fg_shift, bg);
        tft->drawString(sh, cx, y);
        tft->setTextColor(fg_main, bg);
        tft->drawString(k->label, cx, y + 8);
    } else {
        tft->setTextColor(fg_main, bg);
        tft->drawString(k->label, cx, y + 4);
    }
    tft->setTextDatum(TL_DATUM);
}

// Silver case behind the keys, with the RGB dots and machine name of the
// reference keyboard's badge beside the space bar.
static void kb_draw_case(OSDCanvas* tft) {
    int x = kb_origin_x();
    tft->fillRect(x - 8, KB_Y - 4, s_kb_width + 16, KB_ROWS * KB_PITCH_Y + 6, KB_CASE);
    tft->drawRect(x - 8, KB_Y - 4, s_kb_width + 16, KB_ROWS * KB_PITCH_Y + 6, SVW_DKGRAY);

    int by = KB_Y + 4 * KB_PITCH_Y + 5;
    // CoCo 3 has F1/F2 right of the space bar, so its badge goes on the left.
    int bx = s_kb_coco3 ? x + 6 : x + s_kb_width - 76;
    tft->fillRect(bx,      by + 1, 8, 5, 0xF800);
    tft->fillRect(bx + 10, by + 1, 8, 5, 0x07E0);
    tft->fillRect(bx + 20, by + 1, 8, 5, 0x001F);
    tft->setTextFont(0);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(SVW_BLACK, KB_CASE);
    tft->drawString(s_kb_coco3 ? "CoCo 3" : "CoCo 2", bx + 34, by);
}

// Two lines under the keyboard: what the selected key can be remapped to.
static void kb_draw_info(OSDCanvas* tft) {
    const KbKey* k = &s_keys[km_sel];
    char line[2][52];
    line[1][0] = '\0';
    if (k->km[0] < 0) {
        snprintf(line[0], sizeof(line[0]), "%s  cannot be remapped", k->label);
    } else {
        for (int i = 0; i < 2 && k->km[i] >= 0; i++) {
            snprintf(line[i], sizeof(line[i]), "%-11s -> %s",
                     hal_keyboard_remap_label(k->km[i]), km_binding_name(k->km[i]));
        }
    }
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(k->km[0] < 0 ? SVW_DIM : SVW_BLACK, SVW_GREEN);
    for (int i = 0; i < 2; i++) {
        char padded[52];
        snprintf(padded, sizeof(padded), "%-50s", line[i]);
        tft->drawString(padded, kb_origin_x(), KB_INFO_Y + i * 11);
    }
    DEBUG_PRINTF("keymap: [%s] %s %s", k->label, line[0], line[1]);
}

static void km_draw_popup(void) {
    bool choose = (km_pop == KM_POP_CHOOSE);
    const char* title = choose ? "Remap which character?"
                      : (km_pop == KM_POP_CONFIRM_MAP) ? "Remap key?" : "Clear mappings?";
    const char* labels[2] = { "No", "Yes" };
    const char* values[2] = { NULL, NULL };
    if (choose) {
        const KbKey* k = &s_keys[km_sel];
        for (int i = 0; i < 2; i++) {
            labels[i] = hal_keyboard_remap_label(k->km[i]);
            values[i] = km_binding_name(k->km[i]);
        }
    }

    bool all = (km_pop_drawn < 0);
    int ry = sv_render_popup(title, choose ? "" : km_pop_msg, NULL, 2, all);
    for (int i = 0; i < 2; i++) {
        if (!all && i != km_pop_sel && i != km_pop_drawn) continue;
        // values[] may point at vk_short_name()'s static buffer: copy per row.
        char value[24];
        snprintf(value, sizeof(value), "%s", values[i] ? values[i] : "");
        sv_render_popup_row(ry + i * SVP_ROW_H, labels[i], value, i == km_pop_sel);
    }
    km_pop_drawn = km_pop_sel;
    DEBUG_PRINTF("popup: %s > %s", title, labels[km_pop_sel]);
}

void sv_keymap_render(Supervisor_t* sv) {
    (void)sv;
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    if (km_drawn < 0) {
        // Whole screen: frame, keyboard case, every key, the info lines.
        sv_render_wide_frame("Key Mapper",
                             "Arrows Select key   ENTER Remap   T Test   C Clear all   ESC Back");
        kb_draw_case(tft);
        for (int i = 0; i < s_key_count; i++) kb_draw_key(tft, i, i == km_sel);
        kb_draw_info(tft);
    } else if (km_pop == KM_POP_NONE && km_drawn != km_sel) {
        // Only the two keys that changed, and the info lines.
        kb_draw_key(tft, km_drawn, false);
        kb_draw_key(tft, km_sel, true);
        kb_draw_info(tft);
    }
    km_drawn = km_sel;

    if (km_pop != KM_POP_NONE) km_draw_popup();
}

// Popup over the keyboard, waiting for the key to bind.
void sv_keymap_capture_render(Supervisor_t* sv) {
    // Repaint the keyboard first: this window is smaller than the
    // "which character" popup it may follow.
    km_drawn = -1;
    sv_keymap_render(sv);

    char msg[SVP_MSG_COLS + 1];
    snprintf(msg, sizeof(msg), "CoCo key  %s   (now: %s)",
             hal_keyboard_remap_label(km_capture_idx), km_binding_name(km_capture_idx));
    sv_render_popup("Press the new key", msg, "DEL clears the mapping, ESC cancels.", 0, true);
    DEBUG_PRINTF("popup: Press the new key: %s", msg);
}

void sv_keymap_test_render(Supervisor_t* sv) {
    (void)sv;
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    if (!km_test_frame_drawn) {
        sv_render_wide_frame("Key Mapper Test", "Press keys   ESC Back");
        km_test_frame_drawn = true;
    }
    int cx = SVW_BOX_X + SVW_BOX_W / 2;
    tft->setTextFont(2);
    tft->setTextDatum(TC_DATUM);
    tft->setTextColor(SVW_DKBLUE, SVW_GREEN);
    tft->drawString("Press any key to test", cx, SVW_BOX_Y + 56);

    // Previous result dimmed, newest in black; clear each line first since
    // the centred text changes width.
    for (int i = 0; i < 2; i++) {
        int y = SVW_BOX_Y + 88 + i * 20;
        tft->fillRect(SVW_BOX_X + 8, y, SVW_BOX_W - 16, 14, SVW_GREEN);
        tft->setTextColor(i == 0 ? SVW_DIM : SVW_BLACK, SVW_GREEN);
        if (km_test_line[i][0]) tft->drawString(km_test_line[i], cx, y);
    }
    tft->setTextDatum(TL_DATUM);
}
