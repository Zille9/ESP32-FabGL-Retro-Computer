// Classic99 for ESP32 - TMS9918A VDP
// Derived from Classic99 console/tivdp.cpp and Tiemul.cpp (rvdpbyte/wvdpbyte/wVDPreg/GetRealVDP)
// (C) 2005-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
//
// Port notes: register/address/prefetch behaviour is Classic99's. Rendering follows
// Classic99's per-mode rules but draws one scanline at a time into FabGL's raw
// framebuffer. Sprites are evaluated per line (Classic99 re-scanned all 32 sprites and
// cleared a 48K collision buffer per line) with the same 4-per-line, 5th-sprite and
// coincidence rules. F18A, 80 columns and 128K are not ported.

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include "fabgl.h"
#include "vdp9918.h"
#include "bus.h"
#include "cpu9900.h"

Byte VDPREG[8];
Byte VDPS;

static Byte *VDP = nullptr;             // 16K VRAM (internal RAM)
static Word VDPADD = 0;
static int vdpaccess = 0;
static Byte vdpprefetch = 0;
static int vdpscanline = 0;
static int cycleAccum = 0;              // in 1/262 cycle units, so a line is exactly DEFAULT_60HZ_CPF

static fabgl::VGAController *disp = nullptr;
static uint8_t rawPalette[16];

// TI palette, from Classic99's F18APaletteReset (12-bit RGB)
static const uint16_t tiPalette12[16] = {
    0x000, 0x000, 0x2C3, 0x5D6, 0x54F, 0x76F, 0xD54, 0x4EF,
    0xF54, 0xF76, 0xDC3, 0xED6, 0x2B2, 0xC5C, 0xCCC, 0xFFF
};

// Classic99 timing: line 0 is the top of the top border, active display starts at 27,
// vertical interrupt at 192+27, 262 lines per frame
#define FIRST_ACTIVE_LINE 27
#define VINT_LINE (192 + 27)
// the 320x240 output shows 24 border lines above and below the 192 active lines
#define OUT_TOP_BORDER 24
#define OUT_LEFT 32

/////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////
static void buildNibbleMasks();

static uint8_t to2bit(int v4) {
    return (uint8_t)((v4 * 3 + 7) / 15);
}

void vdpInit(fabgl::VGAController *display) {
    disp = display;
    if (!VDP) {
        VDP = (Byte *)heap_caps_calloc(1, 0x4000, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    for (int i = 0; i < 16; i++) {
        int c = tiPalette12[i];
        rawPalette[i] = disp->createRawPixel(RGB222(to2bit((c >> 8) & 0xf), to2bit((c >> 4) & 0xf), to2bit(c & 0xf)));
    }
    buildNibbleMasks();
}

Byte *vdpRam() {
    return VDP;
}

void vdpReset() {
    memset(VDPREG, 0, sizeof(VDPREG));  // registers cleared on reset per datasheet
    VDPS = 0;
    VDPADD = 0;
    vdpaccess = 0;
    vdpprefetch = 0;
    vdpscanline = 0;
    cycleAccum = 0;
}

/////////////////////////////////////////////////////////
// CPU port (from Classic99 rvdpbyte / wvdpbyte)
/////////////////////////////////////////////////////////
static inline void increment_vdpadd() {
    VDPADD = (VDPADD + 1) & 0x3fff;
}

// 4K/16K address translation, confirmed against a real console in Classic99
static inline int GetRealVDP() {
    if (VDPREG[1] & 0x80) {
        return VDPADD & 0x3fff;
    }
    return (VDPADD & 0x203f) | ((VDPADD & 0x0fc0) << 1) | ((VDPADD & 0x1000) >> 6);
}

IRAM_ATTR Byte vdpRead(Word x) {
    if ((x >= 0x8c00) || (x & 1)) return 0;

    if (x & 0x0002) {
        // status: does not affect prefetch or address; top flags clear on read
        Byte z = VDPS;
        VDPS &= 0x1f;
        vdpaccess = 0;
        return z;
    }

    vdpaccess = 0;
    Byte z = vdpprefetch;
    vdpprefetch = VDP[GetRealVDP()];
    increment_vdpadd();
    return z;
}

IRAM_ATTR void vdpWrite(Word x, Byte c) {
    if (x < 0x8c00 || (x & 1)) return;

    if (x & 0x0002) {
        if (0 == vdpaccess) {
            VDPADD = (VDPADD & 0xff00) | c;     // LSB
            vdpaccess = 1;
            return;
        }
        // MSB - flip-flop reset triggers action
        VDPADD = (VDPADD & 0x00ff) | (c << 8);
        vdpaccess = 0;
        if (VDPADD & 0x8000) {
            // register write; register number masked to 3 bits (verified on hardware)
            VDPREG[(VDPADD & 0x0700) >> 8] = VDPADD & 0xff;
        }
        if ((VDPADD & 0xC000) == 0) {
            // read setup: prefetch
            vdpprefetch = VDP[GetRealVDP()];
            increment_vdpadd();
        }
        VDPADD &= 0x3fff;
        return;
    }

    vdpaccess = 0;
    VDP[GetRealVDP()] = c;
    vdpprefetch = c;                            // verified on hardware
    increment_vdpadd();
}

/////////////////////////////////////////////////////////
// Table addresses (from Classic99 gettables, 9918A only)
/////////////////////////////////////////////////////////
static int SIT, CT, PDT, SAL, SDT, CTsize, PDTsize;

static IRAM_ATTR void gettables() {
    SIT = ((VDPREG[2] & 0x0f) << 10);
    SAL = ((VDPREG[5] & 0x7f) << 7);
    SDT = ((VDPREG[6] & 0x07) << 11);
    if (VDPREG[0] & 0x02) {
        CT = (VDPREG[3] & 0x80) ? 0x2000 : 0;
        CTsize = ((VDPREG[3] & 0x7f) << 6) | 0x3f;
        PDT = (VDPREG[4] & 0x04) ? 0x2000 : 0;
        PDTsize = ((VDPREG[4] & 0x03) << 11);
        if (VDPREG[1] & 0x10) {
            PDTsize |= 0x7ff;
        } else {
            PDTsize |= (CTsize & 0x7ff);
        }
    } else {
        CT = VDPREG[3] << 6;
        PDT = ((VDPREG[4] & 0x07) << 11);
        CTsize = 32;
        PDTsize = 2048;
    }
    SIT &= 0x3fff; SAL &= 0x3fff; SDT &= 0x3fff; CT &= 0x3fff; PDT &= 0x3fff;
}

/////////////////////////////////////////////////////////
// Line renderers
// They write FabGL raw pixels straight into the VGA scanline, 4 pixels per 32-bit
// store. Within a word FabGL stores pixels in the order x+2, x+3, x, x+1
// (VGA_PIXELINROW uses X^2), which nibbleMask[] accounts for.
/////////////////////////////////////////////////////////
static uint32_t nibbleMask[16];         // 0xff in the bytes whose pixel bit is set
static uint32_t lineColor[16];          // raw colour replicated x4, colour 0 = backdrop

#define OUT_WORDS (VDP_SCREEN_W / 4)
#define TI_WORD0 (OUT_LEFT / 4)

static void buildNibbleMasks() {
    for (int n = 0; n < 16; n++) {
        uint32_t m = 0;
        if (n & 8) m |= 0x00ff0000;     // pixel x+0 -> byte 2
        if (n & 4) m |= 0xff000000;     // pixel x+1 -> byte 3
        if (n & 2) m |= 0x000000ff;     // pixel x+2 -> byte 0
        if (n & 1) m |= 0x0000ff00;     // pixel x+3 -> byte 1
        nibbleMask[n] = m;
    }
}

static inline void put8w(uint32_t *d, Byte t, Byte fgc, Byte bgc) {
    uint32_t fg = lineColor[fgc], bg = lineColor[bgc];
    uint32_t m0 = nibbleMask[t >> 4], m1 = nibbleMask[t & 0x0f];
    d[0] = (fg & m0) | (bg & ~m0);
    d[1] = (fg & m1) | (bg & ~m1);
}

static IRAM_ATTR void VDPgraphics(uint32_t *d, int scanline) {
    int o = (scanline / 8) * 32;
    int i3 = scanline & 7;
    for (int i = 0; i < 32; i++, o++, d += 2) {
        Byte ch = VDP[SIT + o];
        Byte c = VDP[CT + (ch >> 3)];
        put8w(d, VDP[PDT + (ch << 3) + i3], c >> 4, c & 0x0f);
    }
}

static IRAM_ATTR void VDPgraphicsII(uint32_t *d, int scanline) {
    int o = (scanline / 8) * 32;
    int i3 = scanline & 7;
    int off = (scanline / 64) * 0x800;
    for (int i = 0; i < 32; i++, o++, d += 2) {
        Byte ch = VDP[SIT + o];
        Byte t = VDP[PDT + (((ch << 3) + off) & PDTsize) + i3];
        Byte c = VDP[CT + (((ch << 3) + off) & CTsize) + i3];
        put8w(d, t, c >> 4, c & 0x0f);
    }
}

static IRAM_ATTR void VDPmulticolor(uint32_t *d, int scanline, bool bitmap) {
    int o = (scanline / 8) * 32;
    int off = (scanline >> 2) & 0x06;
    int half = (scanline & 0x04) >> 2;
    int poff = (scanline / 64) * 0x800;
    for (int i = 0; i < 32; i++, o++, d += 2) {
        Byte ch = VDP[SIT + o];
        Byte c = bitmap ? VDP[PDT + (((ch << 3) + poff) & PDTsize) + (scanline & 0x04)]
                        : VDP[PDT + (ch << 3) + off + half];
        d[0] = lineColor[c >> 4];
        d[1] = lineColor[c & 0x0f];
    }
}

// text: 40 columns of 6 pixels starting at TI x=8 (not word aligned, so build bytes first)
static IRAM_ATTR void VDPtext(uint32_t *d, int scanline, bool bitmap, bool illegal) {
    Byte px[256];
    int o = (scanline / 8) * 40;
    int i3 = scanline & 7;
    int off = bitmap ? (scanline / 64) * 0x800 : 0;
    Byte fgc = VDPREG[7] >> 4;
    Byte bgc = VDPREG[7] & 0x0f;
    memset(px, 0, 8);
    memset(&px[248], 0, 8);
    for (int i2 = 8; i2 < 248; i2 += 6, o++) {
        Byte t;
        if (illegal) {
            t = 0xf0;                   // 4 foreground, 2 background pixels
        } else {
            Byte ch = VDP[SIT + o];
            t = bitmap ? VDP[PDT + (((ch << 3) + off) & PDTsize) + i3] : VDP[PDT + (ch << 3) + i3];
        }
        Byte *p = &px[i2];
        p[0] = (t & 0x80) ? fgc : bgc;
        p[1] = (t & 0x40) ? fgc : bgc;
        p[2] = (t & 0x20) ? fgc : bgc;
        p[3] = (t & 0x10) ? fgc : bgc;
        p[4] = (t & 0x08) ? fgc : bgc;
        p[5] = (t & 0x04) ? fgc : bgc;
    }
    for (int x = 0; x < 256; x += 4, d++) {
        // raw bytes in FabGL order x+2, x+3, x, x+1
        *d = (lineColor[px[x + 2]] & 0x000000ff) | (lineColor[px[x + 3]] & 0x0000ff00) |
             (lineColor[px[x]] & 0x00ff0000) | (lineColor[px[x + 1]] & 0xff000000);
    }
}

/////////////////////////////////////////////////////////
// Sprites for one line (rules from Classic99 DrawSprites)
/////////////////////////////////////////////////////////
static IRAM_ATTR void DrawSprites(uint8_t *dst, int scanline) {
    int highest = 31;
    for (int i1 = 0; i1 < 32; i1++) {
        if (VDP[SAL + (i1 << 2)] == 0xd0) {
            highest = i1 - 1;
            break;
        }
    }

    int b5OnLine = (VDPS & VDPS_5SPR) ? (VDPS & 0x1f) : -1;
    int size = (VDPREG[1] & 0x02) ? 16 : 8;
    int mag = (VDPREG[1] & 0x01) ? 2 : 1;
    int height = size * mag;

    // pick up to 4 sprites on this line, in priority order
    int visible[4];
    int nVis = 0;
    for (int i1 = 0; i1 <= highest; i1++) {
        int yy = VDP[SAL + (i1 << 2)] + 1;
        if (yy > 225) yy -= 256;
        int row = scanline - yy;
        if (row < 0 || row >= height) continue;
        if (nVis == 4) {
            if (b5OnLine == -1) b5OnLine = i1;
            break;
        }
        visible[nVis++] = i1;
    }

    uint32_t hit[8] = {0};              // coincidence bitmap for x = 0..255
    bool collision = false;

    // lowest priority first, so sprite 0 ends up on top
    for (int v = nVis - 1; v >= 0; v--) {
        int sal = SAL + (visible[v] << 2);
        int yy = VDP[sal] + 1;
        if (yy > 225) yy -= 256;
        int xx = VDP[sal + 1];
        int pat = VDP[sal + 2];
        Byte attr = VDP[sal + 3];
        Byte col = attr & 0x0f;
        if (attr & 0x80) xx -= 32;              // early clock
        if (size == 16) pat &= 0xfc;

        int row = (scanline - yy) / mag;
        int p_add = SDT + (pat << 3) + row;     // rows 8-15 continue into the lower-left quadrant
        uint32_t bits = VDP[p_add & 0x3fff] << 8;
        if (size == 16) bits |= VDP[(p_add + 16) & 0x3fff];
        if (!bits) continue;

        uint8_t raw = (uint8_t)lineColor[col];
        int width = size * mag;
        for (int px = 0; px < width; px++) {
            if (!(bits & (0x8000 >> (px / mag)))) continue;
            int x = xx + px;
            if (x < 0 || x > 255) continue;
            // even transparent sprites take part in coincidence
            uint32_t b = 1u << (x & 31);
            if (hit[x >> 5] & b) collision = true; else hit[x >> 5] |= b;
            if (col) VGA_PIXELINROW(dst, x + OUT_LEFT) = raw;
        }
    }

    if (collision) VDPS |= VDPS_SCOL;
    if (b5OnLine != -1) {
        VDPS |= VDPS_5SPR;
        VDPS = (VDPS & (VDPS_INT | VDPS_5SPR | VDPS_SCOL)) | (b5OnLine & 0x1f);
    } else {
        VDPS = (VDPS & (VDPS_INT | VDPS_5SPR | VDPS_SCOL)) | ((highest + 1) & 0x1f);
    }
}

/////////////////////////////////////////////////////////
// Output one VDP scanline (from Classic99 VDPdisplay)
/////////////////////////////////////////////////////////
static IRAM_ATTR void VDPdisplay(int scanline) {
    int gfxline = scanline - FIRST_ACTIVE_LINE;
    int outY = gfxline + OUT_TOP_BORDER;
    if (outY < 0 || outY >= VDP_SCREEN_H) return;

    uint8_t *dst = disp->getScanline(outY);
    uint32_t *dw = (uint32_t *)dst;
    for (int i = 1; i < 16; i++) lineColor[i] = rawPalette[i] * 0x01010101u;
    uint32_t backdrop = rawPalette[VDPREG[7] & 0x0f] * 0x01010101u;
    lineColor[0] = backdrop;

    if ((gfxline < 0) || (gfxline >= 192) || !(VDPREG[1] & 0x40)) {
        // border or blanked display
        for (int i = 0; i < OUT_WORDS; i++) dw[i] = backdrop;
        return;
    }

    for (int i = 0; i < TI_WORD0; i++) {
        dw[i] = backdrop;
        dw[OUT_WORDS - 1 - i] = backdrop;
    }

    gettables();
    uint32_t *d = dw + TI_WORD0;
    if ((VDPREG[1] & 0x18) == 0x18) {
        VDPtext(d, gfxline, false, true);
    } else if (VDPREG[1] & 0x10) {
        VDPtext(d, gfxline, (VDPREG[0] & 0x02) != 0, false);
    } else {
        if (VDPREG[1] & 0x08) {
            VDPmulticolor(d, gfxline, (VDPREG[0] & 0x02) != 0);
        } else if (VDPREG[0] & 0x02) {
            VDPgraphicsII(d, gfxline);
        } else {
            VDPgraphics(d, gfxline);
        }
        DrawSprites(dst, gfxline);      // no sprites in text modes
    }
}

/////////////////////////////////////////////////////////
// Beam timing (from Classic99 updateVDP, 60Hz)
/////////////////////////////////////////////////////////
IRAM_ATTR bool vdpAdvance(int cycles) {
    bool frameDone = false;
    cycleAccum += cycles * SCANLINES;
    while (cycleAccum >= DEFAULT_60HZ_CPF) {
        cycleAccum -= DEFAULT_60HZ_CPF;
        ++vdpscanline;
        if (vdpscanline == VINT_LINE) {
            VDPS |= VDPS_INT;
            frameDone = true;
        } else if (vdpscanline > SCANLINES - 1) {
            vdpscanline = 0;
        }
        VDPdisplay(vdpscanline);
    }
    return frameDone;
}
