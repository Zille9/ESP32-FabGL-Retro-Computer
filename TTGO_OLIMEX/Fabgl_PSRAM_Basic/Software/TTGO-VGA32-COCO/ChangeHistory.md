# Change History

Releases before the two most recent ones. The latest changes are in the
[Changelog section of the README](README.md#changelog).

### v0.12.1 — October 1, 2026

- **Fixed: black rectangle left on screen after closing the supervisor menu in
  CoCo 3 mode** (introduced in v0.12.0). The menu's clear-on-close could land
  after the screen had been redrawn, and unchanged lines were then never
  repainted.

### v0.12.0 — October 1, 2026

**CoCo 3 runs at full speed: 60 FPS in text and graphics modes.**

| CoCo 3 mode | v0.11.0 | v0.12.0 |
|---|---|---|
| BASIC prompt (32-column) | ~39 FPS | **60** |
| `WIDTH 40` | ~34 | **60** |
| `WIDTH 80` | ~24 | **~58** |
| `HSCREEN 2` (320×192, 16 colours) | ~20 | **60** |
| `HSCREEN 3` (640×192, 2 colours) | ~25 | **60** |
| `PMODE 4` | ~33 | **60** |

Measured on a TTGO VGA32 v1.4 with the built-in FPS counter. A program that
redraws the whole screen every frame can still dip below 60.

- **Faster GIME video.** Each video mode now has its own tight scanline
  renderer, instead of one routine that re-checked the mode for every four
  pixels. Decoding a 320×192 16-colour frame went from 32 ms to 5 ms.
- **Direct VGA output.** The renderer produces the final VGA bytes and the
  display code copies them 32 bits at a time, replacing a per-pixel lookup in a
  64 KB table (9.5 ms → 1.5 ms per frame).
- **Unchanged lines are skipped.** A scanline whose video memory and video
  settings are the same as in the previous frame is not redrawn.
- **Real-time frame pacing.** The emulator never runs faster than 60 frames per
  second, so cursor blink, `SOUND`/`PLAY` tempo and game speed match a real CoCo.
  The F5 FPS overlay therefore tops out at 60. Build with
  `FRAME_LIMIT_ENABLED 0` in `config.h` to remove the cap.
- **Debug API:** `GET /api/screenshot.png?fb=1` returns the picture actually on
  the VGA output (after scaling and borders).
- **Developer tools:** `tools/gime_render_test/` checks the new renderers
  against the original one on the host; `tools/perf/` holds the benchmark
  programs and measurement scripts.

Also in this release:

- **DriveWire is External-only.** The Internal DriveWire server (v0.11.0) and the
  planned embedded FujiNet were dropped: External mode against pyDriveWire, DW4
  or FujiNet-PC covers the same use. A board saved in Internal DW mode boots with
  the bus Off, so its mounted disks are served by the floppy controller again.
- **More free internal RAM:** mounted disk images no longer hold an open file
  (~5 KB → ~0.6 KB each), smaller debug-server and emulator task stacks, and a
  Debug Server On/Off setting (WiFi / Debug screen) that, when Off, never starts
  the server (+~7.8 KB).
- **Screenshots no longer drop WiFi** when memory is low (sent in 4 KB chunks).
- **Forget Credentials asks for confirmation.**
- **Debug API:** `/api/status` adds `fps`, `int8_*` (usable internal RAM),
  `srv_stack_free`, `loop_stack_free`, `osd_state`; new `POST /api/key` injects
  OSD keys.

### v0.11.0 — September 24, 2026

**Internal DriveWire mode — HDB-DOS with no PC and no WiFi.** See
[DriveWire / FujiNet Support](README.md#drivewire--fujinet-support-experimental) in the README.

- **`src/net/dw_server.*`** — a disk-only DriveWire 3/4 server built into the
  firmware (core 0). It serves the Disk Manager's drives 0–3 (the same `.DSK`
  images the floppy controller uses) as DriveWire drives 0–3; HDB-DOS
  `DRIVE 1`–`3` map to Disk Manager drives 1–3. `TIME` uses SNTP when WiFi is
  up. Survives CoCo resets mid-transaction.
- **Background flush** — DriveWire writes reach the SD card ~2 s after the
  last write. Flushes now write only the sectors that changed (floppy path
  too), instead of the whole image.
- **Debug API** — new `/api/disk` (list / mount / eject / flush), server
  counters in `/api/bus`, internal-RAM figures in `/api/status`.

### v0.10.0 — September 23, 2026

**Experimental DriveWire / FujiNet support (External mode).** See
[DriveWire / FujiNet Support](README.md#drivewire--fujinet-support-experimental) in the README.

- **`src/core/becker.*`** — an emulated Becker port (`$FF41`/`$FF42`) with
  lock-free ring buffers between the CPU core and a core-0 network task;
  flushes and reconnects cleanly across CoCo resets and link drops.
- **`src/net/dw_client.*` / `dw_bus.*`** — a TCP DriveWire client to an
  external server (pyDriveWire, DW4, FujiNet-PC), with settings (mode, host,
  port, HDB-DOS ROM variant) in NVS and a new `/api/bus` debug endpoint.
- **HDB-DOS ROM selection** — `hdbdw3bck.rom`/`hdbdw3bc3.rom` load in place of
  `disk11.rom` while the bus is enabled, with automatic fallback (and an OSD
  warning) if they're missing.
- **New Settings → DriveWire OSD screen** to configure and monitor the link.
- **`tools/dw_test_server.py`**, **`tools/dw_proxy.py`** — a minimal Python 3
  DriveWire test server and a logging TCP proxy, for testing without a full
  FujiNet-PC install.
- Closes the DriveWire socket cleanly before any supervisor-triggered restart,
  so an external server isn't left holding a dead connection.

### v0.9.0 — September 21, 2026

**ESP32_Bootloader support.** The emulator can now be launched from
[ESP32_Bootloader](https://github.com/ESP-WORKS/ESP32_Bootloader)'s SD-card menu,
so one TTGO VGA32 can hold several emulators and switch between them without a
USB cable. See [Running under ESP32_Bootloader](#running-under-esp32_bootloader-sd-card-menu).

- **New `BUILD_TARGET` switch in `config.h`**, defaulting to standalone. The
  bootloader image is selected with `-DBUILD_TARGET=1` on the command line, so
  both flavours build from identical sources and the USB build is untouched.

- **`otadata` is blanked at the top of `setup()`** in bootloader builds, before
  Serial, video or the SD probe. Without it the ESP32 would boot this app on
  every power-up and the menu would become unreachable; with it, the ROM falls
  back to the `factory` partition and the menu returns. The call compiles out
  entirely in standalone builds.

- **No partition-scheme change.** An ESP32 app image carries no partition table
  and both `0x10000` and `ota_0`'s `0x130000` are 64 KB-aligned, so the same app
  runs at either offset. `huge_app` stays the standalone scheme.

- **`tools/build_firmware.sh` now builds both images** and validates them:
  `0xE9` bare-image magic, the 2816 KB `ota_0` size limit, and a disassembly
  check that the `otadata` erase is present in the bootloader build and absent
  from the standalone one — the one failure that is otherwise silent.

### v0.81 — September 2, 2026

**CoCo 2 VDG colour accuracy.** Two bugs in the MC6847 path cancelled each other
out, so artifact-colour games looked correct while true 4-colour games did not.

- **PMODE 4 (RG6) is no longer rendered as PMODE 3 (CG6).** The VDG's GM0–GM2
  mode bits were being taken from the SAM's V0–V2. On real hardware they come
  from PIA1 PB4–PB6; the SAM's V bits only describe fetch geometry, which CG6
  and RG6 share (both `V=110`, 6144 bytes, 32 per row), so the SAM cannot tell
  the two apart. Every PMODE 4 screen was being decoded as 2bpp colour.

- **CG6 with CSS=1 now uses the MC6847's real palette** — buff, cyan, magenta,
  orange — in place of the NTSC artifact quad. Fixes titles such as *Pooyan*,
  which rendered as black/blue/orange/white.

- **NTSC artifact colour is now emulated where it actually occurs:** the 1bpp
  RG6 path, where the pixel clock runs at the colour-subcarrier rate and a
  composite TV decodes adjacent pixel *pairs* as black / blue / orange / white.
  Always on, matching XRoar's default for NTSC machines — games such as
  *Zaxxon* are drawn for it and are near-unreadable without it. The
  lower-resolution RG modes clock at half that rate and stay monochrome.

Verified against XRoar on hardware: the *Pooyan* title screen now matches the
reference pixel-for-pixel (0 of 49152 pixel codes differ), and injected RG6 and
CG6 test patterns produce the correct palette in each mode.
