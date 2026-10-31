/*
 * GB Bridge — Connects gnuboy Game Boy/Color emulator to FabGL
 * For Olimex ESP32-SBC-FabGL Rev B
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// GB display constants
#define GB_SCREEN_WIDTH   160
#define GB_SCREEN_HEIGHT  144

// Audio
#define GB_AUDIO_SAMPLE_RATE  22050

// Initialize the GB emulator
bool gb_bridge_init(int sample_rate);

// Load a ROM file from SD card
int gb_bridge_load_rom(const char *path);

// Run one frame of emulation
void gb_bridge_run_frame(bool draw);

// Get the framebuffer (RGB565 Big Endian, 160x144)
uint16_t* gb_bridge_get_framebuffer(void);

// Get audio buffer from last frame
int16_t* gb_bridge_get_audio(int *num_samples);

// Set controller input (use GB_PAD_* from gnuboy.h)
void gb_bridge_set_input(uint32_t buttons);

// Shutdown
void gb_bridge_shutdown(void);

// Save/Load state
bool gb_bridge_save_state(const char *path);
bool gb_bridge_load_state(const char *path);

// Palette control (for non-CGB games)
void gb_bridge_set_palette(int palette_id);
int  gb_bridge_get_palette(void);
int  gb_bridge_get_palette_count(void);
const char* gb_bridge_get_palette_name(int id);

// SRAM (battery save) functions
bool gb_bridge_sram_dirty(void);
bool gb_bridge_save_sram(const char *path);
bool gb_bridge_load_sram(const char *path);

// GB controller button masks (matching gnuboy)
#define GB_BTN_RIGHT   0x01
#define GB_BTN_LEFT    0x02
#define GB_BTN_UP      0x04
#define GB_BTN_DOWN    0x08
#define GB_BTN_A       0x10
#define GB_BTN_B       0x20
#define GB_BTN_SELECT  0x40
#define GB_BTN_START   0x80

#ifdef __cplusplus
}
#endif
