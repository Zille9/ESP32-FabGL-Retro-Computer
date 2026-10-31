// Classic99 for ESP32 - cartridge loader
// Rules from Classic99 Tiemul.cpp LoadOneImg / findXBbank (C) Mike Brent aka Tursi.
// See the original licence there. Not for distribution without the author's permission.
//
// Supported: V9T9 names (xxxC = 8K ROM, xxxD = XB second bank, xxxG = GROM at >6000),
// single-file 378 (non-inverted, suffix 8 or no suffix) and 379 (inverted, suffix 9 or 3)
// banked ROMs. ROM banks and cartridge GROM live in PSRAM.
// Not ported: MBX, NVRAM, UberGROM, MPD, multi-base GROM banking.

#include <Arduino.h>
#include <dirent.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include <ctype.h>
#include "cart.h"
#include "bus.h"

static CartEntry *entries = nullptr;
static int nEntries = 0;
static char currentName[48] = "";

static char typeOf(const char *file) {
    const char *dot = strrchr(file, '.');
    if (!dot || dot == file) return '8';            // no extension: FinalGROM-style 378
    return toupper(dot[-1]);
}

static int cmpEntry(const void *a, const void *b) {
    return strcasecmp(((const CartEntry *)a)->name, ((const CartEntry *)b)->name);
}

int cartScan() {
    if (!entries) {
        entries = (CartEntry *)heap_caps_calloc(MAX_CARTS, sizeof(CartEntry), MALLOC_CAP_SPIRAM);
    }
    nEntries = 0;
    DIR *dir = opendir(TI_CART_DIR);
    if (!dir) {
        DBG("No " TI_CART_DIR " folder\n");
        return 0;
    }
    struct dirent *de;
    while ((de = readdir(dir)) != nullptr && nEntries < MAX_CARTS) {
        const char *fn = de->d_name;
        if (fn[0] == '.' || strlen(fn) >= 64) continue;
        char t = typeOf(fn);

        // parts of one cartridge share the name minus the type letter
        // (PARSECC/PARSECG, TIEXTC/D/G, RXB2026_8/RXB2026_G)
        char stem[48];
        const char *dot = strrchr(fn, '.');
        int len = dot ? (int)(dot - fn) : (int)strlen(fn);
        bool typed = dot && strchr("CDG893", t);
        if (typed) len--;
        if (len <= 0) continue;
        if (len > 47) len = 47;
        memcpy(stem, fn, len);
        stem[len] = 0;
        // names like tiworkshop379.bin / game378.bin: drop the rest of the "37x" tag too
        if ((t == '8' || t == '9') && len > 2 && stem[len - 2] == '3' && stem[len - 1] == '7') stem[len -= 2] = 0;
        while (len > 1 && (stem[len - 1] == '_' || stem[len - 1] == '-')) stem[--len] = 0;

        CartEntry *e = nullptr;
        if (typed) {
            for (int i = 0; i < nEntries; i++) {
                if (strcasecmp(entries[i].name, stem) == 0 && entries[i].nFiles < 3) {
                    e = &entries[i];
                    break;
                }
            }
        }
        if (!e) {
            e = &entries[nEntries++];
            memset(e, 0, sizeof(*e));
            strcpy(e->name, stem);
        }
        strcpy(e->files[e->nFiles++], fn);
    }
    closedir(dir);
    qsort(entries, nEntries, sizeof(CartEntry), cmpEntry);
    return nEntries;
}

const CartEntry *cartGet(int idx) {
    return (idx >= 0 && idx < nEntries) ? &entries[idx] : nullptr;
}

const char *cartCurrentName() {
    return currentName;
}

// reads a whole file into a PSRAM buffer
static Byte *readFile(const char *name, size_t *len) {
    char path[160];
    snprintf(path, sizeof(path), TI_CART_DIR "/%s", name);
    FILE *fp = fopen(path, "rb");
    if (!fp) return nullptr;
    struct stat st;
    long statSize = (stat(path, &st) == 0) ? (long)st.st_size : -1;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) size = statSize;
    if (size <= 0) { fclose(fp); return nullptr; }
    if (size > 4 * 1024 * 1024) size = 4 * 1024 * 1024;
    Byte *buf = (Byte *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (buf) *len = fread(buf, 1, size, fp);
    fclose(fp);
    return buf;
}

// old 6-byte program headers: flag 00/FF, load address in bytes 4-5
static void stripHeader(Byte *&data, size_t &len, int loadAddr) {
    if (len > 6 && (data[0] == 0x00 || data[0] == 0xff) && ((data[4] << 8) | data[5]) == loadAddr) {
        data += 6;
        len -= 6;
    }
}

static int bankMaskFor(int banks) {
    int mask = 0;
    while (mask + 1 < banks) mask = (mask << 1) | 1;
    return mask;
}

void cartEject() {
    if (cart.rom) heap_caps_free(cart.rom);
    cart.rom = nullptr;
    cart.bankMask = 0;
    cart.inverted = false;
    cart.bank = 0;
    memset(cartGrom, 0, 0xa000);
    currentName[0] = 0;
}

bool cartLoad(int idx) {
    const CartEntry *e = cartGet(idx);
    if (!e) return false;
    cartEject();

    for (int f = 0; f < e->nFiles; f++) {
        size_t len = 0;
        Byte *buf = readFile(e->files[f], &len);
        if (!buf) {
            DBG("Can't read %s\n", e->files[f]);
            continue;
        }
        Byte *data = buf;
        char t = typeOf(e->files[f]);

        switch (t) {
            case 'G':
                stripHeader(data, len, 0x6000);
                if (len > 0xa000) len = 0xa000;
                memcpy(cartGrom, data, len);
                DBG("GROM %s: %u bytes at >6000\n", e->files[f], (unsigned)len);
                break;

            case 'C':
            case 'D': {
                stripHeader(data, len, 0x6000);
                // C is bank 0, D is bank 1 (XB style, non-inverted). A C file larger
                // than 8K is treated as a non-inverted banked image.
                int base = (t == 'D') ? 1 : 0;
                int banks = base + (int)((len + 0x1fff) / 0x2000);
                int need = (t == 'D') ? 2 : banks;
                if (need < 1) need = 1;
                int mask = bankMaskFor(need > cart.bankMask + 1 ? need : cart.bankMask + 1);
                if (!cart.rom || mask > cart.bankMask) {
                    Byte *n = (Byte *)heap_caps_calloc(mask + 1, 0x2000, MALLOC_CAP_SPIRAM);
                    if (cart.rom) {
                        memcpy(n, cart.rom, (cart.bankMask + 1) * 0x2000);
                        heap_caps_free(cart.rom);
                    }
                    cart.rom = n;
                    cart.bankMask = mask;
                }
                memcpy(cart.rom + base * 0x2000, data, len);
                DBG("ROM %s: %u bytes in bank %d, mask %d\n", e->files[f], (unsigned)len, base, cart.bankMask);
                break;
            }

            default: {
                // single-file banked image: 8/no suffix = 378, 9/3 = 379 inverted
                int banks = (int)((len + 0x1fff) / 0x2000);
                int mask = bankMaskFor(banks);
                cart.rom = (Byte *)heap_caps_calloc(mask + 1, 0x2000, MALLOC_CAP_SPIRAM);
                if (!cart.rom) {
                    DBG("Out of PSRAM for cartridge\n");
                    break;
                }
                memcpy(cart.rom, data, len);
                cart.bankMask = mask;
                cart.inverted = (t == '9' || t == '3');
                DBG("Banked %s: %u bytes, %s, mask %d\n", e->files[f], (unsigned)len,
                              cart.inverted ? "379 inverted" : "378", mask);
                break;
            }
        }
        heap_caps_free(buf);
    }
    strcpy(currentName, e->name);
    return true;
}

// from Classic99 findXBbank: start in the bank holding the >AA header
void cartResetBank() {
    cart.bank = 0;
    if (cart.rom && cart.bankMask > 0) {
        if (cart.rom[0] != 0xaa && cart.rom[0x2000 * cart.bankMask] == 0xaa) {
            cart.bank = cart.bankMask;
        }
    }
}
