// Classic99 for ESP32 - F12 on-screen menu
#pragma once
#include "fabgl.h"

// pauses emulation, runs the menu on the VGA screen, resumes. Call from the input task.
void menuRun(fabgl::VGAController *display, fabgl::Keyboard *keyboard);

// Full-screen warning for a missing SD card (sdMounted false) or missing ROM files,
// shown instead of starting the emulator. Never returns: any key restarts the board.
struct BootRomStatus { const char *path; bool present; bool optional; };
void bootErrorScreen(fabgl::VGAController *display, fabgl::Keyboard *keyboard, bool sdMounted,
                     const BootRomStatus *roms, int count) __attribute__((noreturn));

// last used cartridge and disks, stored in TI_ROOT/config.txt
void configLoad();
void configSave();

// the saved "Debug log" setting, readable before the rest of the configuration is applied
bool configDebugEnabled();
