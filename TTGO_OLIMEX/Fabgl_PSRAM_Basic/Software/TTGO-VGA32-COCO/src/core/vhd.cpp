/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : vhd.cpp
 *  Module : VCC-compatible virtual hard disk at $FF80-$FF86
 * ============================================================
*/

#include "vhd.h"
#include "blockfile.h"
#include "machine.h"
#include "../utils/debug.h"
#include <Arduino.h>
#include <string.h>

bool g_vhd_enabled = false;

static BlockFile s_drive[VHD_NUM_DRIVES];

static uint32_t s_offset;        // byte offset: $FF80-$FF82 are bits 31-8
static uint16_t s_buf_addr;      // $FF84-$FF85
static uint8_t  s_drive_sel;     // $FF86
static uint8_t  s_status = VHD_ST_PWRUP;
static uint8_t  s_sector[256];

static void cmd_read(BlockFile* bf) {
    if (s_offset >= VHD_MAX_BYTES) { s_status = VHD_ST_NODISK; return; }
    size_t got = blockfile_read(bf, s_offset, s_sector, sizeof(s_sector));
    // Past the end of the image reads as zeros. (VCC's `Temp > BytesMoved`
    // test leaves byte [BytesMoved] stale; this fills from there.)
    if (got < sizeof(s_sector)) memset(s_sector + got, 0, sizeof(s_sector) - got);
    for (unsigned i = 0; i < sizeof(s_sector); i++)
        machine_write((uint16_t)(s_buf_addr + i), s_sector[i]);
    s_status = VHD_ST_OK;
}

static void cmd_write(BlockFile* bf) {
    if (s_offset >= VHD_MAX_BYTES) { s_status = VHD_ST_NODISK; return; }
    if (bf->read_only)             { s_status = VHD_ST_WPROT;  return; }
    for (unsigned i = 0; i < sizeof(s_sector); i++)
        s_sector[i] = machine_read((uint16_t)(s_buf_addr + i));
    size_t wrote = blockfile_write(bf, s_offset, s_sector, sizeof(s_sector));
    s_status = (wrote == sizeof(s_sector)) ? VHD_ST_OK : VHD_ST_NODISK;
}

static void command(uint8_t cmd) {
    BlockFile* bf = &s_drive[s_drive_sel];
    if (!blockfile_is_open(bf)) { s_status = VHD_ST_NODISK; return; }
    switch (cmd) {
    case 0: cmd_read(bf);  break;
    case 1: cmd_write(bf); break;
    case 2:
        blockfile_flush(bf);
        s_offset = 0;
        s_buf_addr = 0;
        s_status = VHD_ST_OK;
        break;
    default:
        s_status = VHD_ST_INVALID;
        break;
    }
}

uint8_t vhd_read(uint16_t addr) {
    switch (addr - VHD_BASE_ADDR) {
    case 0: return (uint8_t)(s_offset >> 24);
    case 1: return (uint8_t)(s_offset >> 16);
    case 2: return (uint8_t)(s_offset >> 8);
    case 3: return s_status;
    case 4: return (uint8_t)(s_buf_addr >> 8);
    case 5: return (uint8_t)s_buf_addr;
    case 6: return s_drive_sel;
    }
    return 0xFF;
}

void vhd_write(uint16_t addr, uint8_t val) {
    switch (addr - VHD_BASE_ADDR) {
    case 0: s_offset = (s_offset & 0x00FFFF00u) | ((uint32_t)val << 24); break;
    case 1: s_offset = (s_offset & 0xFF00FF00u) | ((uint32_t)val << 16); break;
    case 2: s_offset = (s_offset & 0xFFFF0000u) | ((uint32_t)val << 8);  break;
    case 3: command(val); break;
    case 4: s_buf_addr = (uint16_t)((s_buf_addr & 0x00FF) | (val << 8)); break;
    case 5: s_buf_addr = (uint16_t)((s_buf_addr & 0xFF00) | val);        break;
    case 6: if (val < VHD_NUM_DRIVES) s_drive_sel = val; break;
    }
}

void vhd_reset(void) {
    vhd_flush_all();   // a reset is a natural point to commit writes
    s_drive_sel = 0;
}

void vhd_tick(void) {
    uint32_t now = millis();
    for (int d = 0; d < VHD_NUM_DRIVES; d++) blockfile_tick(&s_drive[d], now);
}

bool vhd_mount(uint8_t drive, const char* path) {
    if (drive >= VHD_NUM_DRIVES) return false;
    bool ok = blockfile_open(&s_drive[drive], path);
    s_offset = 0;
    s_buf_addr = 0;
    s_status = ok ? VHD_ST_OK : VHD_ST_NODISK;
    DEBUG_PRINTF("VHD: drive %u %s %s", drive, ok ? "mounted" : "mount failed:", path);
    return ok;
}

void vhd_unmount(uint8_t drive) {
    if (drive >= VHD_NUM_DRIVES || !blockfile_is_open(&s_drive[drive])) return;
    blockfile_close(&s_drive[drive]);
    s_status = VHD_ST_NODISK;
    DEBUG_PRINTF("VHD: drive %u ejected", drive);
}

bool vhd_is_mounted(uint8_t drive) {
    return drive < VHD_NUM_DRIVES && blockfile_is_open(&s_drive[drive]);
}

bool vhd_is_read_only(uint8_t drive) {
    return vhd_is_mounted(drive) && s_drive[drive].read_only;
}

const char* vhd_get_path(uint8_t drive) {
    return drive < VHD_NUM_DRIVES ? s_drive[drive].path : "";
}

void vhd_flush_all(void) {
    for (int d = 0; d < VHD_NUM_DRIVES; d++) blockfile_flush(&s_drive[d]);
}
