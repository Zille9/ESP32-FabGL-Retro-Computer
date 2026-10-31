# TODO — Classic99 ESP32

Planned features for the TTGO VGA32 port, after version 0.1.0. Items are in
the order they were requested, not in priority order.

## Planned features

### 1. Standalone and ESP32 Bootloader builds

- [x] Add a script that builds the firmware in two forms:
  - **standalone**: flashed directly, as today;
  - **ESP32boot**: an image that can be loaded by
    [ESP32_Bootloader](https://github.com/ESP-WORKS/ESP32_Bootloader).
- [x] Select the build type in one place: a `config.h`, or another option such
      as a PlatformIO environment or build flag.
- [ ] Test on a board with ESP32_Bootloader flashed: the entry appears and
      flashes, a power cycle returns to the menu, an unchanged `version.txt`
      skips the reflash.

Notes:
- Done 2026-10-05: `src/config.h` (`BUILD_TARGET`, standalone by default),
  `[env:bootloader]` in `platformio.ini`, `tools/build.sh`. The bootloader build
  erases `otadata` first thing in `setup()`. `partitions.csv` is unchanged: the
  same application image runs from the bootloader's `ota_0`.
- Checked by disassembly (erase calls only in the bootloader build) and on the
  build output; not yet run under ESP32_Bootloader.

### 2. On-screen message when ROMs are missing

- [x] Show a clear message on the VGA output when the console ROMs are not on
      the SD card, instead of a blank or unreadable screen.
- [x] List which files are missing and where they belong
      (`/ti99/rom/994AROM.BIN`, `994AGROM.BIN`, `DISK.BIN`).
- [x] Do the same for a missing or unreadable SD card.

Notes:
- Done 2026-10-05: `bootErrorScreen()` in `menu.cpp`, called from `setup()` in
  `main.cpp` before the emulator starts. Red WARNING band, chip drawing, the
  three paths with OK/MISSING, any key restarts the board. The reason is also
  printed on serial (`BOOT ERROR: ...`).
- Checked over serial only; the look on a monitor is still to be confirmed.
  To see it without touching the card, build with
  `PLATFORMIO_BUILD_FLAGS="-DBOOT_ERROR_TEST=1"` (ROMs missing) or `=2` (no SD
  card).
- A missing `DISK.BIN` alone is still not fatal: the machine runs without disk
  drives and says nothing on screen. It is only listed (as optional) when the
  warning appears for the console ROMs.

### 3. Speech synthesiser

- [x] Emulate the TI Speech Synthesizer (TMS5220) and its speech ROM.

Notes:
- Done 2026-10-05: `src/speech.cpp` plus the MAME core in `src/speech/`
  (copied from `SpeechDll/`, BSD-3-Clause). Switched from Setup > Speech,
  saved as `speech=` in `config.txt`, OFF by default. Needs
  `/ti99/rom/SPCHROM.BIN`; without it the row shows `no ROM`.
- Costs about 1 % of frame time when ON.
- Original notes:
- Speech was out of scope for 0.1.0.
- Classic99 does speech through a Windows DLL (`SpeechDll/` in the parent
  directory), so the TMS5220 code has to be ported from there or taken from
  another source with a compatible licence.
- The speech ROM would be one more user-supplied file in `/ti99/rom/`.
- Sound output goes through FabGL's sound generator on GPIO25; speech has to
  be mixed with the TMS9919 output.
- Cost in CPU time is unknown. The emulation task uses about 75 to 80 % of
  each frame today.

### 4. WiFi

- [ ] Add WiFi support.
- [ ] Configure it from a text file on the SD card (SSID and password).
- [ ] Offer an access-point mode for configuration when no network is set up.
- [ ] Add the WiFi settings and status to the Supervisor menu (Setup).

Notes:
- WiFi needs roughly 50 to 70 KB of internal RAM. About 122 KB is free since
  the cartridge GROM and 32K expansion moved to PSRAM, so it should fit; this
  has not been tried.
- The CoCo project has a working WiFi manager to follow:
  `~/proyectos/ESP32TTGO/coco3/TTGO-VGA32-COCO/src/net/wifi_mgr.cpp` and the
  `sv_wifi.cpp` supervisor screen.
- The emulation task owns core 1. The network stack has to stay on core 0 with
  the keyboard and serial tasks.

### 5. Remote debug interface (MCP or API)

- [ ] Add a debug interface reachable over the network, as in the CoCo
      project, so the running machine can be inspected and driven remotely.
- [ ] Minimum set, matching the CoCo debug tools: status, pause and resume,
      reset, read and write memory, read and write CPU registers, screenshot.
- [ ] TI-specific additions: VDP RAM and registers, GROM address, key
      injection (already available as serial commands).

Notes:
- Depends on item 4 (WiFi).
- Reference implementation in the CoCo project: `src/net/debug_server.cpp`,
  `debug_rpc.cpp`, `png_writer.cpp`, and the `mcp-bridge/` directory.
- The serial console in `main.cpp` already has `screen`, `type`, `key`,
  `cart`, `disk` and `debug on|off`; the remote interface can share that
  command code.

## Carried over from 0.1.0

Things that work in principle but have not been tested:

- [ ] 360K disk images
- [ ] Assembling and running programs under Editor/Assembler (needs the E/A
      system disk files)
- [ ] Multicolor and text-bitmap video modes
- [ ] 5th-sprite flag behaviour
- [ ] Keyboard layouts typed on real non-US keyboards other than Spanish
      (Latam): Spain, French, Italian, Portuguese (Brazil)
