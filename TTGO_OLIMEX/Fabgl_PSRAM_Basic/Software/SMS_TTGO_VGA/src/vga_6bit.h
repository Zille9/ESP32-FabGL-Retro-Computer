// vga_6bit.h
#ifndef VGA_6BIT_H_FILE
#define VGA_6BIT_H_FILE

#include <cstdint>
#include <Arduino.h>

// Available VGA modes
extern const int VgaMode_vga_mode_320x240[12];
extern const int VgaMode_vga_mode_320x200[12];
extern const int VgaMode_vga_mode_256x240[12];

// Main VGA functions
void vga_init(const unsigned char *pin_map, const int *mode, bool double_buffered);
void vga_swap_buffers(bool wait_vsync = true);
void vga_clear_screen(uint8_t color);
unsigned char **vga_get_framebuffer();
uint8_t* vga_get_raw_framebuffer();
unsigned char vga_get_sync_bits();
int vga_get_xres();
int vga_get_yres();
void vga_set_buffer_needs_update(bool update);

// 6-bit VGA color definitions for TTGO VGA32
#define VGA_COLOR_RED     0x03
#define VGA_COLOR_GREEN   0x0C
#define VGA_COLOR_BLUE    0x30
#define VGA_COLOR_WHITE   0x3F
#define VGA_COLOR_BLACK   0x00
#define VGA_COLOR_YELLOW  0x0F
#define VGA_COLOR_CYAN    0x3C
#define VGA_COLOR_MAGENTA 0x33

#endif