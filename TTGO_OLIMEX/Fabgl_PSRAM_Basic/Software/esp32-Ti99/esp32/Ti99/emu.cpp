// Classic99 for ESP32 - emulation task
// Instruction stepping follows Classic99 Tiemul.cpp do1() (C) Mike Brent aka Tursi.
// See the original licence there. Not for distribution without the author's permission.

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include "emu.h"
#include "bus.h"
#include "cpu9900.h"
#include "vdp9918.h"
#include "sound9919.h"
#include "speech.h"
#include "cart.h"
#include "ticc.h"
#include "keyboard_ti.h"

CPU9900 *pCPU = nullptr;

static volatile bool pauseRequest = false;
static volatile bool paused = false;
static TaskHandle_t emuTaskHandle = nullptr;

#define FRAME_US 16667          // 60Hz

void emuInit() {
    pCPU = new CPU9900();       // builds the decode tables
    emuReset();
}

void emuReset() {
    busReset();
    cartResetBank();
    vdpReset();
    soundReset();
    speechReset();
    pCPU->reset();              // reads the reset vector from console ROM
    pCPU->ResetCycleCount();
}

// one instruction, as in Classic99 do1(): interrupt check, execute, then
// feed the elapsed cycles to the 9901 timer and the VDP beam
static IRAM_ATTR bool step() {
    if (interruptPending() && ((pCPU->ST & 0x000f) >= 1) && (!skip_interrupt)) {
        pCPU->TriggerInterrupt(0x0004, 2);
    }
    if ((pCPU->PC == TICC_SECTOR_HOOK_PC) && busDiskDsrActive()) {
        HandleTICCSector();     // sector transfer done here, DSR continues at its exit
    }
    bool nopFrame = pCPU->idling || pCPU->halted;
    pCPU->ExecuteOpcode(nopFrame);
    if (skip_interrupt > 0) --skip_interrupt;

    int cycles = pCPU->nCycleCount;
    pCPU->nCycleCount = 0;
    update9901(cycles);
    if (speechEnabled()) speechAdvance(cycles);
    return vdpAdvance(cycles);
}

static void emuTask(void *) {
    int64_t nextFrame = esp_timer_get_time() + FRAME_US;
    int64_t statStart = esp_timer_get_time();
    uint32_t instructions = 0, frames = 0;
    int64_t busyUs = 0;             // time spent emulating, to report load

    for (;;) {
        if (pauseRequest) {
            paused = true;
            while (pauseRequest) vTaskDelay(10 / portTICK_PERIOD_MS);
            paused = false;
            nextFrame = esp_timer_get_time() + FRAME_US;
        }

        int64_t frameStart = esp_timer_get_time();
        while (!step()) {
            ++instructions;
        }
        ++instructions;
        ++frames;
        busFrameTick();
        tiTypeFrame();

        // pace to 60 frames per second
        int64_t now = esp_timer_get_time();
        busyUs += now - frameStart;
        if (now < nextFrame) {
            int64_t wait = nextFrame - now;
            if (wait > 1500) vTaskDelay((wait - 1000) / 1000 / portTICK_PERIOD_MS);
            while (esp_timer_get_time() < nextFrame) { }
        } else if (now - nextFrame > 3 * FRAME_US) {
            nextFrame = now;    // too far behind: don't try to catch up
        }
        nextFrame += FRAME_US;

        if (now - statStart >= 5000000) {
            float secs = (now - statStart) / 1e6f;
            DBG("emu: %.1f fps, %u instr/s, load %.0f%%, PC=>%04X\n", frames / secs,
                          (unsigned)(instructions / secs), busyUs / (secs * 1e4f), pCPU->PC);
            busyUs = 0;
            statStart = now;
            instructions = frames = 0;
        }
    }
}

bool emuStart() {
    return xTaskCreatePinnedToCore(emuTask, "emu", 8192, nullptr, 5, &emuTaskHandle, 1) == pdPASS;
}

void emuPause(bool pause) {
    pauseRequest = pause;
    if (pause) {
        while (!paused) vTaskDelay(1);
    }
}

bool emuIsPaused() {
    return paused;
}
