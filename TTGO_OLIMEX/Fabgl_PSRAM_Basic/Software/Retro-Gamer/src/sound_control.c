// sound_control.c
#include "sound_control.h"

bool g_sound_enabled = true;

void osd_sound_toggle(void) {
    g_sound_enabled = !g_sound_enabled;
}

void osd_sound_reset(void) {
    g_sound_enabled = true;
}