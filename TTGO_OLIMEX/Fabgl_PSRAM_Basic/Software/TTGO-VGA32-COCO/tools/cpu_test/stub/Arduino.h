// Host stand-in for Arduino.h: the CPU core only needs fixed-width types and memset.
#ifndef CPU_TEST_ARDUINO_H
#define CPU_TEST_ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define PROGMEM
#endif
