/*
 * Lynx Bridge — Atari Lynx emulator bridge (C++ wrapper for Handy)
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

#define LYNX_SCREEN_WIDTH   160
#define LYNX_SCREEN_HEIGHT  102
#define LYNX_AUDIO_SAMPLE_RATE 22050

bool     lynx_bridge_init(int sample_rate);
int      lynx_bridge_load_rom(const char *path);
void     lynx_bridge_run_frame(bool draw);
uint16_t* lynx_bridge_get_framebuffer(void);
int16_t* lynx_bridge_get_audio(int *num_samples);
void     lynx_bridge_set_input(uint32_t buttons);
void     lynx_bridge_shutdown(void);
bool     lynx_bridge_save_state(const char *path);
bool     lynx_bridge_load_state(const char *path);

// Generic button masks (mapped internally to Lynx BUTTON_*)
#define LYNX_BTN_UP      0x01
#define LYNX_BTN_DOWN    0x02
#define LYNX_BTN_LEFT    0x04
#define LYNX_BTN_RIGHT   0x08
#define LYNX_BTN_A       0x10
#define LYNX_BTN_B       0x20
#define LYNX_BTN_OPT1    0x40
#define LYNX_BTN_OPT2    0x80

#ifdef __cplusplus
}
#endif

