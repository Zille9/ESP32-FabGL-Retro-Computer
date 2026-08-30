/*
 * ============================================================================
 *  MSX Emulator for ESP32 (Work in Progress)
 * ============================================================================
 * 
 * [ Credits & Acknowledgments ]
 *  - Graphics/Audio Engine: FabGL by Fabrizio Di Vittorio.
 *  - Z80 CPU Core: Modified based on the work by Marat Fayzullin.
 *  - VDP Section: Modified based on the TMS9918A core originally by David Latham 
 *                 and maintained by Marat Fayzullin.
 *  Special thanks to all authors for providing these excellent foundations.
 * 
 * [ Development Environment ]
 *  - Board Manager: ESP32 by Espressif Systems (v2.0.5 recommended)
 *  - Library: FabGL v1.0.9
 * 
 * [ Recommended Arduino IDE Settings ]
 *  - PSRAM: "Enabled"
 *  - Partition Scheme: "Default 4MB with SPIFFS" or similar
 *  - ESP32 Dev Module
 * 
 * [ Disclaimer ]
 *  IN NO EVENT SHALL FABRIZIO DI VITTORIO, MARAT FAYZULLIN, DAVID LATHAM, 
 *  OR THE DEVELOPER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, OR 
 *  CONSEQUENTIAL DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE. 
 *  USE AT YOUR OWN RISK.
 * 
 * [ Important Notes ]
 *  - This project is under development and may contain bugs or redundant code.
 *  - Localization (e.g., keyboard mappings) should be customized by the user.
 *  - Subject to the license policies of FabGL and Marat Fayzullin (fMSX License).
 * ============================================================================
 */

/* Japanese
 * ============================================================================
 *  MSX Emulator for ESP32 (Work in Progress)
 * ============================================================================
 * 
 * 【クレジットと謝辞】
 *  - グラフィック/オーディオエンジン: Fabrizio Di Vittorio氏による FabGL を使用しています。
 *  - Z80 CPUコア: Marat Fayzullin氏によるコードをベースに改変して使用しています。
 *  - VDPセクション: David Latham氏によるTMS9918Aエミュレーションコアをベースに、
 *                  Marat Fayzullin氏が調整を加えた実装を改変して使用しています。
 *  素晴らしい基盤を公開されている諸氏に深く感謝の意を表します。
 * 
 * 【動作確認済み環境 / Required Libraries】
 *  - ボードマネージャ: ESP32 by Espressif Systems (v2.0.5 推奨)
 *  - 使用ライブラリ: FabGL v1.0.9
 * 
 * 【Arduino IDE 推奨設定】
 *  - PSRAM: "Enabled" 
 *  - Partition Scheme: "Default 4MB with SPIFFS" 等
 *  - ESP32 Dev Module
 * 
 * 【免責事項】
 *  本ソフトウェアの使用によって生じたいかなる損害（直接的・間接的を問わず）
 *  についても、Fabrizio Di Vittorio氏、Marat Fayzullin氏、David Latham氏、
 *  および開発者本人は一切の責任を負いません。利用者の責任において使用してください。
 * 
 * 【注意事項】
 *  - 本プロジェクトは開発中であり、不具合や最適化不足なコードが含まれる場合があります。
 *  - キーボード配列等のローカライズに関しては、各自の環境に合わせて修正してください。
 *  - 本ソフトウェア内の各コアのライセンスは、FabGL、および Marat Fayzullin氏 
 *    (fMSX License) の各ポリシーに従います。
 * ============================================================================
 */


#pragma GCC optimize("O3")

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <SPI.h>
#include <SD.h>
#include <fabgl.h>
#include <vector>
#include <WiFi.h>
#include "soc/rtc_wdt.h"
#include "esp_task_wdt.h"
#include "tms9918a.h"
#include "Z80.h"
#include "KeyboardSystem.hpp"

//Please select one of the following options.
#define KEYBORD_MAP (KeyboardLayout::GERMAN)
//#define KEYBORD_MAP (KeyboardLayout::US_ENGLISH)
//#define KEYBORD_MAP (KeyboardLayout::PORTUGUESE)
//#define KEYBORD_MAP (KeyboardLayout::SPANISH)

//#define USE_SERIAL_DEBUG

//Profiling load for cpuTask (IO14) & loop (IO2); SD access is exclusive.
//#define USE_SIGNAL_DEBUG

#define USE_FDC

// TTGO VGA32, 
#define SD_SCLK 14
#define SD_MISO 35 //2
#define SD_MOSI 12
#define SD_CS 13

// // ORANGE ESPer
// #define SD_SCLK 18
// #define SD_MISO 19
// #define SD_MOSI 23 // or 12
// #define SD_CS 5

#define VDP_SAVE_SIZE offsetof(struct tms9918a, rasterbuffer0)
#define VRAM_SIZE 16384

extern "C" {
  int readByte(void* self, int addr);
  void writeByte(void* self, int addr, int val);
  int readIO(void* context, int port);
  void writeIO(void* context, int port, int val);

  extern Z80 cpu;

  byte RdZ80(zword Addr);
  void WrZ80(zword Addr, byte Value);
  byte InZ80(zword Port);
  void OutZ80(zword Port, byte Value);
  void PatchZ80(Z80* R);
}

Z80 cpu;

byte IRAM_ATTR RdZ80(zword Addr) {
  return (byte)readByte(NULL, Addr);
}
void IRAM_ATTR WrZ80(zword Addr, byte Value) {
  writeByte(NULL, Addr, Value);
}
byte IRAM_ATTR InZ80(zword Port) {
  return (byte)readIO(NULL, Port);
}
void IRAM_ATTR OutZ80(zword Port, byte Value) {
  writeIO(NULL, Port, Value);
}
void PatchZ80(Z80* R) {}

// --- Prototypes ---
void listROMs();
void detectMapper(uint32_t size);
void updateKeyboard();
void updateFabGLSound();
void handleDiskSelect(int drive);
String showFileSelector(const char* title, const char* dirPath, const char* ext);
void handleRomSelect();
void flashStatus(const char* message, int ms);
bool loadSdToPsram(int drive, String path);
bool savePsramToSd(int drive, String path);
void clearCanvasDouble();

enum MapperType { MAPPER_NONE,
                  MAPPER_KONAMI,
                  MAPPER_ASCII8,
                  MAPPER_ASCII16,
                  MAPPER_RTYPE };
MapperType currentMapper = MAPPER_NONE;

fabgl::VGA16Controller DisplayController;
fabgl::Canvas* canvas;
fabgl::Bitmap* msxBitmap;
fabgl::SoundGenerator soundGenerator;
fabgl::NoiseWaveformGenerator noiseGen;
fabgl::Mouse Mouse;
fabgl::PS2Controller PS2Controller;

// state save
struct StateSaveData {
  uint16_t AF, BC, DE, HL, IX, IY, PC, SPtr;
  uint16_t AF1, BC1, DE1, HL1;
  uint8_t I, R, IFF;
  uint16_t IRequest;
  uint8_t primary_slot;
  uint8_t rom_banks[4];
};
uint8_t* psram_slots[5] = { NULL, NULL, NULL, NULL, NULL };
const size_t SAVE_BUFFER_SIZE = 100 * 1024;
StateSaveData tempState;
bool isPaused = false;
enum class SaveMode { NONE,
                      GAME_TO_PSRAM,
                      PSRAM_TO_GAME,
                      PSRAM_TO_SD,
                      SD_TO_PSRAM };
volatile SaveMode currentSaveMode = SaveMode::NONE;

//FDC
struct WD2793 {
  uint8_t Command;
  uint8_t Status;
  uint8_t Track;
  uint8_t Sector;
  uint8_t Data;
  uint8_t Side;
};
struct WD2793 fdc = { 0x00, 0x80, 0x00, 0x01, 0x00, 0x00 };
struct Drive {
  uint8_t* buffer = nullptr;
  size_t size = 0;
};
Drive drives[2];
uint8_t current_drive = 0;
bool disk_changed[2] = { false, false };
uint8_t* sector_ptr = nullptr;
bool is_reading = false;
bool is_writing = false;
int fdc_data_index = 0;
uint8_t fdc_control_reg = 0x00;
bool fdc_intrq = false;
int fdc_step_direction = -1;
bool fdc_multi_sector = false;
uint8_t drive_sides[2] = { 0, 0 };
uint8_t drive_sectors[2] = { 0, 0 };
char drive_filepath[2][256];
size_t drive_imagesize[2];
uint8_t* disk_ram_a = nullptr;
uint8_t* disk_ram_b = nullptr;

// MemoryMapper/ROM
std::vector<String> romFiles;
String selectedRom = "";
uint8_t* game_rom_ptr;
uint8_t* slot_mem[4] = { nullptr, nullptr, nullptr, nullptr };
int rom_banks[4] = { 0, 1, 2, 3 };
uint8_t* current_pages[4];
uint32_t rom_mask_8k;
uint32_t rom_mask;
uint32_t rom_size_bytes;
long rom_size = 0;
bool isRomListLoaded = false;
uint8_t* read_map[8];
uint8_t* write_map[8];
uint8_t dummy_page[8192];

//psg scc
volatile uint8_t psg_regs[16];
uint8_t old_psg_regs[16] = { 0xFF };
uint8_t psg_address = 0;
int8_t scc_waveform[5][32] = { 0 };
uint16_t scc_f[] = { 0, 0, 0, 0, 0, 0 };  //6ch for converting each PSG ch to 2 SCC chs.
uint8_t scc_v[] = { 0, 0, 0, 0, 0, 0 };
uint8_t scc_enable;
bool scc_active = false;
bool psg2scc = false;
static const int BUFFER_SIZE = 256;
const uint8_t SCC_VOL_TABLE[16] = {
  0, 2, 4, 7, 10, 15, 21, 26, 33, 39, 47, 54, 62, 68, 73, 75
};

uint8_t Dirty_Bass[32]{
  0x80, 0x88, 0x70, 0x98, 0x80, 0x90, 0x78, 0xA0, 0x90, 0xB0, 0xA0, 0xD0, 0xC0, 0xF0, 0xEF, 0xA0,
  0x78, 0x60, 0x70, 0x50, 0x60, 0x40, 0x50, 0x30, 0x40, 0x20, 0x30, 0x10, 0x20, 0x00, 0x10, 0x20
};

uint8_t Pseudo_Vocal[32] = {
  0x80, 0x85, 0x90, 0xA0, 0xC0, 0xFF, 0xC0, 0xA0, 0x90, 0x85, 0x80, 0x7B, 0x70, 0x60, 0x40, 0x00,
  0x40, 0x60, 0x70, 0x7B, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xFF, 0xD0, 0xC0, 0xB0, 0xA0, 0x90
};

uint8_t Warm_Body_PhaseShift[32] = {
  0x80, 0x68, 0x50, 0x38, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x38, 0x50, 0x68,
  0x80, 0x98, 0xB0, 0xC8, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC8, 0xB0, 0x98
};

uint8_t Metallic_Lead[32] = {
  0x80, 0xC0, 0xFF, 0xC0, 0x80, 0x40, 0x00, 0x40, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0x40, 0x00, 0x40,
  0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF, 0xE0, 0xC0, 0xA0, 0x80, 0x60, 0x40, 0x20
};


//keyboard
volatile uint8_t msx_key_matrix[12];
//static uint8_t key_map[256];
static bool key_map_inited = false;
uint8_t joy_bits;
volatile uint8_t joy_bits1;
volatile uint8_t joy_bits2;
uint8_t selected_row = 0;
uint8_t slot_reg = 0;

//VDP
struct tms9918a* vdp;
volatile bool frameReady = false;
bool displayEnabled = true;
//V9938 color
static const fabgl::RGB888 msx_palette[16] = {
  { 0x00, 0x00, 0x00 },  // 0: Transparent (Black)
  { 0x00, 0x00, 0x00 },  // 1: Black
  { 0x2D, 0xDD, 0x22 },  // 2: Medium Green
  { 0x66, 0xFF, 0x66 },  // 3: Light Green
  { 0x22, 0x22, 0xFF },  // 4: Dark Blue
  { 0x44, 0x66, 0xFF },  // 5: Light Blue
  { 0xAA, 0x22, 0x22 },  // 6: Dark Red
  { 0x44, 0xDD, 0xFF },  // 7: Cyan
  { 0xFF, 0x22, 0x22 },  // 8: Medium Red
  { 0xFF, 0x66, 0x66 },  // 9: Light Red
  { 0xDD, 0xDD, 0x22 },  // 10: Dark Yellow
  { 0xDD, 0xDD, 0x88 },  // 11: Light Yellow
  { 0x22, 0x88, 0x22 },  // 12: Dark Green
  { 0xDD, 0x44, 0xAA },  // 13: Magenta
  { 0xAA, 0xAA, 0xAA },  // 14: Gray
  { 0xFF, 0xFF, 0xFF }  // 15: White
};

class SCCGenerator : public fabgl::WaveformGenerator {
private:
  int16_t sampleBuffer[BUFFER_SIZE];
  volatile int head = 0;
  volatile int tail = 0;
  int lastSample = 0;

  int8_t waveform[32];
  uint32_t phase = 0;
  uint32_t phaseInc = 0;
  uint8_t curVol = 0;

public:
  int getSample() override {
    if (head == tail) return lastSample;
    lastSample = sampleBuffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return lastSample;
  }

  void setFrequency(int value) override {}

  void updateParams(int8_t* newWave, uint32_t freq, uint8_t vol) {
    if (freq == 0) {
      phaseInc = 0;
    } else {
      uint32_t freqHz = 111860 / (freq + 1);
      phaseInc = (uint32_t)(freqHz << 18);
    }
    curVol = vol;
    memcpy(waveform, newWave, 32);
  }

  void fillBuffer() {
    int nextHead = (head + 1) % BUFFER_SIZE;

    while (nextHead != tail) {
      sampleBuffer[head] = calculateNextSample();
      head = nextHead;
      nextHead = (head + 1) % BUFFER_SIZE;
    }
  }

private:
  inline int16_t calculateNextSample() {
    if (curVol == 0 || phaseInc == 0) return 0;

    int8_t sample = waveform[(phase >> 27) & 0x1F];
    phase += phaseInc;

    return (int16_t)((int)sample * curVol >> 8);
  }
};

SCCGenerator* scc_ch[5];

void IRAM_ATTR updateSCCSound() {
  for (int i = 0; i < 5; i++) {
    auto* ch = scc_ch[i];
    if ((scc_enable & (1 << i)) && scc_f[i] > 10) {
      int waveIdx = (i < 4) ? i : 3;
      ch->updateParams((int8_t*)scc_waveform[waveIdx], scc_f[i], SCC_VOL_TABLE[scc_v[i]]);
    } else {
      ch->updateParams((int8_t*)scc_waveform[0], 0, 0);
    }
  }
}

class AY38910Generator : public fabgl::WaveformGenerator {
private:
  const uint32_t psgClock = 111860;
  uint32_t tickAccum = 0;
  uint32_t tickStep = 0;

  volatile uint32_t tonePeriod[3] = { 1, 1, 1 };
  volatile uint32_t noisePeriod = 1;
  volatile uint32_t envPeriod = 1;
  volatile uint8_t envShape = 0;
  volatile uint8_t regs[16] = { 0 };
  volatile bool holding = true;
  volatile bool cont = false, alt = false, hold = false, att = false;
  volatile uint32_t envCounter = 0;
  volatile uint32_t envStep = 0;
  volatile uint32_t envVol = 0;

  uint32_t toneCounter[3] = { 0, 0, 0 };
  uint32_t toneOut = 0;
  uint32_t noiseCounter = 0;
  uint32_t lfsr = 1;
  uint32_t noiseOut = 0;

  int16_t volTable[16] = { 0, 5, 10, 15, 20, 26, 32, 40, 48, 58, 68, 78, 90, 102, 114, 127 };

  int16_t sampleBuffer[BUFFER_SIZE];
  volatile int head = 0;
  volatile int tail = 0;
  int lastSample = 0;

public:
  void writeReg(uint8_t reg, uint8_t val) {
    if (reg > 13) return;
    regs[reg] = val;

    switch (reg) {
      case 0:
      case 1:
        tonePeriod[0] = (regs[0] | ((regs[1] & 0x0F) << 8)) ? (regs[0] | ((regs[1] & 0x0F) << 8)) : 1;
        break;
      case 2:
      case 3:
        tonePeriod[1] = (regs[2] | ((regs[3] & 0x0F) << 8)) ? (regs[2] | ((regs[3] & 0x0F) << 8)) : 1;
        break;
      case 4:
      case 5:
        tonePeriod[2] = (regs[4] | ((regs[5] & 0x0F) << 8)) ? (regs[4] | ((regs[5] & 0x0F) << 8)) : 1;
        break;
      case 6:
        noisePeriod = (regs[6] & 0x1F) ? (regs[6] & 0x1F) : 1;
        break;
      case 11:
      case 12:
        envPeriod = (regs[11] | (regs[12] << 8)) ? (regs[11] | (regs[12] << 8)) : 1;
        break;
      case 13:
        envShape = val & 0x0F;
        envStep = 0;
        envCounter = 0;
        cont = envShape & 0x08;
        att = envShape & 0x04;
        alt = envShape & 0x02;
        hold = envShape & 0x01;
        envVol = att ? 0 : 15;
        holding = false;
        break;
    }
  }

  void fillBuffer() {
    if (tickStep == 0) {
      int rate = sampleRate();
      if (rate > 0) setSampleRate(rate);
      else return;
    }

    int nextHead = (head + 1) % BUFFER_SIZE;
    while (nextHead != tail) {
      sampleBuffer[head] = calculateNextSample();
      head = nextHead;
      nextHead = (head + 1) % BUFFER_SIZE;
    }
  }

  void setSampleRate(int rate) {
    this->fabgl::WaveformGenerator::setSampleRate(rate);

    if (rate > 0) {
      tickStep = (uint32_t)(((uint64_t)psgClock << 16) / rate);
    } else {
      tickStep = (uint32_t)(((uint64_t)psgClock << 16) / 16384);
    }
  }

private:
  inline int calculateNextSample() {
    uint32_t curR7 = regs[7];
    uint32_t curR8 = regs[8];
    uint32_t curR9 = regs[9];
    uint32_t curR10 = regs[10];
    uint32_t curTP0 = tonePeriod[0];
    uint32_t curTP1 = tonePeriod[1];
    uint32_t curTP2 = tonePeriod[2];
    uint32_t curNP = noisePeriod;

    tickAccum += tickStep;
    uint32_t ticks = tickAccum >> 16;
    tickAccum &= 0xFFFF;

    while (ticks--) {
      toneCounter[0] += 2;
      while (toneCounter[0] >= curTP0) {
        toneCounter[0] -= curTP0;
        toneOut ^= 1;
      }
      toneCounter[1] += 2;
      while (toneCounter[1] >= curTP1) {
        toneCounter[1] -= curTP1;
        toneOut ^= 2;
      }
      toneCounter[2] += 2;
      while (toneCounter[2] >= curTP2) {
        toneCounter[2] -= curTP2;
        toneOut ^= 4;
      }

      if (++noiseCounter >= curNP) {
        noiseCounter = 0;
        lfsr = (lfsr >> 1) | (((lfsr & 1) ^ ((lfsr >> 3) & 1)) << 16);
        noiseOut = lfsr & 1;
      }

      if (!holding && ++envCounter >= envPeriod) {
        envCounter = 0;
        stepEnvelope();
      }
    }

    uint32_t tMask = toneOut | curR7;
    uint32_t nMask = (noiseOut ? 0x07 : 0) | (curR7 >> 3);
    uint32_t mixMask = tMask & nMask;

    int volA = volTable[(curR8 & 0x10) ? envVol : (curR8 & 0x0F)];
    int volB = volTable[(curR9 & 0x10) ? envVol : (curR9 & 0x0F)];
    int volC = volTable[(curR10 & 0x10) ? envVol : (curR10 & 0x0F)];

    // ノイズ控えめ処理
    if ((curR7 & 0x09) == 0x01) volA >>= 1;
    if ((curR7 & 0x12) == 0x02) volB >>= 1;
    if ((curR7 & 0x24) == 0x04) volC >>= 1;

    int mixedSample = 0;
    if (mixMask & 1) mixedSample += volA;
    if (mixMask & 2) mixedSample += volB;
    if (mixMask & 4) mixedSample += volC;

    return (mixedSample >> 1) - 95;
  }

  int getSample() override {
    if (head == tail) {
      return lastSample;
    }

    lastSample = sampleBuffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return lastSample;
  }

  inline void stepEnvelope() {
    if (envStep < 15) {
      envStep++;
      envVol = att ? envStep : (15 - envStep);
    } else {
      if (!cont) {
        holding = true;
        envVol = 0;
      } else {
        if (hold) {
          holding = true;
          if (alt) att = !att;
          envVol = att ? 15 : 0;
        } else {
          envStep = 0;
          if (alt) att = !att;
          envVol = att ? 0 : 15;
        }
      }
    }
  }

  void setFrequency(int value) override {}
};
AY38910Generator psgGen;

void updateMemoryMap() {
  for (int page = 0; page < 8; page++) {
    int msx_page16 = page >> 1;
    int slot = (slot_reg >> (msx_page16 * 2)) & 0x03;

    read_map[page] = dummy_page;
    write_map[page] = nullptr;

    switch (slot) {
      case 0:
        if (page < 4) {
          read_map[page] = &slot_mem[0][page * 0x2000];
        }
        break;

      case 1:
        if (page >= 2 && page <= 5) {
          read_map[page] = current_pages[page - 2];
        }
        break;

      case 2:
        if (page == 2) read_map[page] = &slot_mem[2][0x4000];
        if (page == 3) read_map[page] = &slot_mem[2][0x6000];
        if (page == 4) read_map[page] = &slot_mem[2][0x8000];
        if (page == 5) read_map[page] = &slot_mem[2][0xA000];
        break;

      case 3:
        read_map[page] = &slot_mem[3][page * 0x2000];
        write_map[page] = &slot_mem[3][page * 0x2000];
        break;
    }
  }
}

inline void updateBankPointer(int page_idx, uint8_t v) {
  rom_banks[page_idx] = v;
  current_pages[page_idx] = &game_rom_ptr[((uint32_t)v & rom_mask_8k) << 13];
  updateMemoryMap();
}

void initMegaROM() {
  rom_mask_8k = (rom_size / 8192) - 1;

  switch (currentMapper) {
    case MAPPER_KONAMI:
      updateBankPointer(0, 0);
      updateBankPointer(1, 1);
      updateBankPointer(2, 2);
      updateBankPointer(3, 3);
      break;

    case MAPPER_ASCII8:
      updateBankPointer(0, 0);
      updateBankPointer(1, 0);
      updateBankPointer(2, 0);
      updateBankPointer(3, 0);
      break;

    case MAPPER_ASCII16:
      updateBankPointer(0, 0);
      updateBankPointer(1, 1);
      updateBankPointer(2, 0);
      updateBankPointer(3, 1);
      break;

    case MAPPER_RTYPE:
      current_pages[0] = &game_rom_ptr[46 * 8192];
      current_pages[1] = &game_rom_ptr[47 * 8192];
      current_pages[2] = &game_rom_ptr[0 * 8192];
      current_pages[3] = &game_rom_ptr[1 * 8192];
      updateMemoryMap();
      break;

    case MAPPER_NONE:
    default:
      updateBankPointer(0, 0);
      updateBankPointer(1, 1);
      updateBankPointer(2, 2);
      updateBankPointer(3, 3);
      break;
  }
}

void initDiskPSRAM() {
  disk_ram_a = (uint8_t*)ps_malloc(720 * 1024);
  disk_ram_b = (uint8_t*)ps_malloc(720 * 1024);
  drives[0].buffer = disk_ram_a;
  drives[0].size = 720 * 1024;
  drives[1].buffer = disk_ram_b;
  drives[1].size = 720 * 1024;
  memset(disk_ram_a, 0, 720 * 1024);
  memset(disk_ram_b, 0, 720 * 1024);
}

bool loadSdToPsram(int drive, String path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t readSize = f.read(drives[drive].buffer, 720 * 1024);
  f.close();

  strncpy(drive_filepath[drive], path.c_str(), sizeof(drive_filepath[drive]) - 1);
  drive_filepath[drive][sizeof(drive_filepath[drive]) - 1] = '\0';  // 終端文字を保証
  drive_imagesize[drive] = readSize;

  uint8_t mediaByte = drives[drive].buffer[0x15];
  uint8_t sectorsPerTrack = 9;
  uint8_t sides = 2;

  switch (mediaByte) {
    case 0xF8:  // 1DD
      sectorsPerTrack = 9;
      sides = 1;
      break;
    case 0xF9:  // 2DD
      sectorsPerTrack = 9;
      sides = 2;
      break;
    case 0xFA:  // 1DD (8 sectors)
      sectorsPerTrack = 8;
      sides = 1;
      break;
    case 0xFB:  // 2DD (8 sectors)
      sectorsPerTrack = 8;
      sides = 2;
      break;
    case 0xFC:  // 1D (40 tracks)
      sectorsPerTrack = 9;
      sides = 1;
      break;
    case 0xFD:  // 2D (40 tracks)
      sectorsPerTrack = 9;
      sides = 2;
      break;
    case 0xFE:  // 1D (8 sectors)
      sectorsPerTrack = 8;
      sides = 1;
      break;
    case 0xFF:  // 2D (8 sectors)
      sectorsPerTrack = 8;
      sides = 2;
      break;
    default:
      if (readSize <= 368640) {
        sides = 1;
        sectorsPerTrack = 9;
      } else {
        sides = 2;
        sectorsPerTrack = 9;
      }
      break;
  }

  drive_sides[drive] = sides;
  drive_sectors[drive] = sectorsPerTrack;
  fdc.Track = 0;
  fdc.Sector = 1;
  disk_changed[drive] = true;
  drives[drive].size = readSize;

  return (readSize > 0);
}

bool savePsramToSd(int drive) {
  if (drive_filepath[drive][0] == '\0' || drive_imagesize[drive] == 0) {
    // do nothing
  } else {
    File f = SD.open(drive_filepath[drive], FILE_WRITE);
    if (f) {
      size_t written = f.write(drives[drive].buffer, drive_imagesize[drive]);
      f.close();
      flashStatus("SAVE DISK PSRAM->SD OK", 2000);
      return true;
    }
  }
  flashStatus("SAVE DISK PSRAM->SD NG", 2000);
  return false;
}

void executeWD2793Command(uint8_t cmd) {
  fdc.Command = cmd;
  fdc_multi_sector = (cmd & 0x10) != 0;

  bool disk_inserted = (disk_changed[current_drive]);
  is_reading = false;
  is_writing = false;
  fdc_data_index = 0;

  // --- Type IV: Force Interrupt (1101 xxxx) ---
  if ((cmd & 0xF0) == 0xD0) {
    fdc.Status = 0x00;
    if (!disk_inserted) fdc.Status |= 0x80;
    if (fdc.Track == 0) fdc.Status |= 0x04;  // Track 00フラグ

    fdc_intrq = true;
    return;
  }

  // --- Type I: Restore, Seek, Step (0xxx xxxx) ---
  if ((cmd & 0x80) == 0) {
    is_reading = false;
    is_writing = false;

    bool update_track = (cmd & 0x10) != 0;
    uint8_t op = cmd & 0xE0;

    if (op == 0x00) {  // Restore / Seek
      if ((cmd & 0x10) == 0) {
        fdc.Track = 0;
        fdc_step_direction = -1;
      } else {
        fdc.Track = fdc.Data;
      }
    } else if (op == 0x20) {  // Step
      if (update_track) {
        if (fdc_step_direction == 1 && fdc.Track < 83) fdc.Track++;
        else if (fdc_step_direction == -1 && fdc.Track > 0) fdc.Track--;
      }
    } else if (op == 0x40) {  // Step-In
      fdc_step_direction = 1;
      if (update_track && fdc.Track < 83) fdc.Track++;
    } else if (op == 0x60) {  // Step-Out
      fdc_step_direction = -1;
      if (update_track && fdc.Track > 0) fdc.Track--;
    }

    uint8_t s = 0;
    if (!disk_changed[current_drive]) s |= 0x80;
    if (fdc.Track == 0) s |= 0x04;
    fdc.Status = s;
    fdc_intrq = true;
    return;
  }

  // --- Type II: Read/Write Sector (10xx xxxx) ---
  if ((cmd & 0xC0) == 0x80) {
    fdc.Status = 0x01;  // Busy

    if (!disk_inserted) {
      fdc.Status |= 0x80;  // Not Ready
      fdc.Status &= ~0x01;
      fdc_intrq = true;
      return;
    }

    sector_ptr = getSectorPointer(fdc.Track, fdc.Side, fdc.Sector);
    if (sector_ptr) {
      uint8_t op = cmd & 0xF0;
      if (op == 0x80 || op == 0x90) {  // Read Sector (0x80-0x9F)
        is_reading = true;
        is_writing = false;
      } else if (op == 0xA0 || op == 0xB0) {  // Write Sector (0xA0-0xBF)
        is_writing = true;
        is_reading = false;
        is_writing = true;
      }

      fdc.Status |= 0x02;  // DRQ
    } else {
      fdc.Status |= 0x10;   // Record Not Found
      fdc.Status &= ~0x01;  // Remove Busy
      fdc_intrq = true;
    }
    return;
  }
  fdc.Status = 0x10;
  if (!disk_inserted) fdc.Status |= 0x80;
  fdc.Status &= ~0x01;
  fdc_intrq = true;
}

uint8_t readWD2793Status() {
  uint8_t s = fdc.Status;

  if (is_reading || is_writing) {
    s |= 0x01;

    if (fdc_data_index < 512) {
      s |= 0x02;
    } else {
      s &= ~0x03;
      is_reading = false;
      is_writing = false;
    }
  } else {
    s &= ~0x01;  // 実行中でなければBusyは0
  }

  fdc_intrq = false;
  // s |= (is_write_protected ? 0x40 : 0x00);
  return s;
}

uint8_t* getSectorPointer(uint8_t track, uint8_t side, uint8_t sector) {
  Drive& d = drives[current_drive];
  if (!disk_changed[current_drive]) return nullptr;

  uint32_t sectorIndex;
  sectorIndex = ((uint32_t)track * drive_sides[current_drive] + side) * drive_sectors[current_drive] + (sector - 1);

  uint32_t offset = sectorIndex * 512;
  return &d.buffer[offset];
}

void loadDiskROM() {
  File file = SD.open("/disk.rom");
  if (file) {
    file.read(&slot_mem[2][0x4000], 16384);
    file.close();
  }
}

uint8_t readFDCData() {
  if (is_reading && sector_ptr) {
    uint8_t val = sector_ptr[fdc_data_index++];

    if (fdc_data_index >= 512) {
      if (fdc_multi_sector) {
        fdc.Sector++;
        if (fdc.Sector <= drive_sectors[current_drive]) {
          sector_ptr = getSectorPointer(fdc.Track, fdc.Side, fdc.Sector);
          fdc_data_index = 0;
          fdc.Status |= 0x03;
          return val;
        }
      } else {
        fdc.Status &= ~0x03;
        is_reading = false;
        fdc_intrq = true;
      }
    } else {
      fdc.Status |= 0x02;
    }
    return val;
  }
  return fdc.Data;
}

void writeFDCData(int val) {
  if (is_writing && sector_ptr) {
    sector_ptr[fdc_data_index++] = (uint8_t)val;

    if (fdc_data_index >= 512) {
      fdc.Status &= ~0x03;
      is_writing = false;
      fdc_intrq = true;
    } else {
      fdc.Status |= 0x03;
    }
  } else {
    fdc.Data = val;
  }
}

inline int IRAM_ATTR readByte(void* self, int addr) {
  //   int slot = (slot_reg >> ((addr >> 13) & 0x06)) & 0x03;
  //   if (slot == 2 && addr >= 0x7FF0 && addr <= 0x7FFF) {
  //     int ret = 0xFF;
  //     switch (addr) {
  //       case 0x7FF8:
  //         ret = readWD2793Status();
  //         fdc_intrq = false;
  //         break;
  //       case 0x7FF9: ret = fdc.Track; break;
  //       case 0x7FFA: ret = fdc.Sector; break;
  //       case 0x7FFB: ret = (uint8_t)readFDCData(); break;
  //       case 0x7FFC:
  //       case 0x7FFD:
  //       case 0x7FFF:
  //         ret = 0x3F;
  //         if (!fdc_intrq) {
  //           ret |= 0x80;
  //         }

  //         if ((fdc.Status & 0x02) == 0) {
  //           ret |= 0x40;
  //         }
  //         break;
  //     }
  // #ifdef USE_SERIAL_DEBUG
  //       Serial.printf("FdcR %04X %02X\n", addr, ret);
  // #endif
  //     return ret;
  //   }
  return read_map[addr >> 13][addr & 0x1FFF];
}

inline void IRAM_ATTR writeByte(void* self, int addr, int val) {
  uint16_t a = (uint16_t)addr;
  uint8_t* ptr = write_map[a >> 13];
  int slot = (slot_reg >> ((a >> 13) & 0x06)) & 0x03;

  if (slot == 3) {
    ptr[a & 0x1FFF] = (uint8_t)val;
  } else {
    if (slot == 1 && (uint16_t)(a - 0x4000) < 0x8000) {
      int b_idx;
      switch (currentMapper) {
        case MAPPER_KONAMI:
          if (scc_active && a >= 0x9800 && a <= 0x98FF) {
            if (a < 0x9880) {
              int ch_idx = (a - 0x9800) >> 5;
              scc_waveform[ch_idx][a & 0x1F] = (int8_t)((int)val - 128);
            } else if (a < 0x98E0) {
              int reg = a & 0x0F;

              if (reg < 0x0A) {
                int ch_idx = reg >> 1;
                if (reg & 1) scc_f[ch_idx] = (scc_f[ch_idx] & 0x00FF) | ((val & 0x0F) << 8);
                else scc_f[ch_idx] = (scc_f[ch_idx] & 0x0F00) | (val & 0xFF);
              } else if (reg < 0x0F) {
                scc_v[reg - 0x0A] = val & 0x0F;
              } else {
                scc_enable = val & 0x1F;
              }
            }
            return;
          }
          if (a < 0x6000) {
            updateBankPointer(0, val);
          } else if (a < 0x8000) {
            updateBankPointer(1, val);
          } else if (a < 0xA000) {
            if (a >= 0x9000 && a <= 0x97FF) {
              scc_active = ((val & 0x3F) == 0x3F);
            }
            updateBankPointer(2, val);
          } else if (a < 0xC000) {
            updateBankPointer(3, val);
          }
          return;

        case MAPPER_ASCII8:
          if (a >= 0x6000 && a <= 0x7FFF) {
            b_idx = (a >> 11) & 3;
            updateBankPointer(b_idx, val);
          }
          return;

        case MAPPER_ASCII16:
          if (a >= 0x6000 && a <= 0x67FF) {
            updateBankPointer(0, val << 1);
            updateBankPointer(1, (val << 1) + 1);
          } else if (a >= 0x7000 && a <= 0x77FF) {
            updateBankPointer(2, val << 1);
            updateBankPointer(3, (val << 1) + 1);
          }
          return;

        case MAPPER_RTYPE:
          if (a >= 0x7000 && a <= 0x7FFF) {
            uint8_t bank8k = (val % 24) * 2;
            current_pages[2] = &game_rom_ptr[bank8k * 8192];
            current_pages[3] = &game_rom_ptr[(bank8k + 1) * 8192];
            updateMemoryMap();
          }
          return;
      }
    } else if (slot == 2) {
      //       if (slot == 2 && addr >= 0x7FF0 && addr <= 0x7FFF) {
      // #ifdef USE_SERIAL_DEBUG
      //         Serial.printf("FdcW %04X %02X\n", addr, val);
      // #endif
      //         switch (addr) {
      //           case 0x7FF8:
      //             fdc_intrq = false;
      //             executeWD2793Command(val);
      //             break;
      //           case 0x7FF9: fdc.Track = val; break;
      //           case 0x7FFA: fdc.Sector = val; break;
      //           case 0x7FFB: writeFDCData(val); break;

      //           case 0x7FFC:
      //             if (val & 0x01) current_drive = 1;
      //             else if (val & 0x02) current_drive = 0;
      //             break;

      //           case 0x7FFD:
      //             fdc.Side = val & 0x01;
      //             break;
      //         }
      //         return;
      //       }
    }
  }
  return;
}

void writeWord(void* ctx, int addr, int val) {
  writeByte(ctx, addr, val & 0xFF);
  writeByte(ctx, addr + 1, val >> 8);
}

inline int IRAM_ATTR readIO(void* context, int port) {
  uint8_t p = (uint8_t)port;

  if ((p & 0xFE) == 0x98) {
    return (int)tms9918a_read(vdp, p & 0x01);
  }
  if (p == 0xA2) {
    if (psg_address == 14) {
      if ((psg_regs[15] & 0x40) == 0) {
        return (int)joy_bits1;
      } else {
        return (int)joy_bits2;
      }
    }

    if (psg_address == 15) {
      return (int)psg_regs[15];
    }

    return (int)psg_regs[psg_address];
  }
  if (p == 0xA8) return slot_reg;
  if (p == 0xA9) {
    return (selected_row < 12) ? (int)msx_key_matrix[selected_row] : 0xFF;
  }
  if (p == 0xAA) return selected_row;

  if ((p >= 0x7C && p <= 0x7F) || (p >= 0xD0 && p <= 0xD3)) {
    p = (p >= 0xD0) ? (p - 0x54) : p;
    if (p == 0x7C) return readWD2793Status();
    else if (p == 0x7D) return fdc.Track;
    else if (p == 0x7E) return fdc.Sector;
    else if (p == 0x7F) return (uint8_t)readFDCData();
  }

  return 0xFF;
}

inline void IRAM_ATTR writeIO(void* context, int port, int val) {
  uint8_t p = (uint8_t)port;
  uint8_t v = (uint8_t)val;
  static int cnt = 0;

  if ((p & 0xFE) == 0x98) {
    tms9918a_write(vdp, p & 0x01, v);
    return;
  }
  if (p == 0xA0) {
    psg_address = v & 0x0F;
    return;
  } else if (p == 0xA1) {
    psg_regs[psg_address] = v;
    psgGen.writeReg(psg_address, v);

    if (psg2scc) {
      int reg = psg_address;
      switch (reg) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          {
            int ch = reg / 2;
            if (reg % 2 == 0) {
              scc_f[ch] = (scc_f[ch] & 0x0F00) | v;
              scc_f[ch + 3] = (scc_f[ch] >> 1) - 3;
            } else {
              scc_f[ch] = (scc_f[ch] & 0x00FF) | ((v & 0x0F) << 8);
              scc_f[ch + 3] = (scc_f[ch] >> 1) - 3;
            }
          }
          break;

        case 8:
        case 9:
        case 10:
          {
            scc_v[reg - 8] = (v & 0x0F);
            scc_v[reg - 8 + 3] = scc_v[reg - 8] >> 1;
          }
          break;

        case 7:
          {
            scc_enable = (~v) & 0x07;
          }
          break;
      }
    }
    return;
  }
  if (p == 0xA8) {
    slot_reg = v;
    updateMemoryMap();
    return;
  }
  if (p == 0xAA) {
    selected_row = v & 0x0F;
    return;
  }
  if ((p >= 0x7B && p <= 0x7F) || (p >= 0xD0 && p <= 0xD4)) {
    p = (p >= 0xD0) ? (p - 0x54) : p;
    switch (p) {
      case 0x7C: executeWD2793Command(val); return;
      case 0x7D: fdc.Track = val; return;
      case 0x7E: fdc.Sector = val; return;
      case 0x7F: writeFDCData(val); return;
      case 0x80:
      case 0x7B:  // Control Register
        if (val & 0x01) current_drive = 0;
        else if (val & 0x02) current_drive = 1;
        fdc.Side = (val & 0x10) ? 1 : 0;
        break;
      default: return;
    }
  }
}

void clearStatus() {
  canvas->setBrushColor(Color::Black);
  canvas->fillRectangle(48, 192, 48 + 224, 192 + 8);
}

void flashStatus(const char* msg, int waitMs = 0) {
  clearStatus();
  canvas->setPenColor(Color::White);
  canvas->setBrushColor(Color::Black);
  canvas->drawText(48, 192, msg);

  if (waitMs > 0) {
    delay(waitMs);
    clearStatus();
  }
}

void initSaveSystem() {
  uint8_t* base_ptr = (uint8_t*)ps_malloc(SAVE_BUFFER_SIZE * 5);
  if (base_ptr) {
    for (int i = 0; i < 5; i++) {
      psram_slots[i] = base_ptr + (SAVE_BUFFER_SIZE * i);
    }
  }
}

String getSavePath(int slotIdx) {
  String path = String(selectedRom);
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash != -1) {
    path = path.substring(lastSlash + 1);
  }

  int dotIndex = path.lastIndexOf('.');
  String base = (dotIndex != -1) ? path.substring(0, dotIndex) : path;

  return "/ROM/SAVE/" + base + String(slotIdx + 1) + ".sav";
}

void syncGametoPSRAM(int idx) {
  if (psram_slots[idx] == NULL) return;
  uint8_t* p = psram_slots[idx];

  // 1. RAM (64KB)
  memcpy(p, slot_mem[3], 65536);
  p += 65536;

  // 2. CPU / System State
  tempState.AF = cpu.AF.W;
  tempState.BC = cpu.BC.W;
  tempState.DE = cpu.DE.W;
  tempState.HL = cpu.HL.W;
  tempState.IX = cpu.IX.W;
  tempState.IY = cpu.IY.W;
  tempState.PC = cpu.PC.W;
  tempState.SPtr = cpu.SPtr.W;
  tempState.AF1 = cpu.AF1.W;
  tempState.BC1 = cpu.BC1.W;
  tempState.DE1 = cpu.DE1.W;
  tempState.HL1 = cpu.HL1.W;
  tempState.I = cpu.I;
  tempState.R = cpu.R;
  tempState.IFF = cpu.IFF;
  tempState.IRequest = cpu.IRequest;
  tempState.primary_slot = slot_reg;
  for (int i = 0; i < 4; i++) tempState.rom_banks[i] = rom_banks[i];

  memcpy(p, &tempState, sizeof(StateSaveData));
  p += sizeof(StateSaveData);

  // 3. VRAM(16KB)
  memcpy(p, vdp->framebuffer, VRAM_SIZE);
  p += VRAM_SIZE;

  // 4. VDP struct
  memcpy(p, vdp, VDP_SAVE_SIZE);
  p += VDP_SAVE_SIZE;

  // 5. PSG
  memcpy(p, (void*)psg_regs, 16);
  p += 16;
  memcpy(p, &psg_address, 1);
  p += 1;

  // 6. SCC
  memcpy(p, scc_waveform, 5 * 32);
  p += (5 * 32);  // 160 bytes
  memcpy(p, scc_f, sizeof(scc_f));
  p += sizeof(scc_f);  // 10 bytes
  memcpy(p, scc_v, sizeof(scc_v));
  p += sizeof(scc_v);  // 5 bytes
  memcpy(p, &scc_enable, 1);
  p += 1;
  memcpy(p, &scc_active, 1);
  p += 1;
  memcpy(p, &psg2scc, 1);
  p += 1;
}

void syncPSRAMtoGame(int idx) {
  if (psram_slots[idx] == NULL) return;
  uint8_t* p = psram_slots[idx];

  memcpy(slot_mem[3], p, 65536);
  p += 65536;

  StateSaveData state;
  memcpy(&state, p, sizeof(StateSaveData));
  p += sizeof(StateSaveData);

  cpu.AF.W = state.AF;
  cpu.BC.W = state.BC;
  cpu.DE.W = state.DE;
  cpu.HL.W = state.HL;
  cpu.IX.W = state.IX;
  cpu.IY.W = state.IY;
  cpu.PC.W = state.PC;
  cpu.SPtr.W = state.SPtr;
  cpu.AF1.W = state.AF1;
  cpu.BC1.W = state.BC1;
  cpu.DE1.W = state.DE1;
  cpu.HL1.W = state.HL1;
  cpu.I = state.I;
  cpu.R = state.R;
  cpu.IFF = state.IFF;
  cpu.IRequest = state.IRequest;
  if (cpu.IPeriod <= 0) cpu.IPeriod = 597;
  cpu.ICount = cpu.IPeriod;

  memcpy(vdp->framebuffer, p, VRAM_SIZE);
  p += VRAM_SIZE;

  uint8_t* tmp_framebuffer = vdp->framebuffer;
  uint8_t* tmp_active_raster = vdp->active_raster;
  uint8_t* tmp_visible_raster = vdp->visible_raster;
  uint32_t* tmp_colourmap = vdp->colourmap;
  vdp->framebuffer = tmp_framebuffer;
  vdp->active_raster = tmp_active_raster;
  vdp->visible_raster = tmp_visible_raster;
  vdp->colourmap = tmp_colourmap;

  memcpy(vdp, p, VDP_SAVE_SIZE);
  p += VDP_SAVE_SIZE;

  memcpy((void*)psg_regs, p, 16);
  p += 16;
  memcpy(&psg_address, p, 1);
  p += 1;
  memcpy(scc_waveform, p, 5 * 32);
  p += (5 * 32);
  memcpy(scc_f, p, sizeof(scc_f));
  p += sizeof(scc_f);
  memcpy(scc_v, p, sizeof(scc_v));
  p += sizeof(scc_v);
  memcpy(&scc_enable, p, 1);
  p += 1;
  memcpy(&scc_active, p, 1);
  p += 1;
  memcpy(&psg2scc, p, 1);
  p += 1;

  slot_reg = state.primary_slot;
  for (int i = 0; i < 4; i++) {
    rom_banks[i] = state.rom_banks[i];
    if (currentMapper == MAPPER_RTYPE && i < 2) continue;
    updateBankPointer(i, rom_banks[i]);
  }
}

void stateSavePSRAMtoSD(int idx) {
  String path = getSavePath(idx);
  if (!SD.exists("/ROM")) SD.mkdir("/ROM");
  if (!SD.exists("/ROM/SAVE")) SD.mkdir("/ROM/SAVE");

  File file = SD.open(path.c_str(), FILE_WRITE);
  if (file) {
    file.write(psram_slots[idx], SAVE_BUFFER_SIZE);
    file.close();
  }
}

void stateLoadSDtoPSRAM(int idx) {
  String path = getSavePath(idx);
  if (!SD.exists(path)) {
    flashStatus("NO SD FILE", 2000);
    return;
  }
  File file = SD.open(path.c_str(), FILE_READ);
  if (file) {
    file.read(psram_slots[idx], SAVE_BUFFER_SIZE);
    file.close();
  }
}

void clearKeyBuffer() {
  auto kb = PS2Controller.keyboard();
  while (kb->scancodeAvailable() > 0) {
    kb->getNextScancode(0);
    delay(10);
  }
}

void clearCanvasDouble() {
  canvas->setBrushColor(fabgl::Color(0));
  canvas->clear();
  canvas->swapBuffers();
  canvas->clear();
  canvas->swapBuffers();
}

String showFileSelector(const char* title, const char* dirPath, const char* ext) {
  auto kb = PS2Controller.keyboard();

  std::vector<String> files;
  File dir = SD.open(dirPath);
  if (!dir) {
    SD.mkdir(dirPath);
    dir = SD.open(dirPath);
  }
  while (File file = dir.openNextFile()) {
    if (!file.isDirectory()) {
      String name = file.name();
      String lowerName = name;
      lowerName.toLowerCase();
      String lowerExt = String(ext);
      lowerExt.toLowerCase();
      if (lowerName.endsWith(lowerExt)) {
        if (name != "MSX.ROM" && name != "DISK.ROM") files.push_back(name);
      }
    }
    file.close();
  }
  if (files.empty()) return "";

  int cursor = 0, topIndex = 0;
  const int maxRows = 14;
  bool isBreak = false;
  bool redrawn = true;

  while (true) {
    if (redrawn) {
      canvas->setBrushColor(fabgl::Color(4));
      canvas->clear();
      canvas->setPenColor(fabgl::Color(15));
      canvas->drawText(48, 5, title);

      if (cursor < topIndex) topIndex = cursor;
      if (cursor >= topIndex + maxRows) topIndex = cursor - maxRows + 1;

      for (int i = 0; i < maxRows && (topIndex + i) < files.size(); ++i) {
        int idx = topIndex + i;
        canvas->setPenColor((idx == cursor) ? fabgl::Color(10) : fabgl::Color(15));
        canvas->drawText(64, 25 + (i * 11), files[idx].c_str());
      }

      canvas->swapBuffers();
      redrawn = false;
    }

    if (kb) {
      while (kb->scancodeAvailable() > 0) {
        int sc = kb->getNextScancode(0);
        if (sc == 0xF0) {
          isBreak = true;
          continue;
        }
        if (sc == 0xE0) continue;

        if (!isBreak) {
          if (sc == 0x75 && cursor > 0) {
            cursor--;
            redrawn = true;
          } else if (sc == 0x72 && cursor < (int)files.size() - 1) {
            cursor++;
            redrawn = true;
          } else if (sc == 0x76) {  // ESC
            clearCanvasDouble();
            clearKeyBuffer();
            return "";
          } else if (sc == 0x5A) {  // ENTER
            String res = files[cursor];
            clearCanvasDouble();
            return res;
          }
        }
        isBreak = false;
      }
    }
    delay(10);
  }

  return "";
}

void handleDiskSelect(int drive) {
  char title[32];
  sprintf(title, "--- SELECT %c (ESC TO EXIT) ---", (drive == 0) ? 'A' : 'B');
  String selected = showFileSelector(title, "/DISK", ".DSK");
  if (selected != "") {
    if (loadSdToPsram(drive, "/DISK/" + selected)) {
      flashStatus("LOAD SUCCESS", 1000);
    } else {
      flashStatus("LOAD ERROR!", 2000);
    }
  }
}

void handleRomSelect() {
  String rom = showFileSelector("--- SELECT ROM (ESC TO BASIC) ---", "/ROM", ".ROM");
  if (rom == "" || rom == "BASIC (Internal)") {
    selectedRom = "";
  } else {
    selectedRom = "/ROM/" + rom;
  }
}

void updateKeyboard() {
  static bool isBreak = false;
  static bool isE0 = false;
  static bool isShift = false;
  static bool f6_held = false;
  static bool f7_held = false;
  static bool f8_held = false;
  static bool f9_held = false;
  static bool f10_held = false;
  static bool f11_held = false;
  static bool f12_held = false;

  auto kb = PS2Controller.keyboard();
  if (!kb) return;
  int count = kb->scancodeAvailable();
  if (count <= 0) return;

  while (count--) {
    int sc = kb->getNextScancode(0);
    if (sc == -1) break;
    if (sc == 0xF0) {
      isBreak = true;
      continue;
    }
    if (sc == 0xE0) {
      isE0 = true;
      continue;
    }
    if (sc == 0x12 || sc == 0x59) {
      isShift = !isBreak;
    }

    if (isPaused && !isBreak && !isE0 && currentSaveMode != SaveMode::NONE) {
      int slot = -1;
      if (sc == 0x16) slot = 0;       // 1
      else if (sc == 0x1E) slot = 1;  // 2
      else if (sc == 0x26) slot = 2;  // 3
      else if (sc == 0x25) slot = 3;  // 4
      else if (sc == 0x2E) slot = 4;  // 5

      if (slot != -1) {
        char msg[32];
        switch (currentSaveMode) {
          case SaveMode::GAME_TO_PSRAM:
            syncGametoPSRAM(slot);
            sprintf(msg, "SLOT %d: QUICK SAVED", slot + 1);
            break;
          case SaveMode::PSRAM_TO_GAME:
            syncPSRAMtoGame(slot);
            sprintf(msg, "SLOT %d: RESTORED", slot + 1);
            break;
          case SaveMode::PSRAM_TO_SD:
            stateSavePSRAMtoSD(slot);
            sprintf(msg, "SLOT %d: PSRAM -> SD", slot + 1);
            break;
          case SaveMode::SD_TO_PSRAM:
            stateLoadSDtoPSRAM(slot);
            sprintf(msg, "SLOT %d: SD -> PSRAM", slot + 1);
            break;
        }
        flashStatus(msg, 1000);
        flashStatus("PAUSED!", 0);
        currentSaveMode = SaveMode::NONE;
        isBreak = false;
        isE0 = false;
        continue;
      }
    }

    if (sc == 0x0B) {  // F6
      if (!isBreak && !f6_held) {
        f6_held = true;
        if (isShift) {
          // todo anything
        } else {
          // todo anything
        }
      } else if (isBreak) f6_held = false;
    } else if (sc == 0x83) {  // F7
      if (!isBreak && !f7_held) {
        f7_held = true;
        if (isShift) {
          // todo anything
        } else {
        }
      } else if (isBreak) f7_held = false;
    } else if (sc == 0x0A) {  // F8
      if (!isBreak && !f8_held) {
        f8_held = true;
        if (!isShift) {
          if (!psg2scc && !scc_enable) {
            psg2scc = true;
            flashStatus("SCC ON", 1000);
          } else {
            psg2scc = false;
            scc_enable = false;
            for (int i = 0; i < 5; i++) scc_ch[i]->setVolume(0);
            flashStatus("SCC OFF", 1000);
          }
        } else {
          if (vdp->limit_sprites == 0) {
            vdp->limit_sprites = 1;
            flashStatus("LIMIT SPRITES", 1000);
          } else {
            vdp->limit_sprites = 0;
            flashStatus("NO LIMIT SPRITES", 1000);
          }
        }
      } else if (isBreak) f8_held = false;
    } else if (sc == 0x01) {  // F9
#ifdef USE_FDC
      Serial.printf("F9 pressed\n");
      if (!isBreak && !f9_held) {
        f9_held = true;
        if (isPaused) {
          if (!isShift) {
            handleDiskSelect(0);
          } else {
            savePsramToSd(0);
          }
        }
      } else if (isBreak) {
        f9_held = false;
      }
#endif
    } else if (sc == 0x09) {  // F10
      if (!isBreak && !f10_held) {
        f10_held = true;
        if (!isShift) {
          if (isPaused) {
            currentSaveMode = SaveMode::SD_TO_PSRAM;
            flashStatus("LOAD SD->PSRAM: SELECT 1-5", 0);
          }
        } else {
          if (isPaused) {
            currentSaveMode = SaveMode::PSRAM_TO_SD;
            flashStatus("SAVE PSRAM->SD: SELECT 1-5", 0);
          }
        }

      } else if (isBreak) f10_held = false;
    } else if (sc == 0x78) {  // F11
      if (!isBreak && !f11_held) {
        f11_held = true;
        if (!isShift) {
          if (isPaused) {
            currentSaveMode = SaveMode::PSRAM_TO_GAME;
            flashStatus("RESTORE PSRAM: SELECT 1-5", 0);
          }
        } else {
          if (isPaused) {
            currentSaveMode = SaveMode::GAME_TO_PSRAM;
            flashStatus("QUICK SAVE: SELECT 1-5", 0);
          }
        }
      } else if (isBreak) f11_held = false;
    } else if (sc == 0x07) {  // F12
      if (!isBreak && !f12_held) {
        f12_held = true;

        if (isShift) {
          ESP.restart();
        } else {
          isPaused = !isPaused;
          currentSaveMode = SaveMode::NONE;
          if (!isPaused) {
            clearStatus();
            currentSaveMode = SaveMode::NONE;
          } else {
            flashStatus("PAUSED!", 0);
          }
        }
      } else if (isBreak) f12_held = false;
    }

    int row = -1, bit = -1;
    if (!isE0) {
      uint8_t res = key_map[sc & 0xFF];
      if (res != 0xFF) {
        row = res >> 4;
        bit = res & 0x0F;
      }
    } else {
      switch (sc) {
        case 0x5A:
          row = 7;
          bit = 7;
          break;  // KP ENTER
        case 0x4A:
          row = 2;
          bit = 4;
          break;  // KP /
        case 0x75:
          row = 8;
          bit = 5;
          break;  // UP
        case 0x72:
          row = 8;
          bit = 6;
          break;  // DOWN
        case 0x6B:
          row = 8;
          bit = 4;
          break;  // LEFT
        case 0x74:
          row = 8;
          bit = 7;
          break;  // RIGHT
        case 0x71:
          row = 8;
          bit = 3;
          break;  // Delete
        case 0x70:
          row = 8;
          bit = 2;
          break;  // Insert
        case 0x6C:
          row = 8;
          bit = 1;
          break;    // Home
        case 0x69:  // End
          row = 7;
          bit = 6;
          break;  // Select
      }
    }

    if (row >= 0 && row < 12) {
      if (isBreak) msx_key_matrix[row] |= (1 << bit);
      else msx_key_matrix[row] &= ~(1 << bit);
    }

    isBreak = false;
    isE0 = false;
  }
}

void IRAM_ATTR updateController() {
  joy_bits1 = joy_bits2 = 0x3F;

  while (Serial1.available() > 0) {
    joy_bits = Serial1.read();
    if (joy_bits & 0x80) joy_bits2 = joy_bits & 0x3F;
    else joy_bits1 = joy_bits & 0x3F;
  }
}

void detectMapper(const char* filename, uint32_t size) {
  if (size <= 0x8000) {
    currentMapper = MAPPER_NONE;
    return;
  }

  char* upperName = strdup(filename);
  for (int i = 0; upperName[i]; i++) {
    upperName[i] = toupper((unsigned char)upperName[i]);
  }

  if (strstr(upperName, "(A16)")) {
    currentMapper = MAPPER_ASCII16;
  } else if (strstr(upperName, "(A8)")) {
    currentMapper = MAPPER_ASCII8;
  } else if (strstr(upperName, "(A)")) {
    currentMapper = MAPPER_ASCII8;
  } else if (strstr(upperName, "(K)")) {
    currentMapper = MAPPER_KONAMI;
  } else if (strstr(upperName, "(R)")) {
    currentMapper = MAPPER_RTYPE;
  } else {
    currentMapper = MAPPER_NONE;
  }

  free(upperName);
}

//M1WAIT is closer to the original hardware behavior and results in a lower CPU load.
#define CPU_CYCLE_M1WAIT 210 // -8%
//#define CPU_CYCLE_M1WAIT 215 // -6%
#define CPU_CYCLE_NOWAIT 228

void IRAM_ATTR cpuTask(void* pvParameters) {
  const int64_t frameTimeUs = 16684;  // NTSC 59.94FPS
  int64_t nextFrameTime = esp_timer_get_time();
  int64_t now;
  int64_t waitTime;
  bool irq_line_status = false;
  
  while (1) {
    if (isPaused) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      nextFrameTime = esp_timer_get_time();
      continue;
    }

#ifdef USE_SIGNAL_DEBUG
    digitalWrite(14, HIGH);
#endif
    nextFrameTime += frameTimeUs;

    for (int i = 0; i < 262; ++i) {
      ExecZ80(&cpu, CPU_CYCLE_M1WAIT);
      if (i == 192) {
        tms9918a_rasterize(vdp);
        frameReady = true;
      }
      if (tms9918a_irq_pending(vdp) && (cpu.IFF & IFF_1)) {
        IntZ80(&cpu, 0x38);
      }
    }
#ifdef USE_SIGNAL_DEBUG
    digitalWrite(14, LOW);
#endif
    now = esp_timer_get_time();
    waitTime = nextFrameTime - now;

    if (waitTime > 0) {
      if (waitTime >= 1000) {
        vTaskDelay(pdMS_TO_TICKS(waitTime / 1000));
      }
      while (esp_timer_get_time() < nextFrameTime) {
        asm volatile("nop");
      }
    }
#ifdef USE_SERIAL_DEBUG
    else {
      //'x' appears when the frame timing exceeds 16.67ms.
      Serial.printf("x");
    }
#endif
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(240);
  disableCore0WDT();
  disableCore1WDT();
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  esp_task_wdt_delete(NULL);
  delay(100);

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, fabgl::KbdMode::NoVirtualKeys);
  PS2Controller.keyboard()->begin(GPIO_NUM_33, GPIO_NUM_32, false, false);

  auto kb = PS2Controller.keyboard();
  for (int i = 0; i < 12; i++) {
    msx_key_matrix[i] = 0xFF;
  }
  initKeyMap(KEYBORD_MAP);

  initSaveSystem();

  slot_mem[0] = (uint8_t*)heap_caps_malloc(0x8000, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  if (slot_mem[0]) {
    memset(slot_mem[0], 0, 0x8000);
    Serial.println("Slot 0 (Fast RAM) allocated in Internal SRAM.");
  } else {
    Serial.println("Slot 0 RAM Allocation Failed!");
    while (1)
      ;
  }

  slot_mem[1] = (uint8_t*)ps_malloc(0x10000);
  if (slot_mem[1]) {
    memset(slot_mem[1], 0xFF, 0x10000);
    Serial.println("Slot 1 allocated in PSRAM.");
  } else {
    Serial.println("Slot 1 RAM Allocation Failed!");
    while (1)
      ;
  }
  slot_mem[2] = (uint8_t*)ps_malloc(0x10000);
  if (slot_mem[2]) {
    memset(slot_mem[2], 0xFF, 0x10000);
    Serial.println("Slot 2 allocated in PSRAM.");
  } else {
    Serial.println("Slot 2 RAM Allocation Failed!");
    while (1)
      ;
  }
  slot_mem[3] = (uint8_t*)heap_caps_malloc(0x10000, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  if (slot_mem[3]) {
    memset(slot_mem[3], 0, 0x10000);
    Serial.println("Slot 3 (Fast RAM) allocated in Internal SRAM.");
  } else {
    Serial.println("Slot 3 RAM Allocation Failed!");
    while (1)
      ;
  }

  DisplayController.begin();
  DisplayController.setResolution(VGA_320x200_75Hz);//, -1, -1, false);
  canvas = new fabgl::Canvas(&DisplayController);
  for (int i = 0; i < 16; i++) {
    DisplayController.setPaletteItem(i, msx_palette[i]);
  }

  canvas->clear();
  canvas->swapBuffers();
  canvas->clear();

  vdp = tms9918a_create();
  tms9918a_reset(vdp);
  vdp->active_raster = vdp->rasterbuffer0;
  vdp->visible_raster = vdp->rasterbuffer1;

  msxBitmap = new fabgl::Bitmap(256, 192, vdp->visible_raster, fabgl::PixelFormat::Native);

  soundGenerator.attach(&psgGen);
  psgGen.enable(true);
  psgGen.setVolume(100);

  for (int i = 0; i < 32; i++) {
    scc_waveform[0][i] = Pseudo_Vocal[i];
    scc_waveform[1][i] = Metallic_Lead[i];
    scc_waveform[2][i] = Dirty_Bass[i];
    scc_waveform[3][i] = Warm_Body_PhaseShift[i];
  }

  for (int i = 0; i < 5; i++) {
    scc_ch[i] = new SCCGenerator();
    soundGenerator.attach(scc_ch[i]);
    scc_ch[i]->enable(true);
    scc_ch[i]->setVolume(0);
  }

  soundGenerator.setVolume(64);
  soundGenerator.play(true);
  psgGen.enable(true);

  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, SPI)) {
    File biosFile = SD.open("/MSX.ROM");
    if (biosFile) {
      size_t size = biosFile.size();
      if (size > 0x8000) size = 0x8000;
      biosFile.read(slot_mem[0], size);
      biosFile.close();
      Serial.printf("Main BIOS loaded: %d bytes to Slot 0\n", size);
    } else {
      Serial.println("Error: /MSX.ROM not found!");
    }

#ifdef USE_FDC
    initDiskPSRAM();
    handleDiskSelect(0);
    File diskRomFile = SD.open("/DISK.ROM");
    if (diskRomFile) {
      size_t size = diskRomFile.size();
      if (size > 0x4000) size = 0x4000;  // 通常16KB
      diskRomFile.read(&slot_mem[2][0x4000], size);
      diskRomFile.read(&slot_mem[2][0x8000], size);
      diskRomFile.close();
      Serial.printf("DiskROM Magic: %02X %02X\n", slot_mem[2][0x4000], slot_mem[2][0x4001]);
    } else {
      Serial.println("Warning: /disk.rom not found. FDD will be disabled.");
    }

    fdc.Status = 0x80;
    fdc.Track = 0;
    fdc.Sector = 1;
#endif

    handleRomSelect();
    if (selectedRom != "" && selectedRom != "/MSX.ROM") {
      File romFile = SD.open(selectedRom.c_str());
      if (romFile) {

        // 暫定処置 Diskが選ばれてなくゲームROMが選択されたらDISK.ROMを無効化する
        if (disk_changed[0] == false) memset(slot_mem[2], 0xFF, 0x10000);

        uint32_t size = romFile.size();
        const char* filename = romFile.name();
        Serial.printf("Loading Game ROM: %s (%d bytes)\n", selectedRom.c_str(), size);

        uint8_t header0[2];
        romFile.seek(0);
        romFile.read(header0, 2);
        Serial.printf("DEBUG: File Start (Bank 0) = %02X %02X\n", header0[0], header0[1]);
        uint8_t header23[2];
        romFile.seek(384 * 1024 - 16 * 1024);  // バンク23の開始位置へ
        romFile.read(header23, 2);
        Serial.printf("DEBUG: File End (Bank 23) = %02X %02X\n", header23[0], header23[1]);

        if ((header0[0] == 'A' && header0[1] == 'B') || (header23[0] == 'A' && header23[1] == 'B')) {

          long original_size = size;
          Serial.printf("DEBUG: Actual ROM File Size = %ld bytes\n", original_size);
          rom_size = 1;
          while (rom_size < original_size) rom_size <<= 1;
          rom_mask = rom_size - 1;
          rom_size_bytes = rom_size;

          game_rom_ptr = (uint8_t*)ps_malloc(rom_size);
          detectMapper(filename, original_size);
          const char* mapperNames[] = { "NONE", "KONAMI", "ASCII8", "ASCII16", "RTYPE" };
          Serial.printf("DEBUG: Mapper Assigned = %s (%d)\n", mapperNames[currentMapper], currentMapper);

          if (game_rom_ptr) {
            romFile.seek(0);
            romFile.read(game_rom_ptr, original_size);
            if (rom_size != original_size) {
              if (currentMapper == MAPPER_RTYPE) {
                memset(game_rom_ptr + original_size, 0xFF, rom_size - original_size);
                //memcpy(game_rom_ptr + original_size, game_rom_ptr, rom_size - original_size);
              } else {
                memset(game_rom_ptr + original_size, 0xFF, rom_size - original_size);
              }
            }
          }

          initMegaROM();
          Serial.println("ROM Read and Mapper Init OK!");

        } else {
          Serial.printf("Warning: No 'AB' header in [%s]! This might be a raw data or corrupted.\n", romFile.name());
        }
        romFile.close();
      }
    } else {
      game_rom_ptr = (uint8_t*)ps_malloc(32768);
      detectMapper("BASIC", 32768);
      initMegaROM();
      Serial.println("Booting in DISK BASIC mode.");
    }
  } else {
    Serial.println("SD open failed.");
  }
  memset(dummy_page, 0xFF, 8192);

  for (int i = 0; i < 12; i++) {
    msx_key_matrix[i] = 0xFF;
  }

  slot_reg = 0x00;
  for (int i = 0; i < 4; i++) {
    rom_banks[i] = i;
  }
  updateMemoryMap();

  memset(&cpu, 0, sizeof(Z80));
  ResetZ80(&cpu);
  cpu.IPeriod = 512;
  cpu.SPtr.W = 0xF380;
  cpu.AF.B.h = 0x00;
  xTaskCreatePinnedToCore(cpuTask, "cpuTask", 8192, NULL, 24, NULL, 0);

  Serial.println("MSX Booting with Dual Core...");

#ifndef USE_SERIAL_DEBUG
  Serial.end();
#else
  Serial.println("MSX Booting with Debug Serial...");
#endif
  Serial1.begin(115200, SERIAL_8N1, 39, -1);

#ifdef USE_SIGNAL_DEBUG
  SPI.end();
  SD.end();
  pinMode(14, OUTPUT);
  pinMode(2, OUTPUT);
#endif
}

void updateAllSound() {
  if (isPaused) {
    psgGen.setVolume(0);
    for (int i = 0; i < 5; i++) {
      scc_ch[i]->setVolume(0);
    }
  } else {
    updateSCCSound();
    psgGen.fillBuffer();
    for (int i = 0; i < 5; i++) {
      scc_ch[i]->fillBuffer();
    }
  }
}

// void drawDiskIndicator() {
//   // Display position (bottom right)
//   const int x = 294;  // Position for single indicator (320 - 8)
//   const int y = 192;

//   // Check Drive A status
//   bool isSelected = (current_drive == 0);
//   bool isAccessing = isSelected && (is_reading || is_writing);

//   if (disk_changed[0]) {
//     // --- Disk Inserted ---
//     if (isAccessing) {
//       canvas->setBrushColor(Color::BrightGreen);
//     } else {
//       canvas->setBrushColor(Color::Green);
//     }
//     canvas->setPenColor(Color::Green);
//     canvas->fillRectangle(x, y + 2, x + 6, y + 5);
//   } else {
//     // --- No Disk ---
//     canvas->setBrushColor(Color::Black);
//     canvas->setPenColor(Color::Green);
//     canvas->drawRectangle(x, y + 2, x + 6, y + 5);
//   }
// }

void loop() {
  if (frameReady) {
    frameReady = false;
#ifdef USE_SIGNAL_DEBUG
    digitalWrite(2, HIGH);
#endif
    if (!isPaused) {
      flip_buffer(vdp);
      msxBitmap->data = vdp->visible_raster;
      canvas->drawBitmap(48, 0, msxBitmap);
    }
    updateAllSound();
    DisplayController.processPrimitives();

    updateAllSound();
    updateController();
    updateKeyboard();

    //drawDiskIndicator();

#ifdef USE_SIGNAL_DEBUG
    digitalWrite(2, LOW);
#endif
  }
  updateAllSound();

  if (isPaused) {
    vTaskDelay(pdMS_TO_TICKS(5));
    updateKeyboard();
  } else {
    delay(1);
  }
}
