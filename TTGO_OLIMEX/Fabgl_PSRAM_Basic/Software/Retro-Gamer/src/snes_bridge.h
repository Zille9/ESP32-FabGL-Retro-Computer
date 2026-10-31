/*
 * SNES Bridge Header
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

#define SNES_SCREEN_WIDTH  256
#define SNES_SCREEN_HEIGHT 224
#define SNES_AUDIO_SAMPLE_RATE 22050
#define SNES_PIXEL_FORMAT PIXEL_FORMAT

bool     snes_bridge_init(int sample_rate);
int      snes_bridge_load_rom(const char *path);
void     snes_bridge_run_frame(bool draw);
uint16_t* snes_bridge_get_framebuffer(int *width, int *height);
int16_t* snes_bridge_get_audio(int *num_samples);
void     snes_bridge_set_input(uint32_t buttons);
void     snes_bridge_shutdown(void);
bool snes_bridge_save_state(const char *path);
bool snes_bridge_load_state(const char *path);

#define SNES_BTN_A      0x0080
#define SNES_BTN_B      0x8000
#define SNES_BTN_X      0x0040
#define SNES_BTN_Y      0x4000
#define SNES_BTN_L      0x0020
#define SNES_BTN_R      0x0010
#define SNES_BTN_START  0x1000
#define SNES_BTN_SELECT 0x2000
#define SNES_BTN_UP     0x0800
#define SNES_BTN_DOWN   0x0400
#define SNES_BTN_LEFT   0x0200
#define SNES_BTN_RIGHT  0x0100

#ifdef __cplusplus
}
#endif
