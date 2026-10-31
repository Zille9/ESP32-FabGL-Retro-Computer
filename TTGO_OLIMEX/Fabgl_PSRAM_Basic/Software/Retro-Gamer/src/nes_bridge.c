/*
 * NES Bridge Implementation
 * Connects nofrendo NES emulator to our FabGL-based system
 */

#include "nes_bridge.h"
#include "debug.h"
#include "nofrendo.h"
#include "nes/nes.h"
#include "nes/input.h"
#include "nes/state.h"
#include "sound_control.h"

#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

static nes_t *nes = NULL;
static uint8_t *nes_framebuffer = NULL;
static uint32_t current_buttons = 0;
static bool nes_initialized = false;

// Declare external functions from nofrendo
extern int state_save(const char *filename);
extern int state_load(const char *filename);

// No blit_callback needed: nofrendo renders directly into nes_framebuffer
// because nes->vidbuf is set to nes_framebuffer in nes_bridge_init().
// The previous callback did memcpy(nes_framebuffer, bmp, 272*240) where
// bmp == nes->vidbuf == nes_framebuffer — a 65 KB self-copy every frame.

bool nes_bridge_init(int sample_rate)
{
    if (nes_initialized) {
        DBG_VERBOSE("NES", "Already initialized");
        return true;
    }

    // Allocate framebuffer
    nes_framebuffer = (uint8_t *)calloc(272 * 240, 1);
    if (!nes_framebuffer) {
        DBG_ERROR("NES", "Failed to allocate framebuffer!");
        return false;
    }

    // Initialize NES
    nes = nes_init(SYS_DETECT, sample_rate, false, NULL);
    if (!nes) {
        DBG_ERROR("NES", "nes_init failed!");
        free(nes_framebuffer);
        nes_framebuffer = NULL;
        return false;
    }

    // Set vidbuf so nofrendo renders directly into our framebuffer.
    // No blit_func needed — the callback would be a no-op (bmp == nes_framebuffer).
    nes->vidbuf = nes_framebuffer;
    nes_setvidbuf(nes_framebuffer);
    
    // Verify it was assigned correctly
    if (nes->vidbuf != nes_framebuffer) {
        DBG_WARN("NES", "vidbuf mismatch! Expected %p, got %p", 
               nes_framebuffer, nes->vidbuf);
    }

    nes_initialized = true;
    DBG_INFO("NES", "Initialized successfully");
    return true;
}

int nes_bridge_load_rom(const char *path)
{
    if (!nes || !nes_initialized) {
        DBG_ERROR("NES", "Emulator not initialized!");
        return -1;
    }

    DBG_INFO("NES", "Loading ROM: %s", path);

    if (nes_framebuffer) {
        memset(nes_framebuffer, 0, 272 * 240);
    }

    int ret = nes_loadfile(path);

    if (ret == 0) {
        DBG_INFO("NES", "ROM loaded successfully!");
        DBG_VERBOSE("NES", "System: %s, Refresh: %d Hz, Scanlines: %d",
               nes->system == SYS_NES_PAL ? "PAL" : "NTSC",
               nes->refresh_rate,
               nes->scanlines_per_frame);
        
        // Warm-up frames: pass false so nofrendo skips rendering
        nes_reset(true);
        for (int i = 0; i < 5; i++) {
            nes_emulate(false);
        }
        current_buttons = 0;
        input_update(0, 0);
        
        // Reset sound to ON when loading a new game
        osd_sound_reset();
        
        DBG_INFO("NES", "Ready to run");
    } else {
        DBG_ERROR("NES", "ROM load failed! Error code: %d", ret);
    }

    return ret;
}

uint8_t* nes_bridge_run_frame(bool draw)
{
    if (!nes || !nes_initialized) {
        return NULL;
    }

    // Verify vidbuf hasn't been lost
    if (nes->vidbuf != nes_framebuffer) {
        DBG_WARN("NES", "vidbuf lost! Recovering...");
        nes->vidbuf = nes_framebuffer;
    }

    input_update(0, current_buttons);
    nes_emulate(draw);

    return nes_framebuffer;
}

int16_t* nes_bridge_get_audio(int *num_samples)
{
    if (!nes || !nes_initialized || !nes->apu) {
        *num_samples = 0;
        return NULL;
    }
    
    // If sound is disabled, skip the audio pipeline entirely.
    // Returning NULL causes the caller to skip audioFeedSamples().
    if (!g_sound_enabled) {
        *num_samples = 0;
        return NULL;
    }
    
    // Sound enabled - normal behavior
    *num_samples = nes->apu->samples_per_frame;
    if (!nes->apu->buffer) {
        *num_samples = 0;
        return NULL;
    }
    return nes->apu->buffer;
}

void nes_bridge_set_input(uint32_t buttons)
{
    current_buttons = buttons;
}

void nes_bridge_shutdown(void)
{
    if (nes) {
        nes_shutdown();
        nes = NULL;
    }
    if (nes_framebuffer) {
        free(nes_framebuffer);
        nes_framebuffer = NULL;
    }
    nes_initialized = false;
    DBG_VERBOSE("NES", "Shutdown complete");
}

int nes_bridge_get_refresh_rate(void)
{
    return nes ? nes->refresh_rate : 60;
}

uint8_t* nes_bridge_get_internal_framebuffer(void)
{
    if (nes) {
        return nes->vidbuf;
    }
    return NULL;
}

// ============================================================================
// Save / Load State
// ============================================================================

bool nes_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    
    DBG_INFO("NES", "Saving state to: %s", fullPath);
    
    int ret = state_save(fullPath);
    
    if (ret == 0) {
        DBG_VERBOSE("NES", "Save successful");
        return true;
    } else {
        DBG_ERROR("NES", "Save failed! (error %d)", ret);
        return false;
    }
}

bool nes_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    
    DBG_INFO("NES", "Loading state from: %s", fullPath);
    
    int ret = state_load(fullPath);
    
    if (ret != 0) {
        DBG_ERROR("NES", "Load failed! (error %d)", ret);
        return false;
    }
    
    DBG_VERBOSE("NES", "Load successful");
    return true;
}