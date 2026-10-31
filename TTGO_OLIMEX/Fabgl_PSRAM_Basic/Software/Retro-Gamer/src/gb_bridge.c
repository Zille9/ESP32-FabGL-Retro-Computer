/*
 * GB Bridge Implementation
 * Connects gnuboy emulator to our FabGL-based system
 */

#include "gb_bridge.h"
#include "debug.h"
#include "gnuboy/gnuboy.h"
#include "sound_control.h"

#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

static uint16_t *gb_framebuffer = NULL;
static int16_t  *gb_audiobuffer = NULL;
static int       gb_audio_samples = 0;

#define GB_AUDIO_BUFFER_LEN 2048

/**
 * Video callback - called by gnuboy when a frame is ready
 */
static void gb_video_cb(void *buffer)
{
    // Framebuffer is already set via gnuboy_set_framebuffer
    (void)buffer;
}

/**
 * Audio callback - called by gnuboy with audio data
 */
static void gb_audio_cb(void *buffer, size_t length)
{
    // length is in bytes, for mono S16 it's samples * 2
    int16_t *src = (int16_t*)buffer;
    gb_audio_samples = length / sizeof(int16_t);
    
    // If sound is disabled, keep buffer silent
    if (g_sound_enabled) {
        if (gb_audiobuffer && gb_audio_samples <= GB_AUDIO_BUFFER_LEN) {
            memcpy(gb_audiobuffer, src, length);
        }
    } else {
        if (gb_audiobuffer && gb_audio_samples <= GB_AUDIO_BUFFER_LEN) {
            memset(gb_audiobuffer, 0, length);
        }
    }
}

bool gb_bridge_init(int sample_rate)
{
    // Allocate framebuffer in PSRAM (160*144*2 = 46080 bytes for RGB565)
    gb_framebuffer = (uint16_t *)heap_caps_calloc(GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT, 2, MALLOC_CAP_SPIRAM);
    if (!gb_framebuffer) {
        DBG_ERROR("GB", "Failed to allocate framebuffer!");
        return false;
    }

    // Allocate audio buffer in PSRAM
    gb_audiobuffer = (int16_t *)heap_caps_calloc(GB_AUDIO_BUFFER_LEN * 2, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!gb_audiobuffer) {
        DBG_ERROR("GB", "Failed to allocate audio buffer!");
        return false;
    }

    // Initialize gnuboy: stereo 16-bit audio, RGB565 Big Endian video
    int ret = gnuboy_init(sample_rate, GB_AUDIO_STEREO_S16, GB_PIXEL_565_BE, &gb_video_cb, &gb_audio_cb);
    if (ret < 0) {
        DBG_ERROR("GB", "gnuboy_init failed!");
        return false;
    }

    gnuboy_set_framebuffer(gb_framebuffer);
    gnuboy_set_soundbuffer(gb_audiobuffer, GB_AUDIO_BUFFER_LEN);

    DBG_INFO("GB", "Initialized. Sample rate: %d", sample_rate);
    return true;
}

int gb_bridge_load_rom(const char *path)
{
    DBG_INFO("GB", "Loading ROM: %s", path);

    int ret = gnuboy_load_rom_file(path);
    if (ret < 0) {
        DBG_ERROR("GB", "ROM load failed! Error: %d", ret);
        return ret;
    }

    // Set default DMG palette for non-color games
    gnuboy_set_palette(GB_PALETTE_DMG);

    // Hard reset
    gnuboy_reset(true);

    // Reset sound to ON when loading a new game
    osd_sound_reset();

    DBG_INFO("GB", "ROM loaded. HW type: %s",
           gnuboy_get_hwtype() == GB_HW_CGB ? "Game Boy Color" :
           gnuboy_get_hwtype() == GB_HW_SGB ? "Super Game Boy" : "Game Boy");

    return 0;
}

void gb_bridge_run_frame(bool draw)
{
    gnuboy_run(draw);
}

uint16_t* gb_bridge_get_framebuffer(void)
{
    return gb_framebuffer;
}

int16_t* gb_bridge_get_audio(int *num_samples)
{
    // If sound is disabled, return NULL (skip audio pipeline)
    if (!g_sound_enabled) {
        *num_samples = 0;
        return NULL;
    }
    
    // Sound enabled - normal behavior
    *num_samples = gb_audio_samples;
    return gb_audiobuffer;
}

void gb_bridge_set_input(uint32_t buttons)
{
    gnuboy_set_pad(buttons);
}

void gb_bridge_shutdown(void)
{
    gnuboy_free_rom();
    if (gb_framebuffer) { heap_caps_free(gb_framebuffer); gb_framebuffer = NULL; }
    if (gb_audiobuffer) { heap_caps_free(gb_audiobuffer); gb_audiobuffer = NULL; }
    DBG_VERBOSE("GB", "Shutdown complete");
}

// Declare external functions
extern int gnuboy_save_state(const char *filename);
extern int gnuboy_load_state(const char *filename);

bool gb_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("GB", "Saving state to: %s", fullPath);
    return (gnuboy_save_state(fullPath) == 0);
}

bool gb_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("GB", "Loading state from: %s", fullPath);
    return (gnuboy_load_state(fullPath) == 0);
}

// ============================================================================
//  GB Palette Control
// ============================================================================
static const char *gb_palette_names[] = {
    "Classic Green",   // GB_PALETTE_DMG
    "Pocket Gray",     // GB_PALETTE_MGB0
    "Pocket Light",    // GB_PALETTE_MGB1
    "GBC Auto",        // GB_PALETTE_CGB
    "SGB Auto",        // GB_PALETTE_SGB
    "Brown",           // 0
    "Red",             // 1
    "Dark Brown",      // 2
    "Pastel",          // 3
    "Orange",          // 4
    "Yellow",          // 5
    "Blue",            // 6
    "Dark Blue",       // 7
    "Grayscale",       // 8
    "Green",           // 9
    "Dark Green",      // 10
    "Inverted",        // 11
};

// Map our UI index to gnuboy palette enum
static const gb_palette_t gb_pal_map[] = {
    GB_PALETTE_DMG, GB_PALETTE_MGB0, GB_PALETTE_MGB1,
    GB_PALETTE_CGB, GB_PALETTE_SGB,
    GB_PALETTE_0, GB_PALETTE_1, GB_PALETTE_2, GB_PALETTE_3,
    GB_PALETTE_4, GB_PALETTE_5, GB_PALETTE_6, GB_PALETTE_7,
    GB_PALETTE_8, GB_PALETTE_9, GB_PALETTE_10, GB_PALETTE_11,
};

static int gb_current_pal_idx = 0; // UI index

void gb_bridge_set_palette(int palette_id) {
    int count = sizeof(gb_pal_map) / sizeof(gb_pal_map[0]);
    if (palette_id < 0 || palette_id >= count) return;
    gb_current_pal_idx = palette_id;
    gnuboy_set_palette(gb_pal_map[palette_id]);
    DBG_VERBOSE("GB", "Palette: %s", gb_palette_names[palette_id]);
}

int gb_bridge_get_palette(void) {
    return gb_current_pal_idx;
}

int gb_bridge_get_palette_count(void) {
    return sizeof(gb_pal_map) / sizeof(gb_pal_map[0]);
}

const char* gb_bridge_get_palette_name(int id) {
    int count = sizeof(gb_palette_names) / sizeof(gb_palette_names[0]);
    if (id < 0 || id >= count) return "Unknown";
    return gb_palette_names[id];
}

// ============================================================================
//  SRAM (Battery Save) Functions
// ============================================================================
bool gb_bridge_sram_dirty(void) {
    return gnuboy_sram_dirty();
}

bool gb_bridge_save_sram(const char *path) {
    if (!path || !*path) return false;
    int ret = gnuboy_save_sram(path, false);
    DBG_VERBOSE("GB", "SRAM saved: %s (ret=%d)", path, ret);
    return ret == 0;
}

bool gb_bridge_load_sram(const char *path) {
    if (!path || !*path) return false;
    int ret = gnuboy_load_sram(path);
    DBG_VERBOSE("GB", "SRAM loaded: %s (ret=%d)", path, ret);
    return ret == 0;
}