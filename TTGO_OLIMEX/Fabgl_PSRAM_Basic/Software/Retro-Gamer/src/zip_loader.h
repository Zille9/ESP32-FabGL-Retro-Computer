/*
 * Minimal ZIP ROM loader for Retro-Gamer
 * Supports reading the first file from a .zip archive
 * Uses ESP32's built-in ROM inflate (tinfl/miniz) for deflate decompression
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Check if a file is a ZIP archive (by extension)
bool zip_is_zip_file(const char *filename);

// Extract the first file from a ZIP to a PSRAM buffer
// Returns allocated buffer (caller must free) and sets out_size
// Returns NULL on failure
uint8_t* zip_extract_first(const char *zip_path, size_t *out_size, char *out_name, size_t name_len);

#ifdef __cplusplus
}
#endif
