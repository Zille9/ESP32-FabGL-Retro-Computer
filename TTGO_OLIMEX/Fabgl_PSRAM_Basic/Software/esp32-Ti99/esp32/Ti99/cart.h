// Classic99 for ESP32 - cartridge loader
// Rules from Classic99 Tiemul.cpp LoadOneImg / findXBbank (C) Mike Brent aka Tursi.
// See the original licence there. Not for distribution without the author's permission.

#pragma once
#include "ti_types.h"

#define MAX_CARTS 200

struct CartEntry {
    char name[48];              // display name (file stem)
    char files[3][64];          // up to 3 files (C, D, G) or one banked file
    int nFiles;
};

int cartScan();                         // reads TI_CART_DIR, returns number of entries
const CartEntry *cartGet(int idx);
bool cartLoad(int idx);                 // emulation must be paused; returns false on error
void cartEject();
void cartResetBank();                   // power-on bank selection, called on reset
const char *cartCurrentName();
