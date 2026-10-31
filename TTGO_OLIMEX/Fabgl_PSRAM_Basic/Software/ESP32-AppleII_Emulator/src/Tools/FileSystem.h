/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : FileSystem.h
 *  Module : Small SD-card helper used to open and read ROMs and
 *           the .nib / .dsk / .do / .po disk images of the emulated
 *           floppy drives.
 * ============================================================
*/

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Log.h"

class FileSystem
{
public :
     FileSystem()
     {
        DEBUG_PRINTLN("Construct FileSystem");
     }

    ~FileSystem()
    {
        SD.end();
    }

    // SD.open with one more try. Opens from the emulation task have been
    // seen to fail now and then (a ROM reported missing, a disk image not
    // mounting) with no cause found yet; a second try is logged when it
    // succeeds, so it shows up if it keeps happening.
    static File Open(const char *path)
    {
        File file = SD.open(path);
        if (!file)
        {
            delay(5);
            file = SD.open(path);
            if (file)
                LOGF("[sd] %s opened on the second try\n", path);
        }
        return file;
    }

    // Reads up to len bytes. With fileSize, also reports the file's whole
    // length, so a caller can turn away a file that is longer than len.
    int ReadFile(const char *path, unsigned char *buffer, int len, size_t *fileSize = nullptr)
    {
        File file = Open(path);
        if(!file)
        {
            LOGF("- failed to open file for reading: %s\n", path);
            return -1;
        }

        if (fileSize)
            *fileSize = file.size();
        int readlen = file.read(buffer, len);
        LOGF("- read from file : %d\n", readlen);
        file.close();

        return readlen;
    }

    // Overwrites len bytes at offset in an existing file without truncating
    // it. False when the file cannot be opened for update (missing, card
    // read-only or gone) or not every byte was written.
    static bool WriteAt(const char *path, size_t offset, const uint8_t *data, size_t len)
    {
        File file = SD.open(path, "r+");
        if (!file)
        {
            LOGF("- failed to open file for update: %s\n", path);
            return false;
        }
        bool ok = file.seek(offset) && file.write(data, len) == len;
        file.close();
        return ok;
    }
};