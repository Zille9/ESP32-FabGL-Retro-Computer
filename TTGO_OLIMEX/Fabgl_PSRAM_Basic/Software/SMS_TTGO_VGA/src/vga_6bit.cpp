// vga_6bit.cpp
// Ricardo Massaro mod library (Bitluni) mod by ackerman (convert C, resize)
// Adapted for Anemoia NES Emulator

#include <esp_heap_caps.h>
#include <soc/rtc.h>
#include <soc/i2s_reg.h>
#include <soc/i2s_struct.h>
#include <soc/io_mux_reg.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/periph_ctrl.h>

#if ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <rom/lldesc.h>
#define DELAY(n) delay(n)
#else
#include <esp32/rom/lldesc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define DELAY(n) vTaskDelay((n) / portTICK_PERIOD_MS)
#endif

#include "vga_6bit.h"

// VGA mode timing definitions
const int VgaMode_vga_mode_320x240[12] = { 
    8, 48, 24, 320, 11, 2, 31, 480, 2, 12587500, 1, 1
};

const int VgaMode_vga_mode_320x200[12] = {
    8, 48, 24, 320, 12, 2, 35, 400, 2, 12587500, 0, 1
};

const int VgaMode_vga_mode_256x240[12] = {
    8, 48, 24, 256, 11, 2, 31, 480, 2, 12587500, 1, 1
};

// VGA timing variables
static int h_front, h_sync, h_back, h_pixels;
static int v_front, v_sync, v_back, v_pixels;
static int v_div, pixel_clock;
static unsigned char h_polarity, v_polarity;

// DMA buffer descriptors
static lldesc_t* dma_buf_desc = nullptr;
static int dma_buf_desc_count = 0;
static uint8_t* dma_buf_hblank_vnorm = nullptr;
static uint8_t* dma_buf_hblank_vsync = nullptr;
static uint8_t* dma_buf_vblank_vnorm = nullptr;
static uint8_t* dma_buf_vblank_vsync = nullptr;

// Double buffering
static uint8_t** framebuffer[2] = { nullptr, nullptr };
static uint8_t* framebuffer_linear[2] = { nullptr, nullptr };
static uint8_t active_fb = 0;
static uint8_t back_fb = 1;
static uint8_t num_fbs = 2;
static bool buffer_needs_update = false;

// I2S interrupt handler
static intr_handle_t i2s_isr_handle = nullptr;
static volatile int vga_frame_count = 0;

// Helper functions for sync bit manipulation
inline unsigned char vsync_inv_bit() { return v_polarity << 7; }
inline unsigned char hsync_inv_bit() { return h_polarity << 6; }
inline unsigned char vsync_bit() { return (1 - v_polarity) << 7; }
inline unsigned char hsync_bit() { return (1 - h_polarity) << 6; }
inline unsigned char sync_bits() { return vsync_inv_bit() | hsync_inv_bit(); }
inline int x_res() { return h_pixels; }
inline int y_res() { return v_pixels / v_div; }

// I2S interrupt handler - increments frame counter
static void IRAM_ATTR i2s_isr(void* arg) {
    REG_WRITE(I2S_INT_CLR_REG(1), (REG_READ(I2S_INT_RAW_REG(1)) & 0xffffffc0) | 0x3f);
    vga_frame_count++;
}

// Set active framebuffer for display
static void set_active_framebuffer(unsigned char** fb) {
    for (int i = 0; i < v_pixels; i++) {
        int desc_idx = 2 * (v_front + v_sync + v_back + i) + 1;
        if (desc_idx < dma_buf_desc_count && fb) {
            dma_buf_desc[desc_idx].buf = fb[i / v_div];
            dma_buf_desc[desc_idx].length = h_pixels;
            dma_buf_desc[desc_idx].size = h_pixels;
        }
    }
}

// Allocate contiguous framebuffer for linear access
static unsigned char** alloc_framebuffer(int index) {
    int yres = y_res();
    int pitch = h_pixels;
    
    // Allocate single contiguous DMA-capable block
    int total_size = yres * pitch;
    uint8_t* linear = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_DMA);
    if (!linear) return nullptr;
    
    framebuffer_linear[index] = linear;
    
    unsigned char** fb = (unsigned char**)malloc(sizeof(unsigned char*) * yres);
    if (!fb) {
        free(linear);
        framebuffer_linear[index] = nullptr;
        return nullptr;
    }
    
    unsigned char sync = sync_bits();
    for (int i = 0; i < yres; i++) {
        fb[i] = linear + (i * pitch);
        memset(fb[i], sync, pitch);
    }
    
    return fb;
}

// Initialize DMA buffers for VGA output
static void allocate_vga_i2s_buffers() {
    int hblank_len = h_front + h_sync + h_back;
    int pixel_len = h_pixels;
    
    dma_buf_hblank_vnorm = (uint8_t*)heap_caps_malloc(hblank_len, MALLOC_CAP_DMA);
    dma_buf_hblank_vsync = (uint8_t*)heap_caps_malloc(hblank_len, MALLOC_CAP_DMA);
    dma_buf_vblank_vnorm = (uint8_t*)heap_caps_malloc(pixel_len, MALLOC_CAP_DMA);
    dma_buf_vblank_vsync = (uint8_t*)heap_caps_malloc(pixel_len, MALLOC_CAP_DMA);
    
    // Initialize blanking buffers with sync patterns
    for (int i = 0; i < hblank_len; i++) {
        if (i < h_front || i >= h_front + h_sync) {
            dma_buf_hblank_vnorm[i] = hsync_inv_bit() | vsync_inv_bit();
            dma_buf_hblank_vsync[i] = hsync_inv_bit() | vsync_bit();
        } else {
            dma_buf_hblank_vnorm[i] = hsync_bit() | vsync_inv_bit();
            dma_buf_hblank_vsync[i] = hsync_bit() | vsync_bit();
        }
    }
    for (int i = 0; i < h_pixels; i++) {
        dma_buf_vblank_vnorm[i] = hsync_inv_bit() | vsync_inv_bit();
        dma_buf_vblank_vsync[i] = hsync_inv_bit() | vsync_bit();
    }
    
    dma_buf_desc_count = 2 * (v_front + v_sync + v_back + v_pixels);
    dma_buf_desc = (lldesc_t*)heap_caps_malloc(sizeof(lldesc_t) * dma_buf_desc_count, MALLOC_CAP_DMA);
    
    // Link DMA descriptors in circular chain
    int d = 0;
    for (int i = 0; i < dma_buf_desc_count; i++) {
        dma_buf_desc[i].qe.stqe_next = &dma_buf_desc[(i+1) % dma_buf_desc_count];
    }
    dma_buf_desc[dma_buf_desc_count-1].eof = true;
    
    // Configure vertical timing sequence
    d = 0;
    // Front porch
    for (int i = 0; i < v_front; i++) {
        dma_buf_desc[d].length = hblank_len;
        dma_buf_desc[d].buf = dma_buf_hblank_vnorm;
        d++;
        dma_buf_desc[d].length = pixel_len;
        dma_buf_desc[d].buf = dma_buf_vblank_vnorm;
        d++;
    }
    // Sync pulse
    for (int i = 0; i < v_sync; i++) {
        dma_buf_desc[d].length = hblank_len;
        dma_buf_desc[d].buf = dma_buf_hblank_vsync;
        d++;
        dma_buf_desc[d].length = pixel_len;
        dma_buf_desc[d].buf = dma_buf_vblank_vsync;
        d++;
    }
    // Back porch
    for (int i = 0; i < v_back; i++) {
        dma_buf_desc[d].length = hblank_len;
        dma_buf_desc[d].buf = dma_buf_hblank_vnorm;
        d++;
        dma_buf_desc[d].length = pixel_len;
        dma_buf_desc[d].buf = dma_buf_vblank_vnorm;
        d++;
    }
    // Active video area (to be filled later)
    for (int i = 0; i < v_pixels; i++) {
        dma_buf_desc[d].length = hblank_len;
        dma_buf_desc[d].buf = dma_buf_hblank_vnorm;
        d++;
        dma_buf_desc[d].length = 0;
        dma_buf_desc[d].buf = (uint8_t*)0;
        d++;
    }
}

// Configure I2S peripheral for VGA output
static void setup_i2s_output(const unsigned char* pin_map) {
    periph_module_enable(PERIPH_I2S1_MODULE);
    for (int i = 0; i < 8; i++) {
        int pin = pin_map[i];
        if (pin != 255) {
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
            gpio_matrix_out(pin, I2S1O_DATA_OUT0_IDX + i, false, false);
        }
    }
    
    // Reset I2S
    I2S1.conf.tx_reset = 1;
    I2S1.conf.tx_reset = 0;
    I2S1.conf.rx_reset = 1;
    I2S1.conf.rx_reset = 0;
    
    // Configure for LCD/VGA mode
    I2S1.conf2.val = 0;
    I2S1.conf2.lcd_en = 1;
    I2S1.conf2.lcd_tx_wrx2_en = 1;
    I2S1.sample_rate_conf.tx_bits_mod = 8;
    
    // Enable APLL for precise clock
    rtc_clk_apll_enable(true, 0x0A, 0x57, 0x07, 0x07);
    
    I2S1.clkm_conf.val = 0;
    I2S1.clkm_conf.clka_en = 1;
    I2S1.clkm_conf.clkm_div_num = 2;
    I2S1.sample_rate_conf.tx_bck_div_num = 1;
    
    // Configure FIFO for DMA
    I2S1.fifo_conf.val = 0;
    I2S1.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S1.fifo_conf.tx_fifo_mod = 1;
    I2S1.fifo_conf.dscr_en = 1;
    
    I2S1.conf_chan.tx_chan_mod = 1;
    I2S1.conf.tx_right_first = 1;
    
    // Install interrupt handler
    esp_intr_alloc(ETS_I2S1_INTR_SOURCE, 
                   ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM,
                   i2s_isr, NULL, &i2s_isr_handle);
}

// Start I2S output
static void start_i2s_output() {
    esp_intr_disable(i2s_isr_handle);
    
    I2S1.lc_conf.val |= I2S_IN_RST_M | I2S_OUT_RST_M;
    I2S1.lc_conf.val &= ~(I2S_IN_RST_M | I2S_OUT_RST_M);
    I2S1.lc_conf.val |= I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
    I2S1.out_link.addr = (uint32_t)&dma_buf_desc[0];
    I2S1.out_link.start = 1;
    
    I2S1.int_clr.val = I2S1.int_raw.val;
    I2S1.int_ena.out_eof = 1;
    esp_intr_enable(i2s_isr_handle);
    I2S1.conf.tx_start = 1;
}

// ==================== PUBLIC API ====================

// Initialize VGA with specified pin mapping, mode, and double buffering
void vga_init(const unsigned char* pin_map, const int* mode, bool double_buffered) {
    h_front = mode[0]; h_sync = mode[1]; h_back = mode[2]; h_pixels = mode[3];
    v_front = mode[4]; v_sync = mode[5]; v_back = mode[6]; v_pixels = mode[7];
    v_div = mode[8]; pixel_clock = mode[9]; v_polarity = mode[10]; h_polarity = mode[11];
    
    num_fbs = double_buffered ? 2 : 1;
    for (int i = 0; i < num_fbs; i++) {
        framebuffer[i] = alloc_framebuffer(i);
    }
    active_fb = 0;
    back_fb = (active_fb + 1) % num_fbs;
    
    allocate_vga_i2s_buffers();
    set_active_framebuffer(framebuffer[active_fb]);
    setup_i2s_output(pin_map);
    start_i2s_output();
}

// Swap front and back buffers (optionally wait for vsync)
void vga_swap_buffers(bool wait_vsync) {
    if (wait_vsync) {
        vga_frame_count = 0;
        while (vga_frame_count == 0) {
            vTaskDelay(1);
        }
    }
    active_fb = back_fb;
    back_fb = (active_fb + 1) % num_fbs;
    set_active_framebuffer(framebuffer[active_fb]);
}

// Clear back buffer with specified color
void vga_clear_screen(uint8_t color) {
    if (!framebuffer_linear[back_fb]) return;
    unsigned char clear_byte = sync_bits() | (color & 0x3f);
    memset(framebuffer_linear[back_fb], clear_byte, x_res() * y_res());
}

// Get pointer to current back buffer
unsigned char** vga_get_framebuffer() {
    return framebuffer[back_fb];
}

// Get linear framebuffer pointer
uint8_t* vga_get_raw_framebuffer() {
    return framebuffer_linear[back_fb];
}

// Get current sync bits value
unsigned char vga_get_sync_bits() {
    return sync_bits();
}

// Get horizontal resolution
int vga_get_xres() {
    return x_res();
}

// Get vertical resolution
int vga_get_yres() {
    return y_res();
}

// Set buffer update flag
void vga_set_buffer_needs_update(bool update) {
    buffer_needs_update = update;
}