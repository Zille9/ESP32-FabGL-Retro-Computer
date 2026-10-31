// Classic99 for ESP32 - TMS9918A VDP
// Derived from Classic99 console/tivdp.cpp and Tiemul.cpp (rvdpbyte/wvdpbyte/wVDPreg)
// (C) 2005-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.

#pragma once
#include "ti_types.h"

namespace fabgl { class VGAController; }

#define VDPS_INT  0x80
#define VDPS_5SPR 0x40
#define VDPS_SCOL 0x20

extern Byte VDPREG[8];
extern Byte VDPS;

void vdpInit(fabgl::VGAController *display);   // allocates VRAM, builds the palette
void vdpReset();
Byte vdpRead(Word x);
void vdpWrite(Word x, Byte c);
Byte *vdpRam();                 // direct access to the 16K VRAM (disk DSR transfers)
inline bool vdpIntPending() { return (VDPS & VDPS_INT) && (VDPREG[1] & 0x20); }

// advance the beam by CPU cycles; returns true when a frame has just completed
bool vdpAdvance(int cycles);

// output geometry inside the 320x240 VGA mode
#define VDP_SCREEN_W 320
#define VDP_SCREEN_H 240
