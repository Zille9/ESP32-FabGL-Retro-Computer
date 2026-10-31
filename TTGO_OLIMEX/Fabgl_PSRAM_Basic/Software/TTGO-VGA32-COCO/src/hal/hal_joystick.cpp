/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : hal_joystick.cpp
 *  Module : Joystick HAL — PS/2 mouse → CoCo right joystick; arrow keys + Space
 *           as either joystick (Joy-Keyboard Emulation)
 * ============================================================
*/

#include "hal.h"
#include "../utils/debug.h"


// Joystick 1 (port 0) is driven by the PS/2 mouse plugged into the TTGO
// VGA32 mouse header. Joystick 2 (port 1) remains a neutral stub.
//
// Design spec: docs/superpowers/specs/2026-05-21-mouse-joystick-design.md
//
// Position model: accumulate + clamp. The mouse reports relative deltas;
// we integrate them into s_pos_x / s_pos_y in the 0..63 logical range and
// clamp at the edges. The joystick stays where you left it once the mouse
// stops — closest to real CoCo joystick behavior.
//
// Polarity: machine.cpp does `if (hal_joystick_read_button(0)) row_data
// &= ~0x01;` so a non-zero return means "pressed". Initial s_btn_left is
// false so the keyboard matrix stays untouched until the user clicks.

#include "fabgl.h"

static int16_t s_pos_x    = 32;   // 6-bit logical position, range 0..63
static int16_t s_pos_y    = 32;
static bool    s_btn_left = false;

// Runtime-adjustable mouse tunables (Supervisor -> Settings -> Mouse Sensitivity).
// Seeded from the config.h compile-time defaults; overridden at boot from NVS.
//   s_scale  : raw mouse counts are divided by this. Lower = more sensitive.
//   s_invert : true flips the Y axis.
static int  s_scale  = JOYSTICK_MOUSE_SCALE;
static bool s_invert = (JOYSTICK_MOUSE_INVERT_Y != 0);

// Joy-Keyboard Emulation (Supervisor -> Settings): a held arrow key glides
// the stick at KJ_STEP_US per position (64 steps ~ 1 s edge to edge) and it
// stays where it is on release, like the mouse; Space is the fire button. On
// the right joystick it moves the same position as the mouse, and either
// Space or the mouse button fires. The left joystick has its own position.
enum { KJ_UP = 1, KJ_DOWN = 2, KJ_LEFT = 4, KJ_RIGHT = 8, KJ_FIRE = 16 };
#define KJ_STEP_US  15625u
static uint8_t  s_kbd_mode = JOY_KBD_OFF;
static uint8_t  s_kbd_keys = 0;
static int16_t  s_left_x = 32, s_left_y = 32;   // left joystick (port 1)
static uint32_t s_kbd_last_us = 0;
static uint32_t s_kbd_acc_us  = 0;

static inline bool kbd_drives(int port) {
    return s_kbd_mode != JOY_KBD_OFF && port == (s_kbd_mode == JOY_KBD_RIGHT ? 0 : 1);
}

static inline int16_t clamp63(int v) { return (int16_t)(v < 0 ? 0 : v > 63 ? 63 : v); }

// Advance the keyboard stick by the time since the last call.
static void kbd_glide(void) {
    uint32_t now = micros();
    uint32_t dt = now - s_kbd_last_us;
    s_kbd_last_us = now;
    int dx = ((s_kbd_keys & KJ_RIGHT) ? 1 : 0) - ((s_kbd_keys & KJ_LEFT) ? 1 : 0);
    int dy = ((s_kbd_keys & KJ_DOWN)  ? 1 : 0) - ((s_kbd_keys & KJ_UP)   ? 1 : 0);
    if (s_kbd_mode == JOY_KBD_OFF || (!dx && !dy)) { s_kbd_acc_us = 0; return; }
    if (dt > 100000) dt = 100000;      // emulator paused / supervisor open
    s_kbd_acc_us += dt;
    int steps = (int)(s_kbd_acc_us / KJ_STEP_US);
    if (!steps) return;
    s_kbd_acc_us -= (uint32_t)steps * KJ_STEP_US;
    int16_t* px = (s_kbd_mode == JOY_KBD_RIGHT) ? &s_pos_x : &s_left_x;
    int16_t* py = (s_kbd_mode == JOY_KBD_RIGHT) ? &s_pos_y : &s_left_y;
    *px = clamp63(*px + dx * steps);
    *py = clamp63(*py + dy * steps);
}

void hal_joystick_set_kbd_mode(uint8_t mode) {
    s_kbd_mode = (mode <= JOY_KBD_LEFT) ? mode : JOY_KBD_OFF;
    s_kbd_keys = 0;
}

uint8_t hal_joystick_get_kbd_mode(void) { return s_kbd_mode; }

bool hal_joystick_kbd_key(int vk, bool pressed) {
    uint8_t bit;
    switch (vk) {
        case fabgl::VK_UP:    case fabgl::VK_KP_UP:    bit = KJ_UP;    break;
        case fabgl::VK_DOWN:  case fabgl::VK_KP_DOWN:  bit = KJ_DOWN;  break;
        case fabgl::VK_LEFT:  case fabgl::VK_KP_LEFT:  bit = KJ_LEFT;  break;
        case fabgl::VK_RIGHT: case fabgl::VK_KP_RIGHT: bit = KJ_RIGHT; break;
        case fabgl::VK_SPACE:                          bit = KJ_FIRE;  break;
        default: return false;
    }
    // Releases always clear (a key held when the mode was turned off or the
    // supervisor opened must not stick); presses only count while on.
    kbd_glide();                        // settle the time held so far
    if (!pressed)                       s_kbd_keys &= ~bit;
    else if (s_kbd_mode != JOY_KBD_OFF) s_kbd_keys |= bit;
    return s_kbd_mode != JOY_KBD_OFF;
}

void hal_joystick_init(void) {
    // The PS2Controller itself is brought up in hal_keyboard_init() via
    // PS2Preset::KeyboardPort0_MousePort1. Here we just verify the Mouse
    // got allocated and log the result.
    fabgl::Mouse* m = fabgl::PS2Controller::mouse();
    if (m) {
        DEBUG_PRINTF("  Joystick: PS/2 mouse on port 1 (CLK=GPIO%d DATA=GPIO%d), scale=%d",
                     PIN_PS2_MOUSE_CLK, PIN_PS2_MOUSE_DATA, JOYSTICK_MOUSE_SCALE);
    } else {
        DEBUG_PRINT("  Joystick: PS/2 mouse not enumerated (yet) — port 0 will read center");
    }
}

void hal_joystick_update(void) {
    kbd_glide();
    fabgl::Mouse* m = fabgl::PS2Controller::mouse();
    if (!m) return;
    while (m->deltaAvailable()) {
        fabgl::MouseDelta d;
        if (!m->getNextDelta(&d, 0)) break;

        int divisor = (s_scale < 1) ? 1 : s_scale;
        int dx = (int)d.deltaX / divisor;
        int dy = (int)d.deltaY / divisor;

        s_pos_x += dx;
        if (s_invert) {
            s_pos_y += dy;
        } else {
            s_pos_y -= dy;   // FabGL Y is positive-up; CoCo Y is positive-down
        }

        if (s_pos_x < 0)  s_pos_x = 0;
        if (s_pos_x > 63) s_pos_x = 63;
        if (s_pos_y < 0)  s_pos_y = 0;
        if (s_pos_y > 63) s_pos_y = 63;

        s_btn_left = (d.buttons.left != 0);
    }
}

uint8_t hal_joystick_read_axis(int port, int axis) {
    if (port != 0) return (uint8_t)((axis == 0) ? s_left_x : s_left_y);
    return (uint8_t)((axis == 0) ? s_pos_x : s_pos_y);
}

uint8_t hal_joystick_read_button(int port) {
    bool fire = kbd_drives(port) && (s_kbd_keys & KJ_FIRE);
    if (port == 0 && s_btn_left) fire = true;
    return fire ? 1 : 0;
}

bool hal_joystick_compare(int port, int axis, uint8_t dac_value) {
    int v = hal_joystick_read_axis(port, axis);
    // 6-bit (0..63) → 8-bit DAC space (2..254), matches the inline math at
    // the PIA0 PA read site in machine.cpp.
    return (v * 4 + 2) >= (int)dac_value;
}

// ---- Runtime tunables API (Supervisor Mouse Sensitivity screen) ----
//
// Level is the user-facing 1..10 value; divisor is JOYSTICK_MOUSE_SCALE.
// Higher level = lower divisor = more sensitive:  divisor = 11 - level.

void hal_joystick_set_sensitivity(uint8_t level) {
    if (level < 1)  level = 1;
    if (level > 10) level = 10;
    s_scale = 11 - (int)level;          // level 10 -> 1, level 1 -> 10
}

uint8_t hal_joystick_get_sensitivity(void) {
    int level = 11 - s_scale;
    if (level < 1)  level = 1;
    if (level > 10) level = 10;
    return (uint8_t)level;
}

void hal_joystick_set_invert_y(bool on) {
    s_invert = on;
}

bool hal_joystick_get_invert_y(void) {
    return s_invert;
}

void hal_joystick_get_pos(uint8_t* x, uint8_t* y) {
    if (x) *x = (uint8_t)s_pos_x;
    if (y) *y = (uint8_t)s_pos_y;
}
