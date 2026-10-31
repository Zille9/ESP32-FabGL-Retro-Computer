// Classic99 for ESP32 - TI disk controller card
// Derived from Classic99 disk/TICCDisk.cpp (C) Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.

#include <Arduino.h>
#include "ticc.h"
#include "bus.h"
#include "cpu9900.h"
#include "vdp9918.h"

// one controller card: shared CRU bits and FD1771 registers (kept for the DSR's
// status polling; the actual transfer happens in HandleTICCSector)
static unsigned char TICC_CRU[8];
static unsigned char TICC_REG[8];
static unsigned char TICC_DIR = 0;

static char diskName[TICC_DRIVES][64];
static FILE *diskFile[TICC_DRIVES];
static long diskSize[TICC_DRIVES];

void ticcReset() {
    memset(TICC_CRU, 0, sizeof(TICC_CRU));
    memset(TICC_REG, 0, sizeof(TICC_REG));
    TICC_CRU[0x06] = 1;     // always 1
}

int ReadTICCCRU(int adr) {
    adr &= 0x07;
    if (adr == 0x04) {
        TICC_CRU[adr] = 1;  // motor strobe reads back off
    }
    return TICC_CRU[adr];
}

void WriteTICCCRU(int adr, int bit) {
    adr &= 0x07;
    switch (adr) {
        case 0: TICC_CRU[adr] = bit; break;                 // ROM select
        case 1: if (bit == 0) TICC_CRU[0x04] = 0; break;    // motor strobe
        case 2: break;                                      // IRQ/DRQ to READY: not implemented
        case 3: TICC_CRU[0x00] = bit; break;                // head load
        case 4: TICC_CRU[0x01] = bit; break;                // select drive 1
        case 5: TICC_CRU[0x02] = bit; break;                // select drive 2
        case 6: TICC_CRU[0x03] = bit; break;                // select drive 3
        case 7: TICC_CRU[0x07] = bit; break;                // side
    }
}

Byte ReadTICCRegister(Word address) {
    if ((address > 0x5ff7) || (address < 0x5ff0)) return 0xff;
    switch (address & 0xfffe) {
        case 0x5ff0:
            // status: head loaded, ready, not busy; data bus is inverted
            TICC_REG[0] = 0x20;
            if (TICC_REG[1] == 0) TICC_REG[0] |= 0x04;      // track 0
            TICC_REG[0] = ~TICC_REG[0];
            return TICC_REG[0];
        case 0x5ff2: return TICC_REG[1];    // track
        case 0x5ff4: return TICC_REG[2];    // sector
        case 0x5ff6: return TICC_REG[3];    // data
    }
    return 0;
}

void WriteTICCRegister(Word address, Byte val) {
    if ((address < 0x5ff8) || (address > 0x5fff)) return;
    switch (address & 0xfffe) {
        case 0x5ff8:
            switch (val & 0xe0) {
                case 0x00:      // restore or seek
                    TICC_REG[1] = (val & 0x10) ? TICC_REG[3] : 0;
                    break;
                case 0x20:      // step
                    if (val & 0x10) {
                        if (TICC_DIR) { if (TICC_REG[1] < 255) TICC_REG[1]++; }
                        else { if (TICC_REG[1] > 0) TICC_REG[1]--; }
                    }
                    break;
                case 0x40:      // step in
                    if ((val & 0x10) && TICC_REG[1] < 255) TICC_REG[1]++;
                    break;
                case 0x60:      // step out
                    if ((val & 0x10) && TICC_REG[1] > 0) TICC_REG[1]--;
                    break;
                default:
                    debug_write("Disk controller got unimplemented command 0x%02X", val);
                    break;
            }
            break;
        case 0x5ffa: TICC_REG[1] = val; break;
        case 0x5ffc: TICC_REG[2] = val; break;
        case 0x5ffe: TICC_REG[3] = val; break;
    }
}

/////////////////////////////////////////////////////////
// Disk images
/////////////////////////////////////////////////////////
void diskUnmount(int drive) {
    if (drive < 1 || drive > TICC_DRIVES) return;
    int d = drive - 1;
    if (diskFile[d]) fclose(diskFile[d]);
    diskFile[d] = nullptr;
    diskName[d][0] = 0;
    diskSize[d] = 0;
}

bool diskMount(int drive, const char *file) {
    if (drive < 1 || drive > TICC_DRIVES) return false;
    diskUnmount(drive);
    int d = drive - 1;
    char path[160];
    snprintf(path, sizeof(path), TI_DISK_DIR "/%s", file);
    FILE *fp = fopen(path, "r+b");
    if (!fp) fp = fopen(path, "rb");        // read-only card or file
    if (!fp) {
        DBG("Can't open %s\n", path);
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    // sector dumps only: SSSD 90K, DSSD/SSDD 180K, DSDD 360K
    if (size != 90 * 1024 && size != 180 * 1024 && size != 360 * 1024) {
        DBG("%s: %ld bytes is not a 90K/180K/360K sector image\n", file, size);
        fclose(fp);
        return false;
    }
    diskFile[d] = fp;
    diskSize[d] = size;
    strlcpy(diskName[d], file, sizeof(diskName[d]));
    DBG("DSK%d: %s (%ldK)\n", drive, file, size / 1024);
    return true;
}

const char *diskMounted(int drive) {
    return (drive >= 1 && drive <= TICC_DRIVES) ? diskName[drive - 1] : "";
}

/////////////////////////////////////////////////////////
// Sector hook (from Classic99 HandleTICCSector / readsectorwrap / writesectorwrap)
//   >834A = sector number, >834C = drive (1-3)
//   >834D = 0: write, anything else: read, >834E = VDP buffer address
/////////////////////////////////////////////////////////
void HandleTICCSector() {
    int nDrive = scratchpad[0x4c];
    int sector = (scratchpad[0x4a] << 8) | scratchpad[0x4b];
    int vdpAddr = ((scratchpad[0x4e] << 8) | scratchpad[0x4f]) & 0x3fff;
    bool isRead = scratchpad[0x4d] != 0;

    FILE *fp = (nDrive >= 1 && nDrive <= TICC_DRIVES) ? diskFile[nDrive - 1] : nullptr;
    long offset = (long)sector * 256;
    if (!fp || offset + 256 > diskSize[nDrive - 1] || vdpAddr + 256 > 0x4000) {
        pCPU->SetPC(0x42a0);                // DSR error exit: device not found
        return;
    }

    Byte *vram = vdpRam() + vdpAddr;
    fseek(fp, offset, SEEK_SET);
    if (isRead) {
        if (fread(vram, 1, 256, fp) != 256) {
            pCPU->SetPC(0x42a0);
            return;
        }
    } else {
        fwrite(vram, 1, 256, fp);
        fflush(fp);
    }
    pCPU->SetPC(0x4676);                    // DSR's return from sector read/write
}
