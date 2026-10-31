/*
 * Lynx Bridge — C++ implementation wrapping Handy CSystem
 */
#include "lynx_bridge.h"
#include "debug.h"
#include "sound_control.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "handy/handy.h"

#include <esp_heap_caps.h>

// Mikie pixel format enum (from mikie.h)
#ifndef MIKIE_PIXEL_FORMAT_16BPP_565_BE
#define MIKIE_PIXEL_FORMAT_16BPP_565_BE 2
#endif
#ifndef MIKIE_NO_ROTATE
#define MIKIE_NO_ROTATE 0
#endif

static CSystem *lynxSystem = nullptr;

// DO NOT declare gPrimaryFrameBuffer, gAudioBuffer, gAudioEnabled, gAudioBufferPointer
// They are already declared in handy.h / system.h as extern

extern "C" {

bool lynx_bridge_init(int sample_rate)
{
    // Allocate framebuffer in PSRAM (160*160*2 = 51200 bytes)
    gPrimaryFrameBuffer = (UBYTE *)heap_caps_calloc(LYNX_SCREEN_WIDTH * LYNX_SCREEN_WIDTH, 2, MALLOC_CAP_SPIRAM);
    if (!gPrimaryFrameBuffer) {
        DBG_ERROR("Lynx", "Failed to allocate framebuffer!");
        return false;
    }

    // Allocate audio buffer in PSRAM
    gAudioBuffer = (SWORD *)heap_caps_calloc(HANDY_AUDIO_BUFFER_LENGTH * 2, sizeof(SWORD), MALLOC_CAP_SPIRAM);
    if (!gAudioBuffer) {
        DBG_ERROR("Lynx", "Failed to allocate audio buffer!");
        return false;
    }
    gAudioEnabled = 1;
    gAudioBufferPointer = 0;

    DBG_INFO("Lynx", "Initialized. Sample rate: %d", sample_rate);
    return true;
}

int lynx_bridge_load_rom(const char *path)
{
    DBG_INFO("Lynx", "Loading ROM: %s", path);

    if (lynxSystem) { delete lynxSystem; lynxSystem = nullptr; }

    lynxSystem = new CSystem(path, MIKIE_PIXEL_FORMAT_16BPP_565_BE, LYNX_AUDIO_SAMPLE_RATE);

    if (!lynxSystem || lynxSystem->mFileType == HANDY_FILETYPE_ILLEGAL) {
        DBG_ERROR("Lynx", "ROM load failed!");
        if (lynxSystem) { delete lynxSystem; lynxSystem = nullptr; }
        return -1;
    }

    // No rotation
    lynxSystem->mMikie->SetRotation(MIKIE_NO_ROTATE);

    // Reset sound to ON when loading a new game
    osd_sound_reset();

    DBG_INFO("Lynx", "ROM loaded. Screen: %dx%d", LYNX_SCREEN_WIDTH, LYNX_SCREEN_HEIGHT);
    return 0;
}

void lynx_bridge_run_frame(bool draw)
{
    if (!lynxSystem) return;
    gAudioBufferPointer = 0;
    lynxSystem->UpdateFrame(draw);
}

uint16_t* lynx_bridge_get_framebuffer(void)
{
    return (uint16_t *)gPrimaryFrameBuffer;
}

int16_t* lynx_bridge_get_audio(int *num_samples)
{
    // If sound is disabled, return NULL (skip audio pipeline)
    if (!g_sound_enabled) {
        *num_samples = 0;
        return NULL;
    }
    
    // Sound enabled - normal behavior
    *num_samples = gAudioBufferPointer / 2; // stereo pairs
    return (int16_t *)gAudioBuffer;
}

void lynx_bridge_set_input(uint32_t buttons)
{
    if (!lynxSystem) return;
    ULONG lynxBtns = 0;
    
    // Use direct bit mapping with a single expression
    if (buttons & LYNX_BTN_UP)    lynxBtns |= BUTTON_UP;
    if (buttons & LYNX_BTN_DOWN)  lynxBtns |= BUTTON_DOWN;
    if (buttons & LYNX_BTN_LEFT)  lynxBtns |= BUTTON_LEFT;
    if (buttons & LYNX_BTN_RIGHT) lynxBtns |= BUTTON_RIGHT;
    if (buttons & LYNX_BTN_A)     lynxBtns |= BUTTON_A;
    if (buttons & LYNX_BTN_B)     lynxBtns |= BUTTON_B;
    if (buttons & LYNX_BTN_OPT1)  lynxBtns |= BUTTON_OPT1;
    if (buttons & LYNX_BTN_OPT2)  lynxBtns |= BUTTON_OPT2;
    
    lynxSystem->SetButtonData(lynxBtns);
}

void lynx_bridge_shutdown(void)
{
    if (lynxSystem) { delete lynxSystem; lynxSystem = nullptr; }
    if (gPrimaryFrameBuffer) { heap_caps_free(gPrimaryFrameBuffer); gPrimaryFrameBuffer = NULL; }
    if (gAudioBuffer) { heap_caps_free(gAudioBuffer); gAudioBuffer = NULL; }
    gAudioEnabled = 0;
    DBG_VERBOSE("Lynx", "Shutdown complete");
}

// ============================================================================
// Save / Load State
// ============================================================================

bool lynx_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("LYNX", "Saving state to: %s", fullPath);
    
    if (!lynxSystem) {
        DBG_ERROR("LYNX", "No emulator instance!");
        return false;
    }
    
    FILE *fp = fopen(fullPath, "wb");
    if (!fp) {
        DBG_ERROR("LYNX", "Cannot open file for writing!");
        return false;
    }
    
    bool ret = lynxSystem->ContextSave(fp);
    fclose(fp);
    
    if (ret) {
        DBG_VERBOSE("LYNX", "Save successful");
    } else {
        DBG_ERROR("LYNX", "Save failed!");
    }
    
    return ret;
}

bool lynx_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("LYNX", "Loading state from: %s", fullPath);
    
    if (!lynxSystem) {
        DBG_ERROR("LYNX", "No emulator instance!");
        return false;
    }
    
    FILE *fp = fopen(fullPath, "rb");
    if (!fp) {
        DBG_ERROR("LYNX", "Cannot open file for reading!");
        return false;
    }
    
    bool ret = lynxSystem->ContextLoad(fp);
    fclose(fp);
    
    if (ret) {
        DBG_VERBOSE("LYNX", "Load successful");
    } else {
        DBG_ERROR("LYNX", "Load failed!");
    }
    
    return ret;
}

} // extern "C"
