/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : vhd.h
 *  Module : VCC-compatible virtual hard disk at $FF80-$FF86
 * ============================================================
*/

#ifndef VHD_H
#define VHD_H

#include <stdint.h>

// Port of VCC's HardDisk/cc3vhd.cpp (the "EmuDsk" device used by RGB-DOS and
// the NitrOS-9 emudsk driver). The guest sets the LSN and a buffer address,
// then writes a command; the host moves the 256-byte sector to or from CoCo
// memory at once. A .vhd image is raw: LSN n at file offset n * 256.
//
//   $FF80-$FF82  LSN high / mid / low (read back)
//   $FF83        write: command  0 read, 1 write, 2 flush   read: status
//   $FF84-$FF85  buffer address high / low (read back)
//   $FF86        drive select, 0 or 1 (other values ignored)
#define VHD_BASE_ADDR   0xFF80
#define VHD_LAST_ADDR   0xFF86
#define VHD_NUM_DRIVES  2

// Status values read from $FF83
#define VHD_ST_OK       0x00
#define VHD_ST_PWRUP    0xFF   // no command since power-on
#define VHD_ST_INVALID  0xFE   // unknown command
#define VHD_ST_NODISK   0x04   // no image, offset out of range, or short write
#define VHD_ST_WPROT    0x05   // write to a read-only image

// Largest image offset, as in VCC (DRIVESIZE 512 MB).
#define VHD_MAX_BYTES   (512u * 1024u * 1024u)

// Set once at boot from NVS (supervisor_load_cart_config, key "vhd_en"),
// before machine_init(). Gates the $FF80-$FF86 decode in machine.cpp.
extern bool g_vhd_enabled;

static inline bool vhd_enabled(void) {
    return g_vhd_enabled;
}

static inline bool vhd_is_port(uint16_t addr) {
    return addr >= VHD_BASE_ADDR && addr <= VHD_LAST_ADDR;
}

// Bus access to $FF80-$FF86. A command moves a sector through machine_read() /
// machine_write(), so the guest buffer is resolved through the MMU.
uint8_t vhd_read(uint16_t addr);
void    vhd_write(uint16_t addr, uint8_t val);

// Machine reset: flushes, selects drive 0 (VCC VhdReset). Mounts are kept.
void vhd_reset(void);

// Once per frame: closes idle image files (see blockfile.h).
void vhd_tick(void);

// Image management (core 1 only). mount replaces any image in the drive.
bool        vhd_mount(uint8_t drive, const char* path);
void        vhd_unmount(uint8_t drive);
bool        vhd_is_mounted(uint8_t drive);
bool        vhd_is_read_only(uint8_t drive);
const char* vhd_get_path(uint8_t drive);   // "" when empty
void        vhd_flush_all(void);

#endif // VHD_H
