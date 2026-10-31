// Classic99 for ESP32 - TI-99/4A memory, CRU and peripheral bus
// Derived from Classic99 console/Tiemul.cpp (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.

#pragma once
#include "ti_types.h"

// CPU status flags
#define BIT_LGT 0x8000
#define BIT_AGT 0x4000
#define BIT_EQ  0x2000
#define BIT_C   0x1000
#define BIT_OV  0x0800
#define BIT_OP  0x0400
#define BIT_XOP 0x0200
#define INTMASK 0x000F

enum READACCESSTYPE {
    ACCESS_READ = 0,    // normal read
    ACCESS_RMW,         // read-before-write access
    ACCESS_FREE         // internal access, no side effects or wait states
};

extern int skip_interrupt;

// memory
extern Byte *consoleROM;        // 8K, internal RAM
extern Byte *scratchpad;        // 256 bytes at >8300
extern Byte *expRAM;            // 32K expansion: >2000-3FFF then >A000-FFFF
extern Byte *grom;              // console GROM >0000-5FFF (internal RAM)
extern Byte *cartGrom;          // cartridge GROM >6000-FFFF (PSRAM), indexed from 0
extern Byte *diskDSR;           // 8K TI disk controller ROM (>4000 at CRU >1100), or null
extern bool expansionEnabled;

// cartridge ROM at >6000: bank n lives at rom[n*8K]. bankMask = banks-1 (0 = not banked).
struct CartState {
    Byte *rom = nullptr;
    int bankMask = 0;
    bool inverted = false;      // 379-style inverted bank select
    int bank = 0;
};
extern CartState cart;

Word romword(Word x, READACCESSTYPE rmw = ACCESS_READ);
void wrword(Word x, Word y);
Byte rcpubyte(Word x, READACCESSTYPE rmw = ACCESS_READ);
void wcpubyte(Word x, Byte c);
Byte GetSafeCpuByte(int x, int bank);

void wcru(Word ad, int bt);
int  rcru(Word ad);

void busReset();
bool busDiskDsrActive();       // TI disk controller DSR ROM currently paged in
void busAddCycles(int n);       // forwards to the current CPU
void update9901(int cycles);    // advance the 9901 timer
bool interruptPending();        // VDP or 9901 interrupt requested and unmasked by the 9901

// keyboard matrix shared with the PS/2 task: kbMatrix[column] bit r == 1 means key at row r is down
// column 8 = alpha lock state (bit 4 = alpha lock key down)
extern volatile Byte kbMatrix[8];
extern volatile Byte kbMatrixNoArrows[8];   // same, but arrow keys not mapped to FCTN+E/S/D/X
extern volatile Byte typedMatrix[8];        // injected keys (serial "type" command)
extern volatile bool alphaLockDown;
extern volatile Byte joy1;      // bits: 0 fire, 1 left, 2 right, 3 down, 4 up
void busFrameTick();            // once per video frame

void debug_write(const char *s, ...);
void warn(const char *s);
