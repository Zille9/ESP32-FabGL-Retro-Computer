// sound_control.h
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_sound_enabled;

// Función para toggle de sonido
void osd_sound_toggle(void);
// Función para resetear sonido a ON
void osd_sound_reset(void);

#ifdef __cplusplus
}
#endif