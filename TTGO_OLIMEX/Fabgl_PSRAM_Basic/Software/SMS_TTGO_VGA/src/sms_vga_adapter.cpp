// sms_vga_adapter.cpp
#include "sms_vga_adapter.h"
#include "vga_6bit.h"
#include "gb_sdl_font8x8.h"

// Static variable definitions
uint8_t** SMSVGADisplay::framebuffer_ptr = nullptr;
uint8_t* SMSVGADisplay::framebuffer_linear = nullptr;
uint8_t SMSVGADisplay::sync_bits_val = 0;
int SMSVGADisplay::cursorX = 0;
int SMSVGADisplay::cursorY = 0;
uint8_t SMSVGADisplay::textColor = VGA_COLOR_WHITE;
uint8_t SMSVGADisplay::bgColor = VGA_COLOR_BLACK;
bool SMSVGADisplay::initialized = false;
bool SMSVGADisplay::emulationMode = false;
bool SMSVGADisplay::bufferNeedsUpdate = true;
int SMSVGADisplay::screen_x_offset = 0;
uint8_t SMSVGADisplay::color_table[128];
uint8_t SMSVGADisplay::sms_color_table[32];

// Convert RGB565 color to 6-bit VGA format
uint8_t SMSVGADisplay::rgb565_to_vga6(uint16_t rgb565) {
    uint8_t r = (rgb565 >> 8) & 0xF8;
    uint8_t g = (rgb565 >> 3) & 0xFC;
    uint8_t b = (rgb565 << 3) & 0xF8;
    
    uint8_t r2 = r >> 6;
    uint8_t g2 = g >> 6;
    uint8_t b2 = b >> 6;
    
    return (b2 << 4) | (g2 << 2) | r2;
}

// Initialize color lookup table
void SMSVGADisplay::initColorTable() {
    for (int i = 0; i < 128; i++) {
        color_table[i] = sync_bits_val | (i & 0x3F);
    }
    
    // Paleta SMS por defecto (32 colores exactos)
    static const uint8_t default_palette[32] = {
        0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,  // Grises 0-7
        0xE0, 0xE2, 0xE4, 0xE6,                          // Rojos 8-11
        0x1C, 0x1D, 0x1E, 0x1F,                         // Verdes 12-15
        0x03, 0x07, 0x0B, 0x0F,                         // Azules 16-19
        0xFC, 0xFD, 0xFE, 0xFF,                         // Amarillos 20-23
        0xE3, 0xE7, 0xEB, 0xEF,                         // Magentas 24-27
        0x1F, 0x3F, 0x5F, 0x7F                          // Cianes 28-31
    };
    
    for (int i = 0; i < 32; i++) {
        uint8_t rgb332 = default_palette[i];
        uint8_t r = (rgb332 >> 5) & 0x07;
        uint8_t g = (rgb332 >> 2) & 0x07;
        uint8_t b = rgb332 & 0x03;
        
        uint8_t vga_r = r >> 1;
        uint8_t vga_g = g >> 1;
        uint8_t vga_b = b;
        
        uint8_t vga = (vga_b << 4) | (vga_g << 2) | vga_r;
        sms_color_table[i] = sync_bits_val | vga;
    }
}

// Update color table from SMS palette
void SMSVGADisplay::updateColorTable(uint8_t* cramd) {
    if (!cramd) return;
    for (int i = 0; i < 32; i++) {
        sms_color_table[i] = sync_bits_val | (cramd[i] & 0x3F);
    }
}

// Fast pixel drawing function
void SMSVGADisplay::fastDrawPixel(int x, int y, uint8_t color) {
    if (!framebuffer_ptr) return;
    if (x < 0 || x >= getWidth() || y < 0 || y >= getHeight()) {
        return;
    }
    int final_x = (x + screen_x_offset) ^ 2;
    if (final_x < 0 || final_x >= getWidth()) {
        return;
    }
    framebuffer_ptr[y][final_x] = color_table[color & 0x7F];
}

// Initialize VGA display hardware
void SMSVGADisplay::init() {
    if (initialized) return;
    
    #ifdef DEBUG
        Serial.println("SMSVGADisplay: Initializing VGA...");
    #endif
    
    static const unsigned char vga_pins[] = VGA_PINS;
    
    vga_init(vga_pins, VgaMode_vga_mode_320x240, false);
    
    framebuffer_ptr = vga_get_framebuffer();
    framebuffer_linear = vga_get_raw_framebuffer();
    sync_bits_val = vga_get_sync_bits();
    
    initColorTable();
    
    initialized = true;
    
    #ifdef DEBUG
        Serial.printf("SMSVGADisplay: %dx%d initialized\n", getWidth(), getHeight());
    #endif
}

// Fill entire screen with a single color
void SMSVGADisplay::fillScreen(uint16_t color) {
    uint8_t vga_color = rgb565_to_vga6(color);
    if (framebuffer_linear) {
        memset(framebuffer_linear, sync_bits_val | vga_color, getWidth() * getHeight());
    }
}

// Draw a filled rectangle
void SMSVGADisplay::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (!framebuffer_ptr) return;
    
    uint8_t vga_color = rgb565_to_vga6(color);
    uint8_t pixel = sync_bits_val | vga_color;
    int x1 = max(x, 0);
    int y1 = max(y, 0);
    int x2 = min(x + w, getWidth());
    int y2 = min(y + h, getHeight());
    
    for (int row = y1; row < y2; row++) {
        uint8_t* line = framebuffer_ptr[row];
        for (int col = x1; col < x2; col++) {
            int final_col = (col + screen_x_offset) ^ 2;
            line[final_col] = pixel;
        }
    }
}

// Draw hollow rectangle outline
void SMSVGADisplay::drawRect(int x, int y, int w, int h, uint16_t color) {
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y + 1, 1, h - 2, color);
    fillRect(x + w - 1, y + 1, 1, h - 2, color);
}

// Draw a single character using 8x8 font
void SMSVGADisplay::drawChar(char c, int x, int y, uint8_t color, uint8_t backcolor) {
    int auxId = c << 3;
    
    for (int row = 0; row < 8; row++) {
        int y_pos = y + row;
        if (y_pos >= getHeight()) break;
        
        uint8_t bits = gb_sdl_font_8x8[auxId + row];
        
        for (int col = 0; col < 8; col++) {
            int x_pos = x + (6 - col);
            if (x_pos >= getWidth()) continue;
            
            uint8_t auxColor = (bits >> col) & 0x01;
            fastDrawPixel(x_pos, y_pos, auxColor ? color : backcolor);
        }
    }
}

// Draw a string of text
void SMSVGADisplay::drawString(const char* text, int x, int y, int font) {
    (void)font;
    int px = x;
    
    while (*text) {
        drawChar(*text++, px, y, textColor, bgColor);
        px += 7;
    }
}

// Set text foreground and background colors
void SMSVGADisplay::setTextColor(uint16_t fg, uint16_t bg) {
    textColor = rgb565_to_vga6(fg);
    bgColor = rgb565_to_vga6(bg);
}

// Set cursor position
void SMSVGADisplay::setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
}

// Print string
void SMSVGADisplay::print(const char* text) {
    drawString(text, cursorX, cursorY);
    cursorX += strlen(text) * 7;
}

// Print char
void SMSVGADisplay::print(char c) {
    char str[2] = {c, 0};
    print(str);
}

// Calculate text width
int SMSVGADisplay::textWidth(const char* text) {
    return strlen(text) * 7;
}

// Get screen width
int SMSVGADisplay::getWidth() {
    return vga_get_xres();
}

// Get screen height
int SMSVGADisplay::getHeight() {
    return vga_get_yres();
}

// Enter emulation mode
void SMSVGADisplay::beginEmulation() {
    emulationMode = true;
    fillScreen(VGA_COLOR_BLACK_RGB565);
}

// Exit emulation mode
void SMSVGADisplay::endEmulation() {
    emulationMode = false;
}

// Update display
void SMSVGADisplay::updateDisplay() {
    if (emulationMode && bufferNeedsUpdate) {
        bufferNeedsUpdate = false;
    }
}

// Draw scanline for SMS
void SMSVGADisplay::drawScanline(int y, uint8_t* pixels, int width) {
    if (!emulationMode || !framebuffer_ptr || !pixels) return;

    // Centrar verticalmente: SMS = 192 líneas, VGA = 240 líneas
    // Offset = (240 - 192) / 2 = 24 líneas
    const int y_offset = (getHeight() - 192) / 2;
    int screen_y = y + y_offset;

    if (screen_y < 0 || screen_y >= getHeight()) return;

    int x_offset = (getWidth() - width) / 2;
    if (x_offset < 0) x_offset = 0;

    for (int x = 0; x < width && x + x_offset < getWidth(); x++) {
        uint8_t color_index = pixels[x] & 0x1F;
        int final_x = x + x_offset;
        int final_x_swapped = (final_x + screen_x_offset) ^ 2;
        if (final_x_swapped >= 0 && final_x_swapped < getWidth()) {
            framebuffer_ptr[screen_y][final_x_swapped] = sms_color_table[color_index];
        }
    }

    bufferNeedsUpdate = true;
}

// Get raw framebuffer pointer
uint8_t* SMSVGADisplay::getRawFramebuffer() {
    return framebuffer_linear;
}

// Get sync bits
uint8_t SMSVGADisplay::getSyncBits() {
    return sync_bits_val;
}

// Set screen offset
void SMSVGADisplay::setScreenOffset(int offset) {
    screen_x_offset = offset;
}

// Get screen offset
int SMSVGADisplay::getScreenOffset() {
    return screen_x_offset;
}

// ============================================================
// FUNCIONES PUENTE PARA SER LLAMADAS DESDE C (render.c)
// ============================================================

extern "C" void vga_draw_scanline(void* display, int line, uint8_t* pixels, int width) {
    SMSVGADisplay* disp = (SMSVGADisplay*)display;
    if (disp) {
        disp->drawScanline(line, pixels, width);
    }
}

extern "C" void vga_update_palette(void* display, uint8_t* cramd) {
    SMSVGADisplay* disp = (SMSVGADisplay*)display;
    if (disp) {
        disp->updateColorTable(cramd);
    }
}