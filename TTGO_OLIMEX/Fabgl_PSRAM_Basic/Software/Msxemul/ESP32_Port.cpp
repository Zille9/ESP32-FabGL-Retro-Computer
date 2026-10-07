#include "fabgl.h"
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>

#undef word

extern "C" {
  #ifndef _ARDUINO_H_
  #define _ARDUINO_H_
  #endif
  #include "MSX.h"
  
  // MSX.c에서 호출하는 함수 연결
  void SetColor(byte N, byte R, byte G, byte B);
}
#include "config.h"

// [중요] VGA16Controller 유지
extern fabgl::VGA16Controller VGAController;
extern fabgl::PS2Controller PS2Controller;
extern fabgl::Canvas *Canvas;

// 화면 버퍼
fabgl::Bitmap *msxScreen = nullptr;
uint8_t *XBuf = NULL;

static fabgl::VGA16Controller* vga16 = nullptr;

// === 색상 보정 설정 (정상 작동 확인됨) ===
#define COLOR_MAP_RGB  // 변환 없음 (표준 RGB 배열 - 빨간색/녹색/파란색 정상)

fabgl::RGB888 convertColor(byte R, byte G, byte B) {
  fabgl::RGB888 color;
  
  #if defined(COLOR_MAP_BGR)
    color.R = B; color.G = G; color.B = R;
  #elif defined(COLOR_MAP_RGB)
    color.R = R; color.G = G; color.B = B;
  #elif defined(COLOR_MAP_RBG)
    color.R = R; color.G = B; color.B = G; 
  #elif defined(COLOR_MAP_GRB)
    color.R = G; color.G = R; color.B = B;
  #else 
    color.R = R; color.G = G; color.B = B;
  #endif
  
  return color;
}

// 키보드 매핑 매크로 (MSX는 Active Low 방식)
#define PRESS(r, b) KeyState[r] &= ~(b)
#define RELEASE(r, b) KeyState[r] |= (b)

// [복구] 원본 파일의 ProcessKey 함수 (오타 수정 및 전체 매핑 복원)
void ProcessKey(fabgl::VirtualKey vk, bool down) {
  if (down) {
    switch(vk) {
      // Row 0: 0-7
      case fabgl::VK_0: PRESS(0, 0x01); break;
      case fabgl::VK_RIGHTPAREN: PRESS(0, 0x01); break; // 0
      case fabgl::VK_1: PRESS(0, 0x02); break;
      case fabgl::VK_EXCLAIM: PRESS(0, 0x02); break;
      case fabgl::VK_2: PRESS(0, 0x04); break;
      case fabgl::VK_AT: PRESS(0, 0x04); break;
      case fabgl::VK_3: PRESS(0, 0x08); break;
      case fabgl::VK_HASH: PRESS(0, 0x08); break;
      case fabgl::VK_4: PRESS(0, 0x10); break;
      case fabgl::VK_DOLLAR: PRESS(0, 0x10); break;
      case fabgl::VK_5: PRESS(0, 0x20); break;
      case fabgl::VK_PERCENT: PRESS(0, 0x20); break;
      case fabgl::VK_6: PRESS(0, 0x40); break;
      case fabgl::VK_CARET: PRESS(0, 0x40); break;
      case fabgl::VK_7: PRESS(0, 0x80); break;
      case fabgl::VK_AMPERSAND: PRESS(0, 0x80); break;
      
      // Row 1: 8, 9, -, =, \, [, ], ;
      case fabgl::VK_8: PRESS(1, 0x01); break;
      case fabgl::VK_ASTERISK: PRESS(1, 0x01); break;
       
      case fabgl::VK_9: PRESS(1, 0x02); break;
      case fabgl::VK_LEFTPAREN: PRESS(1, 0x02); break;

      case fabgl::VK_MINUS: PRESS(1, 0x04); break;
      case fabgl::VK_UNDERSCORE: PRESS(1, 0x04); break;
      case fabgl::VK_EQUALS: PRESS(1, 0x08); break;
       case fabgl::VK_PLUS: PRESS(1, 0x08); break;
      case fabgl::VK_BACKSLASH: PRESS(1, 0x10); break;
      case fabgl::VK_VERTICALBAR: PRESS(1, 0x10); break;
      case fabgl::VK_LEFTBRACKET: PRESS(1, 0x20); break;
      case fabgl::VK_RIGHTBRACKET: PRESS(1, 0x40); break;
      case fabgl::VK_LEFTBRACE: PRESS(1, 0x20); break;
      case fabgl::VK_RIGHTBRACE: PRESS(1, 0x40); break;
      case fabgl::VK_SEMICOLON: PRESS(1, 0x80); break;
      case fabgl::VK_COLON: PRESS(1, 0x80); break;

      // Row 2: ', `, ,, ., /, Dead, A, B
      case fabgl::VK_QUOTE: PRESS(2, 0x01); break;
      case fabgl::VK_QUOTEDBL: PRESS(2, 0x01); break;
      case fabgl::VK_GRAVEACCENT: PRESS(2, 0x02); break;
      case fabgl::VK_TILDE: PRESS(2, 0x02); break;
      case fabgl::VK_COMMA: PRESS(2, 0x04); break;
      case fabgl::VK_LESS: PRESS(2, 0x04); break;
      case fabgl::VK_PERIOD: PRESS(2, 0x08); break;
      case fabgl::VK_SLASH: PRESS(2, 0x10); break;
      case fabgl::VK_QUESTION: PRESS(2, 0x10); break;
      // Dead key는 스킵
      case fabgl::VK_A: PRESS(2, 0x40); break;
      case fabgl::VK_B: PRESS(2, 0x80); break;

      case fabgl::VK_a: PRESS(2, 0x40); break;
      case fabgl::VK_b: PRESS(2, 0x80); break;

      // Row 3: C, D, E, F, G, H, I, J
      case fabgl::VK_C: PRESS(3, 0x01); break;
      case fabgl::VK_D: PRESS(3, 0x02); break;
      case fabgl::VK_E: PRESS(3, 0x04); break;
      case fabgl::VK_F: PRESS(3, 0x08); break;
      case fabgl::VK_G: PRESS(3, 0x10); break;
      case fabgl::VK_H: PRESS(3, 0x20); break;
      case fabgl::VK_I: PRESS(3, 0x40); break;
      case fabgl::VK_J: PRESS(3, 0x80); break;

      case fabgl::VK_c: PRESS(3, 0x01); break;
      case fabgl::VK_d: PRESS(3, 0x02); break;
      case fabgl::VK_e: PRESS(3, 0x04); break;
      case fabgl::VK_f: PRESS(3, 0x08); break;
      case fabgl::VK_g: PRESS(3, 0x10); break;
      case fabgl::VK_h: PRESS(3, 0x20); break;
      case fabgl::VK_i: PRESS(3, 0x40); break;
      case fabgl::VK_j: PRESS(3, 0x80); break;

      // Row 4: K, L, M, N, O, P, Q, R
      case fabgl::VK_K: PRESS(4, 0x01); break;
      case fabgl::VK_L: PRESS(4, 0x02); break;
      case fabgl::VK_M: PRESS(4, 0x04); break;
      case fabgl::VK_N: PRESS(4, 0x08); break;
      case fabgl::VK_O: PRESS(4, 0x10); break;
      case fabgl::VK_P: PRESS(4, 0x20); break;
      case fabgl::VK_Q: PRESS(4, 0x40); break;
      case fabgl::VK_R: PRESS(4, 0x80); break;

      case fabgl::VK_k: PRESS(4, 0x01); break;
      case fabgl::VK_l: PRESS(4, 0x02); break;
      case fabgl::VK_m: PRESS(4, 0x04); break;
      case fabgl::VK_n: PRESS(4, 0x08); break;
      case fabgl::VK_o: PRESS(4, 0x10); break;
      case fabgl::VK_p: PRESS(4, 0x20); break;
      case fabgl::VK_q: PRESS(4, 0x40); break;
      case fabgl::VK_r: PRESS(4, 0x80); break;


      // Row 5: S, T, U, V, W, X, Y, Z
      case fabgl::VK_S: PRESS(5, 0x01); break;
      case fabgl::VK_T: PRESS(5, 0x02); break;
      case fabgl::VK_U: PRESS(5, 0x04); break;
      case fabgl::VK_V: PRESS(5, 0x08); break;
      case fabgl::VK_W: PRESS(5, 0x10); break;
      case fabgl::VK_X: PRESS(5, 0x20); break;
      case fabgl::VK_Y: PRESS(5, 0x40); break;
      case fabgl::VK_Z: PRESS(5, 0x80); break;

      case fabgl::VK_s: PRESS(5, 0x01); break;
      case fabgl::VK_t: PRESS(5, 0x02); break;
      case fabgl::VK_u: PRESS(5, 0x04); break;
      case fabgl::VK_v: PRESS(5, 0x08); break;
      case fabgl::VK_w: PRESS(5, 0x10); break;
      case fabgl::VK_x: PRESS(5, 0x20); break;
      case fabgl::VK_y: PRESS(5, 0x40); break;
      case fabgl::VK_z: PRESS(5, 0x80); break;

      // Row 6: SHIFT, CTRL, GRAPH, CAPS, CODE, F1, F2, F3
      case fabgl::VK_LSHIFT: 
      case fabgl::VK_RSHIFT: PRESS(6, 0x01); break;
      case fabgl::VK_LCTRL: 
      case fabgl::VK_RCTRL: PRESS(6, 0x02); break;
      case fabgl::VK_LALT: PRESS(6, 0x04); break; // GRAPH key
      case fabgl::VK_CAPSLOCK: PRESS(6, 0x08); break;
      case fabgl::VK_RALT: PRESS(6, 0x10); break; // CODE key
      case fabgl::VK_F1: PRESS(6, 0x20); break;
      case fabgl::VK_F2: PRESS(6, 0x40); break;
      case fabgl::VK_F3: PRESS(6, 0x80); break;

      // Row 7: F4, F5, ESC, TAB, STOP, BS, SELECT, RETURN
      case fabgl::VK_F4: PRESS(7, 0x01); break;
      case fabgl::VK_F5: PRESS(7, 0x02); break;
      case fabgl::VK_ESCAPE: PRESS(7, 0x04); break;
      case fabgl::VK_TAB: PRESS(7, 0x08); break;
      case fabgl::VK_PAUSE: PRESS(7, 0x10); break; // STOP key
      case fabgl::VK_BACKSPACE: PRESS(7, 0x20); break;
      case fabgl::VK_HOME: PRESS(7, 0x40); break; // SELECT key
      case fabgl::VK_RETURN: PRESS(7, 0x80); break;

      // Row 8: SPACE, HOME(CLS), INS, DEL, LEFT, UP, DOWN, RIGHT
      case fabgl::VK_SPACE: PRESS(8, 0x01); break;
      case fabgl::VK_END: PRESS(8, 0x02); break; // HOME(CLS) key
      case fabgl::VK_INSERT: PRESS(8, 0x04); break;
      case fabgl::VK_DELETE: PRESS(8, 0x08); break;
      case fabgl::VK_LEFT: PRESS(8, 0x10); break;
      case fabgl::VK_UP: PRESS(8, 0x20); break;
      case fabgl::VK_DOWN: PRESS(8, 0x40); break;
      case fabgl::VK_RIGHT: PRESS(8, 0x80); break;

      // 텐키 숫자 매핑
      case fabgl::VK_KP_0: PRESS(0, 0x01); break; // 0
      case fabgl::VK_KP_1: PRESS(0, 0x02); break; // 1
      case fabgl::VK_KP_2: PRESS(0, 0x04); break; // 2
      case fabgl::VK_KP_3: PRESS(0, 0x08); break; // 3
      case fabgl::VK_KP_4: PRESS(0, 0x10); break; // 4
      case fabgl::VK_KP_5: PRESS(0, 0x20); break; // 5
      case fabgl::VK_KP_6: PRESS(0, 0x40); break; // 6
      case fabgl::VK_KP_7: PRESS(0, 0x80); break; // 7
      case fabgl::VK_KP_8: PRESS(1, 0x01); break; // 8
      case fabgl::VK_KP_9: PRESS(1, 0x02); break; // 9
      

      // 텐키 기호 매핑
      case fabgl::VK_KP_MULTIPLY: PRESS(2, 0x80); break; // * -> :
      case fabgl::VK_KP_PLUS: PRESS(1, 0x80); break;     // + -> ;
      case fabgl::VK_KP_MINUS: PRESS(1, 0x04); break;    // -
      case fabgl::VK_KP_DIVIDE: PRESS(2, 0x10); break;   // /
      case fabgl::VK_KP_PERIOD: PRESS(2, 0x08); break;   // .
      case fabgl::VK_GREATER: PRESS(2, 0x08); break;

      // 종료 키
      case fabgl::VK_F12: ExitNow = 1; break;
      
      default: break;
    }
  } else {
    // Key Up Event
    switch(vk) {
      // Row 0
      case fabgl::VK_0: RELEASE(0, 0x01); break;
      case fabgl::VK_RIGHTPAREN: RELEASE(0, 0x01); break; // 0
      case fabgl::VK_1: RELEASE(0, 0x02); break;
      case fabgl::VK_EXCLAIM: RELEASE(0, 0x02); break;
     
      case fabgl::VK_2: RELEASE(0, 0x04); break;
      case fabgl::VK_AT: RELEASE(0, 0x04); break;
      case fabgl::VK_3: RELEASE(0, 0x08); break;
      case fabgl::VK_HASH: RELEASE(0, 0x08); break;
      case fabgl::VK_4: RELEASE(0, 0x10); break;
      case fabgl::VK_DOLLAR: RELEASE(0, 0x10); break;
      case fabgl::VK_5: RELEASE(0, 0x20); break;
      case fabgl::VK_PERCENT: RELEASE(0, 0x20); break;
      case fabgl::VK_6: RELEASE(0, 0x40); break;
      case fabgl::VK_CARET: RELEASE(0, 0x40); break;
      case fabgl::VK_7: RELEASE(0, 0x80); break;
      case fabgl::VK_AMPERSAND: RELEASE(0, 0x80); break;
      // Row 1
      case fabgl::VK_8: RELEASE(1, 0x01); break;
      case fabgl::VK_ASTERISK: RELEASE(1, 0x01); break;
      case fabgl::VK_9: RELEASE(1, 0x02); break;
      case fabgl::VK_LEFTPAREN: RELEASE(1, 0x02); break;

      case fabgl::VK_MINUS: RELEASE(1, 0x04); break;
      case fabgl::VK_UNDERSCORE: RELEASE(1, 0x04); break;
      case fabgl::VK_EQUALS: RELEASE(1, 0x08); break;
      case fabgl::VK_PLUS: RELEASE(1, 0x08); break;
      case fabgl::VK_BACKSLASH: RELEASE(1, 0x10); break;
      case fabgl::VK_VERTICALBAR: RELEASE(1, 0x10); break;
     
      case fabgl::VK_LEFTBRACKET: RELEASE(1, 0x20); break;
      case fabgl::VK_RIGHTBRACKET: RELEASE(1, 0x40); break;
      case fabgl::VK_LEFTBRACE: RELEASE(1, 0x20); break;
      case fabgl::VK_RIGHTBRACE: RELEASE(1, 0x40); break;
      case fabgl::VK_SEMICOLON: RELEASE(1, 0x80); break;
      case fabgl::VK_COLON: RELEASE(1, 0x80); break;

      // Row 2
      case fabgl::VK_QUOTE: RELEASE(2, 0x01); break;
      case fabgl::VK_QUOTEDBL: RELEASE(2, 0x01); break;
      case fabgl::VK_GRAVEACCENT: RELEASE(2, 0x02); break;
      case fabgl::VK_TILDE: RELEASE(2, 0x02); break;
      case fabgl::VK_COMMA: RELEASE(2, 0x04); break;
       case fabgl::VK_LESS: RELEASE(2, 0x04); break;
      case fabgl::VK_PERIOD: RELEASE(2, 0x08); break;
      case fabgl::VK_GREATER: RELEASE(2, 0x08); break;
       
      case fabgl::VK_SLASH: RELEASE(2, 0x10); break;
      case fabgl::VK_QUESTION: RELEASE(2, 0x10); break;
      case fabgl::VK_A: RELEASE(2, 0x40); break;
      case fabgl::VK_B: RELEASE(2, 0x80); break;
      case fabgl::VK_a: RELEASE(2, 0x40); break;
      case fabgl::VK_b: RELEASE(2, 0x80); break;

      // Row 3
      case fabgl::VK_C: RELEASE(3, 0x01); break;
      case fabgl::VK_D: RELEASE(3, 0x02); break;
      case fabgl::VK_E: RELEASE(3, 0x04); break;
      case fabgl::VK_F: RELEASE(3, 0x08); break;
      case fabgl::VK_G: RELEASE(3, 0x10); break;
      case fabgl::VK_H: RELEASE(3, 0x20); break;
      case fabgl::VK_I: RELEASE(3, 0x40); break;
      case fabgl::VK_J: RELEASE(3, 0x80); break;

      case fabgl::VK_c: RELEASE(3, 0x01); break;
      case fabgl::VK_d: RELEASE(3, 0x02); break;
      case fabgl::VK_e: RELEASE(3, 0x04); break;
      case fabgl::VK_f: RELEASE(3, 0x08); break;
      case fabgl::VK_g: RELEASE(3, 0x10); break;
      case fabgl::VK_h: RELEASE(3, 0x20); break;
      case fabgl::VK_i: RELEASE(3, 0x40); break;
      case fabgl::VK_j: RELEASE(3, 0x80); break;

      // Row 4
      case fabgl::VK_K: RELEASE(4, 0x01); break;
      case fabgl::VK_L: RELEASE(4, 0x02); break;
      case fabgl::VK_M: RELEASE(4, 0x04); break;
      case fabgl::VK_N: RELEASE(4, 0x08); break;
      case fabgl::VK_O: RELEASE(4, 0x10); break;
      case fabgl::VK_P: RELEASE(4, 0x20); break;
      case fabgl::VK_Q: RELEASE(4, 0x40); break;
      case fabgl::VK_R: RELEASE(4, 0x80); break;

      case fabgl::VK_k: RELEASE(4, 0x01); break;
      case fabgl::VK_l: RELEASE(4, 0x02); break;
      case fabgl::VK_m: RELEASE(4, 0x04); break;
      case fabgl::VK_n: RELEASE(4, 0x08); break;
      case fabgl::VK_o: RELEASE(4, 0x10); break;
      case fabgl::VK_p: RELEASE(4, 0x20); break;
      case fabgl::VK_q: RELEASE(4, 0x40); break;
      case fabgl::VK_r: RELEASE(4, 0x80); break;

      // Row 5
      case fabgl::VK_S: RELEASE(5, 0x01); break;
      case fabgl::VK_T: RELEASE(5, 0x02); break;
      case fabgl::VK_U: RELEASE(5, 0x04); break;
      case fabgl::VK_V: RELEASE(5, 0x08); break;
      case fabgl::VK_W: RELEASE(5, 0x10); break;
      case fabgl::VK_X: RELEASE(5, 0x20); break;
      case fabgl::VK_Y: RELEASE(5, 0x40); break;
      case fabgl::VK_Z: RELEASE(5, 0x80); break;

      case fabgl::VK_s: RELEASE(5, 0x01); break;
      case fabgl::VK_t: RELEASE(5, 0x02); break;
      case fabgl::VK_u: RELEASE(5, 0x04); break;
      case fabgl::VK_v: RELEASE(5, 0x08); break;
      case fabgl::VK_w: RELEASE(5, 0x10); break;
      case fabgl::VK_x: RELEASE(5, 0x20); break;
      case fabgl::VK_y: RELEASE(5, 0x40); break;
      case fabgl::VK_z: RELEASE(5, 0x80); break;

      // Row 6
      case fabgl::VK_LSHIFT: 
      case fabgl::VK_RSHIFT: RELEASE(6, 0x01); break;
      case fabgl::VK_LCTRL: 
      case fabgl::VK_RCTRL: RELEASE(6, 0x02); break;
      case fabgl::VK_LALT: RELEASE(6, 0x04); break;
      case fabgl::VK_CAPSLOCK: RELEASE(6, 0x08); break;
      case fabgl::VK_RALT: RELEASE(6, 0x10); break;
      case fabgl::VK_F1: RELEASE(6, 0x20); break;
      case fabgl::VK_F2: RELEASE(6, 0x40); break;
      case fabgl::VK_F3: RELEASE(6, 0x80); break;

      // Row 7
      case fabgl::VK_F4: RELEASE(7, 0x01); break;
      case fabgl::VK_F5: RELEASE(7, 0x02); break;
      case fabgl::VK_ESCAPE: RELEASE(7, 0x04); break;
      case fabgl::VK_TAB: RELEASE(7, 0x08); break;
      case fabgl::VK_PAUSE: RELEASE(7, 0x10); break;
      case fabgl::VK_BACKSPACE: RELEASE(7, 0x20); break;
      case fabgl::VK_HOME: RELEASE(7, 0x40); break;
      case fabgl::VK_RETURN: RELEASE(7, 0x80); break;

      // Row 8
      case fabgl::VK_SPACE: RELEASE(8, 0x01); break;
      case fabgl::VK_END: RELEASE(8, 0x02); break;
      case fabgl::VK_INSERT: RELEASE(8, 0x04); break;
      case fabgl::VK_DELETE: RELEASE(8, 0x08); break;
      case fabgl::VK_LEFT: RELEASE(8, 0x10); break;
      case fabgl::VK_UP: RELEASE(8, 0x20); break;
      case fabgl::VK_DOWN: RELEASE(8, 0x40); break;
      case fabgl::VK_RIGHT: RELEASE(8, 0x80); break;

      // 텐키
      case fabgl::VK_KP_0: RELEASE(0, 0x01); break;
      case fabgl::VK_KP_1: RELEASE(0, 0x02); break;
      case fabgl::VK_KP_2: RELEASE(0, 0x04); break;
      case fabgl::VK_KP_3: RELEASE(0, 0x08); break;
      case fabgl::VK_KP_4: RELEASE(0, 0x10); break;
      case fabgl::VK_KP_5: RELEASE(0, 0x20); break;
      case fabgl::VK_KP_6: RELEASE(0, 0x40); break;
      case fabgl::VK_KP_7: RELEASE(0, 0x80); break;
      case fabgl::VK_KP_8: RELEASE(1, 0x01); break;
      case fabgl::VK_KP_9: RELEASE(1, 0x02); break;
      case fabgl::VK_KP_MULTIPLY: RELEASE(2, 0x80); break;
      case fabgl::VK_KP_PLUS: RELEASE(1, 0x80); break;
      case fabgl::VK_KP_MINUS: RELEASE(1, 0x04); break;
      case fabgl::VK_KP_DIVIDE: RELEASE(2, 0x10); break;
      case fabgl::VK_KP_PERIOD: RELEASE(2, 0x08); break;

      default: break;
    }
  }
}

// [복구] 키보드 업데이트 함수
void UpdateMSXKeyboard() {
  fabgl::Keyboard *kb = PS2Controller.keyboard();
  if(!kb) return;

  while(kb->virtualKeyAvailable()) {
    bool down;
    fabgl::VirtualKey vk = kb->getNextVirtualKey(&down);
    ProcessKey(vk, down);
  }
}

// MSX 초기 색상 테이블 (부팅 시 사용)
fabgl::RGB888 msxColor(uint8_t c) {
  // MSX 표준 RGB 테이블
  static const uint8_t stdPal[16][3] = {
    {0,0,0}, {0,0,0}, {32,192,32}, {96,224,96},
    {32,32,224}, {64,96,224}, {160,32,32}, {64,192,224},
    {224,32,32}, {224,96,96}, {192,192,32}, {192,192,128},
    {32,128,32}, {192,64,160}, {160,160,160}, {224,224,224}
  };
  
  uint8_t idx = c & 0x0F;
  return convertColor(stdPal[idx][0], stdPal[idx][1], stdPal[idx][2]);
}

int InitMachine(void) {
  Serial.println("InitMachine: Starting...");
  
  // 객체 연결 및 포인터 설정
  vga16 = &VGAController;

  if(!vga16) {
    Serial.println("ERROR: VGA16Controller not available!");
    return 0;
  }
  
  // 초기 팔레트 설정 (부팅 직후 화면 색상)
  if(vga16) {
    for(int i = 0; i < 16; i++) {
      vga16->setPaletteItem(i, msxColor(i));
    }
  }

  // 화면 버퍼 할당
  size_t bufSize = MSX_WIDTH * MSX_HEIGHT;
  XBuf = (uint8_t*)ps_malloc(bufSize);
  if(!XBuf) {
    Serial.println("Failed to allocate XBuf");
    return 0;
  }
  memset(XBuf, 0, bufSize);
  
  msxScreen = new fabgl::Bitmap(MSX_WIDTH, MSX_HEIGHT, XBuf, 
                                 fabgl::PixelFormat::Native, false);
  if(!msxScreen) {
    Serial.println("Failed to create msxScreen");
    free(XBuf);
    XBuf = NULL;
    return 0;
  }
  
  Serial.println("InitMachine completed successfully");
  return 1;
}

void TrashMachine(void) {
  if(msxScreen) { delete msxScreen; msxScreen = nullptr; }
  if(XBuf) { free(XBuf); XBuf = NULL; }
}

extern "C" void SetColor(byte N, byte R, byte G, byte B) {
  if(N >= 16) return;
  
  if(!vga16) vga16 = &VGAController;
  if(!vga16) return;
  
  // 변환된 색상 적용
  fabgl::RGB888 color = convertColor(R, G, B);
  vga16->setPaletteItem(N, color);
}

void RefreshScreen(void) {
  if(Canvas && msxScreen) {
    Canvas->drawBitmap(VGA_OFFSET_X, VGA_OFFSET_Y, msxScreen);
  }
}

void DrawSprites(byte Y, byte *LineBuffer) {
  if (SpritesOFF) return;

  int Size = (VDP[1] & 2) ? 16 : 8;
  int Mag  = (VDP[1] & 1); 

  for (int i = 31; i >=0; i--) {

   

    byte *Attr = SprTab + i * 4;
    int Sy = Attr[0];
    if (Sy != 208) { 

    if (Sy > 240) Sy -= 256;
    Sy++;

    int Diff = Y - Sy;
    if (Mag) Diff >>= 1;

    if (Diff >= 0 && Diff < Size) {
      int Sx = Attr[1];
      int PatIdx = Attr[2];
      byte Color = Attr[3] & 0x0F;

      if (Attr[3] & 0x80) Sx -= 32;
      if (Color == 0) continue;
      if (Size == 16) PatIdx &= 0xFC;

      byte PatLine = SprGen[PatIdx * 8 + Diff];

      for (int p = 0; p < 8; p++) {
        int DrawX = Sx + (Mag ? p * 2 : p);
        if ((PatLine & 0x80) && (DrawX >= 0) && (DrawX < MSX_WIDTH)) {
          LineBuffer[DrawX] = Color;
          if (Mag && (DrawX + 1 < MSX_WIDTH)) LineBuffer[DrawX + 1] = Color;
        }
        PatLine <<= 1;
      }
      if (Size == 16) {
        PatLine = SprGen[PatIdx * 8 + Diff + 16];
        for (int p = 0; p < 8; p++) {
          int DrawX = Sx + (Mag ? (p + 8) * 2 : (p + 8));
           if ((PatLine & 0x80) && (DrawX >= 0) && (DrawX < MSX_WIDTH)) {
            LineBuffer[DrawX] = Color;
            if (Mag && (DrawX + 1 < MSX_WIDTH)) LineBuffer[DrawX + 1] = Color;
          }
          PatLine <<= 1;
        }
      }
    }
 
    }
 
  }


}

void RefreshLine0(byte Y) {
  if(!XBuf || Y >= MSX_HEIGHT) return;
  byte *P = XBuf + Y * MSX_WIDTH;
  byte BG = VDP[7] & 0x0F;
  byte FG = VDP[7] >> 4;
  
  memset(P, BG, 8); 
  P += 8;
  int Row = (Y >> 3) * 40; 
  int Line = Y & 7;
  for (int X = 0; X < 40; X++) {
    int CharCode = ChrTab[Row + X];
    byte Pattern = ChrGen[CharCode * 8 + Line];
    for (int i = 0; i < 6; i++) {
      *P++ = (Pattern & 0x80) ? FG : BG;
      Pattern <<= 1;
    }
  }
  memset(P, BG, 8); 
}

void RefreshLine1(byte Y) {
  if(!XBuf || Y >= MSX_HEIGHT) return;
  byte *P = XBuf + Y * MSX_WIDTH;
  int Row = (Y >> 3) * 32;
  int Line = Y & 7;
  byte BackdropColor = VDP[7] & 0x0F;
  for (int X = 0; X < 32; X++) {
    int CharCode = ChrTab[Row + X];
    byte Pattern = ChrGen[CharCode * 8 + Line];
    byte Color = ColTab[CharCode >> 3]; 
    byte FG = Color >> 4;
    byte BG = Color & 0x0F;
    if(FG == 0) FG = BackdropColor;
    if(BG == 0) BG = BackdropColor;
    for(int i = 0; i < 8; i++) {
      *P++ = (Pattern & 0x80) ? FG : BG;
      Pattern <<= 1;
    }
  }
  DrawSprites(Y, XBuf + Y * MSX_WIDTH);
}

void RefreshLine2(byte Y) {
  if(!XBuf || Y >= MSX_HEIGHT) return;
  byte *P = XBuf + Y * MSX_WIDTH;
  int Zone = Y / 64; 
  int Row  = (Y >> 3) * 32; 
  int Line = Y & 7;         
  byte BackdropColor = VDP[7] & 0x0F;
  for (int X = 0; X < 32; X++) {
    int CharCode = ChrTab[Row + X] + (Zone * 256);
    byte Pattern = ChrGen[CharCode * 8 + Line];
    byte Color   = ColTab[CharCode * 8 + Line];
    byte FG = Color >> 4;   
    byte BG = Color & 0x0F; 
    if(FG == 0) FG = BackdropColor;
    if(BG == 0) BG = BackdropColor;
    for(int i=0; i<8; i++) {
      *P++ = (Pattern & 0x80) ? FG : BG;
      Pattern <<= 1;
    }
  }
  DrawSprites(Y, XBuf + Y * MSX_WIDTH);
}

void ClearLine(byte Y) {
  if(!XBuf || Y >= MSX_HEIGHT) return;
  byte *P = XBuf + Y * MSX_WIDTH;
  byte BG = VDP[7] & 0x0F;
  memset(P, BG, MSX_WIDTH);
}

void RefreshLineTx80(byte Y) { ClearLine(Y); }
void RefreshLine3(byte Y) { ClearLine(Y); }
void RefreshLine4(byte Y) { ClearLine(Y); }
void RefreshLine5(byte Y) { ClearLine(Y); }
void RefreshLine6(byte Y) { ClearLine(Y); }
void RefreshLine7(byte Y) { ClearLine(Y); }
void RefreshLine8(byte Y) { ClearLine(Y); }
void RefreshLine10(byte Y) { ClearLine(Y); }
void RefreshLine12(byte Y) { ClearLine(Y); }

void Keyboard(void) { UpdateMSXKeyboard(); }
unsigned int Joystick(void) {
  unsigned int status = 0xFFFF; 
  fabgl::Keyboard *kb = PS2Controller.keyboard();
  if (!kb) return status;
  if (kb->isVKDown(fabgl::VK_UP))    status &= ~0x0001; 
  if (kb->isVKDown(fabgl::VK_DOWN))  status &= ~0x0002; 
  if (kb->isVKDown(fabgl::VK_LEFT))  status &= ~0x0004; 
  if (kb->isVKDown(fabgl::VK_RIGHT)) status &= ~0x0008; 
  if (kb->isVKDown(fabgl::VK_SPACE)) status &= ~0x0010; 
  if (kb->isVKDown(fabgl::VK_M))     status &= ~0x0020; 
  return status;
}
unsigned int Mouse(byte N) { return 0; }
void PutImage(void) { RefreshScreen(); }

// SD 카드 파일 I/O
extern "C" {
  void* arduino_fopen(const char* filename, const char* mode) {
    if(!filename) return NULL;
    String path = String(filename);
    if(!path.startsWith("/")) path = "/" + path;
    File tempFile = SD.open(path, FILE_READ);
    if(!tempFile) return NULL;
    File* fptr = new File(tempFile);
    return (void*)fptr;
  }
  int arduino_fclose(void* stream) {
    if(!stream) return 0;
    File* f = (File*)stream;
    f->close();
    delete f;
    return 0;
  }
  size_t arduino_fread(void* ptr, size_t size, size_t nmemb, void* stream) {
    if(!stream || !ptr || size == 0 || nmemb == 0) return 0;
    File* f = (File*)stream;
    if(!f->available()) return 0;
    size_t bytesRead = f->read((uint8_t*)ptr, size * nmemb);
    return bytesRead / size;
  }
  size_t arduino_fwrite(const void* ptr, size_t size, size_t nmemb, void* stream) {
    if(!stream || !ptr || size == 0 || nmemb == 0) return 0;
    File* f = (File*)stream;
    size_t bytesWritten = f->write((const uint8_t*)ptr, size * nmemb);
    return bytesWritten / size;
  }
  int arduino_fseek(void* stream, long offset, int whence) {
    if(!stream) return -1;
    File* f = (File*)stream;
    if(whence == SEEK_SET) f->seek(offset);
    else if(whence == SEEK_END) f->seek(f->size() + offset);
    else if(whence == SEEK_CUR) f->seek(f->position() + offset);
    return 0;
  }
  long arduino_ftell(void* stream) {
    if(!stream) return -1;
    File* f = (File*)stream;
    return f->position();
  }
  void arduino_rewind(void* stream) {
    if(!stream) return;
    File* f = (File*)stream;
    f->seek(0);
  }
  int arduino_fgetc(void* stream) {
    if(!stream) return -1;
    File* f = (File*)stream;
    if(!f->available()) return -1;
    return f->read();
  }
  int arduino_feof(void* stream) {
    if(!stream) return 1;
    File* f = (File*)stream;
    return !f->available();
  }
}   