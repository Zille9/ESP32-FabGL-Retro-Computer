# CLAUDE.md

This repository is Classic99 (a Win32 TI-99/4A emulator by Mike Brent "Tursi"). **The active work here is the ESP32 port in `esp32/`.** Everything outside `esp32/` is upstream Classic99 and is **reference only**: read it to understand behaviour, never modify it, and do not try to build it (it is Win32/MSVC/DirectX code).

## Project: Classic99 for TTGO VGA32 v1.4

A standalone TI-99/4A on a TTGO VGA32 v1.4 board (ESP32-PICO-D4 rev 1.1, 4 MB PSRAM, 64-colour VGA, PS/2 keyboard, SD card, audio on GPIO25), using FabGL 1.0.9 (`~/Arduino/libraries/FabGL`) and PlatformIO.

Scope: console, cartridges, 32K expansion, PS/2 keyboard (Tab + arrows = joystick 1), VGA, sound, speech synthesiser (optional), disk images from SD, F12 menu. Out of scope: F18A/80 columns, AMS/SAMS, debugger, tape, RS232, TIPI.

Status and the ordered list of pending work are in `~/.claude/plans/indexed-moseying-creek.md`. Read it before starting.

## Commands

```bash
cd esp32
pio run                                             # build (standalone; -e bootloader for the ESP32_Bootloader build)
tools/build.sh [all|standalone|bootloader] [version]  # release images of both targets in build/
pio run -t upload --upload-port /dev/ttyACM0        # build + flash (port must be free)
pkill -f "tools/[s]ercmd.py"                        # free the port if a test session is still running
python3 tools/port_cpu.py                           # regenerate src/cpu9900.cpp (run from anywhere)

# serial test helper: sercmd.py <listen-seconds> [command | sleep:<seconds>] ...
~/.platformio/penv/bin/python tools/sercmd.py 20 sleep:7 "type  " sleep:3 "type 1" sleep:4 'type PRINT 1+1|'
```

**Diagnostics are off unless enabled**: the `emu:` stats, boot messages, `menu:`/`popup:`/`disks:` traces and `debug_write` only print when Setup > Debug log is ON (`debug=1` in `config.txt`). In a test, send `debug on` right after the boot sleep; it lasts for that session and is saved only if the F12 menu is then closed. New diagnostic output must use `DBG(...)` from `ti_types.h`, not `Serial.printf`; replies to serial commands stay on `Serial`.

Opening the serial port **resets the board**. Do a whole test in one `sercmd.py` call and start it with `sleep:7` (add ~5 s when a large cartridge is in the saved config). To capture the screen mid-test, run `sercmd.py` in the background and wait on its log.

Firmware serial commands (115200 baud): `ls`, `cart <n>`, `eject`, `reset`, `disk <1-3> [file]`, `screen` (dumps the TI screen as text), `debug on|off`, `speech [on|off]` (alone: state and sample/overflow/underrun/halt counters), `type <text>` (`|` = Enter, `~` = FCTN-9), `key f12|up|down|left|right|tab|enter|esc`. Every 5 s the firmware prints `emu: <fps>, <instr/s>, load <%>, PC=>xxxx`.

The board's VGA output can be viewed through the `hdmi-capture` MCP server (`capture_picture`) when it is enabled; otherwise use the `screen` serial command (text only) and ask the user about anything graphical. There are no unit tests; verification is on hardware (serial log + screen capture).

## Layout of `esp32/`

| File | Role |
|---|---|
| `platformio.ini`, `partitions.csv` | espressif32@6.10.0, esp32dev, PSRAM flags, 3 MB app partition |
| `tools/port_cpu.py` | **Generates `src/cpu9900.cpp`** from `../console/cpu9900.cpp` |
| `tools/sercmd.py` | Serial test helper |
| `tools/build.sh` | Builds both targets into `build/`: standalone merged/app images and `sdcard/Classic99/{firmware.bin,version.txt}` for ESP32_Bootloader; checks the `0xE9` magic and the 2816 KB `ota_0` limit |
| `src/config.h` | `BUILD_TARGET`: standalone (default) or ESP32_Bootloader (`-DBUILD_TARGET=1`, set by `[env:bootloader]`). The bootloader build erases `otadata` as the first statement of `setup()`; keep it first |
| `src/main.cpp` | Setup, memory allocation, input task (PS/2 → TI keys, F12 → menu), serial console task |
| `src/emu.cpp/.h` | Emulation task on core 1: per-instruction step, 60 Hz pacing, pause/reset |
| `src/cpu9900.cpp/.h` | TMS9900 CPU. `.cpp` is generated; `.h` is hand-written |
| `src/bus.cpp/.h` | Memory map and wait states, GROM, 9901/CRU, cartridge banking, key matrices, `debug_write` |
| `src/vdp9918.cpp/.h` | TMS9918A ports and per-scanline renderer into FabGL scanlines |
| `src/sound9919.cpp/.h` | TMS9919 sound chip as a `fabgl::WaveformGenerator`; adds the speech samples and clamps |
| `src/speech.cpp/.h` | Speech synthesiser glue (from Classic99 `Tiemul.cpp`): `>9000`/`>9400`, CPU halt on full FIFO, cycle-driven chip, ring buffer to the sound interrupt |
| `src/speech/` | MAME TMS5220 core + speech ROM reader copied from `../SpeechDll/` (BSD-3-Clause). Keep edits minimal: only `mame_wannabe.h` and a `talking()` accessor differ |
| `src/keyboard_ti.cpp/.h` | FabGL virtual keys → TI 8x8 matrix; keystroke injection (`tiType`) |
| `src/cart.cpp/.h` | Scans `/ti99/carts`, groups files by stem, loads C/D/G, 378 and 379 images |
| `src/ticc.cpp/.h` | TI disk controller: real DSR ROM + sector hook at PC `>40E8`; `.dsk` sector images |
| `src/menu.cpp/.h` | F12 Supervisor menu (FabGL Canvas): Cartridges, Disks, Setup, Reset, About; `config.txt` load/save |
| `src/kbd_layouts.cpp/.h` | PS/2 keyboard layouts (US, Spain, Latam, French, Italian, Brazil ABNT2) on top of FabGL's, dead keys disabled |
| `src/ti_types.h` | Types and SD paths |
| `src/version.h` | Firmware version (`CLASSIC99_ESP32_VERSION`, currently 0.2.0) and build date; shown in About and the boot banner. Bump it per release |

SD card (user-supplied, never in the repo):
```
/ti99/rom/994AROM.BIN  994AGROM.BIN  DISK.BIN  SPCHROM.BIN (optional, speech)
/ti99/carts/*          V9T9 names: xxxC/xxxD/xxxG, xxx8 (378), xxx9 (379), no suffix = 378
/ti99/disks/*.dsk      90K / 180K / 360K sector dumps
/ti99/config.txt       written by the menu (debug=, kbd=, speech=, cart=, dsk1=, dsk2=, dsk3=)
```

## Rules for this code

- **Never edit `src/cpu9900.cpp` by hand.** Change `tools/port_cpu.py` and regenerate. The script keeps Tursi's opcode bodies verbatim and only rewrites the decode table, status lookup, Win32 calls and IRAM placement.
- **Do not modify anything outside `esp32/`.** The one exception is the root `readme.md`, whose top section points visitors of the fork (`reyco2000/esp32-classic99`) to `esp32/README.md`; the original text is kept below it. Use `console/`, `disk/`, `keyboard/`, `addons/` only to look up how Classic99 behaves (main sources: `console/Tiemul.cpp`, `console/cpu9900.cpp`, `console/tivdp.cpp`, `console/sound.cpp`, `disk/TICCDisk.cpp`).
- **Licence.** Classic99's licence requires contacting the author before distributing derived works or ports. Keep the "Derived from Classic99 (C) Mike Brent" headers in every ported file. Never add TI ROMs, cartridge or disk images to `esp32/`.
- **SD card is mounted with format-on-fail off** (`FileBrowser::mountSDCard(false, ...)`). Keep it that way.
- Keep `-mfix-esp32-psram-cache-issue` in `platformio.ini`: the chip is rev 1.1 and has the PSRAM cache bug.

## Architecture notes that are easy to get wrong

- **Cores and tasks.** Core 1 runs only the emulation task. Input and serial console tasks run on core 0. Do not put anything user-facing in `loop()` or on core 1: when less than 2 ms of a frame is spare the emulation task's sleep rounds to 0 ticks and lower-priority tasks there starve.
- **Timing model** (from Classic99 `do1`): execute one instruction, take its cycle count, feed it to the 9901 timer and to `vdpAdvance()`, which steps scanlines (262 per frame, interrupt at line 219). Memory wait states and device cycle penalties in `bus.cpp` are part of the timing; keep them when changing the bus.
- **Memory placement.** Internal RAM: console ROM, console GROM (24K), scratchpad, VDP RAM, VGA framebuffer (~77 KB), task stacks. PSRAM: cartridge ROM banks, cartridge GROM (40K, `cartGrom`), 32K expansion, disk DSR, file lists. About 122 KB of internal RAM is free after start (largest block ~82 KB). Moving cartridge GROM and the 32K expansion to PSRAM cost no measurable load, so prefer PSRAM for new buffers; still create the emulation task before the others and check every allocation.
- **Hot path lives in IRAM** (`IRAM_ATTR`): CPU opcodes, bus access, VDP renderer. About 20 KB of IRAM remains.
- **FabGL raw pixels.** Within each 32-bit word of a scanline the pixel order is x+2, x+3, x, x+1 (`VGA_PIXELINROW` is `row[x^2]`). The renderer writes whole words; byte-by-byte writes were 3x slower.
- **Keyboard.** Key releases arrive with `F0`/`E0` prefixes; held keys are matched by physical key id. Column numbering follows Classic99's `KEYS[1]` table, not the TI schematic. While a program scans joystick 1, arrow keys stop mapping to FCTN+E/S/D/X.
- **Menu changes are saved on exit.** Leaving the F12 menu always writes `config.txt` with the current cart and disks, including ones set through serial commands during a test. After a serial test, restore the user's mounts before anyone opens the menu. Each menu selection prints a `menu:`, `disks:` or `popup:` line on serial, so the menu can be driven and checked with `key` commands.
- **Speech.** Off by default (Setup > Speech, `speech=` in `config.txt`); off means the synthesiser is not attached. The chip runs in the emulation task every 10000 CPU cycles (26/27/27 samples, 8 kHz) and its samples reach the sound interrupt through a ring buffer; FabGL does not clamp the mix, `sound9919.cpp` does. Do not use `status_read()` to poll the chip: it consumes a pending read-data byte; use `talking()`.
- **Disk.** No FD1771 emulation: the real TI disk DSR runs, and its sector routine is intercepted (`HandleTICCSector`), exactly as Classic99's `TICCDisk` does. The hook addresses are tied to the standard TI `DISK.BIN`.
- **Cartridge reset bank.** Banked carts start in the bank that holds the `>AA` header (first or last), per Classic99 `findXBbank`.
