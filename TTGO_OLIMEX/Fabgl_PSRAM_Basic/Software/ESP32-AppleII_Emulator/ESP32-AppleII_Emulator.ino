/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : ESP32-VGA_AppleII_Emulator.ino
 *  Module : Arduino sketch entry point. setup() brings up PSRAM,
 *           the SD card, FabGL's VGA DisplayController and the
 *           PS/2 keyboard and mouse (the joystick), then
 *           constructs Apple2Machine, VGA and
 *           Supervisor. The emulation task runs 6502 cycles as
 *           FramePacer schedules them (60.05 Hz at 1X), renders
 *           and presents.
 * ============================================================
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "fabgl.h"
#include "src/AppleII/Apple2Machine.h"
#include "src/AppleII/RomLoader.h"
#include "src/Tools/Settings.h"
#include "src/Tools/KeyboardLayouts.h"
#include "src/VGA/VGA.h"
#include "src/Tools/Log.h"
#include "src/Supervisor/Supervisor.h"
#include "src/Supervisor/SupervisorUI.h"
#include "src/Tools/Bootloader.h"
#include "src/BuildConfig.h"
#include "src/Version.h"
#include "src/AppleII/FramePacer.h"
#include <esp_timer.h>

// Global pointer to keyboard for Apple2Device to read from
fabgl::Keyboard *keyboard_ptr = nullptr;
// PS/2 mouse on the second port, read by Apple2Device as the joystick
fabgl::Mouse *mouse_ptr = nullptr;
fabgl::PS2Controller PS2Controller;
AppleVGAController DisplayController;       // see src/VGA/VGA.h
fabgl::Canvas Canvas(&DisplayController);

VGA *vga;
Apple2Machine *machine;
Supervisor *supervisor;
static FramePacer pacer;

void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

static void EmulationTask(void*);
// set when there was no internal RAM for EmulationTask's stack
static bool emulationInLoop = false;

void setup()
{
    // ESP32_Bootloader builds: hand the next power-up back to its menu. First,
    // before anything else can fail or hang. A no-op in the standalone build.
    Bootloader::ReleaseOtadata();

    // A bigger receive buffer than the default 256 bytes: the Super Serial
    // Card takes one byte per frame out of it, so a burst typed or pasted at
    // the other end has to wait here.
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);
    Serial.printf("\nESP32-AppleII v%s (%s build)\n", FW_VERSION_STR, Bootloader::TargetName());
    
    // Audio pin. GPIO 26 belongs to the mouse port (PS/2 clock).
    pinMode(25, OUTPUT);
    // LilyGO TTGO VGA32 v1.4 PSRAM detection
    if(psramInit()) {
        Serial.println("\nPSRAM is correctly initialized");
        Serial.printf("PSRam Size: %d bytes\n", ESP.getPsramSize());
    } else {
        Serial.println("PSRAM not available");
    }

    // Initialize SPI for SD Card (TTGO VGA32 V1.4 specific pinout)
    SPI.begin(14, 35, 12, 13); // SCK, MISO, MOSI, SS/CS
    if(!SD.begin(13, SPI, 4000000))
    {
        Serial.println("SD Card Mount Failed");
        uint8_t cardType = SD.cardType();
        Serial.printf("SD Card Type Detected: %d\n", cardType);
        if(cardType == CARD_NONE){
            Serial.println("No SD card attached or pins wrong.");
        } else if(cardType == CARD_MMC){
            Serial.println("Card Type: MMC");
        } else if(cardType == CARD_SD){
            Serial.println("Card Type: SDSC");
        } else if(cardType == CARD_SDHC){
            Serial.println("Card Type: SDHC");
        } else {
            Serial.println("Card Type: UNKNOWN");
        }
    } else {
        Serial.println("SD Card Mount Success");
        //listDir(SD, "/", 0);
    }

    // Initialize FabGL VGA controller
    DisplayController.begin();
    // 640x200 in 16 colours (from 64): wide enough for the IIe's 560-dot
    // 80-column text and double hi-res. The mode is 640x240 line-doubled,
    // i.e. standard 640x480@60Hz timing; 640x200@70Hz is not accepted by
    // every monitor. FabGL centres the 200-line viewport in the 240 lines.
    // At 4 bits per pixel the framebuffer is the same 64K of internal RAM
    // the old 320x200 8-bit one took.
    DisplayController.setResolution(VGA_640x240_60Hz, 640, 200);

    // Print VGA timing and resolution diagnostics
    Serial.println("\n=== FabGL Video Mode Diagnostics ===");
    Serial.printf("Physical Screen Width: %d pixels\n", DisplayController.getScreenWidth());
    Serial.printf("Physical Screen Height: %d pixels\n", DisplayController.getScreenHeight());
    Serial.printf("Active Viewport Width: %d pixels\n", DisplayController.getViewPortWidth());
    Serial.printf("Active Viewport Height: %d pixels\n", DisplayController.getViewPortHeight());
    auto timings = DisplayController.getResolutionTimings();
    if (timings) {
        Serial.printf("Signal Clock Frequency: %.2f MHz\n", timings->frequency / 1000000.0);
    }
    Serial.println("====================================\n");


    // Initialize FabGL PS2 controller: keyboard on port 0 (GPIO 33/32), mouse
    // on port 1 (GPIO 26/27) as the joystick. With no mouse plugged in, its
    // reset retries for about 1.7 s and the joystick stays centred.
    PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1);
    keyboard_ptr = PS2Controller.keyboard();
    mouse_ptr = PS2Controller.mouse();
    Serial.printf("[mouse] joystick: %s\n",
                  mouse_ptr && mouse_ptr->isMouseAvailable() ? "present" : "absent");

    // Keyboard layout from NVS. FabGL starts on US, so this only has to run
    // when something else was chosen, but applying it unconditionally keeps
    // the boot log honest about which layout is live.
    const KeyboardLayoutProfile* kbd =
        ApplyKeyboardLayout(Settings::LoadKeyboard(KEYBOARD_LAYOUT_US), keyboard_ptr);
    Serial.printf("[kbd] layout: %s\n", kbd->name);

    // Machine model from NVS. A model with no built-in ROM can only boot when
    // its ROMs are on the card; otherwise fall back to the ][+ and say why.
    const MachineProfile* profile = &GetMachineProfile(Settings::LoadMachine(MACHINE_APPLE2PLUS));
    const char* bootNote = "";
    const char* missing = RomLoader::FirstMissing(*profile);
    if (missing)
    {
        Serial.printf("[rom] %s needs %s/%s - booting the ][+\n", profile->name, ROM_DIR, missing);
        bootNote = "ROMS MISSING - BOOTED APPLE ][+";
        profile = &GetMachineProfile(MACHINE_APPLE2PLUS);
    }
    Serial.printf("Machine: %s\n", profile->name);

    // Create objects
    machine = new Apple2Machine(*profile);
    machine->bootNote = bootNote;
    vga = new VGA();
    vga->setCanvas(&Canvas);

    DEBUG_PRINTLN("===> INIT Machine");
    machine->InitMachine();
    // speed as the user last left it; a measurement build runs flat out
    // unless built with -DPERF_SPEED=0 (1X)
#if PERF_TRACE
#ifdef PERF_SPEED
    machine->device.speedMode = PERF_SPEED;
#else
    machine->device.speedMode = FramePacer::SPEED_MAX;
#endif
#else
    machine->device.speedMode = Settings::LoadSpeed(FramePacer::SPEED_1X);
#endif
    pacer.SetSpeed((FramePacer::Speed)machine->device.speedMode);
    // menu trace and serial keys; a development build can start with them on
#ifndef SUP_DEBUG_DEFAULT
#define SUP_DEBUG_DEFAULT 0
#endif
    SupUI::debugOn = Settings::LoadDebug(SUP_DEBUG_DEFAULT);
    supervisor = new Supervisor(machine);

    // Disks that were mounted when the machine was switched: the switch
    // restarts the ESP32, so mount them again, once, and boot from them.
    bool remounted = false;
    for (int drive = 0; drive < 2; drive++)
    {
        String path = Settings::LoadDisk(drive);
        if (path.length() == 0)
            continue;
        Settings::SaveDisk(drive, "");
        remounted |= machine->Mount(path.c_str(), drive);
    }
    String hd = Settings::LoadHardDisk();
    if (hd.length() > 0)
    {
        Settings::SaveHardDisk("");
        remounted |= machine->MountHardDisk(hd.c_str());
    }
    if (remounted)
        machine->Reset();

    // a model that could not boot says why
    if (bootNote[0])
        supervisor->Open();

    // VGA16Controller converts every scanline in an ISR pinned to core 1,
    // where the Arduino loop runs; sharing that core cost the emulator about
    // a third of its speed. Core 0 has nothing else to do (no WiFi or BT),
    // so the emulator gets a task of its own there. The task never yields,
    // so core 0's idle-task watchdog has to go. The stack is 8K, as the
    // Arduino loop task that used to run all this had; it comes from
    // internal RAM, of which a IIe leaves little.
    Serial.printf("[mem] internal free %u, largest block %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    disableCore0WDT();
    if (xTaskCreatePinnedToCore(EmulationTask, "emulation", 8192, NULL, 1, NULL, 0) != pdPASS)
    {
        // slower, sharing core 1 with the VGA interrupt, but running
        Serial.println("[emu] no RAM for the emulation task: running in loop() on core 1");
        emulationInLoop = true;
    }
}

int frame = 0;
int fpscount = 0;
unsigned long fpsMillis = 0;
unsigned long heapCheckMillis = 0;

#if PERF_TRACE
// Measurement build (BuildConfig.h): mount Karateka 20 s after boot and log
// each drive motor change, which tools/capture-log.py turns into a load time.
static void PerfTrace()
{
    static bool mounted = false;
    static bool lastMotor = false;
    if (!mounted && millis() > 20000)
    {
        mounted = true;
        // the card has held the image in either place
        static const char* const paths[] = { "/karateka.nib", "/AppleII/karateka.nib" };
        bool ok = false;
        for (const char* path : paths)
            if (!ok && SD.exists(path))
                ok = machine->Mount(path, 0);
        if (!ok)
            listDir(SD, "/AppleII", 0);
        Serial.printf("[perf] mount %s at %lu\n", ok ? "ok" : "FAILED", millis());
        machine->Reset();
    }
    bool motor = machine->device.GetDiskMotorState();
    if (motor != lastMotor)
    {
        lastMotor = motor;
        Serial.printf("[perf] motor %s at %lu PC=%04X\n", motor ? "ON" : "off", millis(), machine->cpu.PC);
    }
}

static uint32_t perfRenderUs = 0, perfRenderMax = 0, perfRenders = 0;
#endif

// Sleeps the whole milliseconds, which lets core 0's idle task run, then
// spins the last part for an exact deadline (the FreeRTOS tick is 1 ms).
static void WaitMicros(uint32_t us)
{
    if (us == 0)
        return;
    int64_t until = esp_timer_get_time() + us;
    if (us > 2000)
        vTaskDelay(pdMS_TO_TICKS((us - 1000) / 1000));
    while (esp_timer_get_time() < until)
        ;
}

// F1 typed at a serial terminal, which opens the supervisor like the key
// does. Only looked for with the debug setting on and no Super Serial Card,
// whose data the port otherwise carries.
static bool SerialMenuKey()
{
    static SerialKeyDecoder decoder;
    static uint32_t lastByte = 0;
    // a sequence cut short long ago must not pair up with this byte
    decoder.Idle(millis() - lastByte);
    lastByte = millis();
    while (Serial.available())
        if (decoder.Feed((uint8_t)Serial.read()) == K_F1)
            return true;
    return false;
}

// One video frame: emulate (or run the supervisor), render, count FPS.
// Runs in EmulationTask on core 0, see setup().
static void RunFrame()
{
    bool drew = true;
    bool paced = false;
    if (SupUI::debugOn && machine->device.SerialSlot() == 0 && Serial.available() && SerialMenuKey())
        supervisor->Open();
    if (supervisor->Requested())
    {
        // The menu draws in the Apple's palette over the Apple's picture:
        // make sure both are on screen, even on the very first frame.
        machine->Render(vga, frame);
        supervisor->Run();               // returns when the menu is closed
        // a new schedule, rather than catching up on the time in the menu
        pacer.SetSpeed((FramePacer::Speed)machine->device.speedMode);
        fpsMillis = millis();
        fpscount = 0;
        return;
    }
    {
        // F4 changed the speed: apply it, and keep it for the next boot
        if (pacer.GetSpeed() != machine->device.speedMode)
        {
            pacer.SetSpeed((FramePacer::Speed)machine->device.speedMode);
#if !PERF_TRACE
            Settings::SaveSpeed(machine->device.speedMode);
#endif
        }

        FrameStep step = pacer.Begin((uint32_t)esp_timer_get_time(), machine->device.GetDiskMotorState());
        machine->Run((long long)FramePacer::CYCLES_PER_FRAME * step.frames);
        if (machine->device.supervisorRequested)
        {
            machine->device.supervisorRequested = false;
            supervisor->Open();
        }
        drew = step.render;
        paced = true;
        if (step.render)
        {
#if PERF_TRACE
            PerfTrace();
            uint32_t t0 = micros();
#endif
            machine->Render(vga, frame);
#if PERF_TRACE
            uint32_t dt = micros() - t0;
            perfRenderUs += dt;
            perfRenders++;
            if (dt > perfRenderMax)
                perfRenderMax = dt;
#endif
        }
#if PERF_TRACE
        else
            PerfTrace();
#endif
    }
    vga->show();

    if (frame++ > TARGET_FRAME)
        frame = 0;

    if(millis() - heapCheckMillis > 15000)
    {
        heapCheckMillis = millis();
        LOGF("Heap : %d / %d\n", ESP.getFreeHeap(), ESP.getHeapSize());
        LOGF("PSRam : %d / %d\n", ESP.getFreePsram(), ESP.getPsramSize());
    }

    // FPS counts frames actually drawn; speed is emulated cycles against a
    // real Apple II's 1.020484 MHz
    if (drew)
        fpscount++;
    unsigned long elapsed = millis() - fpsMillis;
    if (elapsed > 1000)
    {   /*
        static long long lastTick = 0;
        long long ticks = machine->cpu.tick - lastTick;
        lastTick = machine->cpu.tick;
        if (ticks < 0)                     // the CPU was reset
            ticks = 0;
        int percent = (int)(ticks * 100000LL / ((long long)elapsed * 1020484LL));
        fpsMillis = millis();
        LOGF("FPS : %d speed : %d%%\n", fpscount, percent);
        */
#if PERF_TRACE
        if (perfRenders)
            Serial.printf("[perf] render avg %u us max %u us\n",
                          (unsigned)(perfRenderUs / perfRenders), (unsigned)perfRenderMax);
            
        perfRenderUs = perfRenderMax = perfRenders = 0;
#endif
        // feeds the F2 on-screen counter (drawn by Apple2Device::Render)
        machine->device.fpsValue = fpscount;
        fpscount = 0;
    }

    if (paced)
        WaitMicros(pacer.End((uint32_t)esp_timer_get_time()));
}

static void EmulationTask(void*)
{
    for (;;)
        RunFrame();
}

void loop()
{
    if (emulationInLoop)
    {
        RunFrame();
        return;
    }
    // everything runs in EmulationTask; the Arduino loop task has no work
    vTaskDelete(NULL);
}
