#ifndef SSC_TEST_TASK_H
#define SSC_TEST_TASK_H
#include "FreeRTOS.h"
#include <chrono>
static inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char*, unsigned, void* arg,
                                                 unsigned, TaskHandle_t* h, int) {
    *h = new std::thread(fn, arg);
    (*h)->detach();
    return 1;
}
// One tick is 1 ms on the device; shorter here so the test runs quickly.
static inline void vTaskDelay(TickType_t) { std::this_thread::sleep_for(std::chrono::microseconds(50)); }
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
static inline void xTaskNotifyGive(TaskHandle_t) {}
static inline unsigned ulTaskNotifyTake(BaseType_t, TickType_t) { vTaskDelay(1); return 0; }
static inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
#endif
