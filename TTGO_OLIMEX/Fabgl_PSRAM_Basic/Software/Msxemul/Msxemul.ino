/*
   Menu_FabGL_Msxemul.ino
   MSX Emulator for ESP32 TTGO VGA32

   SD Card connections (SPI):
     MISO => GPIO  2  for TTGO 35 for OLIMEX-SBC
     MOSI => GPIO 12  
     CLK  => GPIO 14
     CS   => GPIO 13

   To exit a game: press F12.
   Compile with Arduino IDE + Espressif ESP32 v2.0.17 (PSRAM enabled)
*/

// ─── Includes ────────────────────────────────────────────────────────────────
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <stdio.h>
#include <string.h>

#include "fabgl.h"
#include "fabui.h"
#include "MSX.h"
#include "ESP32_MSX.h"
#include "esp32-hal-psram.h"

#include "Hourglass.h"


extern "C" {
#include "esp_spiram.h"
}

//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------


// ─── Configuration constants ─────────────────────────────────────────────────
#define FORMAT_ON_FAIL    false
#define SDCARD_MOUNT_PATH "/SD"

// SPI pins for SD card
#define SD_MISO 35  //2
#define SD_MOSI 12
#define SD_CLK  14
#define SD_CS   13

#define DBLCLICK_GUARD_MS  400

#ifndef LSB_FIRST
#define LSB_FIRST
#endif

// ─── Global variables ────────────────────────────────────────────────────────
char directorio[80];

Preferences preferences;

fabgl::VGA16Controller VGAController;
fabgl::PS2Controller   PS2Controller;
fabgl::Canvas         *Canvas;

bool emuRunning = false;
bool selecting  = false;
byte PSLReg     = 0;

// ─── fMSX global variable bindings ──────────────────────────────────────────
extern "C" {
  extern int         Mode;
  extern int         RAMPages;
  extern int         VRAMPages;
  extern const char *ROMName[MAXCARTS];
  extern const char *ProgDir;
  extern byte        Verbose;
  extern int         SetAudio(int, int);
  extern void        StopAudio();
}

static fabgl::Cursor hourglassCursor;

// ─── Helper functions ────────────────────────────────────────────────────────
static bool isRomFile(const char *filename) {
  if (!filename) return false;
  return (strstr(filename, ".ROM") != NULL) ||
         (strstr(filename, ".rom") != NULL) ||
         (strstr(filename, ".MX1") != NULL) ||
         (strstr(filename, ".mx1") != NULL);
}

static void mountSDAndHaltOnError() {
  Serial.println("Mounting SD card...");
  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR: SD Card Mount Failed!");
    Canvas->setPenColor(Color::Red);
    Canvas->drawText(10, 30, "SD Card Mount Failed!");
    Canvas->waitCompletion();
    while (1) {
      delay(1000);
    }
  }
  SD.open("/");
}

static void mountAndStartMSX() {
  Canvas->clear();
  Canvas->waitCompletion();

  VGAController.setResolution(VGA_320x200_70Hz);
  VGAController.setMouseCursor(nullptr);

  mountSDAndHaltOnError();

  emuRunning = true;
  int result = StartMSX(Mode, RAMPages, VRAMPages);
  Serial.printf("StartMSX returned: %d\n", result);
  emuRunning = false;

  StopAudio();
  TrashMachine();
}

// ─── UI Application class ────────────────────────────────────────────────────
class MyApp : public uiApp {

    uiFileBrowser *fileBrowser;
    uiFrame       *frame;
    uiButton      *formatBtn;


    unsigned long lastDirNavMs = 0;

    void setBusy() {
      selecting = true;
      VGAController.setMouseCursor(&hourglassCursor);
    }

    void setReady() {
      selecting = false;
      VGAController.setMouseCursor(fabgl::CursorName::CursorPointerSimpleReduced);
    }

    void enterDirectory() {
      setBusy();


      fileBrowser->changeDirectory(fileBrowser->filename());



      fileBrowser->update();

      lastDirNavMs = millis();

      setReady();
    }

    bool isPhantomClick() {
      return (millis() - lastDirNavMs) < DBLCLICK_GUARD_MS;
    }

    void init() {

      Mode      = MSX_MSX1 | MSX_NTSC | MSX_GUESSA | MSX_GUESSB;
      RAMPages  = 4;
      VRAMPages = 2;
      Verbose   = 1;
      ProgDir   = "/";

      rootWindow()->frameStyle().backgroundColor = RGB888(0, 0, 64);

      frame = new uiFrame(rootWindow(), "MSXEmul Emulator", Point(15, 10), Size(375, 275));
      frame->frameProps().hasCloseButton = false;

      fileBrowser = new uiFileBrowser(frame, Point(10, 25), Size(355, 210));
      fileBrowser->setDirectory(SDCARD_MOUNT_PATH);

      char ruta[160];
      strcpy(ruta, "");
      strncat(ruta, directorio + 4, strlen(directorio) - 4);
      ruta[sizeof(ruta) - 1] = '\0';

      if (fileBrowser->content().exists(ruta, true)) {
        fileBrowser->setDirectory(directorio);
      }
      fileBrowser->update();

      formatBtn = new uiButton(frame, "Run BASIC", Point(10, 240), Size(100, 25));
      formatBtn->onClick = [&]() {
        if (selecting) return;
          ROMName[0] = 0;
          ROMName[1] = 0;
          Serial.println("Starting MSX BASIC");
          mountAndStartMSX();
        quit(0);
      };
      formatBtn = new uiButton(frame, "Quit", Point(260, 240), Size(100, 25));
      formatBtn->onClick = [&]() {
        if (selecting) return;
        load_binary();
      };

      fileBrowser->onClick = [&]() {
        if (selecting)        return;
        if (isPhantomClick()) {
          Serial.println("Phantom double-click discarded");
          return;
        }
        if (fileBrowser->isDirectory()) {

          enterDirectory();
        } else {
          RunEmulator();
        }
      };

      // ─── onChange: se dispara cuando el directorio cambia (navegación completada) ───
      fileBrowser->onChange = [&]() {
        // Restaurar la flecha después de que la navegación nativa termine
        VGAController.setMouseCursor(fabgl::CursorName::CursorPointerSimpleReduced);
        Canvas->waitCompletion();
        selecting = false;
      };

      fileBrowser->onKeyType = [&](fabgl::uiKeyEventInfo evt) {
        if (selecting)        return;
        if (isPhantomClick()) {
          Serial.println("Phantom key event discarded");
          return;
        }
        if (evt.VK == fabgl::VirtualKey::VK_RETURN) {
          selecting = true;
          if (!fileBrowser->isDirectory()) {
            RunEmulator();
          }
        } else {
          // Para cualquier otra tecla, asegurar que la flecha esté visible
          VGAController.setMouseCursor(fabgl::CursorName::CursorPointerSimpleReduced);
        }
      };

      setFocusedWindow(fileBrowser);
      VGAController.setMouseCursor(fabgl::CursorName::CursorPointerSimpleReduced);
    }

    void updateBrowser() {
      fileBrowser->update();
    }

    void RunEmulator() {
      char  fichero[160];
      char *Sdir = "/";

      strcpy(directorio, fileBrowser->content().directory());
      strcpy(fichero, "");
      strncat(fichero, directorio + 3, strlen(directorio) - 3);
      strncat(fichero, Sdir, 160);
      strncat(fichero, fileBrowser->filename(), 160);
      fichero[sizeof(fichero) - 1] = '\0';

      if (!isRomFile(fichero)) {
        Serial.printf("Not a ROM file: %s\n", fichero);
        return;
      }

      ROMName[0] = fichero;
      ROMName[1] = 0;
      Serial.println("Starting MSX emulation...");
      Serial.printf("Loading ROM: %s\n", fichero);

      mountAndStartMSX();
      quit(0);
    }

} app;

// ─── Arduino setup ───────────────────────────────────────────────────────────
void setup() {
  setCpuFrequencyMhz(240);

  Serial.begin(115200);
  Serial.println("\n\n=== MSX Emulator Starting ===");

  hourglassCursor.hotspotX      = 6;
  hourglassCursor.hotspotY      = 11;
  hourglassCursor.bitmap.width  = 13;
  hourglassCursor.bitmap.height = 22;
  hourglassCursor.bitmap.format = fabgl::PixelFormat::RGBA2222;
  hourglassCursor.bitmap.data   = (uint8_t *)hourglassBitmap;

  preferences.begin("Datos", false);
  String dir = preferences.getString("Dir", SDCARD_MOUNT_PATH);
  strcpy(directorio, dir.c_str());

  if (!psramInit()) {
    Serial.println("ERROR: PSRAM initialization FAILED!");
    while (1) {
      delay(1000);
    }
  }
  Serial.printf("PSRAM initialized: %d bytes total, %d bytes free\n",
                ESP.getPsramSize(), ESP.getFreePsram());

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);
  Serial.println("PS2 Controller initialized");

  // ─── CALLBACK GLOBAL DE TECLADO VIA onVirtualKey con firma correcta ────────
  PS2Controller.keyboard()->onVirtualKey = [](fabgl::VirtualKey * key, bool down) {
    if ((down && *key == fabgl::VirtualKey::VK_RETURN) && (!emuRunning)) {
      // Cambiar cursor INMEDIATAMENTE al reloj de arena
      VGAController.setMouseCursor(&hourglassCursor);
      Canvas->waitCompletion();

    }
  };

  if (!InitMemorySystem()) {
    Serial.println("ERROR: InitMemorySystem() failed!");
    while (1) {
      delay(1000);
    }
  }
  if (!InitMachine()) {
    Serial.println("ERROR: InitMachine() failed!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Initializing sound system...");
  if (!SetAudio(22050, 127)) {
    Serial.println("WARNING: Sound initialization failed!");
  } else {
    Serial.println("Sound system initialized");
  }

  while (!FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH)) {
    Serial.println("Waiting for SD card...");
    delay(1000);
  }
  Serial.println("SD card mounted successfully");

  VGAController.begin();
  Serial.println("VGA16 Controller initialized");

  Canvas = new fabgl::Canvas(&VGAController);
  VGAController.setResolution(VGA_400x300_60Hz);
  Canvas->clear();
  Canvas->waitCompletion();
}

//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
void performUpdate(Stream &updateSource, size_t updateSize) {

  if (Update.begin(updateSize, U_FLASH)) {
    size_t written = Update.writeStream(updateSource);
    if (Update.end()) {
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}
//*********************************
void load_binary() {
  mountSDAndHaltOnError();
  if ( !SD.exists("/basic.bin") ) Serial.println("Basic.bin not found!");
  File updateBin = SD.open("/basic.bin");
  Serial.println("load Basic...");

  if (updateBin) {
    size_t updateSize = updateBin.size();
    Serial.println(updateSize, DEC);
    if (updateSize > 0) {
      performUpdate(updateBin, updateSize);
    }
    updateBin.close();
  }
}

// ─── Arduino loop ────────────────────────────────────────────────────────────
void loop() {
  app.run(&VGAController);

  Serial.println("Emulator finished");
  preferences.putString("Dir", String(directorio));
  ESP.restart();
}
