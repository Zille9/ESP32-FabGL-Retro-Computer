/* SMS VGA Emulator for TTGO VGA32 + OLIMEX SBC - Auswahl in config.h */
// Für OTA-Funktion musste sms_keyboard.cpp und sms_ui.cpp angepasst werden!!!

#pragma GCC optimize("O3")

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/dac.h>
#include <driver/timer.h>
#include "config.h"
#include "src/sms_vga_adapter.h"
#include "src/sms_ui.h"
#include "src/sms_keyboard.h"
//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>
extern bool backto;  //Marker für OTA-Funktion
//-------------------------------------------------------------------------------------------------------------------------------------------------

extern "C" {
#include "src/sms_core/shared.h"
#include "src/sms_core/system.h"
#include "src/sms_core/sms.h"
#include "src/sms_core/sn76496.h"
}

// ==================== GLOBAL OBJECTS ====================
SPIClass SD_SPI(HSPI);
SMSVGADisplay display;
SMS_UI ui(&display);

static uint8_t* sms_videodata = nullptr;

static volatile int audio_idx  = 0;
static int32_t prev_sample     = 0;
static int32_t prev2_sample    = 0;
static int32_t dc_offset       = 0;


// ==================== FUNCTION PROTOTYPES ====================
bool initSD();
void initAudio();
void loadGame(const char* path);
void emulate();

// ==================== TIMER ISR - AUDIO ====================
static void IRAM_ATTR audio_timer_isr(void* arg) {
  timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
  timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

  if (!snd.enabled || !snd.buffer[0] || !snd.buffer[1] || snd.bufsize <= 0) {
    dac_output_voltage(DAC_CHANNEL_1, 128);
    return;
  }

  int idx = audio_idx;

  // Mix mono channels
  int32_t s = ((int32_t)snd.buffer[0][idx] + (int32_t)snd.buffer[1][idx]) >> 1;

  // DC offset filter - removes hum
  dc_offset += (s - (dc_offset >> 8));
  s -= (dc_offset >> 8);

  // FIR low-pass filter [1,2,1] - reduces aliasing
  int32_t filtered = (s + (prev_sample << 1) + prev2_sample) >> 2;
  prev2_sample = prev_sample;
  prev_sample  = filtered;

  // Convert to 8-bit for DAC
  int32_t out = (filtered >> 8) + 128;
  if (out > 255) out = 255;
  if (out <   0) out = 0;

  dac_output_voltage(DAC_CHANNEL_1, (uint8_t)out);
  audio_idx = (idx + 1 >= snd.bufsize) ? 0 : idx + 1;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== SMS VGA EMULATOR ===\n");

  setCpuFrequencyMhz(240);

  SMSVGADisplay::init();

  SMSVGADisplay::fillScreen(VGA_COLOR_RED_RGB565);   delay(300);
  SMSVGADisplay::fillScreen(VGA_COLOR_GREEN_RGB565); delay(300);
  SMSVGADisplay::fillScreen(VGA_COLOR_BLUE_RGB565);  delay(300);
  SMSVGADisplay::fillScreen(VGA_COLOR_BLACK_RGB565);

  Serial.print("SD card... ");
  if (!initSD()) {
    Serial.println("ERROR");
    while (true) delay(1000);
  }
  Serial.println("OK");

  Serial.print("Keyboard... ");
  SMSKeyboard::init();
  Serial.println("OK");

  Serial.print("Audio DAC... ");
  initAudio();
  Serial.println("OK");
  String str2 = "BASIC";
  while (!backto) {
    std::string gamePath = ui.selectGame();
    //Serial.println(gamePath.c_str());

    if (!gamePath.empty()) {
      if (gamePath == "BASIC") {
        backto = true;
      }
      else {
        loadGame(gamePath.c_str());
        emulate();
      }
    }

  }
}



// ==================== SD CARD ====================
bool initSD() {
  SD_SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  return SD.begin(SD_CS_PIN, SD_SPI, SD_FREQ);
}

// ==================== AUDIO ====================
void initAudio() {
  dac_output_enable(DAC_CHANNEL_1);
  dac_output_voltage(DAC_CHANNEL_1, 128);

  timer_config_t timer_config = {
    .alarm_en    = TIMER_ALARM_EN,
    .counter_en  = TIMER_PAUSE,
    .intr_type   = TIMER_INTR_LEVEL,
    .counter_dir = TIMER_COUNT_UP,
    .auto_reload = TIMER_AUTORELOAD_EN,
    .divider     = 2   // 80MHz APB / 2 = 40MHz base
  };

  timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

  uint64_t alarm_value = 40000000ULL / SAMPLE_RATE;
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, alarm_value);
  timer_enable_intr(TIMER_GROUP_0, TIMER_0);
  timer_isr_register(TIMER_GROUP_0, TIMER_0, audio_timer_isr, NULL,
                     ESP_INTR_FLAG_IRAM, NULL);
  timer_start(TIMER_GROUP_0, TIMER_0);

  Serial.printf("Audio timer: %d Hz (alarm=%llu ticks)\n", SAMPLE_RATE, alarm_value);
}

// ==================== LOAD GAME ====================
void loadGame(const char* path) {
  Serial.printf("Loading: %s\n", path);

  if (!sms.dummy) {
    sms.dummy = new uint8_t[0x2000];
    if (sms.dummy) memset(sms.dummy, 0, 0x2000);
  }
  if (!sms.sram) {
    sms.sram = new uint8_t[0x8000];
    if (sms.sram) memset(sms.sram, 0, 0x8000);
  }

  File rom = SD.open(path);
  if (!rom) {
    Serial.println("ERROR: no ROM");
    return;
  }

  int len = rom.size();
  uint8_t* rom_data = (uint8_t*)malloc(len);
  if (!rom_data) {
    Serial.println("ERROR: no memory");
    rom.close();
    return;
  }
  rom.read(rom_data, len);
  rom.close();

  cart.rom   = rom_data;
  cart.pages = (len + 0x3FFF) / 0x4000;

  String ext = String(path);
  ext.toLowerCase();
  cart.type = ext.endsWith(".sms") ? TYPE_SMS : TYPE_GG;

  Serial.printf("ROM: %d bytes, %d pages, %s\n",
                len, cart.pages, cart.type == TYPE_SMS ? "SMS" : "GG");

  if (!sms_videodata) {
    sms_videodata = (uint8_t*)calloc(256 * 240, 1);
    if (sms_videodata) {
      bitmap.data   = sms_videodata;
      bitmap.width  = 256;
      bitmap.height = 240;
      bitmap.pitch  = 256;
      bitmap.depth  = 8;
    }
  }

  // Reset audio state
  audio_idx    = 0;
  prev_sample  = 0;
  prev2_sample = 0;
  dc_offset    = 0;

  emu_system_init(SAMPLE_RATE);
  sms_init();

  sms_set_vga_display(&display);
  g_vga_display = &display;

  Serial.printf("Ready. Heap: %d, bufsize: %d\n", ESP.getFreeHeap(), snd.bufsize);
}

// ==================== EMULATION LOOP ====================
void emulate() {
  Serial.println("Emulation started");
  display.beginEmulation();

  const uint8_t FRAMESKIP = FRAME_SKIP;
  uint8_t frame_counter = 0;

  const uint64_t FRAME_TIME = 16639;
  uint64_t next_frame = esp_timer_get_time();
  uint32_t frame = 0;
  uint32_t rendered_frames = 0;

  while (!backto) {

    // --- Input ---
    uint8_t key = SMSKeyboard::read();

    if ((key & SMSKeyboard::SMS_START) && (key & SMSKeyboard::SMS_PAUSE)) {
      input.system |= INPUT_SOFT_RESET;
    } else {
      input.system &= ~INPUT_SOFT_RESET;
    }

    if ((key & SMSKeyboard::SMS_PAUSE) && !(key & SMSKeyboard::SMS_START)) {
      input.system |= INPUT_PAUSE;
    } else {
      input.system &= ~INPUT_PAUSE;
    }

    if ((key & SMSKeyboard::SMS_START) && !(key & SMSKeyboard::SMS_START)) {
      input.system |= INPUT_START;
    } else {
      input.system &= ~INPUT_START;
    }

    input.pad[0] = 0;
    if (key & SMSKeyboard::SMS_UP)    input.pad[0] |= INPUT_UP;
    if (key & SMSKeyboard::SMS_DOWN)  input.pad[0] |= INPUT_DOWN;
    if (key & SMSKeyboard::SMS_LEFT)  input.pad[0] |= INPUT_LEFT;
    if (key & SMSKeyboard::SMS_RIGHT) input.pad[0] |= INPUT_RIGHT;
    if (key & SMSKeyboard::SMS_BTN1)  input.pad[0] |= INPUT_BUTTON1;
    if (key & SMSKeyboard::SMS_BTN2)  input.pad[0] |= INPUT_BUTTON2;
    if (key & SMSKeyboard::SMS_PAUSE)  {
      backto = true;
      break;
    }

    input.pad[1] = 0;

    // --- Emulation with frameskip ---
    bool should_render = (frame_counter == 0);

    if (should_render) {
      audio_idx = 0;
      sms_frame(0);
      rendered_frames++;
    } else {
      sms_frame(1);
      if (snd.enabled && snd.buffer[0] && snd.buffer[1]) {
        audio_idx = 0;
        SN76496Update(0, snd.buffer, snd.bufsize, sms.psg_mask);
      }
    }

    frame_counter++;
    if (frame_counter > FRAMESKIP) frame_counter = 0;

    // --- Timing ---
    uint64_t now = esp_timer_get_time();
    if (now < next_frame) {
      ets_delay_us(next_frame - now);
    }
    next_frame += FRAME_TIME;
    frame++;

#ifdef DEBUG
    if (frame % 500 == 0) {
      float render_pct = (rendered_frames * 100.0f) / frame;
      Serial.printf("Frame: %d | Heap: %d | Rendered: %.0f%%\n",
                    frame, ESP.getFreeHeap(), render_pct);
    }
#endif
  }
  vTaskDelete(NULL);
}


//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
void performUpdate(Stream &updateSource, size_t updateSize) {
  timer_pause(TIMER_GROUP_0, TIMER_0);       // Stoppt den Hardware-Zähler
  timer_disable_intr(TIMER_GROUP_0, TIMER_0); // Deaktiviert den Alarm-Interrupt

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


void loop() {
  if (backto) {
    load_binary();
  }
 
}
