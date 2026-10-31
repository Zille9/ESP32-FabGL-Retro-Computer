/*
 * Minimal ZIP ROM loader implementation
 * Reads ZIP local file header, extracts first file
 * Supports: stored (method 0) and deflate (method 8)
 * Uses ESP32 ROM's miniz tinfl for inflate
 */
#include "zip_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

// We use the ESP32 ROM's built-in miniz decompressor
// which is available via rom/miniz.h on ESP-IDF / Arduino ESP32
#include "rom/miniz.h"

// ZIP Local File Header signature
#define ZIP_LOCAL_HEADER_SIG  0x04034b50

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;    // 0=stored, 8=deflate
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t name_length;
    uint16_t extra_length;
} zip_local_header_t;
#pragma pack(pop)

bool zip_is_zip_file(const char *filename) {
    if (!filename) return false;
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".zip") == 0);
}

uint8_t* zip_extract_first(const char *zip_path, size_t *out_size, char *out_name, size_t name_len) {
    if (!zip_path || !out_size) return NULL;

    FILE *f = fopen(zip_path, "rb");
    if (!f) {
        printf("[ZIP] Failed to open: %s\n", zip_path);
        return NULL;
    }

    // Read local file header
    zip_local_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        printf("[ZIP] Failed to read header\n");
        fclose(f);
        return NULL;
    }

    if (hdr.signature != ZIP_LOCAL_HEADER_SIG) {
        printf("[ZIP] Invalid ZIP signature: 0x%08X\n", hdr.signature);
        fclose(f);
        return NULL;
    }

    // Read filename
    char fname[256] = {0};
    int nameRead = hdr.name_length < 255 ? hdr.name_length : 255;
    fread(fname, 1, nameRead, f);
    fname[nameRead] = '\0';

    if (out_name && name_len > 0) {
        strncpy(out_name, fname, name_len - 1);
        out_name[name_len - 1] = '\0';
    }

    // Skip extra field
    if (hdr.extra_length > 0) {
        fseek(f, hdr.extra_length, SEEK_CUR);
    }

    printf("[ZIP] File: %s  Method: %d  Compressed: %u  Uncompressed: %u\n",
           fname, hdr.compression, hdr.compressed_size, hdr.uncompressed_size);

    // Handle data descriptor (bit 3 of flags)
    uint32_t comp_size = hdr.compressed_size;
    uint32_t uncomp_size = hdr.uncompressed_size;

    if (comp_size == 0 && uncomp_size == 0 && (hdr.flags & 0x08)) {
        // Data descriptor follows data — we can't easily handle this
        // Try reading until EOF for stored files
        printf("[ZIP] Data descriptor not supported\n");
        fclose(f);
        return NULL;
    }

    uint8_t *output = NULL;

    if (hdr.compression == 0) {
        // Stored (no compression)
        output = (uint8_t *)heap_caps_malloc(uncomp_size, MALLOC_CAP_SPIRAM);
        if (!output) {
            printf("[ZIP] PSRAM alloc failed (%u bytes)\n", uncomp_size);
            fclose(f);
            return NULL;
        }
        size_t read = fread(output, 1, uncomp_size, f);
        if (read != uncomp_size) {
            printf("[ZIP] Read error: got %u, expected %u\n", (unsigned)read, uncomp_size);
            free(output);
            fclose(f);
            return NULL;
        }
        *out_size = uncomp_size;

    } else if (hdr.compression == 8) {
        // Deflate — use ESP32's built-in tinfl
        uint8_t *comp_data = (uint8_t *)heap_caps_malloc(comp_size, MALLOC_CAP_SPIRAM);
        output = (uint8_t *)heap_caps_malloc(uncomp_size, MALLOC_CAP_SPIRAM);

        if (!comp_data || !output) {
            printf("[ZIP] PSRAM alloc failed (comp=%u, uncomp=%u)\n", comp_size, uncomp_size);
            if (comp_data) free(comp_data);
            if (output) free(output);
            fclose(f);
            return NULL;
        }

        fread(comp_data, 1, comp_size, f);

        // Use tinfl to decompress
        size_t decomp_size = uncomp_size;
        int status = tinfl_decompress_mem_to_mem(output, decomp_size, comp_data, comp_size,
                                                  TINFL_FLAG_PARSE_ZLIB_HEADER ? 0 : 0);

        free(comp_data);

        if (status == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
            printf("[ZIP] Decompression failed!\n");
            free(output);
            fclose(f);
            return NULL;
        }

        *out_size = (size_t)status;
        printf("[ZIP] Decompressed: %u bytes\n", (unsigned)*out_size);

    } else {
        printf("[ZIP] Unsupported compression method: %d\n", hdr.compression);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return output;
}
