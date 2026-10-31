/*
 * NES Bridge — Connects nofrendo NES emulator to FabGL VGA/Audio/Input
 * For Olimex ESP32-SBC-FabGL Rev B
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// NES display constants
#define NES_WIDTH        256
#define NES_HEIGHT       240
#define NES_PITCH        272  // 8 + 256 + 8 = NES_SCREEN_PITCH
#define NES_OVERDRAW     8

// Audio
#define NES_AUDIO_SAMPLE_RATE  22050

// Initialize the NES emulator
bool nes_bridge_init(int sample_rate);

// Load a ROM file from SD card (path like "/sd/roms/game.nes")
int nes_bridge_load_rom(const char *path);

// Run one frame of emulation. Returns pointer to 8-bit framebuffer.
uint8_t* nes_bridge_run_frame(bool draw);

// Get audio buffer and sample count from last frame
int16_t* nes_bridge_get_audio(int *num_samples);

// Update NES controller with button state
void nes_bridge_set_input(uint32_t buttons);

// NES controller button masks
#define NES_BTN_A      0x01
#define NES_BTN_B      0x02
#define NES_BTN_SELECT 0x04
#define NES_BTN_START  0x08
#define NES_BTN_UP     0x10
#define NES_BTN_DOWN   0x20
#define NES_BTN_LEFT   0x40
#define NES_BTN_RIGHT  0x80

// Shutdown
void nes_bridge_shutdown(void);
bool nes_bridge_save_state(const char *path);
bool nes_bridge_load_state(const char *path);

// Get NES refresh rate (50 PAL, 60 NTSC)
int nes_bridge_get_refresh_rate(void);

// NEW: Get internal framebuffer directly from nofrendo
uint8_t* nes_bridge_get_internal_framebuffer(void);

#ifdef __cplusplus
}
#endif
