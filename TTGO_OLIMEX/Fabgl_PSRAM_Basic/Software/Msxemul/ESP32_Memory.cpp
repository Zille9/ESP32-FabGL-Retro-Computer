/*
 * ESP32_Memory.cpp
 * ESP32용 메모리 시스템 초기화
 *
 * MSX.c의 GetMemory()는 그대로 사용하되,
 * ESP32 heap allocator를 PSRAM 우선으로 설정
 */

#include <Arduino.h>

extern "C" {
    int InitMemorySystem(void) {
        Serial.println("\n=== Memory System ===");

        if(!psramFound()) {
            Serial.println("ERROR: PSRAM not found!");
            return 0;
        }

        Serial.printf("PSRAM: %d bytes total\n", ESP.getPsramSize());
        Serial.printf("PSRAM: %d bytes free\n", ESP.getFreePsram());
        Serial.printf("Heap: %d bytes free\n", ESP.getFreeHeap());
        Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
        
        // PSRAM 할당 테스트
        void* test1 = ps_malloc(65536);  // 64KB
        void* test2 = ps_malloc(32768);  // 32KB
        
        if(test1 && test2) {
            Serial.println("PSRAM allocation test: OK");
            free(test1);
            free(test2);
        } else {
            Serial.println("WARNING: PSRAM allocation test failed!");
            if(test1) free(test1);
            if(test2) free(test2);
        }
        
        Serial.printf("PSRAM after test: %d bytes free\n", ESP.getFreePsram());
        Serial.println("=====================\n");

        return 1;
    }

    /**
     * TrashMemorySystem() - 정리
     */
    void TrashMemorySystem(void) {
        Serial.println("Memory system cleanup");
    }

} // extern "C"
