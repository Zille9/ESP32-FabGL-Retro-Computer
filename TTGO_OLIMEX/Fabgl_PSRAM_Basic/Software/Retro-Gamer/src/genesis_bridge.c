/*
 * Genesis Bridge — Sega Mega Drive / Genesis (gwenesis)
 * Simplified version - single buffer
 */

#include "genesis_bridge.h"
#include "debug.h"
#include "gwenesis/gwenesis.h"
#include "sound_control.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <esp_heap_caps.h>

// =============================================
// GLOBAL VARIABLES FOR SAVESTATE
// =============================================
// These variables are referenced by gwenesis_save_state() and gwenesis_load_state()
// Also used by gwenesis_ym2612_save_state, gwenesis_z80inst_load_state, etc.
FILE *savestate_fp = NULL;
int savestate_errors = 0;

// Globals required by gwenesis core
unsigned char *VRAM = NULL;
int system_clock = 0;
int scan_line = 0;

#define GENESIS_AUDIO_BUF_LEN 888
int16_t gwenesis_sn76489_buffer[GENESIS_AUDIO_BUF_LEN];
int sn76489_index = 0;
int sn76489_clock = 0;
int16_t gwenesis_ym2612_buffer[GENESIS_AUDIO_BUF_LEN];
int ym2612_index = 0;
int ym2612_clock = 0;

// Single buffer
static uint8_t *genesis_fb = NULL;

static uint32_t genesis_buttons = 0;
static unsigned int gen_screen_w = 320, gen_screen_h = 224;
extern int zclk;

// Button masks array - constant, stored in flash
static const uint32_t gwenesis_masks[] = {
    GEN_BTN_UP, GEN_BTN_DOWN, GEN_BTN_LEFT, GEN_BTN_RIGHT,
    GEN_BTN_A, GEN_BTN_B, GEN_BTN_C, GEN_BTN_START
};

// =============================================
// SAVESTATE FUNCTIONS IMPLEMENTATION
// These are called by gwenesis_savestate.c
// =============================================

typedef struct {
    FILE *fp;
} SaveStateInternal;

SaveState* saveGwenesisStateOpenForRead(const char* tag) {
    SaveStateInternal *s = (SaveStateInternal*)malloc(sizeof(SaveStateInternal));
    if (s) {
        s->fp = savestate_fp;
    }
    return (SaveState*)s;
}

SaveState* saveGwenesisStateOpenForWrite(const char* tag) {
    SaveStateInternal *s = (SaveStateInternal*)malloc(sizeof(SaveStateInternal));
    if (s) {
        s->fp = savestate_fp;
    }
    return (SaveState*)s;
}

int saveGwenesisStateGet(SaveState* state, const char* tag) {
    SaveStateInternal *s = (SaveStateInternal*)state;
    if (!s || !s->fp) return 0;
    int value;
    if (fread(&value, sizeof(int), 1, s->fp) != 1) {
        savestate_errors++;
        return 0;
    }
    return value;
}

void saveGwenesisStateSet(SaveState* state, const char* tag, int value) {
    SaveStateInternal *s = (SaveStateInternal*)state;
    if (!s || !s->fp) return;
    if (fwrite(&value, sizeof(int), 1, s->fp) != 1) {
        savestate_errors++;
    }
}

void saveGwenesisStateGetBuffer(SaveState* state, const char* tag, void* buffer, int length) {
    SaveStateInternal *s = (SaveStateInternal*)state;
    if (!s || !s->fp) return;
    if (fread(buffer, 1, length, s->fp) != (size_t)length) {
        savestate_errors++;
    }
}

void saveGwenesisStateSetBuffer(SaveState* state, const char* tag, void* buffer, int length) {
    SaveStateInternal *s = (SaveStateInternal*)state;
    if (!s || !s->fp) return;
    if (fwrite(buffer, 1, length, s->fp) != (size_t)length) {
        savestate_errors++;
    }
}

void gwenesis_io_get_buttons(void) {
    // Stub - buttons are handled by the bridge
}

// =============================================
// Initialization
// =============================================
bool genesis_bridge_init(int sample_rate)
{
    // If buffers already allocated, don't reallocate
    if (!genesis_fb) {
        genesis_fb = (uint8_t *)heap_caps_calloc(320 * 240, 1, MALLOC_CAP_SPIRAM);
        if (!genesis_fb) {
            DBG_ERROR("GEN", "Failed to allocate framebuffer!");
            return false;
        }
    } else {
        // Clear existing buffer
        memset(genesis_fb, 0, 320 * 240);
    }

    if (!VRAM) {
        VRAM = (unsigned char *)heap_caps_calloc(0x10000, 1, MALLOC_CAP_SPIRAM);
        if (!VRAM) {
            DBG_ERROR("GEN", "Failed to allocate VRAM!");
            return false;
        }
    } else {
        memset(VRAM, 0, 0x10000);
    }

    extern unsigned char *M68K_RAM;
    if (!M68K_RAM) {
        M68K_RAM = (unsigned char *)heap_caps_calloc(0x10000, 1, MALLOC_CAP_SPIRAM);
        if (!M68K_RAM) {
            DBG_ERROR("GEN", "Failed to allocate M68K_RAM!");
            return false;
        }
    } else {
        memset(M68K_RAM, 0, 0x10000);
    }

    extern unsigned char *ZRAM;
    if (!ZRAM) {
        ZRAM = (unsigned char *)heap_caps_calloc(8192, 1, MALLOC_CAP_SPIRAM);
        if (!ZRAM) {
            DBG_ERROR("GEN", "Failed to allocate ZRAM!");
            return false;
        }
    } else {
        memset(ZRAM, 0, 8192);
    }

    // Reset emulator state
    system_clock = 0;
    scan_line = 0;
    ym2612_index = 0;
    sn76489_index = 0;
    ym2612_clock = 0;
    sn76489_clock = 0;

    // Reset savestate variables
    savestate_fp = NULL;
    savestate_errors = 0;

    DBG_INFO("GEN", "Initialized (reused memory). Sample rate: %d", sample_rate);
    return true;
}

// =============================================
// Load ROM
// =============================================
int genesis_bridge_load_rom(const char *path)
{
    DBG_INFO("GEN", "Loading ROM: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        DBG_ERROR("GEN", "Failed to open: %s", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t alloc_size = (rom_size + 0xFFFF) & ~0xFFFF;
    void *rom_data = heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM);
    if (!rom_data) {
        DBG_ERROR("GEN", "Failed to allocate ROM buffer!");
        fclose(f);
        return -1;
    }
    memset(rom_data, 0xFF, alloc_size);
    fread(rom_data, 1, rom_size, f);
    fclose(f);

    load_cartridge(rom_data, rom_size);
    power_on();
    reset_emulation();

    // Reset sound to ON when loading a new game
    osd_sound_reset();

    DBG_INFO("GEN", "ROM loaded. Size: %d bytes", (int)rom_size);
    return 0;
}

// =============================================
// Run a frame
// =============================================
void genesis_bridge_run_frame(bool draw)
{
    extern unsigned char gwenesis_vdp_regs[0x20];
    extern unsigned int gwenesis_vdp_status;
    extern unsigned int screen_width, screen_height;
    extern int hint_pending;

    int lines_per_frame = REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
    int hint_counter = gwenesis_vdp_regs[10];

    screen_width = REG12_MODE_H40 ? 320 : 256;
    screen_height = REG1_PAL ? 240 : 224;
    gen_screen_w = screen_width;
    gen_screen_h = screen_height;

    // Use single buffer
    gwenesis_vdp_set_buffer((unsigned short*)genesis_fb);
    gwenesis_vdp_render_config();

    system_clock = 0;
    zclk = 0;
    ym2612_clock = 0; ym2612_index = 0;
    sn76489_clock = 0; sn76489_index = 0;
    scan_line = 0;

    while (scan_line < lines_per_frame)
    {
        m68k_run(system_clock + VDP_CYCLES_PER_LINE);
        z80_run(system_clock + VDP_CYCLES_PER_LINE);

        if (GWENESIS_AUDIO_ACCURATE == 0) {
            gwenesis_SN76489_run(system_clock + VDP_CYCLES_PER_LINE);
            ym2612_run(system_clock + VDP_CYCLES_PER_LINE);
        }

        if (draw && scan_line < (int)screen_height)
            gwenesis_vdp_render_line(scan_line);

        if ((scan_line == 0) || (scan_line > (int)screen_height))
            hint_counter = REG10_LINE_COUNTER;

        if (--hint_counter < 0) {
            if ((REG0_LINE_INTERRUPT != 0) && (scan_line <= (int)screen_height)) {
                hint_pending = 1;
                if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
                    m68k_update_irq(4);
            }
            hint_counter = REG10_LINE_COUNTER;
        }

        scan_line++;

        if (scan_line == (int)screen_height) {
            if (REG1_VBLANK_INTERRUPT != 0) {
                gwenesis_vdp_status |= STATUS_VIRQPENDING;
                m68k_set_irq(6);
            }
            z80_irq_line(1);
        }
        if (scan_line == (int)screen_height + 1) {
            z80_irq_line(0);
        }

        system_clock += VDP_CYCLES_PER_LINE;
    }

    if (GWENESIS_AUDIO_ACCURATE == 1) {
        gwenesis_SN76489_run(system_clock);
        ym2612_run(system_clock);
    }

    m68k.cycles -= system_clock;
}

// =============================================
// Get framebuffer
// =============================================
uint8_t* genesis_bridge_get_framebuffer(int *width, int *height)
{
    *width = gen_screen_w;
    *height = gen_screen_h;
    return genesis_fb;
}

// =============================================
// Get palette
// =============================================
uint16_t* genesis_bridge_get_palette(void)
{
    return NULL;
}

// =============================================
// Get audio
// =============================================
int16_t* genesis_bridge_get_audio(int *num_samples)
{
    // If sound is disabled, return NULL (no audio)
    if (!g_sound_enabled) {
        *num_samples = 0;
        return NULL;
    }
    
    // Sound enabled - normal behavior
    *num_samples = ym2612_index;
    return gwenesis_ym2612_buffer;
}

// =============================================
// Set input
// =============================================
void genesis_bridge_set_input(uint32_t buttons)
{
    static uint32_t prev = 0;
    if (buttons == prev) return;

    for (int i = 0; i < 8; i++) {
        if (buttons & gwenesis_masks[i])
            gwenesis_io_pad_press_button(0, i);
        else
            gwenesis_io_pad_release_button(0, i);
    }
    prev = buttons;
}

// =============================================
// Shutdown - Reset state without freeing memory
// =============================================
void genesis_bridge_shutdown(void)
{
    DBG_VERBOSE("GEN", "Shutdown started...");
    
    // Clear audio buffers
    ym2612_index = 0;
    sn76489_index = 0;
    ym2612_clock = 0;
    sn76489_clock = 0;
    
    // Clear emulator state (gwenesis global variables)
    system_clock = 0;
    scan_line = 0;
    zclk = 0;
    
    // Clear framebuffer (content only, don't free)
    if (genesis_fb) {
        memset(genesis_fb, 0, 320 * 240);
    }
    
    // Reset savestate variables
    savestate_fp = NULL;
    savestate_errors = 0;
    
    // DO NOT free VRAM, M68K_RAM, ZRAM
    // Let next genesis_bridge_init() reuse or reallocate them
    
    DBG_VERBOSE("GEN", "Shutdown complete (state reset, memory preserved)");
}

// =============================================
// Savestate using emulator functions
// =============================================
bool genesis_bridge_save_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    DBG_INFO("GEN", "Saving state to: %s", fullPath);
    
    // Open file directly - fopen with "wb" creates or truncates the file
    FILE *fp = fopen(fullPath, "wb");
    if (!fp) {
        DBG_ERROR("GEN", "Cannot open file for writing! Check SD card permissions.");
        return false;
    }
    
    // Save original values and assign ours
    FILE *old_fp = savestate_fp;
    int old_errors = savestate_errors;
    
    savestate_fp = fp;
    savestate_errors = 0;
    
    gwenesis_save_state();
    
    bool success = (savestate_errors == 0);
    
    // Restore original values
    savestate_fp = old_fp;
    savestate_errors = old_errors;
    
    fclose(fp);
    
    if (success) {
        DBG_VERBOSE("GEN", "Save successful");
    } else {
        DBG_ERROR("GEN", "Save failed! (%d errors)", savestate_errors);
    }
    
    return success;
}

bool genesis_bridge_load_state(const char *path)
{
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "/sd%s", path);
    
    DBG_INFO("GEN", "Loading state from: %s", fullPath);
    
    FILE *fp = fopen(fullPath, "rb");
    if (!fp) {
        DBG_ERROR("GEN", "Cannot open file for reading!");
        return false;
    }
    
    FILE *old_fp = savestate_fp;
    int old_errors = savestate_errors;
    
    savestate_fp = fp;
    savestate_errors = 0;
    
    gwenesis_load_state();
    
    bool success = (savestate_errors == 0);
    
    savestate_fp = old_fp;
    savestate_errors = old_errors;
    
    fclose(fp);
    
    if (success) {
        DBG_VERBOSE("GEN", "Load successful");
    } else {
        DBG_ERROR("GEN", "Load failed! (%d errors)", savestate_errors);
    }
    
    return success;
}