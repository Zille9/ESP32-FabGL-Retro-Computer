/*
 * Genesis Bridge Header — Sega Mega Drive / Genesis
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

#define GENESIS_AUDIO_SAMPLE_RATE 22050

bool     genesis_bridge_init(int sample_rate);
int      genesis_bridge_load_rom(const char *path);
void     genesis_bridge_run_frame(bool draw);
uint8_t* genesis_bridge_get_framebuffer(int *width, int *height);
uint16_t* genesis_bridge_get_palette(void);
int16_t* genesis_bridge_get_audio(int *num_samples);
void     genesis_bridge_set_input(uint32_t buttons);
void     genesis_bridge_shutdown(void);
bool genesis_bridge_save_state(const char *path);
bool genesis_bridge_load_state(const char *path);

// Button masks (matching gwenesis_io pad button indices 0-7)
#define GEN_BTN_UP     0x01
#define GEN_BTN_DOWN   0x02
#define GEN_BTN_LEFT   0x04
#define GEN_BTN_RIGHT  0x08
#define GEN_BTN_A      0x10
#define GEN_BTN_B      0x20
#define GEN_BTN_C      0x40
#define GEN_BTN_START  0x80

#ifdef __cplusplus
}
#endif
