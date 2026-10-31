// Classic99 for ESP32 - TI-99/4A memory, CRU and peripheral bus
// Derived from Classic99 console/Tiemul.cpp (rcpubyte, wcpubyte, rgrmbyte, wgrmbyte,
// wcru, rcru, do1) (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.

#include <Arduino.h>
#include <stdarg.h>
#include <esp_attr.h>
#include "bus.h"
#include "cpu9900.h"
#include "vdp9918.h"
#include "sound9919.h"
#include "speech.h"
#include "ticc.h"

int skip_interrupt = 0;

Byte *consoleROM = nullptr;
Byte *scratchpad = nullptr;
Byte *expRAM = nullptr;
Byte *grom = nullptr;
Byte *cartGrom = nullptr;
Byte *diskDSR = nullptr;
bool expansionEnabled = true;

CartState cart;

volatile Byte kbMatrix[8];
volatile Byte kbMatrixNoArrows[8];
volatile Byte typedMatrix[8];
volatile bool alphaLockDown = true;     // TI software generally expects upper case
volatile Byte joy1 = 0;

// like Classic99: once a program scans joystick 1, arrow keys stop acting as
// FCTN+E/S/D/X for a while (180 frames) so they can drive the joystick
static int joyScanFrames = 0;

void busFrameTick() {
    if (joyScanFrames > 0) --joyScanFrames;
}

// GROM address counter and prefetch (single base: console + cartridge share one 64K space)
static Word GRMADD = 0;

// console GROMs are in internal RAM, cartridge GROMs in PSRAM
static inline Byte gromByte(Word a) {
    return (a < 0x6000) ? grom[a] : cartGrom[a - 0x6000];
}
static Byte grmaccess = 2;
static Byte grmdata = 0;

// CRU / 9901
static Byte CRU[4096];
static int nCurrentDSR = -1;
static int timer9901 = 0;
static int timer9901Read = 0;
static int starttimer9901 = 0;
static int timer9901IntReq = 0;
static int CRUTimerTicks = 0;

bool busDiskDsrActive() {
    return (nCurrentDSR == 0x01) && diskDSR;
}

void busAddCycles(int n) {
    pCPU->nCycleCount += (n);
}

// 8-bit multiplexed bus: one 4-cycle wait state per word access, counted on the even byte
#define WAITSTATE(x, rmw) do { if (((rmw) != ACCESS_FREE) && (((x) & 1) == 0)) pCPU->nCycleCount += (4); } while (0)

/////////////////////////////////////////////////////////
// Word access - the 9900 reads LSB then MSB (verified in Classic99)
/////////////////////////////////////////////////////////
IRAM_ATTR Word romword(Word x, READACCESSTYPE rmw) {
    x &= 0xfffe;
    Word lsb = rcpubyte(x + 1, rmw);
    Word msb = rcpubyte(x, rmw);
    return (msb << 8) + lsb;
}

IRAM_ATTR void wrword(Word x, Word y) {
    x &= 0xfffe;
    wcpubyte(x + 1, (Byte)(y & 0xff));
    wcpubyte(x, (Byte)(y >> 8));
}

static inline Byte *expPtr(Word x) {
    // >2000-3FFF -> 0-1FFF, >A000-FFFF -> 2000-7FFF
    return (x < 0x4000) ? &expRAM[x - 0x2000] : &expRAM[x - 0xa000 + 0x2000];
}

/////////////////////////////////////////////////////////
// GROM (from Classic99 ReadValidGrom / WriteValidGrom)
/////////////////////////////////////////////////////////
static inline void IncrementGROMAddress() {
    Word base = GRMADD & 0xe000;
    GRMADD = ((GRMADD + 1) & 0x1fff) | base;
}

static Byte rgrmbyte(Word x) {
    if (x >= 0x9c00) return 0;
    if (x & 0x0002) {
        // address read is destructive
        grmaccess = 2;
        Byte z = (GRMADD & 0xff00) >> 8;
        GRMADD = ((GRMADD & 0xff) << 8) | (GRMADD & 0xff);
        pCPU->nCycleCount += (13);
        return z;
    }
    grmaccess = 2;
    Byte z = grmdata;
    grmdata = gromByte(GRMADD);
    IncrementGROMAddress();
    pCPU->nCycleCount += (19);
    return z;
}

static void wgrmbyte(Word x, Byte c) {
    if (x < 0x9c00) return;
    if (x & 0x0002) {
        GRMADD = (GRMADD << 8) | c;
        grmaccess--;
        if (grmaccess == 0) {
            grmaccess = 2;
            pCPU->nCycleCount += (21);
            grmdata = gromByte(GRMADD);
            IncrementGROMAddress();
        } else {
            pCPU->nCycleCount += (15);
        }
    } else {
        // GROM is read-only here; data writes still advance the address and prefetch
        grmaccess = 2;
        grmdata = gromByte(GRMADD);
        IncrementGROMAddress();
        pCPU->nCycleCount += (22);
    }
}

/////////////////////////////////////////////////////////
// CPU byte read
/////////////////////////////////////////////////////////
IRAM_ATTR Byte rcpubyte(Word x, READACCESSTYPE rmw) {
    switch (x & 0xe000) {
        case 0x0000:                        // console ROM, 16-bit, no wait states
            return consoleROM[x];

        case 0x2000:
        case 0xa000:
        case 0xc000:
        case 0xe000:                        // 32K expansion
            WAITSTATE(x, rmw);
            return expansionEnabled ? *expPtr(x) : 0;

        case 0x6000:                        // cartridge ROM
            WAITSTATE(x, rmw);
            if (cart.rom) {
                return cart.rom[(cart.bank << 13) + (x - 0x6000)];
            }
            return 0;

        case 0x4000:                        // DSR ROM
            WAITSTATE(x, rmw);
            if ((nCurrentDSR == 0x01) && diskDSR) {
                if (x >= 0x5ff0) {
                    return (rmw == ACCESS_FREE) ? 0 : ReadTICCRegister(x);
                }
                return diskDSR[x - 0x4000];
            }
            return 0;

        case 0x8000:
            switch (x & 0xfc00) {
                case 0x8000:                // scratchpad, 256 bytes mirrored, no wait states
                    return scratchpad[x & 0xff];
                case 0x8800:                // VDP read data / status
                    WAITSTATE(x, rmw);
                    if ((x & 1) || (rmw == ACCESS_FREE)) return 0;
                    return vdpRead(x);
                case 0x9800:                // GROM read data / address
                    WAITSTATE(x, rmw);
                    if ((x & 1) || (rmw == ACCESS_FREE)) return 0;
                    return rgrmbyte(x);
                case 0x9000:                // speech read data / status
                    WAITSTATE(x, rmw);
                    if ((x & 1) || (rmw == ACCESS_FREE) || !speechEnabled()) return 0;
                    return speechRead();    // timing handled there
                default:                    // sound, VDP write, speech write, GROM write: no read
                    WAITSTATE(x, rmw);
                    return 0;
            }
    }
    return 0;
}

Byte GetSafeCpuByte(int x, int /*bank*/) {
    return rcpubyte((Word)x, ACCESS_FREE);
}

/////////////////////////////////////////////////////////
// CPU byte write
/////////////////////////////////////////////////////////
IRAM_ATTR void wcpubyte(Word x, Byte c) {
    if ((x & 0x01) == 0) {
        pCPU->nCycleCount += (4);             // wait state, cancelled below for ROM and scratchpad
    }

    switch (x & 0xe000) {
        case 0x0000:                        // console ROM: no wait state, not writable
            if ((x & 0x01) == 0) pCPU->nCycleCount += (-4);
            return;

        case 0x2000:
        case 0xa000:
        case 0xc000:
        case 0xe000:
            if (expansionEnabled) *expPtr(x) = c;
            return;

        case 0x6000:
            if (cart.rom && cart.bankMask) {
                // collect bits from address and data buses - x is address, c is data
                int bits = (c << 13) | (x & 0x1fff);
                if (cart.inverted) {
                    cart.bank = ((~bits) >> 1) & cart.bankMask;
                } else {
                    cart.bank = (bits >> 1) & cart.bankMask;
                }
            }
            return;

        case 0x4000:
            if ((nCurrentDSR == 0x01) && diskDSR) {
                WriteTICCRegister(x, c);
            }
            return;

        case 0x8000:
            switch (x & 0xfc00) {
                case 0x8000:                // scratchpad
                    if ((x & 0x01) == 0) pCPU->nCycleCount += (-4);
                    scratchpad[x & 0xff] = c;
                    return;
                case 0x8400:                // sound chip
                    if (x & 1) return;
                    soundWrite(c);
                    pCPU->nCycleCount += (28);    // verified on hardware (Classic99)
                    return;
                case 0x8c00:                // VDP write data / address
                    if (x & 1) return;
                    vdpWrite(x, c);
                    return;
                case 0x9400:                // speech write data
                    if ((x & 1) || !speechEnabled()) return;
                    speechWrite(c);         // timing handled there
                    return;
                case 0x9c00:                // GROM write data / address
                    if (x & 1) return;
                    wgrmbyte(x, c);
                    return;
                default:
                    return;
            }
    }
}

/////////////////////////////////////////////////////////
// 9901 timer (from Classic99 do1) - call with elapsed CPU cycles
/////////////////////////////////////////////////////////
IRAM_ATTR void update9901(int cycles) {
    CRUTimerTicks += cycles;
    int nTimerCnt = CRUTimerTicks >> 6;     // decrements every 64 clocks
    if (nTimerCnt) {
        if (timer9901 == 0) {
            timer9901 = 0x4000 - nTimerCnt;
        } else {
            timer9901 -= nTimerCnt;
        }
        CRUTimerTicks -= (nTimerCnt << 6);

        if (timer9901 < 1) {
            timer9901 = starttimer9901 + timer9901;
            timer9901 &= 0x3fff;
            timer9901IntReq = 1;
        }
        if (CRU[0] != 1) {
            timer9901Read = timer9901;
        }
    }
}

IRAM_ATTR bool interruptPending() {
    return (vdpIntPending() && CRU[2]) || (timer9901IntReq && CRU[3]);
}

/////////////////////////////////////////////////////////
// CRU write (from Classic99 wcru - console 9901 and the TI disk card only)
/////////////////////////////////////////////////////////
IRAM_ATTR void wcru(Word ad, int bt) {
    if (ad >= 0x800) {
        // peripheral cards: bit 0 of each >1x00 base enables its DSR ROM
        ad <<= 1;
        int base = (ad >> 8) & 0xf;
        if ((ad & 0xff) == 0) {
            if (bt) {
                nCurrentDSR = base;
            } else if (base == nCurrentDSR) {
                nCurrentDSR = -1;
            }
        }
        if ((ad & 0xff00) == 0x1100 && diskDSR) {
            WriteTICCCRU((ad & 0xff) >> 1, bt);
        }
        return;
    }
    if (ad >= 0x400) {
        CRU[ad & 0xfff] = bt ? 1 : 0;       // unused space (SuperSpace/PopCart banking not ported)
        return;
    }

    ad = (ad & 0x01f);                      // 9901 bits repeat through the first 1K

    if (bt) {
        if ((ad > 0) && (ad < 16) && (CRU[0] == 1)) {
            if (ad != 15) {
                // write to the 9901 clock register; non-zero starts it immediately
                starttimer9901 |= (0x01 << (ad - 1));
                if (starttimer9901 != 0) timer9901 = starttimer9901;
            }
        } else {
            CRU[ad] = 1;
            if (ad == 3) timer9901IntReq = 0;   // writing the timer mask clears its request
        }
    } else {
        if ((ad < 16) && (CRU[0] == 1)) {
            if (ad == 15) {
                // 9901 soft reset
                memset(CRU, 1, 32);
                CRU[0] = 0; CRU[1] = 0; CRU[2] = 0; CRU[3] = 0;
                CRU[25] = 0; CRU[27] = 0;
            } else if (ad == 0) {
                // leaving clock mode: latch the read register, don't reset the timer
                CRU[ad] = 0;
                timer9901Read = timer9901;
                CRUTimerTicks = 0;
            } else {
                starttimer9901 &= ~(0x01 << (ad - 1));
                if (starttimer9901 != 0) timer9901 = starttimer9901;
            }
        } else {
            CRU[ad] = 0;
            if (ad == 3) timer9901IntReq = 0;
        }
    }
    if ((ad > 15) && (ad < 31) && (CRU[0] == 1)) {
        wcru(0, 0);                         // I/O access exits clock mode
    }
}

/////////////////////////////////////////////////////////
// CRU read (from Classic99 rcru + CheckJoysticks keyboard path)
/////////////////////////////////////////////////////////
IRAM_ATTR int rcru(Word ad) {
    if ((CRU[0] == 1) && (ad < 16) && (ad > 0)) {
        if (ad == 15) {
            return (timer9901IntReq && CRU[3]) || (vdpIntPending() && CRU[2]);
        }
        return (timer9901Read & (0x01 << (ad - 1))) ? 1 : 0;
    }
    if ((ad > 15) && (ad < 31) && (CRU[0] == 1)) {
        wcru(0, 0);
    }

    if (ad >= 0x0800) {
        ad <<= 1;
        if ((ad & 0xff00) == 0x1100 && diskDSR) {
            return ReadTICCCRU((ad & 0xff) >> 1);
        }
        return 1;
    }
    if (ad >= 0x0400) {
        return CRU[ad];
    }

    ad = (ad & 0x001f);
    if (ad == 0x02) return vdpIntPending() ? 0 : 1;
    if (ad == 0x01) return 1;               // no peripheral interrupts
    if (ad == 27) return 1;                 // no cassette input

    if ((ad >= 0x03) && (ad <= 0x0a)) {
        // column numbering follows Classic99 (CRU bits 18-20 inverted and reversed)
        int col = (CRU[0x14] == 0 ? 1 : 0) | (CRU[0x13] == 0 ? 2 : 0) | (CRU[0x12] == 0 ? 4 : 0);
        int row = ad - 3;

        if ((ad == 0x07) && (CRU[0x15] == 0) && alphaLockDown) {
            return 0;                       // alpha lock line selected and locked
        }
        if (col == 4) {                     // joystick 1 shares this column
            joyScanFrames = 180;
            Byte j = joy1;
            if ((row == 0) && (j & 0x01)) return 0;     // fire
            if ((row == 1) && (j & 0x02)) return 0;     // left
            if ((row == 2) && (j & 0x04)) return 0;     // right
            if ((row == 3) && (j & 0x08)) return 0;     // down
            if ((row == 4) && (j & 0x10)) return 0;     // up
        }
        Byte keys = (joyScanFrames ? kbMatrixNoArrows[col] : kbMatrix[col]) | typedMatrix[col];
        return (keys & (1 << row)) ? 0 : 1;
    }
    if ((ad >= 11) && (ad <= 31)) {
        return CRU[ad];                     // I/O pins read back what was written
    }
    return 1;
}

/////////////////////////////////////////////////////////
// Reset
/////////////////////////////////////////////////////////
void busReset() {
    memset(CRU, 1, sizeof(CRU));            // Classic99 defaults CRU to 1, then clears these
    CRU[0] = 0; CRU[1] = 0; CRU[2] = 0; CRU[3] = 0;
    CRU[25] = 0; CRU[27] = 0;
    nCurrentDSR = -1;
    ticcReset();
    timer9901 = timer9901Read = starttimer9901 = timer9901IntReq = 0;
    CRUTimerTicks = 0;
    GRMADD = 0;
    grmaccess = 2;
    grmdata = 0;
    cart.bank = 0;
    skip_interrupt = 0;
}

/////////////////////////////////////////////////////////
// Debug output (rate limited so a crashed program can't flood serial)
/////////////////////////////////////////////////////////
void debug_write(const char *s, ...) {
    if (!debugLog) return;
    static uint32_t windowStart = 0;
    static int count = 0;
    uint32_t now = millis();
    if (now - windowStart > 1000) {
        windowStart = now;
        count = 0;
    }
    if (++count > 20) return;

    char buf[160];
    va_list args;
    va_start(args, s);
    vsnprintf(buf, sizeof(buf), s, args);
    va_end(args);
    Serial.println(buf);
}

void warn(const char *s) {
    debug_write("%s", s);
}
