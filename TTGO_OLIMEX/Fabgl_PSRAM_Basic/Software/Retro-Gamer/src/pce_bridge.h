/*
 * PCE Bridge — PC Engine / TurboGrafx-16 emulator bridge
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>

#define PCE_SCREEN_WIDTH   352
#define PCE_SCREEN_HEIGHT  242
#define PCE_AUDIO_SAMPLE_RATE 22050

// Variable global para indicar que la paleta cambió (definida en pce_bridge.c)
extern bool pce_pal_dirty;

bool     pce_bridge_init(int sample_rate);
int      pce_bridge_load_rom(const char *path);
void     pce_bridge_run_frame(void);
uint8_t* pce_bridge_get_framebuffer(int *width, int *height);
uint16_t* pce_bridge_get_palette(void);
int16_t* pce_bridge_get_audio(int *num_samples);
void     pce_bridge_set_input(uint32_t buttons);
void     pce_bridge_shutdown(void);
bool     pce_bridge_save_state(const char *path);
bool     pce_bridge_load_state(const char *path);

// Button masks (matching pce-go.h JOY_* defines)
#define PCE_BTN_A       0x01
#define PCE_BTN_B       0x02
#define PCE_BTN_SELECT  0x04
#define PCE_BTN_RUN     0x08
#define PCE_BTN_UP      0x10
#define PCE_BTN_RIGHT   0x20
#define PCE_BTN_DOWN    0x40
#define PCE_BTN_LEFT    0x80

#ifdef __cplusplus
}
#endif