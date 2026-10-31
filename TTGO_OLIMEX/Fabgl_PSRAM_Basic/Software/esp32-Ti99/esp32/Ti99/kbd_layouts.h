// Classic99 for ESP32 - PS/2 keyboard layouts selectable from the Setup menu
#pragma once

int kbdLayoutCount();
const char *kbdLayoutName(int idx);     // "Spanish (Latam)"
const char *kbdLayoutId(int idx);       // "LA", stored in config.txt
int kbdLayoutFind(const char *id);      // -1 if unknown
int kbdLayoutGet();
void kbdLayoutSet(int idx);
