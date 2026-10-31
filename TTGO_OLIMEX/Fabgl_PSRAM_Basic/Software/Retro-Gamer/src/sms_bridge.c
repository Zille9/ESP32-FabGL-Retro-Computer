/*
 * SMS Bridge Implementation
 * Connects smsplus Sega Master System / Game Gear emulator to FabGL
 */

#include "sms_bridge.h"
#include "debug.h"
#include "smsplus/shared.h"
#include "sound_control.h"

#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <stdio.h>

extern coleco_t coleco;

static uint16_t sms_palette_cache[PALETTE_SIZE];
static int frame_count = 0;
static uint8_t *sms_framebuffer = NULL;
static uint8_t *sms_framebuffer_back = NULL;  // Double buffer
static bool framebuffer_allocated = false;
static int current_buffer = 0;  // 0 or 1

// Debug counters
static int executed_frames = 0;
static int rendered_frames = 0;
static int last_log_frame = 0;
static bool is_running = false;

// ColecoVision keypad mapping table
static const int coleco_keypad_map[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 10, 11
};

bool sms_bridge_init(int sample_rate)
{
    DBG_INFO("SMS", "Initializing. Sample rate: %d", sample_rate);
    
    // If framebuffers already exist, free them first
    if (sms_framebuffer) {
        heap_caps_free(sms_framebuffer);
        sms_framebuffer = NULL;
    }
    if (sms_framebuffer_back) {
        heap_caps_free(sms_framebuffer_back);
        sms_framebuffer_back = NULL;
    }
    
    // Allocate framebuffers for SMS (256x192 = 49152 bytes) in PSRAM
    sms_framebuffer = (uint8_t *)heap_caps_calloc(256 * 192, 1, MALLOC_CAP_SPIRAM);
    if (!sms_framebuffer) {
        DBG_WARN("SMS", "PSRAM allocation failed, trying normal RAM...");
        sms_framebuffer = (uint8_t *)calloc(256 * 192, 1);
    }
    
    sms_framebuffer_back = (uint8_t *)heap_caps_calloc(256 * 192, 1, MALLOC_CAP_SPIRAM);
    if (!sms_framebuffer_back) {
        DBG_WARN("SMS", "PSRAM allocation failed, trying normal RAM...");
        sms_framebuffer_back = (uint8_t *)calloc(256 * 192, 1);
    }
    
    if (!sms_framebuffer || !sms_framebuffer_back) {
        DBG_ERROR("SMS", "Failed to allocate framebuffers!");
        return false;
    }
    
    framebuffer_allocated = true;

    // Clear buffers
    memset(sms_framebuffer, 0, 256 * 192);
    memset(sms_framebuffer_back, 0, 256 * 192);
    
    // Configure bitmap BEFORE system_poweron
    bitmap.width = 256;
    bitmap.height = 192;
    bitmap.pitch = 256;
    bitmap.data = sms_framebuffer;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 192;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.changed = 1;

    // Configure smsplus options
    system_reset_config();
    option.sndrate = sample_rate;
    option.overscan = 0;
    option.extra_gg = 0;
    option.console = 0;  // Auto-detect from ROM
    option.spritelimit = 1;
    option.fm = SND_NONE;

    // Reset counters
    executed_frames = 0;
    rendered_frames = 0;
    last_log_frame = 0;
    is_running = false;
    
    DBG_INFO("SMS", "Initialized successfully");
    return true;
}

int sms_bridge_load_rom(const char *path)
{
    DBG_INFO("SMS", "Loading ROM: %s", path);

    // Ensure framebuffer exists
    if (!framebuffer_allocated || !sms_framebuffer) {
        DBG_ERROR("SMS", "Framebuffer not allocated!");
        return -1;
    }

    // Clear framebuffers
    memset(sms_framebuffer, 0, 256 * 192);
    memset(sms_framebuffer_back, 0, 256 * 192);
    current_buffer = 0;
    executed_frames = 0;
    rendered_frames = 0;

    // Detect console type by extension
    const char *ext = strrchr(path, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "gg") == 0) {
            option.console = 3; // Game Gear
            DBG_VERBOSE("SMS", "Detected Game Gear ROM");
        } else if (strcasecmp(ext, "sg") == 0) {
            option.console = 5; // SG-1000
            DBG_VERBOSE("SMS", "Detected SG-1000 ROM");
        } else if (strcasecmp(ext, "col") == 0) {
            option.console = 6; // Colecovision
            DBG_VERBOSE("SMS", "Detected Colecovision ROM");
        } else if (strcasecmp(ext, "sms") == 0) {
            option.console = 0; // Auto (SMS)
            DBG_VERBOSE("SMS", "Detected Master System ROM");
        } else {
            option.console = 0; // Auto
            DBG_VERBOSE("SMS", "Unknown extension, defaulting to Master System");
        }
    }

    // Load ROM
    if (!load_rom_file(path)) {
        DBG_ERROR("SMS", "ROM load failed!");
        return -1;
    }

    // Reconfigure viewport based on console type
    if (option.console == 3) { // Game Gear
        bitmap.viewport.w = 160;
        bitmap.viewport.h = 144;
        bitmap.viewport.x = 48;
        bitmap.viewport.y = 0;
    } else if (option.console == 6) { // ColecoVision
        bitmap.viewport.w = 256;
        bitmap.viewport.h = 192;
        bitmap.viewport.x = 0;
        bitmap.viewport.y = 0;
    } else {
        bitmap.viewport.w = 256;
        bitmap.viewport.h = 192;
        bitmap.viewport.x = 0;
        bitmap.viewport.y = 0;
    }
    
    // Ensure bitmap.data points to our buffer
    bitmap.data = sms_framebuffer;
    bitmap.width = 256;
    bitmap.height = 192;
    bitmap.pitch = 256;
    bitmap.viewport.changed = 1;
    
    // Power on the system (after configuring bitmap)
    system_poweron();
    is_running = true;
    
    // Reset sound to ON when loading a new game
    osd_sound_reset();
    
    DBG_INFO("SMS", "ROM loaded. Console: %s, Viewport: %dx%d at (%d,%d)",
           (option.console == 3) ? "Game Gear" : (option.console == 6) ? "ColecoVision" : "Master System",
           bitmap.viewport.w, bitmap.viewport.h,
           bitmap.viewport.x, bitmap.viewport.y);
    
    // Run initial test frame
    sms_bridge_run_frame(true);
    
    return 0;
}

void sms_bridge_run_frame(bool draw)
{
    if (!bitmap.data) {
        DBG_ERROR("SMS", "run_frame called with NULL bitmap.data!");
        return;
    }
    
    if (!is_running) {
        DBG_WARN("SMS", "Emulator not running (is_running=false)");
        return;
    }
    
    // IMPORTANT: Before executing the frame, switch to the working buffer
    if (current_buffer == 0) {
        bitmap.data = sms_framebuffer_back;
    } else {
        bitmap.data = sms_framebuffer;
    }
    
    // Execute one frame of the emulator
    // draw=0 -> render, draw=1 -> skip render
    system_frame(draw ? 0 : 1);
    executed_frames++;
    
    if (draw) {
        render_copy_palette(sms_palette_cache);
        rendered_frames++;
    }
    
    // Switch buffer for the next frame
    current_buffer = !current_buffer;
}

uint8_t* sms_bridge_get_framebuffer(int *width, int *height)
{
    if (!sms_framebuffer || !sms_framebuffer_back) {
        DBG_ERROR("SMS", "framebuffers are NULL in get_framebuffer!");
        *width = 0;
        *height = 0;
        return NULL;
    }
    
    // Use viewport dimensions directly
    *width = bitmap.viewport.w;
    *height = bitmap.viewport.h;
    
    // Return the buffer that is NOT currently being used for drawing
    // This avoids tearing
    if (current_buffer == 0) {
        return sms_framebuffer;  // The buffer that is already complete
    } else {
        return sms_framebuffer_back;
    }
}

uint16_t* sms_bridge_get_palette(void)
{
    // Palette is already updated in run_frame() when draw=true
    // No need to call render_copy_palette again
    return sms_palette_cache;
}

void sms_bridge_get_audio(int16_t **left, int16_t **right, int *num_samples)
{
    // If sound is disabled, return NULL (skip audio pipeline)
    if (!g_sound_enabled) {
        *left = NULL;
        *right = NULL;
        *num_samples = 0;
        return;
    }
    
    // Sound enabled - normal behavior
    if (!snd.stream[0] || !snd.stream[1]) {
        *left = NULL;
        *right = NULL;
        *num_samples = 0;
        return;
    }
    
    *left = snd.stream[0];
    *right = snd.stream[1];
    *num_samples = snd.sample_count;
}

void sms_bridge_set_input(uint32_t buttons)
{
    input.pad[0] = 0x00;
    input.pad[1] = 0x00;
    input.system = 0x00;

    // Directional and action buttons
    if (buttons & SMS_BTN_UP)    input.pad[0] |= INPUT_UP;
    if (buttons & SMS_BTN_DOWN)  input.pad[0] |= INPUT_DOWN;
    if (buttons & SMS_BTN_LEFT)  input.pad[0] |= INPUT_LEFT;
    if (buttons & SMS_BTN_RIGHT) input.pad[0] |= INPUT_RIGHT;
    if (buttons & SMS_BTN_A)     input.pad[0] |= INPUT_BUTTON2;  // Right
    if (buttons & SMS_BTN_B)     input.pad[0] |= INPUT_BUTTON1;  // Left

    if (option.console == 3) { // Game Gear
        if (buttons & SMS_BTN_START) input.system |= INPUT_START;
    } 
    else if (option.console == 6) { // ColecoVision
        if (buttons & SMS_BTN_START) input.system |= INPUT_START;
        
        // ColecoVision has a 12-key keypad
        // Accessed through coleco.keypad[port]
        // 0-9, * (10), # (11)
        extern coleco_t coleco;
        
        // Map button to keypad value using table
        int key = -1;
        if (buttons & SMS_BTN_1)      key = 0;
        else if (buttons & SMS_BTN_2) key = 1;
        else if (buttons & SMS_BTN_3) key = 2;
        else if (buttons & SMS_BTN_4) key = 3;
        else if (buttons & SMS_BTN_5) key = 4;
        else if (buttons & SMS_BTN_6) key = 5;
        else if (buttons & SMS_BTN_7) key = 6;
        else if (buttons & SMS_BTN_8) key = 7;
        else if (buttons & SMS_BTN_9) key = 8;
        else if (buttons & SMS_BTN_0) key = 9;
        else if (buttons & SMS_BTN_STAR) key = 10;
        else if (buttons & SMS_BTN_POUND) key = 11;
        
        if (key >= 0) {
            coleco.keypad[0] = coleco_keypad_map[key];
            coleco.pio_mode = 0;  // Keypad mode
        } else {
            // No numeric key pressed, return to joystick mode
            coleco.pio_mode = 1;
        }
    } 
    else { // Master System / SG-1000
        if (buttons & SMS_BTN_START) input.system |= INPUT_PAUSE;
    }
}

void sms_bridge_shutdown(void)
{
    DBG_VERBOSE("SMS", "Shutdown started...");
    
    if (is_running) {
        system_poweroff();
        system_shutdown();
        is_running = false;
    }
    
    if (sms_framebuffer) {
        heap_caps_free(sms_framebuffer);
        sms_framebuffer = NULL;
    }
    if (sms_framebuffer_back) {
        heap_caps_free(sms_framebuffer_back);
        sms_framebuffer_back = NULL;
    }
    framebuffer_allocated = false;
    
    // Clear palette cache
    memset(sms_palette_cache, 0, sizeof(sms_palette_cache));
    frame_count = 0;
    current_buffer = 0;
    executed_frames = 0;
    rendered_frames = 0;
    last_log_frame = 0;
    
    // Reset options for the next game
    system_reset_config();
    option.sndrate = SMS_AUDIO_SAMPLE_RATE;
    option.overscan = 0;
    option.extra_gg = 0;
    option.console = 0;
    option.spritelimit = 1;
    option.fm = SND_NONE;
    
    DBG_VERBOSE("SMS", "Shutdown complete");
}

bool sms_bridge_is_gamegear(void)
{
    return (option.console == 3);
}

bool sms_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("SMS", "Saving state to: %s", fullPath);
    
    FILE *f = fopen(fullPath, "wb");
    if (!f) return false;
    system_save_state(f);
    fclose(f);
    return true;
}

bool sms_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("SMS", "Loading state from: %s", fullPath);
    
    FILE *f = fopen(fullPath, "rb");
    if (!f) return false;
    system_load_state(f);
    fclose(f);
    return true;
}

int sms_bridge_get_viewport_x(void)
{
    return bitmap.viewport.x;
}

int sms_bridge_get_viewport_width(void)
{
    return bitmap.viewport.w;
}