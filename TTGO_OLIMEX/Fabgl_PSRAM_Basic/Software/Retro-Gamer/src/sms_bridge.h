/*
 * SMS/GG Bridge Header
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define SMS_SCREEN_WIDTH   256
#define SMS_SCREEN_HEIGHT  192
#define GG_SCREEN_WIDTH    160
#define GG_SCREEN_HEIGHT   144
#define SMS_AUDIO_SAMPLE_RATE 22050

bool     sms_bridge_init(int sample_rate);
int      sms_bridge_load_rom(const char *path);
void     sms_bridge_run_frame(bool draw);
uint8_t* sms_bridge_get_framebuffer(int *width, int *height);
uint16_t* sms_bridge_get_palette(void);
void     sms_bridge_get_audio(int16_t **left, int16_t **right, int *num_samples);
void     sms_bridge_set_input(uint32_t buttons);
void     sms_bridge_shutdown(void);
bool sms_bridge_save_state(const char *path);
bool sms_bridge_load_state(const char *path);
bool     sms_bridge_is_gamegear(void);
int sms_bridge_get_viewport_x(void);
int sms_bridge_get_viewport_width(void);

// Debug function
void sms_bridge_debug_status(void);

// Button masks
#define SMS_BTN_UP      0x0001
#define SMS_BTN_DOWN    0x0002
#define SMS_BTN_LEFT    0x0004
#define SMS_BTN_RIGHT   0x0008
#define SMS_BTN_A       0x0010
#define SMS_BTN_B       0x0020
#define SMS_BTN_START   0x0040

// ColecoVision numeric keypad buttons
#define SMS_BTN_1       0x0100
#define SMS_BTN_2       0x0200
#define SMS_BTN_3       0x0400
#define SMS_BTN_4       0x0800
#define SMS_BTN_5       0x1000
#define SMS_BTN_6       0x2000
#define SMS_BTN_7       0x4000
#define SMS_BTN_8       0x8000
#define SMS_BTN_9       0x10000
#define SMS_BTN_0       0x20000
#define SMS_BTN_STAR    0x40000
#define SMS_BTN_POUND   0x80000

#ifdef __cplusplus
}
#endif