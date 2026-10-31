// Host stand-in for FreeRTOS: just enough for the SSC's core-0 task, mapped
// onto std::thread (tools/ssc_test, SSC_CORE0 builds).
#ifndef SSC_TEST_FREERTOS_H
#define SSC_TEST_FREERTOS_H
#include <thread>
typedef std::thread* TaskHandle_t;
typedef int BaseType_t;
typedef unsigned TickType_t;
#endif
