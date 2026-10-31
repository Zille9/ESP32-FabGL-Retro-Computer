// Classic99 for ESP32 (TTGO VGA32 v1.4 + FabGL)
// Derived from Classic99 (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence in console/Tiemul.cpp. Not for distribution without the author's permission.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include "fabgl.h"
#include "ti_types.h"
#include "bus.h"
#include "emu.h"
#include "vdp9918.h"
#include "sound9919.h"
#include "speech.h"
#include "keyboard_ti.h"
#include "cart.h"
#include "ticc.h"
#include "menu.h"
#include "version.h"
#include "config.h"

#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
#include "esp_ota_ops.h"
#include "esp_partition.h"

// ESP32_Bootloader marks this firmware as the one to boot. Erasing otadata makes
// the next power-up fall back to the factory partition, i.e. the bootloader menu.
static void bootloaderReleaseOtadata() {
  const esp_partition_t *otadata = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (!otadata) return;
  esp_partition_erase_range(otadata, 0, otadata->size);
}
#define BUILD_TARGET_NAME " [ESP32_Bootloader]"
#else
#define BUILD_TARGET_NAME ""
#endif

fabgl::VGAController DisplayController;
fabgl::PS2Controller PS2Controller;
bool debugLog = false;

static void consoleTask(void *);

static Byte *diskROMImage = nullptr;   // TI disk DSR, wired to the bus once the controller is emulated

// Build with -DBOOT_ERROR_TEST=1 (ROMs missing) or =2 (no SD card) to see the warning screen
#ifndef BOOT_ERROR_TEST
#define BOOT_ERROR_TEST 0
#endif

// files expected on the SD card; the machine runs without DISK.BIN, but has no disk drives
static BootRomStatus roms[3] = {
  { TI_ROM_DIR "/994AROM.BIN", false, false },
  { TI_ROM_DIR "/994AGROM.BIN", false, false },
  { TI_ROM_DIR "/DISK.BIN", false, true },
};

static bool loadFile(const char *path, Byte *dest, size_t maxLen, size_t *outLen) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    DBG("Missing: %s\n", path);
    return false;
  }
  size_t n = fread(dest, 1, maxLen, fp);
  fclose(fp);
  if (outLen) *outLen = n;
  DBG("Loaded %s (%u bytes)\n", path, (unsigned)n);
  return n > 0;
}

static void printHeap(const char *tag) {
  DBG("[%s] internal free %u (largest %u), PSRAM free %u\n", tag,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void fatal(fabgl::Canvas &cv, const char *msg) {
  Serial.println(msg);
  cv.drawText(8, 100, msg);
  cv.waitCompletion();
  for (;;) vTaskDelay(1000);
}

// PS/2 keyboard events -> TI key matrix, on core 0 so input works however busy core 1 is
static void inputTask(void *) {
  auto keyboard = PS2Controller.keyboard();
  fabgl::VirtualKeyItem item;
  for (;;) {
    if (keyboard->getNextVirtualKey(&item, 20)) {
      if (!tiKeyEvent(item) && item.down) {
        menuRun(&DisplayController, keyboard);      // F12
      }
    }
  }
}

void setup() {
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
  bootloaderReleaseOtadata();   // must stay the first thing setup() does
#endif
  Serial.begin(115200);
  delay(300);
  Serial.println("\nClassic99 ESP32 " CLASSIC99_ESP32_VERSION " (" CLASSIC99_ESP32_BUILD_DATE ")" BUILD_TARGET_NAME);

  if (!psramFound()) {
    Serial.println("ERROR: no PSRAM found - a TTGO VGA32 v1.4 is required");
  }

  PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
  DisplayController.begin();
  DisplayController.setResolution(QVGA_320x240_60Hz);

  fabgl::Canvas cv(&DisplayController);
  cv.selectFont(&fabgl::FONT_6x8);
  cv.setBrushColor(RGB888(0, 0, 0));
  cv.setPenColor(RGB888(255, 255, 255));
  cv.clear();
  cv.drawText(8, 8, "Classic99 ESP32 " CLASSIC99_ESP32_VERSION);
  cv.waitCompletion();

  // Never format the user's card on mount failure.
  bool sdMounted = FileBrowser::mountSDCard(false, "/SD");
#if BOOT_ERROR_TEST == 2
  sdMounted = false;
#endif
  if (!sdMounted) {
    Serial.println("BOOT ERROR: SD card mount failed");
    bootErrorScreen(&DisplayController, PS2Controller.keyboard(), false, roms, 3);
  }
  debugLog = configDebugEnabled();    // read early so the boot messages follow the setting

  // hottest memory in internal RAM; cartridge ROM/GROM, 32K expansion and disk DSR in PSRAM
  consoleROM = (Byte *)heap_caps_calloc(1, 0x2000, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  scratchpad = (Byte *)heap_caps_calloc(1, 0x100, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  expRAM = (Byte *)heap_caps_calloc(1, 0x8000, MALLOC_CAP_SPIRAM);
  grom = (Byte *)heap_caps_calloc(1, 0x6000, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);    // console GROM: read on every GPL fetch
  cartGrom = (Byte *)heap_caps_calloc(1, 0xa000, MALLOC_CAP_SPIRAM);
  diskROMImage = (Byte *)heap_caps_calloc(1, 0x2000, MALLOC_CAP_SPIRAM);
  if (!consoleROM || !scratchpad || !expRAM || !grom || !cartGrom || !diskROMImage) {
    fatal(cv, "Out of memory");
  }

  // try all three so the warning screen can show the state of each
  roms[0].present = loadFile(roms[0].path, consoleROM, 0x2000, nullptr);
  roms[1].present = loadFile(roms[1].path, grom, 0x6000, nullptr);
  roms[2].present = loadFile(roms[2].path, diskROMImage, 0x2000, nullptr);
#if BOOT_ERROR_TEST == 1
  roms[0].present = roms[1].present = false;
#endif
  if (!roms[0].present || !roms[1].present) {
    for (auto &r : roms) {
      if (!r.present) Serial.printf("BOOT ERROR: missing %s%s\n", r.path + 3, r.optional ? " (optional)" : "");
    }
    bootErrorScreen(&DisplayController, PS2Controller.keyboard(), true, roms, 3);
  }
  if (roms[2].present) {
    diskDSR = diskROMImage;     // enables the disk controller at CRU >1100
  }

  vdpInit(&DisplayController);
  soundInit();
  speechInit();                 // optional speech ROM; switched on by the saved setting
  configLoad();                 // last cartridge and disks
  emuInit();
  printHeap("ready");

  // internal RAM is tight and fragmented: take the largest stack first and check them all
  bool ok = emuStart();
  ok &= xTaskCreatePinnedToCore(inputTask, "input", 6144, nullptr, 3, nullptr, 0) == pdPASS;
  ok &= xTaskCreatePinnedToCore(consoleTask, "console", 4096, nullptr, 2, nullptr, 0) == pdPASS;
  printHeap("tasks");
  if (!ok) fatal(cv, "Out of memory creating tasks");
}

// serial console for testing until the on-screen menu exists:
//   ls | cart <n> | eject | reset
static void serialCommand(char *cmd) {
  if (strcmp(cmd, "ls") == 0) {
    int n = cartScan();
    for (int i = 0; i < n; i++) {
      const CartEntry *e = cartGet(i);
      Serial.printf("%3d  %-24s", i, e->name);
      for (int f = 0; f < e->nFiles; f++) Serial.printf(" %s", e->files[f]);
      Serial.println();
    }
  } else if (strncmp(cmd, "cart ", 5) == 0) {
    emuPause(true);
    tiKeyReleaseAll();
    if (cartGet(0) == nullptr) cartScan();
    if (cartLoad(atoi(cmd + 5))) Serial.printf("Inserted %s\n", cartCurrentName());
    emuReset();
    emuPause(false);
  } else if (strcmp(cmd, "eject") == 0) {
    emuPause(true);
    cartEject();
    emuReset();
    emuPause(false);
  } else if (strncmp(cmd, "disk ", 5) == 0 && cmd[5] >= '1' && cmd[5] <= '3') {
    // disk <n> <file>   or   disk <n>   to unmount
    emuPause(true);
    if (cmd[6] == ' ') diskMount(cmd[5] - '0', cmd + 7); else diskUnmount(cmd[5] - '0');
    emuPause(false);
  } else if (strncmp(cmd, "key ", 4) == 0) {
    // inject a PC key (for driving the F12 menu from serial): f12 up down enter esc
    static const struct { const char *name; fabgl::VirtualKey vk; } keys[] = {
      { "f12", fabgl::VK_F12 }, { "up", fabgl::VK_UP }, { "down", fabgl::VK_DOWN },
      { "enter", fabgl::VK_RETURN }, { "esc", fabgl::VK_ESCAPE },
      { "left", fabgl::VK_LEFT }, { "right", fabgl::VK_RIGHT }, { "tab", fabgl::VK_TAB },
    };
    for (auto &k : keys) {
      if (strcmp(cmd + 4, k.name) == 0) {
        PS2Controller.keyboard()->injectVirtualKey(k.vk, true);
        PS2Controller.keyboard()->injectVirtualKey(k.vk, false);
      }
    }
  } else if (strncmp(cmd, "type ", 5) == 0) {
    tiType(cmd + 5);
  } else if (strcmp(cmd, "screen") == 0) {
    // dump the name table as text (BASIC biases characters by >60)
    int cols = (VDPREG[1] & 0x10) ? 40 : 32;
    const Byte *sit = vdpRam() + (((VDPREG[2] & 0x0f) << 10) & 0x3fff);
    char line[42];
    for (int r = 0; r < 24; r++) {
      for (int c = 0; c < cols; c++) {
        int ch = sit[r * cols + c];
        if (ch >= 0x80) ch -= 0x60;
        line[c] = (ch >= 32 && ch <= 126) ? ch : '.';
      }
      line[cols] = 0;
      Serial.printf("|%s|\n", line);
    }
  } else if (strcmp(cmd, "debug on") == 0 || strcmp(cmd, "debug off") == 0) {
    // for this session; the Setup menu saves the setting
    debugLog = (cmd[7] == 'n');
    Serial.printf("debug %s\n", debugLog ? "on" : "off");
  } else if (strncmp(cmd, "speech", 6) == 0) {
    // speech on|off for this session (the Setup menu saves the setting); alone, prints the state
    if (cmd[6] == ' ') {
      emuPause(true);
      speechSetEnabled(strcmp(cmd + 7, "on") == 0);
      emuPause(false);
    }
    SpeechStats st;
    speechGetStats(&st);
    Serial.printf("speech %s%s: %u samples, %u overflows, %u underruns, %u halts%s%s\n",
                  speechEnabled() ? "on" : "off", speechAvailable() ? "" : " (no ROM)",
                  (unsigned)st.samples, (unsigned)st.overflows, (unsigned)st.underruns, (unsigned)st.halts,
                  st.talking ? ", talking" : "", st.halted ? ", cpu halted" : "");
  } else if (strcmp(cmd, "reset") == 0) {
    emuPause(true);
    emuReset();
    emuPause(false);
  } else if (cmd[0]) {
    Serial.println("commands: ls | cart <n> | eject | reset | disk <1-3> [file] | screen | debug on/off | speech [on/off] | type <text> (| = Enter) | key f12/up/down/left/right/tab/enter/esc");
  }
}

// serial console on core 0: core 1 belongs to the emulation task, which may leave
// no idle time for lower-priority tasks there
static void consoleTask(void *) {
  static char buf[160];
  int len = 0;
  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n') {
        buf[len] = 0;
        serialCommand(buf);
        len = 0;
      } else if (len < (int)sizeof(buf) - 1) {
        buf[len++] = c;
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
