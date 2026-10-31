//******** Oric-Emulator für TTGO32 und Olimex_SBC **********
//        Anpassung der SD-Card-Pins in der sdfat_h         *
//*        Rückkehr zum ESP32-Basic über Taste F1 !!!       *
//*           wird in der Topline angezeigt                 *
//***********************************************************
/*
 * statically allocated ram is limited to ~170KB
 * so ram is allocated on the heap with malloc
 */ 
#include "Arduino.h"

#pragma GCC optimize("O3")

#include "log.h"

#include "video.h"

#include "system_d.h"

SYSTEM System;

#include "esp_timer.h"
#include "freertos/task.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "sdfat.h"
#include "spifat.h"

#include "keyboard.h"

#include "basic11b.h"
#include "basic11bP.h"
#include "charset.h"

#include "m6502.h"
#include "ula.h"
#include "io.h"
#include "ym2149.h"
#include "tape.h"
#include "system.h"

#include "osd.h"

void periodic_timer_callback(void *arg) {
    // put your main code here, to run repeatedly:
    System.tPrev = esp_timer_get_time(); // xthal_get_ccount();

    System.nSum += m6502DoOps(CPU_FREQ/CPU_SLOT_FREQ);
    if (System.Osd.display) {
        osdDisplay2((System.vSyncCount++ >> 5) & 1);
        osdTopDisplay2(0);
        osdBottomDisplay2(0);
//        osdFsm();
    } else {
        if (System.Osd.topDisplay) {
            osdTopDisplay2(0);
            osdBottomDisplay2(0);
        }
        ulaDisplay2((System.vSyncCount++ >> 5) & 1);
    }
    while (System.ps2keybd->dataAvailable(0)) {
        BYTE scancode = System.ps2keybd->getData(0, 1);
//        log_d("got %d", scancode);
        kPutScancode(scancode);
        kDoFSM(); 
    }

    if (System.Psg.audioEnable == 1) {
        if (!System.Psg.m_play) {
            if (ym2149Delta() > 3*(SOUND_SAMPLE_BUFFER_SIZE)) { 
                ym2149I2SPlay(true);
                Serial.println("Sound started");
            }
        }
    }

    if (System.tN == 200) {
        log_d("loop avg %d %d\n", System.tSum / System.tN, System.nSum / System.tN);
        System.tSum = 0;
        System.tN = 0;
        System.nSum = 0;
    }
    System.tDelta = esp_timer_get_time() - System.tPrev;
    if (System.tDelta & 0x80000000) System.tDelta += 0xFFFFFFFF;
    System.tN++;
    System.tSum += System.tDelta;
}

void taskCore1(void *parameters) {
    System.periodic_timer_args.callback = &periodic_timer_callback;
    System.periodic_timer_args.dispatch_method = ESP_TIMER_TASK;
    System.periodic_timer_args.name = "Per-Cpu";
    ESP_ERROR_CHECK(esp_timer_create(&System.periodic_timer_args, &System.periodic_timer));
    /* The timer has been created but is not running yet */
    ESP_ERROR_CHECK(esp_timer_start_periodic(System.periodic_timer, CPU_FREQ/CPU_SLOT_FREQ));    
    
    for( ;; ) {
        yield();
        delay(100);
    }
}

void setup() {
    // put your setup code here, to run once:
    // Ensures the bootloader runs on the next reset
    const esp_partition_t* otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (otadata) {
        esp_partition_erase_range(otadata, 0, otadata->size);
    }

    Serial.begin(115200);
    delay(200);
    log_d("Start");

    // Monta o SD ANTES de videoInit(). O driver de video (I2S bare-metal)
    // gera o sinal VGA continuamente e satura as interrupcoes; se o SD.begin()
    // rodar depois disso, ele trava. Aqui o ambiente esta limpo.
    tapeInit();
    sdFatInit();
    if (sdFatMount()) {
        log_d("SD mounted at startup");
    } else {
        log_d("SD mount FAILED at startup");
    }
    spiFatInit();

    // Loga estado de memoria ANTES do osdInit, que aloca ~53KB via ps_malloc
    // para o buffer de arquivos. Se PSRAM nao estiver habilitada no
    // platformio.ini (board_build.arduino.memory_type ou -DBOARD_HAS_PSRAM),
    // ps_malloc retorna NULL e a listagem de diretorio crasha.
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    videoInit();
    osdInit("/");       // raiz do SD

/*
    Serial.println("Init Micro SD card Fat FS");
    sdFatInit();
    Serial.println("Init SpiFlash Fat FS");
    spiFatInit();
*/

/*
    FFat.mkdir("/Tapes");
    FFat.mkdir("/Disks");
    FFat.mkdir("/Tapes/Games");
    FFat.mkdir("/Tapes/Demos");
    FFat.mkdir("/Tapes/Languages");
*/
    /* 
     * to initialize spi fat from sd fat
     */
    /*
    #ifdef FILESYSSDCARD
        spiFatInit();
    #else 
        sdFatInit();
    #endif
     fileCopySdFatToSpiFat("/Oric/tapes", "/");
    */

    systemInit(1, 1);   // atmos & quick read patch
    ym2149Init(2);      // use real-time sound

    // tapeLoadSD("/Oric/tapes/Defense Force (19xx)(Tansoft).tap");
    // tapeLoadSPI("/Defense Force (19xx)(Tansoft).tap");

    log_d("ROM0 %08X ROM1 %08X RAM %08X", System.rom0, System.rom1, System.ram);
    System.tSum = 0;
    System.tN = 0;
    System.ps2keybd = new(PS2Controller);
    System.ps2keybd->begin(PS2Preset::KeyboardPort0, KbdMode::NoVirtualKeys);

    xTaskCreatePinnedToCore(taskCore1, "ORIC-1", 10240, &System.paramCore1, 1, &System.xHandleCore1, 1);

//    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    /* The timer has been created but is not running yet */
//    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 20000));    
}

void loop() {

    for(;;) {
        if (System.Osd.display) {
            osdFsm();
            delay(10);
        } else {
            delay(200);
            yield();
        }
    }
}
