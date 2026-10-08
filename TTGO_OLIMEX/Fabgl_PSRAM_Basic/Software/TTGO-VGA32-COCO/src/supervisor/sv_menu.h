/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_menu.h
 *  Module : Supervisor OSD main menu interface
 * ============================================================
*/

/*
 * sv_menu.h - Main menu for supervisor OSD
 */

#ifndef SV_MENU_H
#define SV_MENU_H

#include <stdint.h>

// Forward declaration
struct Supervisor_t;

enum SV_MenuAction : uint8_t {
    SV_ACT_MOUNT_DISK,
    SV_ACT_MACHINE_SELECT,
    SV_ACT_RESET,
    SV_ACT_SETTINGS,
    SV_ACT_ABOUT,
    SV_ACT_DEBUG,
    SV_ACT_RESUME,
};

void sv_menu_init(Supervisor_t* sv);
void sv_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_menu_render(Supervisor_t* sv);

// Force the next render to repaint the whole main menu; otherwise only the
// two tiles whose selection changed are redrawn.
void sv_menu_invalidate(void);

// Machine select (SV_MACHINE_SELECT state): a popup over the Settings list,
// opened from its "Machine" row. Picking the other machine asks to confirm
// the restart in the same window (defaults to No); on Yes it calls
// supervisor_set_machine_type(), which persists the choice and restarts.
void sv_machine_select_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_machine_select_render(Supervisor_t* sv);
// Force the next render to repaint the whole popup window.
void sv_machine_select_invalidate(void);

// Settings submenu (SV_SETTINGS state) and its Keyboard submenu
// (SV_KEYBOARD_MENU state): icon lists in the wide green frame. Sub-screens
// use these row numbers to put the cursor back on their own row.
enum SV_SettingsRow : uint8_t {
    SV_SET_MACHINE,     // opens machine-select
    SV_SET_RS232,       // RS-232 Pak toggle (shares UART0 with the Echo Log)
    SV_SET_KEYBOARD,    // opens the Keyboard submenu
    SV_SET_JOYSTICK,    // Joy - Mouse Sensitivity screen
    SV_SET_WIFI,        // WiFi / Debug screen
    SV_SET_DRIVEWIRE,   // DriveWire screen
    SV_SET_COUNT
};
enum SV_KeyboardRow : uint8_t {
    SV_KBD_LANGUAGE,    // cycles the PS/2 keyboard layout
    SV_KBD_MAPPER,      // opens the Key Mapper screens
    SV_KBD_COUNT
};
void sv_settings_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_settings_render(Supervisor_t* sv);
void sv_keyboard_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_keyboard_menu_render(Supervisor_t* sv);
// Force the next render to repaint the whole Settings / Keyboard list.
void sv_settings_invalidate(void);

// 32x12 row icons for lists in the wide frame, drawn in code. bg/fg are the
// row's own colours so an icon reads on the green box and on the selection bar.
enum SV_RowIcon : uint8_t {
    ROW_ICON_MACHINE, ROW_ICON_SERIAL, ROW_ICON_KEYBOARD, ROW_ICON_JOYSTICK,
    ROW_ICON_WIFI, ROW_ICON_DRIVEWIRE, ROW_ICON_LANGUAGE, ROW_ICON_KEYCAP,
    ROW_ICON_CHIP, ROW_ICON_PAGE, ROW_ICON_SD, ROW_ICON_SPIDER,
    ROW_ICON_CHECK, ROW_ICON_STOP, ROW_ICON_CROSS,
};
void sv_menu_draw_row_icon(int icon, int x, int y, uint16_t bg, uint16_t fg);

// Debug submenu (SV_DEBUG_MENU state), an icon list in the wide green frame:
// the debug pages (Status / Hex Dump / RS-232 Pak), "Dump RAM to SD", and the
// Echo Log toggle (debug output on the serial port; forces the RS-232 Pak
// off, they share UART0). ESC returns to the main menu.
void sv_debug_menu_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed);
void sv_debug_menu_render(Supervisor_t* sv);
// Force the next render to repaint the whole Debug submenu.
void sv_debug_menu_invalidate(void);

#endif // SV_MENU_H
