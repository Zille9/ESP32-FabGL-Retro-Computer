/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : blockfile.cpp
 *  Module : Random-access image file on the SD card (hard disk images)
 * ============================================================
*/

#include "blockfile.h"
#include "../utils/debug.h"
#include <SD.h>
#include <string.h>

// SD transfers go through internal RAM: SPI DMA can't reliably reach PSRAM
// (see sv_disk.cpp). One buffer is enough, everything runs on core 1.
static uint8_t s_bounce[512];

// Arduino's VFS gives every File a 4 KB stdio buffer, which a seek discards:
// each 256-byte sector read then pulls 4 KB off the card (~9.6 ms at 4 MHz).
// One SD sector refills in ~1.2 ms and costs 3.5 KB less internal RAM.
#define BLOCKFILE_STDIO_BUF 512

// Open the handle if it was closed by the idle timer.
static bool ensure_open(BlockFile* bf) {
    if (!blockfile_is_open(bf)) return false;
    bf->last_ms = millis();
    if (bf->file) return true;
    bf->file = SD.open(bf->path, bf->read_only ? FILE_READ : "r+");
    if (!bf->file) {
        DEBUG_PRINTF("BLK: reopen of %s failed", bf->path);
        return false;
    }
    bf->file.setBufferSize(BLOCKFILE_STDIO_BUF);   // before any other operation
    bf->pos = UINT32_MAX;
    return true;
}

// Position the file for a read or a write at offset. Sequential accesses of
// the same kind skip the seek, which would throw the stdio buffer away. C
// requires a seek between a write and a following read, so a change of
// direction always seeks.
static bool position(BlockFile* bf, uint32_t offset, bool write) {
    if (bf->pos == offset && bf->last_write == write) return true;
    bf->last_write = write;
    if (bf->file.seek(offset)) { bf->pos = offset; return true; }
    bf->pos = UINT32_MAX;
    return false;
}

bool blockfile_open(BlockFile* bf, const char* path) {
    blockfile_close(bf);
    File f = SD.open(path, "r+");
    bool ro = false;
    if (!f) {
        f = SD.open(path, FILE_READ);
        ro = true;
    }
    if (!f || f.isDirectory()) {
        if (f) f.close();
        DEBUG_PRINTF("BLK: can't open %s", path);
        return false;
    }
    bf->size = (uint32_t)f.size();
    f.close();
    strncpy(bf->path, path, sizeof(bf->path) - 1);
    bf->path[sizeof(bf->path) - 1] = '\0';
    bf->read_only = ro;
    bf->dirty = false;
    bf->last_ms = millis();
    DEBUG_PRINTF("BLK: mounted %s (%lu bytes%s)", path, (unsigned long)bf->size,
                 ro ? ", read-only" : "");
    return true;
}

void blockfile_close(BlockFile* bf) {
    if (bf->file) {
        if (bf->dirty) bf->file.flush();
        bf->file.close();
    }
    bf->path[0] = '\0';
    bf->size = 0;
    bf->read_only = false;
    bf->dirty = false;
}

size_t blockfile_read(BlockFile* bf, uint32_t offset, uint8_t* buf, size_t len) {
    if (offset >= bf->size || !ensure_open(bf)) return 0;
    if (len > bf->size - offset) len = bf->size - offset;
    if (!position(bf, offset, false)) return 0;
    size_t done = 0;
    while (done < len) {
        size_t chunk = len - done;
        if (chunk > sizeof(s_bounce)) chunk = sizeof(s_bounce);
        int got = bf->file.read(s_bounce, chunk);
        if (got <= 0) break;
        memcpy(buf + done, s_bounce, got);
        done += got;
    }
    bf->pos = (done == len) ? offset + done : UINT32_MAX;
    return done;
}

size_t blockfile_write(BlockFile* bf, uint32_t offset, const uint8_t* buf, size_t len) {
    if (bf->read_only || !ensure_open(bf)) return 0;
    if (!position(bf, offset, true)) return 0;
    size_t done = 0;
    while (done < len) {
        size_t chunk = len - done;
        if (chunk > sizeof(s_bounce)) chunk = sizeof(s_bounce);
        memcpy(s_bounce, buf + done, chunk);
        size_t wrote = bf->file.write(s_bounce, chunk);
        if (wrote == 0) break;
        done += wrote;
    }
    bf->pos = (done == len) ? offset + done : UINT32_MAX;
    if (done) {
        bf->dirty = true;
        if (offset + done > bf->size) bf->size = offset + done;
    }
    return done;
}

void blockfile_flush(BlockFile* bf) {
    if (bf->file && bf->dirty) {
        bf->file.flush();
        bf->dirty = false;
    }
}

void blockfile_tick(BlockFile* bf, uint32_t now_ms) {
    if (!bf->file) return;
    if ((uint32_t)(now_ms - bf->last_ms) < BLOCKFILE_IDLE_MS) return;
    blockfile_flush(bf);
    bf->file.close();
}
