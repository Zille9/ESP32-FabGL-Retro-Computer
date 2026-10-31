/*
 * ============================================================================
 *  RETRO-GAMER — Debug System
 * ============================================================================
 *  Compile-time togglable debug output for Serial.
 *
 *  Usage:
 *    #define DEBUG_ENABLED 1   // 0=off, 1=on
 *    #include "src/debug.h"
 *
 *    DBG_ERROR("Init", "SD card mount failed: %d", err);
 *    DBG_WARN("Audio", "Buffer underrun at frame %d", frame);
 *    DBG_INFO("NES", "ROM loaded: %s (%d KB)", name, size/1024);
 *    DBG_VERBOSE("Input", "Keys: 0x%04X", buttons);
 *
 *  Memory Stats:
 *    dbg_print_memory()  — dumps DRAM/PSRAM usage to Serial
 * ============================================================================
 */

#ifndef RETRO_GAMER_DEBUG_H
#define RETRO_GAMER_DEBUG_H

#include <stdio.h>
#include <esp_heap_caps.h>

// ============================================================================
//  DEBUG_ENABLED: Set at compile time via build flags or here
//    0 = OFF  (no debug output, zero overhead)
//    1 = ON   (all debug output enabled)
// ============================================================================
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1  // Default: debug enabled
#endif

// ============================================================================
//  Serial Debug Macros - All conditional on DEBUG_ENABLED
// ============================================================================
#if DEBUG_ENABLED
  #define DBG_ERROR(tag, fmt, ...) \
    printf("\033[31m[ERR][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__)
  #define DBG_WARN(tag, fmt, ...) \
    printf("\033[33m[WRN][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__)
  #define DBG_INFO(tag, fmt, ...) \
    printf("[INF][%s] " fmt "\n", tag, ##__VA_ARGS__)
  #define DBG_VERBOSE(tag, fmt, ...) \
    printf("\033[90m[VRB][%s] " fmt "\033[0m\n", tag, ##__VA_ARGS__)
#else
  #define DBG_ERROR(tag, fmt, ...) ((void)0)
  #define DBG_WARN(tag, fmt, ...) ((void)0)
  #define DBG_INFO(tag, fmt, ...) ((void)0)
  #define DBG_VERBOSE(tag, fmt, ...) ((void)0)
#endif

// ============================================================================
//  Memory Debug Helpers
// ============================================================================
static inline void dbg_print_memory(void) {
#if DEBUG_ENABLED
    printf("\n===== MEMORY REPORT =====\n");
    printf("DRAM  free: %6d bytes (min: %d)\n",
        (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (int)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM free: %6d bytes (min: %d)\n",
        (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (int)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    printf("Total free: %6d bytes\n",
        (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    printf("Largest DRAM block:  %d\n",
        (int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    printf("Largest PSRAM block: %d\n",
        (int)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("=========================\n\n");
#endif
}

#endif // RETRO_GAMER_DEBUG_H
