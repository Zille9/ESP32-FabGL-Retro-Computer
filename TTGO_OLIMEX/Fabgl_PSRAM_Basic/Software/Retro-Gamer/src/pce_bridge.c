/*
 * PCE Bridge — PC Engine / TurboGrafx-16 emulator bridge
 */

#include "pce_bridge.h"
#include "debug.h"
#include "pce-go/pce-go.h"
#include "pce-go/pce.h"
#include "pce-go/psg.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <esp_heap_caps.h>
#include "sound_control.h"

bool pce_pal_dirty = true;

static uint8_t  *pce_fb = NULL;
static uint8_t   pce_input_state = 0;
static int       pce_fb_width = 256;
static int       pce_fb_height = 224;
static bool      pce_frame_done = false;
static bool      pce_initialized = false;

#define PCE_AUDIO_BUFFER_SIZE 2048
static int16_t  *pce_audio_buffer = NULL;
static int      pce_audio_samples = 0;

static uint8_t  pce_rgb222_cache[512];
static uint16_t *pce_palette_cache = NULL;

extern PCE_t PCE;

// ============================================================================
// Internal helper functions
// ============================================================================

static void pce_rebuild_palette_cache(void)
{
    if (!pce_palette_cache) {
        pce_palette_cache = (uint16_t*)PalettePCE(16);
        if (!pce_palette_cache) {
            DBG_ERROR("PCE", "Failed to get palette!");
            return;
        }
    }
    
    for (int i = 0; i < 512; i++) {
        uint16_t rgb565 = (i < 256) ? pce_palette_cache[i] : 0;
        uint8_t r5 = (rgb565 >> 11) & 0x1F;
        uint8_t g6 = (rgb565 >> 5) & 0x3F;
        uint8_t b5 = (rgb565 >> 0) & 0x1F;
        pce_rgb222_cache[i] = ((r5 >> 3) << 4) | ((g6 >> 4) << 2) | (b5 >> 3);
    }
    pce_pal_dirty = false;
    DBG_VERBOSE("PCE", "Palette cache rebuilt");
}

// ============================================================================
// OSD Callbacks (called by the emulator)
// ============================================================================

uint8_t *osd_gfx_framebuffer(int width, int height)
{
    if (width > 0 && height > 0) {
        pce_fb_width = width;
        pce_fb_height = height;
    }
    return pce_fb;
}

void osd_input_read(uint8_t joypads[8])
{
    // CRITICAL: Don't modify state if no changes
    // Just copy current state
    for (int i = 0; i < 8; i++) {
        joypads[i] = pce_input_state;
    }
    
    // DO NOT update PCE.Joypad.regs here because it's already updated in
    // pce_bridge_set_input and the emulator might be resetting it
}

void osd_vsync(void)
{
    pce_frame_done = true;
}

// ============================================================================
// Initialization
// ============================================================================

bool pce_bridge_init(int sample_rate)
{
    if (pce_initialized) {
        DBG_VERBOSE("PCE", "Already initialized");
        return true;
    }
    
    DBG_INFO("PCE", "Init started...");
    
    int fb_size = 368 * 242;
    pce_fb = (uint8_t *)heap_caps_calloc(fb_size, 1, MALLOC_CAP_SPIRAM);
    if (!pce_fb) {
        DBG_ERROR("PCE", "Failed to allocate framebuffer!");
        return false;
    }
    
    pce_audio_buffer = (int16_t *)heap_caps_calloc(PCE_AUDIO_BUFFER_SIZE, sizeof(int16_t) * 2, MALLOC_CAP_SPIRAM);
    if (!pce_audio_buffer) {
        DBG_WARN("PCE", "Failed to allocate audio buffer!");
    }

    if (InitPCE(sample_rate, true) != 0) {
        DBG_ERROR("PCE", "InitPCE failed!");
        return false;
    }

    pce_initialized = true;
    pce_pal_dirty = true;
    pce_audio_samples = 0;
    pce_input_state = 0;
    
    DBG_INFO("PCE", "Initialized. Sample rate: %d", sample_rate);
    return true;
}

int pce_bridge_load_rom(const char *path)
{
    DBG_INFO("PCE", "Loading ROM: %s", path);

    int ret = LoadFile(path);
    if (ret != 0) {
        DBG_ERROR("PCE", "ROM load failed! Error: %d", ret);
        return ret;
    }

    DBG_INFO("PCE", "ROM loaded. Screen: %dx%d", pce_fb_width, pce_fb_height);
    pce_pal_dirty = true;
    pce_audio_samples = 0;
    pce_input_state = 0;

    osd_sound_reset();
    
    return 0;
}

void pce_bridge_run_frame(void)
{
    pce_frame_done = false;
    
    // Generate audio only if sound is enabled
    if (pce_audio_buffer && g_sound_enabled) {
        memset(pce_audio_buffer, 0, PCE_AUDIO_BUFFER_SIZE * sizeof(int16_t) * 2);
        int samples_per_frame = 368;
        psg_update(pce_audio_buffer, samples_per_frame, 0x3F);
        pce_audio_samples = samples_per_frame * 2;
    } else {
        pce_audio_samples = 0;
        // No need to memset audio buffer when sound is disabled
        // The bridge will return NULL and num_samples=0
    }
    
    // Run the frame (emulator will call osd_input_read internally)
    pce_run();
    
    pce_pal_dirty = true;
}

uint8_t* pce_bridge_get_framebuffer(int *width, int *height)
{
    *width = pce_fb_width;
    *height = pce_fb_height;
    return pce_fb;
}

uint16_t* pce_bridge_get_palette(void)
{
    if (pce_pal_dirty) {
        pce_rebuild_palette_cache();
    }
    return pce_palette_cache;
}

int16_t* pce_bridge_get_audio(int *num_samples)
{
    // If sound is disabled, return NULL (skip audio pipeline)
    if (!g_sound_enabled) {
        *num_samples = 0;
        return NULL;
    }
    
    // Sound enabled - normal behavior
    *num_samples = pce_audio_samples;
    return pce_audio_buffer;
}

void pce_bridge_set_input(uint32_t buttons)
{
    // Keep state persistent
    pce_input_state = (uint8_t)(buttons & 0xFF);
    
    // DIRECTLY UPDATE EMULATOR REGISTERS
    // This is CRITICAL so the state is not lost
    PCE.Joypad.regs[0] = pce_input_state;
}

// ============================================================================
// Save / Load State
// ============================================================================

bool pce_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    
    DBG_INFO("PCE", "Saving state to: %s", fullPath);
    
    FILE *f = fopen(fullPath, "wb");
    if (!f) {
        DBG_ERROR("PCE", "Cannot open file for writing!");
        return false;
    }
    
    extern PCE_t PCE;
    
    // Save RAM (0x2000 bytes)
    fwrite(PCE.RAM, 1, 0x2000, f);
    
    // Save VRAM (0x10000 bytes)
    fwrite(PCE.VRAM, 1, 0x10000, f);
    
    // Save SPRAM (512 bytes)
    fwrite(PCE.SPRAM, 1, 512, f);
    
    // Save Palette (512 bytes)
    fwrite(PCE.Palette, 1, 512, f);
    
    // Save VCE registers (512 entries of 16 bits)
    fwrite(PCE.VCE.regs, sizeof(uint16_t), 512, f);
    
    // Save VDC registers (32 entries of 16 bits)
    fwrite(PCE.VDC.regs, sizeof(uint16_t), 32, f);
    
    // Save VDC state
    fwrite(&PCE.VDC.status, sizeof(uint8_t), 1, f);
    fwrite(&PCE.VDC.satb, sizeof(uint8_t), 1, f);
    fwrite(&PCE.VDC.pending_irqs, sizeof(uint32_t), 1, f);
    
    // Save CPU registers
    fwrite(&CPU.PC, sizeof(uint16_t), 1, f);
    fwrite(&CPU.A, sizeof(uint8_t), 1, f);
    fwrite(&CPU.X, sizeof(uint8_t), 1, f);
    fwrite(&CPU.Y, sizeof(uint8_t), 1, f);
    fwrite(&CPU.P, sizeof(uint8_t), 1, f);
    fwrite(&CPU.S, sizeof(uint8_t), 1, f);
    fwrite(&CPU.irq_mask, sizeof(uint8_t), 1, f);
    fwrite(&CPU.irq_lines, sizeof(uint8_t), 1, f);
    
    // Save PSG state
    fwrite(&PCE.PSG, sizeof(PCE.PSG), 1, f);
    
    // Save MMR (8 bytes)
    fwrite(PCE.MMR, 1, 8, f);
    
    // Save timers and misc
    fwrite(&PCE.Timer, sizeof(PCE.Timer), 1, f);
    fwrite(&PCE.Cycles, sizeof(int32_t), 1, f);
    fwrite(&PCE.MaxCycles, sizeof(int32_t), 1, f);
    fwrite(&PCE.SF2, sizeof(uint8_t), 1, f);
    fwrite(&PCE.ScrollYDiff, sizeof(int32_t), 1, f);
    
    fclose(f);
    
    DBG_VERBOSE("PCE", "Save successful");
    return true;
}

bool pce_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    
    DBG_INFO("PCE", "Loading state from: %s", fullPath);
    
    FILE *f = fopen(fullPath, "rb");
    if (!f) {
        DBG_ERROR("PCE", "Cannot open file for reading!");
        return false;
    }
    
    extern PCE_t PCE;
    
    // Load in the same order as saved
    fread(PCE.RAM, 1, 0x2000, f);
    fread(PCE.VRAM, 1, 0x10000, f);
    fread(PCE.SPRAM, 1, 512, f);
    fread(PCE.Palette, 1, 512, f);
    fread(PCE.VCE.regs, sizeof(uint16_t), 512, f);
    fread(PCE.VDC.regs, sizeof(uint16_t), 32, f);
    fread(&PCE.VDC.status, sizeof(uint8_t), 1, f);
    fread(&PCE.VDC.satb, sizeof(uint8_t), 1, f);
    fread(&PCE.VDC.pending_irqs, sizeof(uint32_t), 1, f);
    
    fread(&CPU.PC, sizeof(uint16_t), 1, f);
    fread(&CPU.A, sizeof(uint8_t), 1, f);
    fread(&CPU.X, sizeof(uint8_t), 1, f);
    fread(&CPU.Y, sizeof(uint8_t), 1, f);
    fread(&CPU.P, sizeof(uint8_t), 1, f);
    fread(&CPU.S, sizeof(uint8_t), 1, f);
    fread(&CPU.irq_mask, sizeof(uint8_t), 1, f);
    fread(&CPU.irq_lines, sizeof(uint8_t), 1, f);
    
    fread(&PCE.PSG, sizeof(PCE.PSG), 1, f);
    fread(PCE.MMR, 1, 8, f);
    fread(&PCE.Timer, sizeof(PCE.Timer), 1, f);
    fread(&PCE.Cycles, sizeof(int32_t), 1, f);
    fread(&PCE.MaxCycles, sizeof(int32_t), 1, f);
    fread(&PCE.SF2, sizeof(uint8_t), 1, f);
    fread(&PCE.ScrollYDiff, sizeof(int32_t), 1, f);
    
    fclose(f);
    
    // Rebuild memory banks
    for (int i = 0; i < 8; i++) {
        pce_bank_set(i, PCE.MMR[i]);
    }
    
    // Mark palette as dirty to rebuild cache
    pce_pal_dirty = true;
    
    // Clear audio buffer
    pce_audio_samples = 0;
    if (pce_audio_buffer) {
        memset(pce_audio_buffer, 0, PCE_AUDIO_BUFFER_SIZE * sizeof(int16_t) * 2);
    }
    
    DBG_VERBOSE("PCE", "Load successful");
    return true;
}

void pce_bridge_shutdown(void)
{
    DBG_VERBOSE("PCE", "Shutdown started...");
    
    if (pce_initialized) {
        ShutdownPCE();
        pce_initialized = false;
    }
    
    if (pce_fb) { heap_caps_free(pce_fb); pce_fb = NULL; }
    if (pce_audio_buffer) { heap_caps_free(pce_audio_buffer); pce_audio_buffer = NULL; }
    if (pce_palette_cache) { free(pce_palette_cache); pce_palette_cache = NULL; }
    
    pce_audio_samples = 0;
    pce_input_state = 0;
    pce_pal_dirty = true;
    
    DBG_VERBOSE("PCE", "Shutdown complete");
}