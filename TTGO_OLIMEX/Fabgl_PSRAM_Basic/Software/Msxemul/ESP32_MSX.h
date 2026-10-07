/*
 * ESP32_MSX.h
 * ESP32와 fMSX 코어 간의 브릿지
 */

#ifndef ESP32_MSX_H
#define ESP32_MSX_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
    #endif

    // 메모리 시스템 초기화 함수
    int InitMemorySystem(void);
    void TrashMemorySystem(void);

    // MSX.c에 정의된 변수들 (참조용)
    // 실제로는 MSX.c에서 정의됨

    #ifdef __cplusplus
}
#endif

#endif // ESP32_MSX_H
