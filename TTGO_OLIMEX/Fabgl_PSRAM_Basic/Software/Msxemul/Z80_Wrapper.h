/*
 * Z80_Wrapper.h
 * Z80.h의 extern "C" + Arduino.h 충돌 문제 해결
 */

#ifndef Z80_WRAPPER_H
#define Z80_WRAPPER_H

// Arduino.h를 먼저 include (extern "C" 밖에서)
#ifdef ARDUINO
#include <Arduino.h>
#endif

// Z80.h에서 Arduino.h를 다시 include하지 않도록 방지
#define _ARDUINO_H_

// 이제 Z80.h include
#include "Z80.h"

#endif // Z80_WRAPPER_H
