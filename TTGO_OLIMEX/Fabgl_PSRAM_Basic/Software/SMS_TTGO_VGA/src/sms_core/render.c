// render.c - Versión corregida
#pragma GCC optimize ("O3")

#include "shared.h"
#include "render.h"

// VGA global pointer (void* para compatibilidad C)
void* g_vga_display = NULL;
static int vga_mode = 1;

// Function to set VGA mode
void render_set_vga_mode(int enabled) {
    vga_mode = enabled;
}

/* Background drawing function */
void (*render_bg)(int line);

/* Pointer to output buffer */
uint8 *linebuf;

//Each tile takes up 8*8=64 bytes. We have 512 tiles * 4 attribs, so 2K tiles max.
#define CACHEDTILES 512
#define ALIGN_DWORD 1

int16 cachePtr[512*4];
uint8 cacheStore[CACHEDTILES*64];
uint8 cacheStoreUsed[CACHEDTILES];
uint8 is_vram_dirty;
int cacheKillPtr=0;
int freePtr=0;

/* Pixel look-up table */
#include "lut.h"

/* Attribute expansion table */
uint32 atex[4] = {
    0x00000000,
    0x10101010,
    0x20202020,
    0x30303030,
};

/* Display sizes */
int vp_vstart;
int vp_vend;
int vp_hstart;
int vp_hend;

// Declaración de cramd (definida más abajo)
extern uint8_t cramd[0x20];

// Forward declarations
void render_bg_sms(int line);
void render_bg_gg(int line);
void render_obj(int line);
void palette_sync(int index);
void render_reset(void);
void render_init(void);
void vramMarkTileDirty(int index);
uint8 *getCache(int tile, int attr);
static void render_to_vga(const uint8_t* buf, int line);  // <-- DECLARACIÓN ADELANTADA

// Forward declaration of VGA draw function (implemented in C++)
extern void vga_draw_scanline(void* display, int line, uint8_t* pixels, int width);

void vramMarkTileDirty(int index) {
    int i=index;
    while (i<0x800) {
        if (cachePtr[i]!=-1) {
            freePtr=cachePtr[i]>>6;
            cacheStoreUsed[freePtr]=0;
            cachePtr[i]=-1;
        }
        i+=0x200;
    }
}

uint8 *getCache(int tile, int attr) {
    int n, i, x, y, c;
    int b0, b1, b2, b3;
    int i0, i1, i2, i3;
    int p;
    
    if (cachePtr[tile+(attr<<9)]!=-1) 
        return &cacheStore[cachePtr[tile+(attr<<9)]];
    
    do {
        i=freePtr;
        n=0;
        while (cacheStoreUsed[i] && n<CACHEDTILES) {
            i++;
            n++;
            if (i==CACHEDTILES) i=0;
        }
        if (n==CACHEDTILES) {
            vramMarkTileDirty(cacheKillPtr++);
            if (cacheKillPtr>=512) cacheKillPtr=0;
            i=freePtr;
        }
    } while (cacheStoreUsed[i]);
    
    cacheStoreUsed[i]=1;
    cachePtr[tile+(attr<<9)]=i<<6;
    
    for(y = 0; y < 8; y += 1) {
        b0 = vdp.vram[(tile << 5) | (y << 2) | (0)];
        b1 = vdp.vram[(tile << 5) | (y << 2) | (1)];
        b2 = vdp.vram[(tile << 5) | (y << 2) | (2)];
        b3 = vdp.vram[(tile << 5) | (y << 2) | (3)];
        for(x = 0; x < 8; x += 1) {
            i0 = (b0 >> (x ^ 7)) & 1;
            i1 = (b1 >> (x ^ 7)) & 1;
            i2 = (b2 >> (x ^ 7)) & 1;
            i3 = (b3 >> (x ^ 7)) & 1;
            c = (i3 << 3 | i2 << 2 | i1 << 1 | i0);
            if (attr==0) cacheStore[(i<<6)|(y<<3)|(x)]=c;
            if (attr==1) cacheStore[(i<<6)|(y<<3)|(x^7)]=c;
            if (attr==2) cacheStore[(i<<6)|((y^7)<<3)|(x)]=c;
            if (attr==3) cacheStore[(i<<6)|((y^7)<<3)|(x^7)]=c;
        }
    }
    return &cacheStore[i<<6];
}

#ifdef ALIGN_DWORD

static __inline__ uint32 read_dword(void *address)
{
    if ((uint32)address & 3)
    {
#ifdef LSB_FIRST
        return ( *((uint8 *)address) +
                (*((uint8 *)address+1) << 8)  +
                (*((uint8 *)address+2) << 16) +
                (*((uint8 *)address+3) << 24) );
#else
        return ( *((uint8 *)address+3) +
                (*((uint8 *)address+2) << 8)  +
                (*((uint8 *)address+1) << 16) +
                (*((uint8 *)address)   << 24) );
#endif
    }
    else
        return *(uint32 *)address;
}

static __inline__ void write_dword(void *address, uint32 data)
{
    if ((uint32)address & 3)
    {
#ifdef LSB_FIRST
            *((uint8 *)address) =    data;
            *((uint8 *)address+1) = (data >> 8);
            *((uint8 *)address+2) = (data >> 16);
            *((uint8 *)address+3) = (data >> 24);
#else
            *((uint8 *)address+3) =  data;
            *((uint8 *)address+2) = (data >> 8);
            *((uint8 *)address+1) = (data >> 16);
            *((uint8 *)address)   = (data >> 24);
#endif
        return;
    }
    else
        *(uint32 *)address = data;
}
#else
#define read_dword(address) *(uint32 *)address
#define write_dword(address,data) *(uint32 *)address=data
#endif

void render_init(void)
{
    render_reset();
}

inline uint8_t render_pixel(uint8 bx, uint8 sx)
{
    if (bx & 0x40)
        return (bx & 0x7F);
    if (!(bx & 0x20)) {
        if (!(sx & 0x0F))
            return (bx & 0x7F);
        return (sx & 0x0F) | 0x10 | 0x40;
    }
    if ((bx & 0x0F) || !(sx & 0x0F))
        return (bx & 0x7F);
    return (sx & 0x0F) | 0x10 | 0x40;
}

void render_reset(void)
{
    int i;
    
    memset(bitmap.data, 0, bitmap.pitch * bitmap.height);
    
    for(i = 0; i < PALETTE_SIZE; i += 1)
    {
        palette_sync(i);
    }
    
    for (i=0; i<512*4; i++) cachePtr[i]=-1;
    for (i=0; i<512; i++) vramMarkTileDirty(i);
    
    if(IS_GG)
    {
        vp_vstart = 24;
        vp_vend   = 168;
        vp_hstart = 6;
        vp_hend   = 26;
    }
    else
    {
        vp_vstart = 0;
        vp_vend   = 192;
        vp_hstart = 0;
        vp_hend   = 32;
    }
    
    render_bg = IS_GG ? render_bg_gg : render_bg_sms;
}

static uint32_t linebuf_[256];

void render_line(int line)
{
    if((line < vp_vstart) || (line >= vp_vend)) return;
    
    linebuf = (uint8_t*)linebuf_;
    
    if( (!(vdp.reg[1] & 0x40)) || (((vdp.reg[2] & 1) == 0) && (IS_SMS)))
    {
        memset(linebuf + (vp_hstart << 3), BACKDROP_COLOR, BMP_WIDTH);
    }
    else
    {
        render_bg(line);
        render_obj(line);
        if(vdp.reg[0] & 0x20)
        {
            memset(linebuf, BACKDROP_COLOR, 8);
        }
    }
    
    // Convert to VGA if enabled and display is set
    if (vga_mode && g_vga_display) {
        render_to_vga(linebuf, line);
    }
}



void render_bg_sms(int line)
{
    int locked = 0;
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10)) ? 0 : (0x100 - vdp.reg[8]);
    int column = vp_hstart;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    int shift = (hscroll & 7);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - shift];
    uint8 *ctp;

    if(shift)
    {
        int x, c, a;
        attr = nt[(column + nt_scroll) & 0x1F];
#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;
        for(x = shift; x < 8; x += 1)
        {
            ctp=getCache((attr&0x1ff), (attr>>9)&3);
            c = ctp[(v_row) | (x)];
            linebuf[(0 - shift) + (x)  ] = ((c) | (a));
        }
        column += 1;
    }

    for(; column < vp_hend; column += 1)
    {
        if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
        {
            locked = 1;
            v_row = (line & 7) << 3;
            nt = (uint16 *)&vdp.vram[((vdp.reg[2] << 10) & 0x3800) + ((line >> 3) << 6)];
        }

        attr = nt[(column + nt_scroll) & 0x1F];
#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        atex_mask = atex[(attr >> 11) & 3];
        ctp=getCache((attr&0x1ff), (attr>>9)&3);
        cache_ptr = (uint32 *)&ctp[(v_row)];
        
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }

    if(shift)
    {
        int x, c, a;
        char *p = &linebuf[(0 - shift)+(column << 3)];
        attr = nt[(column + nt_scroll) & 0x1F];
#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        a = (attr >> 7) & 0x30;
        for(x = 0; x < shift; x += 1)
        {
            ctp=getCache((attr&0x1ff), (attr>>9)&3);
            c = ctp[(v_row) | (x)];
            p[x] = ((c) | (a));
        }
    }
}

void render_bg_gg(int line)
{
    int v_line = (line + vdp.reg[9]) % 224;
    int v_row  = (v_line & 7) << 3;
    int hscroll = (0x100 - vdp.reg[8]);
    int column;
    uint16 attr;
    uint16 *nt = (uint16 *)&vdp.vram[vdp.ntab + ((v_line >> 3) << 6)];
    int nt_scroll = (hscroll >> 3);
    uint32 atex_mask;
    uint32 *cache_ptr;
    uint32 *linebuf_ptr = (uint32 *)&linebuf[0 - (hscroll & 7)];
    uint8_t *ctp;

    for(column = vp_hstart; column <= vp_hend; column += 1)
    {
        attr = nt[(column + nt_scroll) & 0x1F];
#ifndef LSB_FIRST
        attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
        atex_mask = atex[(attr >> 11) & 3];
        ctp=getCache((attr&0x1ff), (attr>>9)&3);
        cache_ptr = (uint32 *)&ctp[(v_row)];
        
        write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));
        write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
    }
}

void render_obj(int line)
{
    int i;
    uint8_t *ctp;
    int count = 0;
    int width = 8;
    int height = (vdp.reg[1] & 0x02) ? 16 : 8;
    uint8 *st = (uint8 *)&vdp.vram[vdp.satb];

    if(vdp.reg[1] & 0x01)
    {
        width *= 2;
        height *= 2;
    }

    for(i = 0; i < 64; i += 1)
    {
        int yp = st[i];
        if(yp == 208) return;
        yp += 1;
        if(yp > 240) yp -= 256;
        if((line >= yp) && (line < (yp + height)))
        {
            uint8 *linebuf_ptr;
            int start = 0;
            int end = width;
            int xp = st[0x80 + (i << 1)];
            int n = st[0x81 + (i << 1)];
            count += 1;
            if((vdp.limit) && (count == 9)) return;
            if(vdp.reg[0] & 0x08) xp -= 8;
            if(vdp.reg[6] & 0x04) n |= 0x0100;
            if(vdp.reg[1] & 0x02) n &= 0x01FE;
            linebuf_ptr = (uint8 *)&linebuf[xp];
            if(xp < 0) start = (0 - xp);
            if((xp + width) > 256) end = (256 - xp);
            if(vdp.reg[1] & 0x01) {
                int x;
                ctp=getCache((n&0x1ff)+((line - yp) >> 3), (n>>9)&3);
                uint8 *cache_ptr = (uint8 *)&ctp[(((line - yp) >> 1) << 3)];
                for(x = start; x < end; x += 1) {
                    uint8 sp = cache_ptr[(x >> 1)];
                    if(sp) {
                        uint8 bg = linebuf_ptr[x];
                        linebuf_ptr[x] = render_pixel(bg, sp);
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            } else {
                int x;
                ctp=getCache((n&0x1ff)+((line - yp) >> 3), (n>>9)&3);
                uint8 *cache_ptr = (uint8 *)&ctp[((line - yp) << 3)&0x38];
                for(x = start; x < end; x += 1) {
                    uint8 sp = cache_ptr[x];
                    if(sp) {
                        uint8 bg = linebuf_ptr[x];
                        linebuf_ptr[x] = render_pixel(bg, sp);
                        if(bg & 0x40) vdp.status |= 0x20;
                    }
                }
            }
        }
    }
}


uint8_t cramd[0x20] = {0};

// Añade estas líneas al principio del archivo (después de los includes)
extern void* g_vga_display;
extern void vga_update_palette(void* display, uint8_t* cramd);

void palette_sync(int index)
{
    if (IS_GG) {
        // Game Gear: 4 bits por canal, formato 0000BBBGGGRRR en dos bytes
        uint8_t lo = vdp.cram[(index << 1)];
        uint8_t hi = vdp.cram[(index << 1) | 1];
        uint8_t r = (lo >> 0) & 0x0F;
        uint8_t g = (lo >> 4) & 0x0F;
        uint8_t b = (hi >> 0) & 0x0F;
        // Escalar 4 bits a 2 bits para VGA
        cramd[index] = ((b >> 2) << 4) | ((g >> 2) << 2) | (r >> 2);
    } else {
        // SMS: 2 bits por canal, formato BBGGRR
        uint8_t c = vdp.cram[index];
        uint8_t r = (c >> 0) & 0x03;
        uint8_t g = (c >> 2) & 0x03;
        uint8_t b = (c >> 4) & 0x03;
        // Directamente 2 bits por canal → VGA6 (bbggrrr con 2 bits cada uno)
        cramd[index] = (b << 4) | (g << 2) | r;
    }

    if (g_vga_display) {
        vga_update_palette(g_vga_display, cramd);
    }
}

static void render_to_vga(const uint8_t* buf, int line)
{
    if (!g_vga_display) return;
    // Pasar directamente los índices de color sin convertir
    // drawScanline usa sms_color_table[] que ya tiene los colores correctos
    const uint8_t* src = buf + vp_hstart * 8;
    int n = (vp_hend - vp_hstart) * 8;
    vga_draw_scanline(g_vga_display, line, (uint8_t*)src, n);
}
