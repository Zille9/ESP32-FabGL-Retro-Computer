// Classic99 for ESP32 - TI disk controller card
// Derived from Classic99 disk/TICCDisk.cpp (C) Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
//
// Like Classic99, this runs the real TI disk controller DSR ROM (DISK.BIN) and
// intercepts its sector read/write routine instead of emulating the FD1771 chip.
// Disks are sector-dump images (V9T9 .dsk: 90K, 180K, 360K) on the SD card.

#pragma once
#include "ti_types.h"

#define TICC_DRIVES 3
#define TICC_SECTOR_HOOK_PC 0x40e8      // sector read/write routine in the TI disk DSR

Byte ReadTICCRegister(Word x);
void WriteTICCRegister(Word x, Byte c);
int  ReadTICCCRU(int adr);
void WriteTICCCRU(int adr, int bit);
void ticcReset();

bool diskMount(int drive, const char *file);    // drive 1-3, file inside TI_DISK_DIR; emulation must be paused
void diskUnmount(int drive);
const char *diskMounted(int drive);             // "" if empty

// call when the CPU reaches TICC_SECTOR_HOOK_PC with the disk DSR paged in
void HandleTICCSector();
