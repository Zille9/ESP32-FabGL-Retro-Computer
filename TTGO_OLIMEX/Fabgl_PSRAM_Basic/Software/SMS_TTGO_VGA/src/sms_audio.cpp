// sms_audio.cpp - Audio output for VGA version

#include <Arduino.h>
#include "driver/i2s.h"
#include "sms_core/shared.h"
#include "../config.h"

static int16_t audio_buffer[AUDIO_BUFFER_SIZE * 2];
static uint32_t audio_buffer_index = 0;
static uint32_t audio_buffer_ready = 0;

// Called from SMS core when audio is generated
void IRAM_ATTR sms_audio_callback(int16_t* left, int16_t* right, int samples) {
    for (int i = 0; i < samples && audio_buffer_index < AUDIO_BUFFER_SIZE; i++) {
        // Mix left and right to mono for DAC
        int16_t sample = (left[i] + right[i]) >> 1;
        audio_buffer[audio_buffer_index++] = sample;
        audio_buffer[audio_buffer_index++] = sample;  // Stereo output
    }
    
    if (audio_buffer_index >= AUDIO_BUFFER_SIZE) {
        audio_buffer_index = 0;
        audio_buffer_ready = 1;
    }
}

// Called from main loop to send audio to I2S
void sms_audio_send() {
    if (audio_buffer_ready) {
        size_t bytes_written;
        i2s_write(I2S_NUM_0, audio_buffer, sizeof(audio_buffer), &bytes_written, pdMS_TO_TICKS(10));
        audio_buffer_ready = 0;
    }
}

// Initialize I2S audio (already in main, but ensure callback is set)
void sms_audio_init() {
    audio_buffer_index = 0;
    audio_buffer_ready = 0;
    
    // Set callback in SMS core
    extern void sms_set_audio_callback(void (*callback)(int16_t*, int16_t*, int));
    sms_set_audio_callback(sms_audio_callback);
}