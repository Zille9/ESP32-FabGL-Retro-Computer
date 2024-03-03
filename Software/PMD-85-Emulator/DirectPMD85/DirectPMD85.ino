/* DirectPMD85 is 8-bit computer emulator based on the FabGL library
 * -> see http://www.fabglib.org/ or FabGL on GitHub.
 *  
 * For proper operation, an ESP32 module with a VGA monitor 
 * and a PS2 keyboard connected according to the 
 * diagram on the website www.fabglib.org is required.
 * 
 * Cassette recorder is emulated using SPIFFS. The tape is represented 
 * by a file "tape.ptp" uploaded by ESP32 Filesystem Uploader in Arduino IDE. 
 * ".ptp" files with programs for PMD85 are available e.g. here:
 * https://pmd85.borik.net/wiki/Download
 * It is recommended to number the individual programs in the ptp file 
 * in ascending order. 
 * 
 * DirectPMD85 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or any later version.
 * DirectPMD85 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY.
 * Stan Pechal, 2023
*/

#include "fabgl.h"
#include "emudevs/i8080.h"          // For processor
//#include "SPIFFS.h"

// ---------------------------------- SD-Karten-Zugriff--------------------------------------------------------------------------------------------
//#include "FS.h"
#include <SD.h>
#include <SPI.h>
#include <Update.h>
//-------------------------------------- Verwendung der SD-Karte ----------------------------------------------------------------------------------
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
//Konfiguration der SD-Karte unter FabGl - kann mit OPT geändert werden
#define kSD_CS   13
#define kSD_MISO 16
#define kSD_MOSI 17
#define kSD_CLK  14
#define SD_LED   2


fabgl::VGADirectController DisplayController;
fabgl::PS2Controller PS2Controller;
SoundGenerator  soundGenerator;
SquareWaveformGenerator square;

// Constants for video output
static constexpr int borderSize           = 22;
static constexpr int borderXSize          = 56;
static constexpr int scanlinesPerCallback = 2;  // screen height should be divisible by this value

static TaskHandle_t  mainTaskHandle;
void * auxPoint;

// "Tape recorder"
File file;
bool isTape = false;
int cntTape = 0;
bool basic = false;

// Sound constants
#define SAMPLE_RATE 16000
#define FREQ_HI 4000
#define FREQ_LO 1000
#define FREQ_ME 2000

// Hardware emulated on the PMD85 computer
// Processor I8080 will be used from the library FabGL
fabgl::i8080 m_i8080;
// Variables for emulating 8255 (keyboard connection)
int portPA = 0;    // Data sent to PA port
int keyboardIn[16];      // Value read from keyboard PB port
int shiftFlag = 0x7F;
bool blinkFlag = true;
// Variables for emulating 8255 on extended ROM module
int portExtPB = 0;    // Data sent to PB port
int portExtPC = 0;    // Data sent to PC port

// RAM memory will be just Byte array
volatile unsigned char pmd85ram[65536];   // selected addresses are overwritten by ROM in read mode
// ROM memory is contained in the array "pmd85rom[]"
#include "monit85.h"
// ROM memory on extended module is in the array "basicrom[]"
#include "basic1.h"
// Functions for communication on the bus
static int readByte(void * context, int address)              { if((address > 0x7FFF) && (address < 0x9000)) return(pmd85rom[address-0x8000]); else return(pmd85ram[address]); };
static void writeByte(void * context, int address, int value) { pmd85ram[address] = (unsigned char)value; };
static int readWord(void * context, int addr)                 { return readByte(context, addr) | (readByte(context, addr + 1) << 8); };
static void writeWord(void * context, int addr, int value)    { writeByte(context, addr, value & 0xFF); writeByte(context, addr + 1, value >> 8); } ;
static int readIO(void * context, int address)
{
  switch (address) {
    // *** 8251 port - for read from "tape" recorder
    case 0x1E:      // Data
      if(!file.available()) return 0;
      if((cntTape == 0) &&  file.available()) { cntTape = (file.read() & 0xFF); if(file.available()) cntTape += ((file.read()<<8) & 0xFF00); }
      if((cntTape) &&  file.available()) { cntTape--; return file.read(); } else return 0xFF;
    break;
    case 0x1F:      // Status
      if(file.available()) return (0x02); else return (0x00);
    break;
    // *** 8255 port - only PA and PB for keyboard
    case 0xF4:      // PA port
      return portPA;
    break;
    case 0xF5:      // PB port
      return (keyboardIn[portPA & 0x0F]) & shiftFlag;
    break;
    // *** 8255 extended ROM module
    case 0xF8:      // PA port
      return basicrom[((portExtPC<<8) & 0x3F00) | (portExtPB & 0xFF)];
    break;
    case 0xF9:      // PB port
      return portExtPB;
    break;
    case 0xFA:      // PC port
      return portExtPC;
    break;
    default: return 0xFF; break;  // Return "not selected I/O" - bus with 0xFF
  }
};
static void writeIO(void * context, int address, int value)
{
  switch (address) {
    // *** 8255 ports - keyboard connection
    case 0xF4:      // PA port
      portPA = value;
    break;
    case 0xF6:      // Sound generator
      if((value & 0x3) == 0) soundGenerator.play(false);
      if((value & 0x3) == 1) { square.setFrequency(FREQ_LO); soundGenerator.play(true); }
      if((value & 0x3) == 2) { square.setFrequency(FREQ_HI); soundGenerator.play(true); }
      if((value & 0x3) == 3) { square.setFrequency(FREQ_ME); soundGenerator.play(true); }
    break;
    // *** 8255 ports - extended ROM module
    case 0xF9:      // PB port
      portExtPB = value;
    break;
    case 0xFA:      // PC port
      portExtPC = value;
    break;
    default: break;
  }
};

// Keyboard interface for selected keys
// Handles Key Up following keys:
void procesKeyUp(VirtualKey key) {
  switch (key) {
      case VirtualKey::VK_F1: keyboardIn[0] = 0x7F; break;  // F1
      case VirtualKey::VK_F2: keyboardIn[1] = 0x7F; break;  // F2
      case VirtualKey::VK_F3: keyboardIn[2] = 0x7F; break;  // F3
      case VirtualKey::VK_F4: keyboardIn[3] = 0x7F; break;  // F4
      case VirtualKey::VK_F5: keyboardIn[4] = 0x7F; break;  // F5
      case VirtualKey::VK_F6: keyboardIn[5] = 0x7F; break;  // F6
      case VirtualKey::VK_F7: keyboardIn[6] = 0x7F; break;  // F7
      case VirtualKey::VK_F8: keyboardIn[7] = 0x7F; break;  // F8
      case VirtualKey::VK_F9: keyboardIn[8] = 0x7F; break;  // F9
      case VirtualKey::VK_F10: keyboardIn[9] = 0x7F; break;  // F10
      case VirtualKey::VK_F11: keyboardIn[10] = 0x7F; break;  // F11
      case VirtualKey::VK_F12: keyboardIn[11] = 0x7F; break;  // F12
      case VirtualKey::VK_PRINTSCREEN: keyboardIn[12] = 0x7F;  break; // PRINTSCREEN - WRK
      case VirtualKey::VK_SCROLLLOCK: keyboardIn[13] = 0x7F;  break; // SCROLLLOCK - C-D
      case VirtualKey::VK_PAUSE: keyboardIn[14] = 0x7F;  break; // PAUSE - RCL

      case VirtualKey::VK_KP_0:
      case VirtualKey::VK_RIGHTPAREN:
      case VirtualKey::VK_0: keyboardIn[9] = 0x7F; break;  // 0
      case VirtualKey::VK_KP_1:
      case VirtualKey::VK_EXCLAIM:
      case VirtualKey::VK_1: keyboardIn[0] = 0x7F; break;  // 1
      case VirtualKey::VK_KP_2:
      case VirtualKey::VK_AT:
      case VirtualKey::VK_2: keyboardIn[1] = 0x7F; break;  // 2
      case VirtualKey::VK_KP_3:
      case VirtualKey::VK_HASH:
      case VirtualKey::VK_3: keyboardIn[2] = 0x7F; break;  // 3
      case VirtualKey::VK_KP_4:
      case VirtualKey::VK_DOLLAR:
      case VirtualKey::VK_4: keyboardIn[3] = 0x7F; break;  // 4
      case VirtualKey::VK_KP_5:
      case VirtualKey::VK_PERCENT:
      case VirtualKey::VK_5: keyboardIn[4] = 0x7F; break;  // 5
      case VirtualKey::VK_KP_6:
      case VirtualKey::VK_CARET:
      case VirtualKey::VK_6: keyboardIn[5] = 0x7F; break;  // 6
      case VirtualKey::VK_KP_7:
      case VirtualKey::VK_AMPERSAND:
      case VirtualKey::VK_7: keyboardIn[6] = 0x7F; break;  // 7
      case VirtualKey::VK_KP_8:
      case VirtualKey::VK_ASTERISK:
      case VirtualKey::VK_8: keyboardIn[7] = 0x7F; break;  // 8
      case VirtualKey::VK_KP_9:
      case VirtualKey::VK_LEFTPAREN:
      case VirtualKey::VK_9: keyboardIn[8] = 0x7F; break;  // 9
      case VirtualKey::VK_UNDERSCORE:
      case VirtualKey::VK_MINUS: keyboardIn[10] = 0x7F; break;  // -_
      case VirtualKey::VK_EQUALS:
      case VirtualKey::VK_KP_PLUS:
      case VirtualKey::VK_PLUS: keyboardIn[11] = 0x7F; break;  // +=
      case VirtualKey::VK_INSERT: keyboardIn[12] = 0x7F;  break; // INS
      case VirtualKey::VK_DELETE: keyboardIn[13] = 0x7F;  break; // DEL
      case VirtualKey::VK_BACKSPACE: keyboardIn[14] = 0x7F;  break;// BACKSPACE - CLR 

      case VirtualKey::VK_q:
      case VirtualKey::VK_Q: keyboardIn[0] = 0x7F; break;  // q-Q
      case VirtualKey::VK_w:
      case VirtualKey::VK_W: keyboardIn[1] = 0x7F; break;  // w-W
      case VirtualKey::VK_e:
      case VirtualKey::VK_E: keyboardIn[2] = 0x7F; break;  // e-E
      case VirtualKey::VK_r:
      case VirtualKey::VK_R: keyboardIn[3] = 0x7F; break;  // r-R
      case VirtualKey::VK_t:
      case VirtualKey::VK_T: keyboardIn[4] = 0x7F; break;  // t-T
      case VirtualKey::VK_z:
      case VirtualKey::VK_Z: keyboardIn[5] = 0x7F; break;  // z-Z
      case VirtualKey::VK_u:
      case VirtualKey::VK_U: keyboardIn[6] = 0x7F; break;  // u-U
      case VirtualKey::VK_i:
      case VirtualKey::VK_I: keyboardIn[7] = 0x7F; break;  // i-I
      case VirtualKey::VK_o:
      case VirtualKey::VK_O: keyboardIn[8] = 0x7F; break;  // o-O
      case VirtualKey::VK_p:
      case VirtualKey::VK_P: keyboardIn[9] = 0x7F; break;  // p-P
      case VirtualKey::VK_LEFTBRACKET:
      case VirtualKey::VK_LEFTBRACE: keyboardIn[10] = 0x7F; break;  // [{
      case VirtualKey::VK_RIGHTBRACKET:
      case VirtualKey::VK_RIGHTBRACE: keyboardIn[11] = 0x7F; break;  // ]}
      case VirtualKey::VK_LEFT: keyboardIn[12] = 0x7F;  break; // < left
      case VirtualKey::VK_HOME: keyboardIn[13] = 0x7F;  break; // Home
      case VirtualKey::VK_RIGHT: keyboardIn[14] = 0x7F;  break;// > right 

      case VirtualKey::VK_a:
      case VirtualKey::VK_A: keyboardIn[0] = 0x7F; break;  // a-A
      case VirtualKey::VK_s:
      case VirtualKey::VK_S: keyboardIn[1] = 0x7F; break;  // s-S
      case VirtualKey::VK_d:
      case VirtualKey::VK_D: keyboardIn[2] = 0x7F; break;  // d-D
      case VirtualKey::VK_f:
      case VirtualKey::VK_F: keyboardIn[3] = 0x7F; break;  // f-F
      case VirtualKey::VK_g:
      case VirtualKey::VK_G: keyboardIn[4] = 0x7F; break;  // g-G
      case VirtualKey::VK_h:
      case VirtualKey::VK_H: keyboardIn[5] = 0x7F; break;  // h-H
      case VirtualKey::VK_j:
      case VirtualKey::VK_J: keyboardIn[6] = 0x7F; break;  // j-J
      case VirtualKey::VK_k:
      case VirtualKey::VK_K: keyboardIn[7] = 0x7F; break;  // k-K
      case VirtualKey::VK_l:
      case VirtualKey::VK_L: keyboardIn[8] = 0x7F; break;  // l-L
      case VirtualKey::VK_COLON:
      case VirtualKey::VK_SEMICOLON: keyboardIn[9] = 0x7F; break;  // ;-:
      case VirtualKey::VK_QUOTE:
      case VirtualKey::VK_QUOTEDBL: keyboardIn[10] = 0x7F; break;  // '-"
      case VirtualKey::VK_VERTICALBAR:
      case VirtualKey::VK_BACKSLASH: keyboardIn[11] = 0x7F; break;  // |\ - []
      case VirtualKey::VK_PAGEUP:
      case VirtualKey::VK_UP: keyboardIn[12] = 0x7F;  break; // Up
      case VirtualKey::VK_END: keyboardIn[13] = 0x7F;   break;// End
      case VirtualKey::VK_PAGEDOWN:
      case VirtualKey::VK_DOWN: keyboardIn[14] = 0x7F;  break; // Down 

      case VirtualKey::VK_SPACE: keyboardIn[0] = 0x7F; break;  // space
      case VirtualKey::VK_y:
      case VirtualKey::VK_Y: keyboardIn[1] = 0x7F; break;  // y-Y
      case VirtualKey::VK_x:
      case VirtualKey::VK_X: keyboardIn[2] = 0x7F; break;  // x-X
      case VirtualKey::VK_c:
      case VirtualKey::VK_C: keyboardIn[3] = 0x7F; break;  // c-C
      case VirtualKey::VK_v:
      case VirtualKey::VK_V: keyboardIn[4] = 0x7F; break;  // v-V
      case VirtualKey::VK_b:
      case VirtualKey::VK_B: keyboardIn[5] = 0x7F; break;  // b-B
      case VirtualKey::VK_n:
      case VirtualKey::VK_N: keyboardIn[6] = 0x7F; break;  // n-N
      case VirtualKey::VK_m:
      case VirtualKey::VK_M: keyboardIn[7] = 0x7F; break;  // m-M
      case VirtualKey::VK_LESS:
      case VirtualKey::VK_COMMA: keyboardIn[8] = 0x7F; break;  // <-,
      case VirtualKey::VK_GREATER:
      case VirtualKey::VK_PERIOD: keyboardIn[9] = 0x7F; break;  // >-.
      case VirtualKey::VK_SLASH:
      case VirtualKey::VK_QUESTION: keyboardIn[10] = 0x7F; break;  // /-?
      case VirtualKey::VK_LCTRL:
      case VirtualKey::VK_RCTRL: keyboardIn[13] = 0x7F; break;  // L Enter
      case VirtualKey::VK_RETURN:
      case VirtualKey::VK_KP_ENTER: keyboardIn[14] = 0x7F; break;  // R Enter

      case VirtualKey::VK_TAB:
      case VirtualKey::VK_LSHIFT:
      case VirtualKey::VK_RSHIFT: shiftFlag = 0x7F; break;  // L and R shift + TAB
      default: break;
      }
};

// Handles Key Down following keys:
void procesKeyDown(VirtualKey key) {
  switch (key) {
      case VirtualKey::VK_ESCAPE:
                            //m_i8080.reset();
                            basic=true;
                            break;  // ESC
       
      case VirtualKey::VK_F1: 
                            m_i8080.setPC(0x8000); 
                            if(file) file.seek(0,SeekSet); 
                            break;  // F1
      case VirtualKey::VK_F2: keyboardIn[1] = 0x7E; break;  // F2
      case VirtualKey::VK_F3: keyboardIn[2] = 0x7E; break;  // F3
      case VirtualKey::VK_F4: keyboardIn[3] = 0x7E; break;  // F4
      case VirtualKey::VK_F5: keyboardIn[4] = 0x7E; break;  // F5
      case VirtualKey::VK_F6: keyboardIn[5] = 0x7E; break;  // F6
      case VirtualKey::VK_F7: keyboardIn[6] = 0x7E; break;  // F7
      case VirtualKey::VK_F8: keyboardIn[7] = 0x7E; break;  // F8
      case VirtualKey::VK_F9: keyboardIn[8] = 0x7E; break;  // F9
      
      case VirtualKey::VK_F10: keyboardIn[8] = 0x7E; break;  // F10
      
      case VirtualKey::VK_F11: keyboardIn[10] = 0x7E; break;  // F11
      case VirtualKey::VK_F12: keyboardIn[11] = 0x7E; break;  // F12
      case VirtualKey::VK_PRINTSCREEN: keyboardIn[12] = 0x7E; break;  // PRINTSCREEN
      case VirtualKey::VK_SCROLLLOCK: keyboardIn[13] = 0x7E; break;  // SCROLLLOCK
      case VirtualKey::VK_PAUSE: keyboardIn[14] = 0x7E; break; // PAUSE 

      case VirtualKey::VK_KP_0:
      case VirtualKey::VK_RIGHTPAREN:
      case VirtualKey::VK_0: keyboardIn[9] = 0x7D; break;  // 0
      case VirtualKey::VK_KP_1:
      case VirtualKey::VK_EXCLAIM:
      case VirtualKey::VK_1: keyboardIn[0] = 0x7D; break;  // 1
      case VirtualKey::VK_KP_2:
      case VirtualKey::VK_AT:
      case VirtualKey::VK_2: keyboardIn[1] = 0x7D; break;  // 2
      case VirtualKey::VK_KP_3:
      case VirtualKey::VK_HASH:
      case VirtualKey::VK_3: keyboardIn[2] = 0x7D; break;  // 3
      case VirtualKey::VK_KP_4:
      case VirtualKey::VK_DOLLAR:
      case VirtualKey::VK_4: keyboardIn[3] = 0x7D; break;  // 4
      case VirtualKey::VK_KP_5:
      case VirtualKey::VK_PERCENT:
      case VirtualKey::VK_5: keyboardIn[4] = 0x7D; break;  // 5
      case VirtualKey::VK_KP_6:
      case VirtualKey::VK_CARET:
      case VirtualKey::VK_6: keyboardIn[5] = 0x7D; break;  // 6
      case VirtualKey::VK_KP_7:
      case VirtualKey::VK_AMPERSAND:
      case VirtualKey::VK_7: keyboardIn[6] = 0x7D; break;  // 7
      case VirtualKey::VK_KP_8:
      case VirtualKey::VK_ASTERISK:
      case VirtualKey::VK_8: keyboardIn[7] = 0x7D; break;  // 8
      case VirtualKey::VK_KP_9:
      case VirtualKey::VK_LEFTPAREN:
      case VirtualKey::VK_9: keyboardIn[8] = 0x7D; break;  // 9
      case VirtualKey::VK_UNDERSCORE:
      case VirtualKey::VK_MINUS: keyboardIn[10] = 0x7D; break;  // -_
      case VirtualKey::VK_EQUALS:
      case VirtualKey::VK_KP_PLUS:
      case VirtualKey::VK_PLUS: keyboardIn[11] = 0x7D; break;  // +=
      case VirtualKey::VK_INSERT: keyboardIn[12] = 0x7D; break;  // INS
      case VirtualKey::VK_DELETE: keyboardIn[13] = 0x7D; break;  // DEL
      case VirtualKey::VK_BACKSPACE: keyboardIn[14] = 0x7D; break; // BACKSPACE - CLR 


      case VirtualKey::VK_q:
      case VirtualKey::VK_Q: keyboardIn[0] = 0x7B; break;  // q-Q
      case VirtualKey::VK_w:
      case VirtualKey::VK_W: keyboardIn[1] = 0x7B; break;  // w-W
      case VirtualKey::VK_e:
      case VirtualKey::VK_E: keyboardIn[2] = 0x7B; break;  // e-E
      case VirtualKey::VK_r:
      case VirtualKey::VK_R: keyboardIn[3] = 0x7B; break;  // r-R
      case VirtualKey::VK_t:
      case VirtualKey::VK_T: keyboardIn[4] = 0x7B; break;  // t-T
      case VirtualKey::VK_z:
      case VirtualKey::VK_Z: keyboardIn[5] = 0x7B; break;  // z-Z
      case VirtualKey::VK_u:
      case VirtualKey::VK_U: keyboardIn[6] = 0x7B; break;  // u-U
      case VirtualKey::VK_i:
      case VirtualKey::VK_I: keyboardIn[7] = 0x7B; break;  // i-I
      case VirtualKey::VK_o:
      case VirtualKey::VK_O: keyboardIn[8] = 0x7B; break;  // o-O
      case VirtualKey::VK_p:
      case VirtualKey::VK_P: keyboardIn[9] = 0x7B; break;  // p-P
      case VirtualKey::VK_LEFTBRACKET:
      case VirtualKey::VK_LEFTBRACE: keyboardIn[10] = 0x7B; break;  // [{
      case VirtualKey::VK_RIGHTBRACKET:
      case VirtualKey::VK_RIGHTBRACE: keyboardIn[11] = 0x7B; break;  // ]}
      case VirtualKey::VK_LEFT: keyboardIn[12] = 0x7B; break;  // < left
      case VirtualKey::VK_HOME: keyboardIn[13] = 0x7B; break;  // Home
      case VirtualKey::VK_RIGHT: keyboardIn[14] = 0x7B; break; // > right 

      case VirtualKey::VK_a:
      case VirtualKey::VK_A: keyboardIn[0] = 0x77; break;  // a-A
      case VirtualKey::VK_s:
      case VirtualKey::VK_S: keyboardIn[1] = 0x77; break;  // s-S
      case VirtualKey::VK_d:
      case VirtualKey::VK_D: keyboardIn[2] = 0x77; break;  // d-D
      case VirtualKey::VK_f:
      case VirtualKey::VK_F: keyboardIn[3] = 0x77; break;  // f-F
      case VirtualKey::VK_g:
      case VirtualKey::VK_G: keyboardIn[4] = 0x77; break;  // g-G
      case VirtualKey::VK_h:
      case VirtualKey::VK_H: keyboardIn[5] = 0x77; break;  // h-H
      case VirtualKey::VK_j:
      case VirtualKey::VK_J: keyboardIn[6] = 0x77; break;  // j-J
      case VirtualKey::VK_k:
      case VirtualKey::VK_K: keyboardIn[7] = 0x77; break;  // k-K
      case VirtualKey::VK_l:
      case VirtualKey::VK_L: keyboardIn[8] = 0x77; break;  // l-L
      case VirtualKey::VK_COLON:
      case VirtualKey::VK_SEMICOLON: keyboardIn[9] = 0x77; break;  // ;-:
      case VirtualKey::VK_QUOTE:
      case VirtualKey::VK_QUOTEDBL: keyboardIn[10] = 0x77; break;  // '-"
      case VirtualKey::VK_VERTICALBAR:
      case VirtualKey::VK_BACKSLASH: keyboardIn[11] = 0x77; break;  // |\ - []
      case VirtualKey::VK_PAGEUP:
      case VirtualKey::VK_UP: keyboardIn[12] = 0x77;  break; // Up
      case VirtualKey::VK_END: keyboardIn[13] = 0x77;  break; // End
      case VirtualKey::VK_PAGEDOWN:
      case VirtualKey::VK_DOWN: keyboardIn[14] = 0x77;  break; // Down 

      case VirtualKey::VK_SPACE: keyboardIn[0] = 0x6F; break;  // space
      case VirtualKey::VK_y:
      case VirtualKey::VK_Y: keyboardIn[1] = 0x6F; break;  // y-Y
      case VirtualKey::VK_x:
      case VirtualKey::VK_X: keyboardIn[2] = 0x6F; break;  // x-X
      case VirtualKey::VK_c:
      case VirtualKey::VK_C: keyboardIn[3] = 0x6F; break;  // c-C
      case VirtualKey::VK_v:
      case VirtualKey::VK_V: keyboardIn[4] = 0x6F; break;  // v-V
      case VirtualKey::VK_b:
      case VirtualKey::VK_B: keyboardIn[5] = 0x6F; break;  // b-B
      case VirtualKey::VK_n:
      case VirtualKey::VK_N: keyboardIn[6] = 0x6F; break;  // n-N
      case VirtualKey::VK_m:
      case VirtualKey::VK_M: keyboardIn[7] = 0x6F; break;  // m-M
      case VirtualKey::VK_LESS:
      case VirtualKey::VK_COMMA: keyboardIn[8] = 0x6F; break;  // <-,
      case VirtualKey::VK_GREATER:
      case VirtualKey::VK_PERIOD: keyboardIn[9] = 0x6F; break;  // >-.
      case VirtualKey::VK_SLASH:
      case VirtualKey::VK_QUESTION: keyboardIn[10] = 0x6F; break;  // /-?
      case VirtualKey::VK_LCTRL:
      case VirtualKey::VK_RCTRL: keyboardIn[13] = 0x6F; break;  // L Enter
      case VirtualKey::VK_RETURN:
      case VirtualKey::VK_KP_ENTER: keyboardIn[14] = 0x6F; break;  // R Enter

      case VirtualKey::VK_LSHIFT:
      case VirtualKey::VK_RSHIFT: shiftFlag = 0x5F; break;  // L and R shift
      case VirtualKey::VK_TAB: shiftFlag = 0x3F; break;  // TAB
      default: break;
      }
};


void IRAM_ATTR drawScanline(void * arg, uint8_t * dest, int scanLine)
{
  auto brcolor = DisplayController.createRawPixel(RGB222(0, 3, 0)); // bright green
  auto fgcolor = DisplayController.createRawPixel(RGB222(0, 2, 0)); // green
  auto bgcolor = DisplayController.createRawPixel(RGB222(0, 1, 0)); // dark green
  auto darkbgcolor = DisplayController.createRawPixel(RGB222(0, 0, 0)); // black

  auto width  = DisplayController.getScreenWidth();
  auto height = DisplayController.getScreenHeight();

  // draws "scanlinesPerCallback" scanlines every time drawScanline() is called
  for (int i = 0; i < scanlinesPerCallback; ++i) {
    // fill border with background color
    memset(dest, darkbgcolor, width);
    if (!(scanLine < borderSize || scanLine >= height - borderSize)) {
      for (int i = 0; i < 48; ++i)  // 48 bytes must transformed to 288 pixels on row
        {
          unsigned char videobyte = pmd85ram[0xC000 + (scanLine-borderSize)*64 + i];  // Video RAM start address + row start + byte on row
          if ((videobyte & 0x80) && blinkFlag) {
            VGA_PIXELINROW(dest, i*6+borderXSize) = bgcolor;
            VGA_PIXELINROW(dest, i*6+1+borderXSize) = bgcolor;
            VGA_PIXELINROW(dest, i*6+2+borderXSize) = bgcolor;
            VGA_PIXELINROW(dest, i*6+3+borderXSize) = bgcolor;
            VGA_PIXELINROW(dest, i*6+4+borderXSize) = bgcolor;
            VGA_PIXELINROW(dest, i*6+5+borderXSize) = bgcolor;
          } else {
            if (videobyte & 0x40) {
              if(videobyte & 0x01) VGA_PIXELINROW(dest, i*6+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+borderXSize) = bgcolor;
              if(videobyte & 0x02) VGA_PIXELINROW(dest, i*6+1+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+1+borderXSize) = bgcolor;
              if(videobyte & 0x04) VGA_PIXELINROW(dest, i*6+2+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+2+borderXSize) = bgcolor;
              if(videobyte & 0x08) VGA_PIXELINROW(dest, i*6+3+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+3+borderXSize) = bgcolor;
              if(videobyte & 0x10) VGA_PIXELINROW(dest, i*6+4+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+4+borderXSize) = bgcolor;
              if(videobyte & 0x20) VGA_PIXELINROW(dest, i*6+5+borderXSize) = brcolor;
              else VGA_PIXELINROW(dest, i*6+5+borderXSize) = bgcolor;
            } else {
              if(videobyte & 0x01) VGA_PIXELINROW(dest, i*6+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+borderXSize) = bgcolor;
              if(videobyte & 0x02) VGA_PIXELINROW(dest, i*6+1+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+1+borderXSize) = bgcolor;
              if(videobyte & 0x04) VGA_PIXELINROW(dest, i*6+2+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+2+borderXSize) = bgcolor;
              if(videobyte & 0x08) VGA_PIXELINROW(dest, i*6+3+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+3+borderXSize) = bgcolor;
              if(videobyte & 0x10) VGA_PIXELINROW(dest, i*6+4+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+4+borderXSize) = bgcolor;
              if(videobyte & 0x20) VGA_PIXELINROW(dest, i*6+5+borderXSize) = fgcolor;
              else VGA_PIXELINROW(dest, i*6+5+borderXSize) = bgcolor;
            }
          }
        }
    }
    // go to next scanline
    ++scanLine;
    dest += width;
  }

  if (scanLine == height) {
    // signal end of screen
    vTaskNotifyGiveFromISR(mainTaskHandle, NULL);
  }
}

void setup()
{
  Serial.begin(115200);
  mainTaskHandle = xTaskGetCurrentTaskHandle();

  // Settings for "sound generator"
  soundGenerator.attach(&square);
  square.enable(true);
  square.setSampleRate(SAMPLE_RATE);  // For square it is enough
  pinMode(SD_LED, OUTPUT);
  digitalWrite(SD_LED, HIGH);   //Laufwerksanzeige
/*
  // SPIFS emulating tape recorder
  if(SPIFFS.begin(true)){
    // SPIFS is ready, try open file
    file = SPIFFS.open("/tape.ptp","r");
    if(file) isTape = true;
  }
*/

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  SPI.setFrequency(40000000);
  
  
  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    Serial.println("Mount Error!");
    //return kSD_Fail;
  }
  
  file = SD.open("/tape.ptp");
  if(file) isTape = true;
  Serial.println("Mount /tape.ptp");

  digitalWrite(SD_LED, LOW);
  pinMode(SD_LED, INPUT);
  
  // Set VGA for PMD85 display green monitor
  DisplayController.begin();
  DisplayController.setScanlinesPerCallBack(scanlinesPerCallback);
  DisplayController.setDrawScanlineCallback(drawScanline);
  DisplayController.setResolution(VGA_400x300_60Hz);
  PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::GenerateVirtualKeys);
  PS2Controller.keyboard() -> setLayout(&fabgl::UKLayout);
  // Set CPU bus functions and start it
  m_i8080.setCallbacks(auxPoint, readByte, writeByte, readWord, writeWord, readIO, writeIO); 
  m_i8080.reset();
  m_i8080.setPC(0x8000);  // PMD85 starts at this address due blocking A15
  for (int i = 0; i < 16; ++i) keyboardIn[i]=0xFF;

  // Set function pro Keyboard processing
  PS2Controller.keyboard()->onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
      if (keyDown) {
        procesKeyDown(*vk);
    } else procesKeyUp(*vk);
  };
}

int BlinkCnt=0;   // Counter for video pixel blink

void loop()
{
  static int numCycles;
  numCycles = 0;
  while(numCycles < 30000) numCycles += m_i8080.step(); // approx. 30000 cycles per 16.6 milisec (60 Hz VGA) je höher umso schneller
  BlinkCnt++;
  if(BlinkCnt >= 10) {
    BlinkCnt=0;
    if(blinkFlag) blinkFlag = false; else blinkFlag = true;
  }
  if(basic==true) basic_load();
  
  // wait for vertical sync
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {

  if (Update.begin(updateSize, U_FLASH, 2, 1, " ")) {
    size_t written = Update.writeStream(updateSource);

    if (Update.end()) {
      Serial.println("   Starter loaded successfully, now Reboot   "); 
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}

void basic_load(void) {
  file.close();
  if ( !SD.exists("/run.bin") ) Serial.println("Starter not found!");

  file = SD.open("/run.bin");

  if (file) {
    size_t updateSize = file.size();

    if (updateSize > 0) {
      Serial.println("load Starter");
      performUpdate(file, updateSize);
    }
    file.close();
  }

}
