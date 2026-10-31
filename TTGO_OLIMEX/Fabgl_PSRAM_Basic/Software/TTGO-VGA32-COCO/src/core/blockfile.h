/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : blockfile.h
 *  Module : Random-access image file on the SD card (hard disk images)
 * ============================================================
*/

#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include <stdint.h>
#include <stddef.h>
#include <FS.h>

// An image file read and written in place, a sector at a time, for images
// too large to cache in PSRAM (VHD hard disks, later the CoCo SDC). Unlike
// sv_disk there is no image cache.
//
// An open FatFS handle costs about 5 KB of internal RAM, so the file is not
// held open while mounted: it opens on the first access, stays open while
// the guest keeps using it, and blockfile_tick() flushes and closes it after
// BLOCKFILE_IDLE_MS without an access.
//
// Not thread-safe: use from core 1 only (CPU bus, supervisor, debug_rpc).
#define BLOCKFILE_IDLE_MS 3000

struct BlockFile {
    File     file;          // open handle, or closed while idle
    char     path[256];     // "" = not mounted
    uint32_t size;          // file size in bytes (grows on writes past the end)
    bool     read_only;     // "r+" open failed at mount
    bool     dirty;         // written since the last flush
    uint32_t last_ms;       // millis() of the last access
    uint32_t pos;           // file position after the last access (UINT32_MAX = unknown)
    bool     last_write;    // last access was a write (a read must seek first)
};

// Mount: check the file exists and whether it can be opened for writing.
// Leaves it closed. Returns false (and bf unmounted) if it can't be opened.
bool blockfile_open(BlockFile* bf, const char* path);

// Flush and close; bf is unmounted afterwards.
void blockfile_close(BlockFile* bf);

static inline bool blockfile_is_open(const BlockFile* bf) { return bf->path[0] != '\0'; }

// Read len bytes at offset into buf. Returns the bytes read: short (possibly 0)
// at the end of the file. buf may be in PSRAM.
size_t blockfile_read(BlockFile* bf, uint32_t offset, uint8_t* buf, size_t len);

// Write len bytes at offset. Writing past the end extends the file. Returns
// the bytes written; 0 if read-only or on error.
size_t blockfile_write(BlockFile* bf, uint32_t offset, const uint8_t* buf, size_t len);

// Push buffered writes to the card (file stays open).
void blockfile_flush(BlockFile* bf);

// Call about once per frame: closes the handle (flushing first) once it has
// been idle for BLOCKFILE_IDLE_MS.
void blockfile_tick(BlockFile* bf, uint32_t now_ms);

#endif // BLOCKFILE_H
