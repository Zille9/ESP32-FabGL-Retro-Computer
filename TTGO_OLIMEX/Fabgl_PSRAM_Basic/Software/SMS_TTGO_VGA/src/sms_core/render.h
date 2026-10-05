// render.h
#ifndef _RENDER_H_
#define _RENDER_H_

/* Pack RGB data into a 16-bit RGB 5:6:5 format */
#define MAKE_PIXEL(r,g,b)   (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F))

/* Used for blanking a line in whole or in part */
#define BACKDROP_COLOR      (0x10 | (vdp.reg[7] & 0x0F))

/* Function prototypes */
void render_init(void);
void render_reset(void);
void render_bg_gg(int line);
void render_bg_sms(int line);
void render_obj(int line);
void render_line(int line);
void update_cache(void);
void palette_sync(int index);
void vramMarkTileDirty(int tile);

// VGA-specific functions
void render_set_vga_mode(int enabled);

// External reference to VGA display
extern void* g_vga_display;

// Callback for VGA drawing (implemented in C++)
extern void vga_draw_scanline(void* display, int line, uint8_t* pixels, int width);

#endif