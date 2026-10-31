# Apple II Video Pacing and Disk I/O Improvements — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run the emulated Apple II at true 60.05 Hz with every frame drawn, 1X by default with a MAX option, make the renderer and CPU core cheaper, and give the Disk II write-back to SD plus a ProDOS hard-disk card — ideas taken from AppleWin (`/home/pi/proyectos/sources/AppleWin/docs/`).

**Architecture:** A host-testable `FramePacer` decides, per loop pass, how many 17,030-cycle frames to emulate, whether to draw, and how long to wait; the `.ino` loop obeys it. Rendering loses its periodic full-screen repaint and draws hires through a lookup table. The Disk II marks the tracks it writes and flushes them to the image file when the motor stops; a new `HardDiskCard` in slot 7 serves 512-byte ProDOS blocks straight from SD.

**Tech Stack:** C++ (device: ESP32 Arduino core 2.0.17, `-std=gnu++11`; host tests: g++ `-std=gnu++17`), FabGL VGA16Controller, arduino-cli, esptool, pyserial.

**Spec:** the "Background" section below, which summarises the AppleWin review of 2026-09-29.

## Background (the review this plan implements)

- `RunFrame()` in `ESP32-VGA_AppleII_Emulator.ino` runs `17050 * 4` cycles and draws once, with no timing control. "FPS" therefore counts loop passes: 35 FPS × 68,200 cycles ≈ 2.3× real speed, and only one Apple frame in four is drawn. AppleWin runs 17,030 cycles per frame (65 × 262) at 60.05 Hz and draws every frame. It runs unthrottled only while the disk motor is on.
- `AppleVideo` repaints every cell every 30 passes (`flashCycle`), even though its caches already notice every change. `RenderHires` splits each 2-byte cell into bits and looks up one dot at a time. AppleWin's idealised renderer uses precomputed tables instead.
- The CPU passes its cycle budget as `long long&` to every bus access, which is 64-bit arithmetic on a 32-bit CPU. It also tests `enableLog` on every instruction.
- Disk II writes only change the PSRAM buffer and are lost on eject or power-off. AppleWin writes tracks back to the image.
- An access to `$CFFF` stops the Disk II motor. That is a leftover hack, and the Super Serial Card firmware now uses `$CFFF`.
- AppleWin's `Harddisk.cpp` gives ProDOS a fast block device. Today we reject `.hdv`, `.2mg` and 800K `.po` files.

## Global Constraints

- Line endings: every file in `src/AppleII/` and `src/Tools/Log.h` is **CRLF**; everything else (`.ino`, `src/Supervisor/`, `src/Tools/FileSystem.h`, `src/Tools/Settings.h`, `src/BuildConfig.h`, `tests/`, `tools/`, `docs/`) is **LF**. The Edit tool keeps CRLF; a new file made with Write is LF, so convert new `src/AppleII/` files with `perl -pi -e 's/\r?\n/\r\n/' <files>`. Before each commit check `git ls-files --eol <files>` and that `git diff --stat` shows only the lines you meant to change.
- Device code must compile as `gnu++11` (no C++14/17 features in `src/`). A `static constexpr` data member that is odr-used needs an out-of-class definition.
- Every new source file starts with the project header block used by the existing files (see any `src/AppleII/*.cpp`).
- Comments explain *why*, in the style of the surrounding code; identifiers and log prefixes follow the existing ones (`[disk]`, `[hdd]`, `[perf]`).
- Internal RAM is scarce (a IIe leaves little). New buffers total ≤ 17 KB of internal RAM: the 8 KB hires table, two 4 KB track buffers (`DskImage` and `DiskIICard::Flush`), a 512-byte block buffer and a 256-byte nibble table. After every board flash, compare the boot log line `[mem] internal free …, largest block …` with the Task 0 baseline. A drop of more than 18 KB is a failure.
- Never commit with `PERF_TRACE` set to 1. It is enabled only with `tools/build-dev.sh build/perf -DPERF_TRACE=1`.
- Flash with `tools/flash-parts.sh` (four parts at their offsets). The merged image at `0x0` wipes NVS.
- Board serial port: `/dev/ttyACM0`. The Super Serial Card must be **off** during log captures, because it mutes the log.
- Performance gate: at MAX speed, `speed %` and the Karateka motor-on time must not be more than 5% worse than the Task 0 baseline. Differences under 5% are build-layout noise, so don't bisect them.
- All host suites must pass before every commit: `for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done`.
- Work on branch `perf-and-disk-io`. Make one commit per task. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **SD card read-only, full or pulled while an image is mounted.** A disk write must turn the drive write-protected, so DOS says WRITE PROTECTED, and must not crash or silently drop data. The test is in Task 7 (`TestWriteFailureProtects`).
2. **A program writes a track that no longer decodes** (copy protection, a half-formatted track) to a `.dsk`. The sector image must stay intact, with a log line, while a `.nib` still takes the raw track. Tests are in Task 7 (`TestUndecodableTrackNotSaved`, `TestNibWriteBack`).
3. **Emulation slower than real time** (IIe double hires, heavy hires scrolling). Drawing is skipped at most 3 frames in a row, and the schedule never builds a backlog it later tries to catch up. Test: Task 1 (`TestOverload`).
4. **The microsecond clock wraps after about 71 minutes of uptime.** Pacing must not stall or burst. Test: Task 1 (`TestClockWrap`).
5. **A program leaves the drive motor on** (a crash in the middle of loading). Ctrl-Reset must stop it and take the machine out of fast-disk mode. Test: Task 8 (`TestResetLineStopsMotor`).

---

## File Structure

| File | Status | Responsibility |
|---|---|---|
| `src/BuildConfig.h` | modify | `PERF_TRACE` switch |
| `tools/build-dev.sh`, `tools/flash-parts.sh`, `tools/capture-log.py` | create | Dev build, NVS-safe flash, log capture + FPS/load-time summary |
| `src/AppleII/FramePacer.h/.cpp` | create | Pure frame-pacing logic (no Arduino) |
| `tests/host/pacer_test.cpp`, `run-pacer-tests.sh` | create | FramePacer tests |
| `ESP32-VGA_AppleII_Emulator.ino` | modify | Loop obeys the pacer, waits, logs FPS + speed %, PERF_TRACE hooks, hard-disk remount |
| `src/Tools/Settings.h` | modify | `LoadSpeed/SaveSpeed`, `LoadHardDisk/SaveHardDisk` |
| `src/AppleII/Apple2Device.h/.cpp` | modify | `speedMode`, F4 key, remove `$CFFF` motor hack, hosts `hdd7` |
| `src/AppleII/AppleVideo.h/.cpp` | modify | Overlay shows speed, no periodic repaint, hires through `HiresRender` |
| `src/AppleII/HiresRender.h/.cpp` | create | Hires lookup table |
| `tests/host/hires_test.cpp`, `run-hires-tests.sh` | create | LUT vs the old algorithm, exhaustively |
| `src/AppleII/AppleCpu.h/.cpp`, `AppleCpu65C02.cpp` | modify | 32-bit cycle budget, compile-time trace |
| `src/AppleII/DskImage.h/.cpp` | modify | `DenibblizeTrack`, `DenibblizeTrackToImage`, `.hdv`/`.2mg` types |
| `tests/host/dsk_test.cpp` | modify | Uses DskImage's decoder, tests the new functions |
| `src/Tools/FileSystem.h` | modify | `WriteAt` |
| `tests/host/shim/SD.h` | modify | `"r+"` mode, `seek`, positional `write`, `flush` |
| `src/AppleII/DiskIICard.h/.cpp` | modify | Dirty tracks, `Flush`, `ResetLine`, motor off on reset |
| `tests/host/disk_test.cpp`, `run-disk-tests.sh` | create | Disk II write-back and reset tests |
| `src/AppleII/AppleMem.cpp` | modify | `$CFFF` no longer calls into the device |
| `src/AppleII/HardDiskCard.h/.cpp` | create | ProDOS block device card + firmware |
| `tests/host/hdd_test.cpp`, `run-hdd-tests.sh` | create | Hard-disk card tests |
| `src/AppleII/Apple2Machine.h/.cpp` | modify | `MemoryBus`, slot 7 wiring, `MountHardDisk`, Ctrl-Reset stops the drive |
| `src/Supervisor/Supervisor.cpp` | modify | Hard-disk images mount without the drive picker; persisted across machine switch |
| `README.md`, `docs/ARCHITECTURE.md`, `docs/BUILD_AND_RELEASE.md` | modify | Documentation |

---

### Task 0: Measurement tooling and baseline

**Files:**
- Modify: `src/BuildConfig.h`
- Modify: `ESP32-VGA_AppleII_Emulator.ino` (includes; `RunFrame()` at lines 231-273)
- Create: `tools/build-dev.sh`, `tools/flash-parts.sh`, `tools/capture-log.py`
- Modify: this plan's "Results" table (bottom)

**Interfaces:**
- Produces: `PERF_TRACE` macro (0/1). Serial lines `[perf] mount ok at <ms>`, `[perf] motor ON|off at <ms> PC=<hex>`, `[perf] render avg <us> max <us>`, and `FPS : <n>`. Also the commands `tools/build-dev.sh <outdir> [defines…]`, `tools/flash-parts.sh <outdir> [port]` and `tools/capture-log.py [seconds] [port]`.

- [ ] **Step 1: Create the branch**

```bash
cd /home/pi/proyectos/ESP32-VGA_AppleII_Emulator
git checkout master && git pull --ff-only
git checkout -b perf-and-disk-io
```

- [ ] **Step 2: Add the `PERF_TRACE` switch** — in `src/BuildConfig.h`, insert before the final `#endif`:

```c
// PERF_TRACE 1: a measurement build. 20 s after boot it mounts
// /karateka.nib in drive 1, resets, and logs every Disk II motor change
// and the time spent drawing, which tools/capture-log.py turns into a load
// time. Build it with tools/build-dev.sh build/perf -DPERF_TRACE=1; never
// commit it switched on.
#ifndef PERF_TRACE
#define PERF_TRACE 0
#endif
```

- [ ] **Step 3: Add the trace hooks to the `.ino`**

Add `#include "src/BuildConfig.h"` after `#include "src/Tools/Bootloader.h"`. Then add this block directly above `static void RunFrame()`:

```cpp
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
        bool ok = machine->Mount("/karateka.nib", 0);
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
```

In `RunFrame()`, replace `machine->Render(vga, frame);` with:

```cpp
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
```

Inside the once-a-second block, after `LOGF("FPS : %d\n", fpscount);`, add:

```cpp
#if PERF_TRACE
        if (perfRenders)
            Serial.printf("[perf] render avg %u us max %u us\n",
                          (unsigned)(perfRenderUs / perfRenders), (unsigned)perfRenderMax);
        perfRenderUs = perfRenderMax = perfRenders = 0;
#endif
```

- [ ] **Step 4: Create `tools/build-dev.sh`** (LF, then `chmod +x`)

```bash
#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : build-dev.sh
#  Module : Development build into a scratch directory, optionally
#           with extra preprocessor defines (e.g. -DPERF_TRACE=1).
#           Release builds use tools/build-firmware.sh instead.
# ============================================================
#
# Usage:  tools/build-dev.sh <out dir> [-DNAME=VALUE ...]
#
set -euo pipefail

OUT="${1:?usage: tools/build-dev.sh <out dir> [-DNAME=VALUE ...]}"
shift
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FQBN="esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"

mkdir -p "$OUT"
if [ $# -gt 0 ]; then
  arduino-cli compile --clean --fqbn "$FQBN" \
    --build-property "compiler.cpp.extra_flags=$*" \
    --output-dir "$OUT" "$SKETCH_DIR"
else
  arduino-cli compile --clean --fqbn "$FQBN" --output-dir "$OUT" "$SKETCH_DIR"
fi
```

- [ ] **Step 5: Create `tools/flash-parts.sh`** (LF, then `chmod +x`)

```bash
#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : flash-parts.sh
#  Module : Flashes a build's four parts at their own offsets, so
#           NVS (saved machine, keyboard layout, speed) survives.
#           The merged release image written at 0x0 wipes it.
# ============================================================
#
# Usage:  tools/flash-parts.sh <arduino-cli output dir> [port]
#
set -euo pipefail

DIR="${1:?usage: tools/flash-parts.sh <build dir> [port]}"
PORT="${2:-/dev/ttyACM0}"
ESPTOOL="$(ls -1 "$HOME"/.arduino15/packages/esp32/tools/esptool_py/*/esptool.py | sort -V | tail -1)"
BOOT_APP0="$(ls -1 "$HOME"/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin | sort -V | tail -1)"

python3 "$ESPTOOL" --chip esp32 --port "$PORT" --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq keep --flash_size keep \
  0x1000  "$DIR"/ESP32-VGA_AppleII_Emulator.ino.bootloader.bin \
  0x8000  "$DIR"/ESP32-VGA_AppleII_Emulator.ino.partitions.bin \
  0xe000  "$BOOT_APP0" \
  0x10000 "$DIR"/ESP32-VGA_AppleII_Emulator.ino.bin
```

- [ ] **Step 6: Create `tools/capture-log.py`** (LF, then `chmod +x`)

```python
#!/usr/bin/env python3
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : capture-log.py
#  Module : Resets the board over USB serial, prints its log for a
#           while, then summarises it: FPS and speed % (the first 3 s
#           are dropped as boot noise) and, for a PERF_TRACE build,
#           the Karateka load time and total motor-on time.
# ============================================================
#
# Usage:  tools/capture-log.py [seconds=60] [port=/dev/ttyACM0]
import re
import sys
import time

import serial

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 60
port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM0"

s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = True                     # hold the ESP32 in reset
time.sleep(0.1)
s.rts = False

fps, speed = [], []
mount_ms = last_off_ms = on_since = None
motor_on_ms = 0
end = time.time() + secs
buf = b""
while time.time() < end:
    buf += s.read(4096)
    while b"\n" in buf:
        raw, buf = buf.split(b"\n", 1)
        line = raw.decode("ascii", "replace").rstrip()
        print(line, flush=True)
        m = re.search(r"FPS : (\d+)", line)
        if m:
            fps.append(int(m.group(1)))
        m = re.search(r"speed : (\d+)%", line)
        if m:
            speed.append(int(m.group(1)))
        m = re.search(r"\[perf\] mount ok at (\d+)", line)
        if m:
            mount_ms = int(m.group(1))
        m = re.search(r"\[perf\] motor ON at (\d+)", line)
        if m:
            on_since = int(m.group(1))
        m = re.search(r"\[perf\] motor off at (\d+)", line)
        if m:
            last_off_ms = int(m.group(1))
            if on_since is not None:
                motor_on_ms += last_off_ms - on_since
                on_since = None


def summary(name, values):
    steady = values[3:] or values
    if steady:
        print(f"== {name}: min {min(steady)} max {max(steady)} "
              f"mean {sum(steady) / len(steady):.1f} over {len(steady)} s")


summary("FPS", fps)
summary("speed %", speed)
if mount_ms is not None and last_off_ms is not None:
    print(f"== Karateka load: {last_off_ms - mount_ms} ms (mount -> last motor off), "
          f"motor on {motor_on_ms} ms")
```

- [ ] **Step 7: Build and flash the normal build, and record the idle baseline**

```bash
tools/build-dev.sh build/dev
tools/flash-parts.sh build/dev
tools/capture-log.py 30 | tee build/baseline-idle.log
```

Expected: the log shows the boot and `FPS : ~35` lines (the machine in Supervisor/BASIC; close the Supervisor if it opens). Copy the `== FPS` line and the `[mem] internal free …` line into the Results table.

- [ ] **Step 8: Build and flash the PERF build, and record the Karateka baseline** (the SD card must hold `/karateka.nib`; the repo copy is `data/karateka.nib`)

```bash
tools/build-dev.sh build/perf -DPERF_TRACE=1
tools/flash-parts.sh build/perf
tools/capture-log.py 80 | tee build/baseline-perf.log
```

Expected: `[perf] mount ok`, a run of `[perf] motor ON/off` lines, and `== Karateka load: ~21000-23000 ms`. Record the load time, motor-on time, FPS and `render avg/max` in the Results table.

- [ ] **Step 9: Put the normal build back on the board and commit**

```bash
tools/flash-parts.sh build/dev
git add src/BuildConfig.h ESP32-VGA_AppleII_Emulator.ino tools/build-dev.sh tools/flash-parts.sh tools/capture-log.py docs/appleII-improvements.md
git commit -m "Measurement tooling: PERF_TRACE build, dev build/flash scripts, log capture

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 1: FramePacer

**Files:**
- Create: `src/AppleII/FramePacer.h`, `src/AppleII/FramePacer.cpp` (CRLF)
- Create: `tests/host/pacer_test.cpp`, `tests/host/run-pacer-tests.sh` (LF)

**Interfaces:**
- Produces:
  - `struct FrameStep { int frames; bool render; };`
  - `class FramePacer` with `enum Speed : uint8_t { SPEED_1X=0, SPEED_MAX=1, SPEED_COUNT=2 }`
  - constants `CYCLES_PER_FRAME = 17030`, `FRAME_US = 16652`, `MAX_FRAMES_PER_STEP = 4`, `MAX_SKIP = 3`, `RESYNC_US`, `DISK_RENDER_US = 100000`
  - `void SetSpeed(Speed)`, `Speed GetSpeed() const`, `static const char* Label(Speed)` (returns `"1X"` or `"MX"`), `FrameStep Begin(uint32_t nowUs, bool diskBusy)` and `uint32_t End(uint32_t nowUs) const`

- [ ] **Step 1: Write the failing test** — `tests/host/pacer_test.cpp`:

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : pacer_test.cpp
 *  Module : Host-side FramePacer test: real-time pacing at 1X,
 *           flat out at MAX and while the disk runs, bounded
 *           frame skipping when overloaded, and the clock wrap.
 *           Driven by tests/host/run-pacer-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>

#include "FramePacer.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

struct Result
{
	uint32_t now;          // the clock after the last step
	int drawn;             // steps that drew
	int longestSkip;       // longest run of steps that did not
	int emulated;          // emulated frames run
};

// The emulation loop with a fake clock: each step takes workUs, then waits
// as End() says.
static Result Simulate(FramePacer& p, uint32_t start, int steps, uint32_t workUs, bool disk = false)
{
	Result r = { start, 0, 0, 0 };
	int skip = 0;
	for (int i = 0; i < steps; i++)
	{
		FrameStep s = p.Begin(r.now, disk);
		r.emulated += s.frames;
		r.now += workUs;
		if (s.render)
		{
			r.drawn++;
			skip = 0;
		}
		else if (++skip > r.longestSkip)
			r.longestSkip = skip;
		r.now += p.End(r.now);
	}
	return r;
}

static void TestLabels()
{
	expect(std::string(FramePacer::Label(FramePacer::SPEED_1X)) == "1X", "label 1X");
	expect(std::string(FramePacer::Label(FramePacer::SPEED_MAX)) == "MX", "label MX");
	FramePacer p;
	expect(p.GetSpeed() == FramePacer::SPEED_1X, "starts at 1X");
	p.SetSpeed((FramePacer::Speed)7);
	expect(p.GetSpeed() == FramePacer::SPEED_1X, "an unknown speed falls back to 1X");
}

static void TestRealTime()
{
	FramePacer p;
	Result r = Simulate(p, 1000, 600, 5000);
	uint32_t elapsed = r.now - 1000;
	expect(elapsed >= 599u * FramePacer::FRAME_US && elapsed <= 601u * FramePacer::FRAME_US,
	       "1X: 600 frames take 10 s");
	expect(r.drawn == 600, "1X: every frame drawn when there is time");
	expect(r.emulated == 600, "1X: one emulated frame per step");
}

static void TestOverload()
{
	// every step takes 25 ms, longer than a frame
	FramePacer p;
	Result r = Simulate(p, 0, 400, 25000);
	expect(r.longestSkip <= FramePacer::MAX_SKIP, "overload: at most MAX_SKIP frames skipped in a row");
	expect(r.drawn >= 400 / (FramePacer::MAX_SKIP + 1), "overload: at least one frame in MAX_SKIP+1 drawn");
	expect(r.now == 400u * 25000u, "overload: never waits while behind");
}

static void TestMaxSpeed()
{
	FramePacer p;
	p.SetSpeed(FramePacer::SPEED_MAX);
	Result r = Simulate(p, 0, 10, 3000);
	expect(r.emulated == 10 * FramePacer::MAX_FRAMES_PER_STEP, "MAX: four frames a step");
	expect(r.drawn == 10, "MAX: draws every step");
	expect(r.now == 30000, "MAX: never waits");
}

static void TestDiskBusy()
{
	// motor on at 1X: flat out, drawing about every DISK_RENDER_US
	FramePacer p;
	Result r = Simulate(p, 0, 100, 10000, true);
	expect(r.now == 1000000, "disk: never waits");
	expect(r.drawn >= 9 && r.drawn <= 11, "disk: draws about ten times a second");
	expect(r.emulated == 100 * FramePacer::MAX_FRAMES_PER_STEP, "disk: four frames a step");

	// motor off again: back to real time without a catch-up burst
	FrameStep s = p.Begin(r.now, false);
	expect(s.frames == 1 && s.render, "after disk: one frame, drawn");
	uint32_t wait = p.End(r.now + 2000);
	expect(wait > 0 && wait <= FramePacer::FRAME_US, "after disk: waits at most one frame");
}

static void TestClockWrap()
{
	FramePacer p;
	uint32_t start = 0xFFFFFFFFu - 5u * FramePacer::FRAME_US;
	Result r = Simulate(p, start, 20, 4000);
	expect(r.drawn == 20, "wrap: every frame drawn across the clock wrap");
	expect((uint32_t)(r.now - start) == 20u * FramePacer::FRAME_US, "wrap: 20 frames still take 20 frame times");
}

static void TestSpeedChangeRestarts()
{
	FramePacer p;
	p.SetSpeed(FramePacer::SPEED_MAX);
	Simulate(p, 0, 10, 4000);
	p.SetSpeed(FramePacer::SPEED_1X);                         // MAX -> 1X
	uint32_t late = 10u * 4000u + 50000;                      // arrives 50 ms later
	FrameStep s = p.Begin(late, false);
	expect(s.render && s.frames == 1, "speed change: first step drawn, one frame");
	expect(p.End(late + 4000) == FramePacer::FRAME_US - 4000, "speed change: the schedule restarts from now");
}

int main()
{
	TestLabels();
	TestRealTime();
	TestOverload();
	TestMaxSpeed();
	TestDiskBusy();
	TestClockWrap();
	TestSpeedChangeRestarts();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
```

- [ ] **Step 2: Create `tests/host/run-pacer-tests.sh`** (LF, `chmod +x`)

```bash
#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-pacer-tests.sh
#  Module : Builds and runs the host-side FramePacer test
#           (tests/host/pacer_test.cpp plus the real
#           src/AppleII/FramePacer.cpp). No shim needed.
# ============================================================
#
# Usage:  tests/host/run-pacer-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building pacer_test"
g++ -std=gnu++17 -O1 -Wall \
  -I "$ROOT/src/AppleII" \
  -o "$BUILD/pacer_test" \
  "$HERE/pacer_test.cpp" "$ROOT/src/AppleII/FramePacer.cpp"

"$BUILD/pacer_test"
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `tests/host/run-pacer-tests.sh`
Expected: compile error, `FramePacer.h: No such file or directory`.

- [ ] **Step 4: Write `src/AppleII/FramePacer.h`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : FramePacer.h
 *  Module : Frame pacing. The Apple II draws 60.05 frames a second,
 *           17030 CPU cycles each (65 cycles x 262 lines). At 1X the
 *           loop runs one frame's cycles per 16652 us and waits out
 *           the rest; MAX runs four and never waits.
 *           While the disk motor runs it never waits either, and
 *           draws only every DISK_RENDER_US, so loading stays fast.
 *           No Arduino dependency: tests/host/pacer_test.cpp.
 * ============================================================
*/

#ifndef FRAME_PACER_H
#define FRAME_PACER_H

#include <cstdint>

struct FrameStep
{
	int  frames;    // emulated frames to run now, CYCLES_PER_FRAME cycles each
	bool render;    // draw once they have run
};

class FramePacer
{
public:
	enum Speed : uint8_t { SPEED_1X = 0, SPEED_MAX = 1, SPEED_COUNT = 2 };

	static constexpr int32_t  CYCLES_PER_FRAME    = 17030;
	static constexpr uint32_t FRAME_US            = 16652;       // 17030 cycles at 1.020484 MHz
	static constexpr int      MAX_FRAMES_PER_STEP = 4;           // MAX speed and fast disk
	static constexpr int      MAX_SKIP            = 3;           // when late, draw at least every 4th frame
	static constexpr uint32_t RESYNC_US           = 8 * FRAME_US; // further behind: drop the backlog
	static constexpr uint32_t DISK_RENDER_US      = 100000;      // drawing rate while the disk runs

	FramePacer();

	void  SetSpeed(Speed s);
	Speed GetSpeed() const { return speed; }
	static const char* Label(Speed s);

	// Before emulating. nowUs is a free-running microsecond clock that may
	// wrap; only differences are used.
	FrameStep Begin(uint32_t nowUs, bool diskBusy);
	// After emulating and drawing: microseconds to wait before the next Begin.
	uint32_t End(uint32_t nowUs) const;

private:
	Speed    speed;
	bool     started;      // false: the next Begin starts the schedule afresh
	bool     paced;        // the current step waits for its deadline
	uint32_t deadline;     // when the next step is due
	uint32_t lastRender;
	int      skipped;      // frames not drawn in a row
};

#endif
```

- [ ] **Step 5: Write `src/AppleII/FramePacer.cpp`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : FramePacer.cpp
 *  Module : Frame pacing, see FramePacer.h.
 * ============================================================
*/

#include "FramePacer.h"

// gnu++11 (the ESP32 core) still wants these defined somewhere once odr-used
constexpr int32_t  FramePacer::CYCLES_PER_FRAME;
constexpr uint32_t FramePacer::FRAME_US;
constexpr int      FramePacer::MAX_FRAMES_PER_STEP;
constexpr int      FramePacer::MAX_SKIP;
constexpr uint32_t FramePacer::RESYNC_US;
constexpr uint32_t FramePacer::DISK_RENDER_US;

FramePacer::FramePacer()
	: speed(SPEED_1X), started(false), paced(false), deadline(0), lastRender(0), skipped(0)
{
}

void FramePacer::SetSpeed(Speed s)
{
	speed = (s < SPEED_COUNT) ? s : SPEED_1X;
	// a new speed starts its own schedule rather than catching up on the old one
	started = false;
}

const char* FramePacer::Label(Speed s)
{
	switch (s)
	{
		case SPEED_MAX: return "MX";
		default:        return "1X";
	}
}

FrameStep FramePacer::Begin(uint32_t now, bool diskBusy)
{
	FrameStep step;
	if (!started)
	{
		started = true;
		deadline = now;
		lastRender = now - DISK_RENDER_US;
		skipped = 0;
	}

	if (diskBusy || speed == SPEED_MAX)
	{
		// flat out; a paced step after this one starts its schedule from here
		paced = false;
		step.frames = MAX_FRAMES_PER_STEP;
		step.render = !diskBusy || (int32_t)(now - lastRender) >= (int32_t)DISK_RENDER_US;
		deadline = now;
	}
	else
	{
		paced = true;
		step.frames = 1;
		int32_t late = (int32_t)(now - deadline);
		if (late > (int32_t)RESYNC_US)
		{
			// too far behind to catch up: forget the backlog
			deadline = now;
			late = 0;
		}
		// more than a frame behind: skip drawing, but never for long
		step.render = !(late > (int32_t)FRAME_US && skipped < MAX_SKIP);
		skipped = step.render ? 0 : skipped + 1;
		deadline += FRAME_US;
	}

	if (step.render)
		lastRender = now;
	return step;
}

uint32_t FramePacer::End(uint32_t now) const
{
	if (!paced)
		return 0;
	int32_t wait = (int32_t)(deadline - now);
	return wait > 0 ? (uint32_t)wait : 0;
}
```

Convert both new files to CRLF: `perl -pi -e 's/\r?\n/\r\n/' src/AppleII/FramePacer.h src/AppleII/FramePacer.cpp`

- [ ] **Step 6: Run the test to verify it passes**

Run: `tests/host/run-pacer-tests.sh`
Expected: `PASS 25 checks` (the exact count may differ; there must be no `FAIL` lines).

- [ ] **Step 7: Commit**

```bash
git add src/AppleII/FramePacer.h src/AppleII/FramePacer.cpp tests/host/pacer_test.cpp tests/host/run-pacer-tests.sh
git commit -m "FramePacer: 60.05 Hz frame pacing with 1X/MAX and fast disk

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Paced main loop, speed setting, F4 key, overlay and log

**Files:**
- Modify: `src/Tools/Settings.h` (before `private:`)
- Modify: `src/AppleII/Apple2Device.h` (includes; public members near `fpsOverlay` at line 133)
- Modify: `src/AppleII/Apple2Device.cpp` (constructor near line 61; `UpdateKeyBoard` near line 492)
- Modify: `src/AppleII/AppleVideo.h`, `src/AppleII/AppleVideo.cpp` (overlay defines at lines 146-148, `RenderFpsOverlay`, the call in `Render`)
- Modify: `ESP32-VGA_AppleII_Emulator.ino` (`setup()`, `RunFrame()`, `EmulationTask`)
- Modify: `README.md` (key table near line 220; feature list near line 56; roadmap lines 312-314)

**Interfaces:**
- Consumes: `FramePacer`, `FrameStep` (Task 1); `PERF_TRACE` (Task 0).
- Produces: `uint8_t Apple2Device::speedMode` (a `FramePacer::Speed` value); `Settings::LoadSpeed(uint8_t def)`, `Settings::SaveSpeed(uint8_t)`; `AppleVideo::RenderFpsOverlay(int fps, const char* speed)`; the log line `FPS : <drawn frames> speed : <percent>%`.

- [ ] **Step 1: Settings** — add to `src/Tools/Settings.h` above `private:`:

```cpp
	// Emulation speed, a FramePacer::Speed: 0 = 1X, 1 = MAX.
	static uint8_t LoadSpeed(uint8_t def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		uint8_t s = p.getUChar("speed", def);
		p.end();
		return s > 1 ? def : s;
	}

	// false when NVS could not be written
	static bool SaveSpeed(uint8_t s)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putUChar("speed", s) == 1;
		p.end();
		return ok;
	}
```

- [ ] **Step 2: Device state and the F4 key**

In `Apple2Device.h` add `#include "FramePacer.h"` next to the other includes. Below `int  fpsValue;` add:

```cpp
	// FramePacer::Speed, cycled by F4; the main loop applies and saves it
	uint8_t speedMode;
```

In `Apple2Device.cpp`, after `fpsValue = 0;` in the constructor, add `speedMode = FramePacer::SPEED_1X;`. In `UpdateKeyBoard()`, after the `VK_F3` block, add:

```cpp
        if (vk == fabgl::VK_F4)
        {
            // emulation speed: 1X (a real Apple II) or MAX
            speedMode = (speedMode + 1) % FramePacer::SPEED_COUNT;
            InvalidateFpsOverlayRegion();
            continue;
        }
```

- [ ] **Step 3: Overlay shows the speed**

In `AppleVideo.cpp`, add `#include "FramePacer.h"`. Replace the three overlay defines (and the comment above them):

```cpp
// F2 overlay: 9 text cells in the top right corner ("999FPS 1X").
// FPS_COL is where the glyphs go; hires caches at a 2-byte (14 dot)
// granularity, so the invalidated span starts one byte column earlier.
#define FPS_COL			31
#define FPS_LEN			9
#define FPS_HIRES_COL	30
```

Replace `RenderFpsOverlay` with:

```cpp
void AppleVideo::RenderFpsOverlay(int fps, const char* speed)
{
	char text[FPS_LEN + 1];
	if (fps < 0)   fps = 0;
	if (fps > 999) fps = 999;
	snprintf(text, sizeof(text), "%3dFPS %s", fps, speed);

	// RenderFont paints the whole cell, so the black background still
	// covers whatever the emulator drew underneath
	for (int i = 0; i < FPS_LEN; i++)
		font.RenderFont(vga, (BYTE)text[i], SCREEN_X0 + (FPS_COL + i) * CELL_W, SCREEN_Y0,
		                false, A2_GREEN, A2_BLACK);
}
```

In `Render()`, change the call to `RenderFpsOverlay(dev.fpsValue, FramePacer::Label((FramePacer::Speed)dev.speedMode));`. In `AppleVideo.h`, change the declaration to `void RenderFpsOverlay(int fps, const char* speed);`.

- [ ] **Step 4: The paced loop** — in the `.ino`:

Add `#include <esp_timer.h>` and `#include "src/AppleII/FramePacer.h"` with the other includes. Add `static FramePacer pacer;` below `Supervisor *supervisor;`.

In `setup()`, right after `machine->InitMachine();`:

```cpp
    // speed as the user last left it; a measurement build always runs flat out
#if PERF_TRACE
    machine->device.speedMode = FramePacer::SPEED_MAX;
#else
    machine->device.speedMode = Settings::LoadSpeed(FramePacer::SPEED_1X);
#endif
    pacer.SetSpeed((FramePacer::Speed)machine->device.speedMode);
```

Add this helper above `RunFrame()`:

```cpp
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
```

Replace the body of `RunFrame()` from its start up to and including the FPS block with the following. Keep the heap-check block, and keep the `PERF_TRACE` render timing from Task 0 around `machine->Render`:

```cpp
static void RunFrame()
{
    bool drew = true;
    bool paced = false;
    if (supervisor->IsActive())
    {
        supervisor->Update();
        if (supervisor->IsActive())      // may have closed itself on ESC
            supervisor->Render(vga);
    }
    else
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
    {
        static long long lastTick = 0;
        long long ticks = machine->cpu.tick - lastTick;
        lastTick = machine->cpu.tick;
        if (ticks < 0)                     // the CPU was reset
            ticks = 0;
        int percent = (int)(ticks * 100000LL / ((long long)elapsed * 1020484LL));
        fpsMillis = millis();
        LOGF("FPS : %d speed : %d%%\n", fpscount, percent);
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
```

Also update the `.ino` file header's module comment, "loop() runs one video frame's worth of 6502 cycles, renders and presents", to: "the emulation task runs 6502 cycles as FramePacer schedules them (60.05 Hz at 1X), renders and presents".

- [ ] **Step 5: Host suites still pass, and the firmware builds**

Run: `for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done`
Expected: every suite prints `PASS`.
Run: `tools/build-dev.sh build/dev`
Expected: the build ends with `Sketch uses …` and no errors.

- [ ] **Step 6: Board check at 1X**

```bash
tools/flash-parts.sh build/dev
tools/capture-log.py 30
```

Expected: `FPS : 59`–`61` and `speed : 99`–`101%` at the BASIC prompt, and `== FPS: … mean ~60`. With F2 on, the corner shows `60FPS 1X`.

- [ ] **Step 7: Board check of F4 (needs the user at the keyboard)** — ask the user to press F4 once, wait 10 s, then press it again while `tools/capture-log.py 30` runs. Expected: MAX shows `speed :` at about the old 2.3× figure (≥ 220%) and the overlay reads `MX`; the second press returns to `speed : ~100%`, FPS ~60 and `1X`. Power-cycle the board: the speed chosen last is still set, because it was saved in NVS.

- [ ] **Step 8: PERF build check against the baseline** — `tools/build-dev.sh build/perf -DPERF_TRACE=1 && tools/flash-parts.sh build/perf && tools/capture-log.py 80`. Expected: Karateka loads, and the motor-on time and load time are within 5% of the baseline or better. Record the results, then reflash `build/dev`.

- [ ] **Step 9: README** — add `| **F4** | emulation speed: 1X (real Apple II) or MAX |` below the F3 row of the key table. Change the F2 feature bullet to say that the counter shows frames drawn per second plus the current speed. In the roadmap, tick `Improve FPS` and replace the `CPU speed control` item with `- [x] CPU speed control — F4 switches 1X / MAX, saved across power cycles`.

- [ ] **Step 10: Commit**

```bash
git add src/Tools/Settings.h src/AppleII/Apple2Device.h src/AppleII/Apple2Device.cpp src/AppleII/AppleVideo.h src/AppleII/AppleVideo.cpp ESP32-VGA_AppleII_Emulator.ino README.md docs/appleII-improvements.md
git commit -m "Pace emulation at 60.05 Hz: every frame drawn, F4 speed 1X/MAX

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Remove the periodic full-screen repaint

**Files:**
- Modify: `src/AppleII/AppleVideo.h` (member `flashCycle`, line 62)
- Modify: `src/AppleII/AppleVideo.cpp` (`Reset`, `Render`, `RenderText`, `RenderLores`, `RenderDoubleHires`, `RenderHires`)

**Interfaces:**
- Consumes: nothing new.
- Produces: `AppleVideo::Reset()` now leaves every cache entry dirty (-1), exactly like `InvalidateCells()`.

Why it is safe: the text cache key already includes the flash phase (the IIe glyph gets `0x80`, and the ][+ key gets `0x100` when inverse), so flashing cells redraw without help. Mode changes and the Supervisor already invalidate everything. The one real hole is `Reset()`, which marks lores and hires cells as drawn with value 0. Until now the periodic repaint hid that. This task fixes it.

- [ ] **Step 1: `Reset()` leaves every cell dirty** — replace the body of `AppleVideo::Reset()` with:

```cpp
void AppleVideo::Reset()
{
	// after a reset the screen still shows the old picture: every cell has
	// to be drawn again, whatever memory now holds
	InvalidateCells();
}
```

- [ ] **Step 2: Drop `flashCycle`** — delete `BYTE flashCycle;` from `AppleVideo.h`. In `AppleVideo.cpp`:
  - `Render()`: delete `if (++flashCycle == 30)` and `flashCycle = 0;`.
  - `RenderText()`: change `if (TextCache[line][c] != drawn || !flashCycle)` to `if (TextCache[line][c] != drawn)`.
  - `RenderLores()`: change `if (LoResCache[line][col] == glyph && flashCycle)` to `if (LoResCache[line][col] == glyph)`.
  - `RenderDoubleHires()`: change `if (HiResCache[line][col] == (int)dots && flashCycle)` to `if (HiResCache[line][col] == (int)dots)`.
  - `RenderHires()`: change `if (HiResCache[line][col] == word && flashCycle)` to `if (HiResCache[line][col] == word)`.

  Then check: `grep -n flashCycle src/AppleII/*` prints nothing.

- [ ] **Step 3: Build, and run host suites** — `for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done && tools/build-dev.sh build/dev`. Expected: all PASS; the build succeeds.

- [ ] **Step 4: Board check (visual, needs the user)** — flash `build/dev`. Ask the user to confirm on the monitor:
  1. The ][+ BASIC cursor still flashes. On the IIe, `INVERSE`/`FLASH` text in BASIC (`FLASH:PRINT "HI":NORMAL`) flashes.
  2. `HGR : HCOLOR=3 : HPLOT 0,0 TO 279,159`, then `TEXT`, then `HGR` again: no leftover pixels.
  3. Karateka title → story → fight: no stale areas.
  4. Open and close the Supervisor (F1, ESC): the screen comes back complete.
  5. Ctrl-F12 warm reset and a Supervisor RESET both leave a clean screen.
  6. F2 on and off over hires: the corner repaints.

  If the hdmi-capture MCP is connected, take a snapshot of steps 2–4 as well.

- [ ] **Step 5: PERF check** — `tools/build-dev.sh build/perf -DPERF_TRACE=1 && tools/flash-parts.sh build/perf && tools/capture-log.py 80`. Expected: `render max` lower than before (it no longer has the full-repaint spike). Record the results, then reflash `build/dev`.

- [ ] **Step 6: Commit**

```bash
git add src/AppleII/AppleVideo.h src/AppleII/AppleVideo.cpp docs/appleII-improvements.md
git commit -m "Video: drop the periodic full repaint; reset leaves every cell dirty

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Hires through a lookup table

**Files:**
- Create: `src/AppleII/HiresRender.h`, `src/AppleII/HiresRender.cpp` (CRLF)
- Create: `tests/host/hires_test.cpp`, `tests/host/run-hires-tests.sh` (LF)
- Modify: `src/AppleII/AppleVideo.cpp` (`hiresColor` table lines 82-90, `Create()`, `RenderHires()`)

**Interfaces:**
- Produces: `namespace HiresRender { extern const uint8_t COLOR[16]; void Init(); uint8_t DrawCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out); }`. `DrawCell` writes 14 framebuffer bytes (palette index × 0x11) and returns the cell's last dot, which becomes the next cell's `pbit`.

- [ ] **Step 1: Write the failing test** — `tests/host/hires_test.cpp`. The oracle is a verbatim copy of today's per-dot loop:

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : hires_test.cpp
 *  Module : Host-side hires renderer test: the lookup-table
 *           HiresRender::DrawCell against the per-dot loop
 *           AppleVideo::RenderHires used before it, for every pair
 *           of bytes and both left neighbours.
 *           Driven by tests/host/run-hires-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <string>

#include "HiresRender.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

// AppleVideo::RenderHires's inner loop as it was before the table, for one
// 14-dot cell. Returns the cell's last dot.
static uint8_t OldCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out)
{
	static const uint8_t hiresColor[16] =
	{
		0, 12, 3, 15, 0, 9, 6, 15,
		0, 3, 12, 15, 0, 6, 9, 15
	};
	uint16_t word = ((uint16_t)b1 << 8) | b0;
	uint8_t bits[16];
	for (int bit = 0; bit < 16; bit++)
		bits[bit] = (word >> bit) & 1;

	int x = 0;
	uint8_t colorSet = bits[7] * 4;
	uint8_t even = 0;
	int bit = 0;
	while (bit < 15)
	{
		if (bit == 7)
		{
			colorSet = bits[15] * 4;
			bit++;
		}
		out[x++] = hiresColor[even + colorSet + (bits[bit] << 1) + pbit] * 0x11;
		pbit = bits[bit++];
		even = even ? 0 : 8;
	}
	return pbit;
}

static void TestEveryCell()
{
	HiresRender::Init();
	int bad = 0;
	for (int pbit = 0; pbit < 2; pbit++)
		for (int w = 0; w < 0x10000; w++)
		{
			uint8_t want[14], got[16];
			memset(got, 0xAB, sizeof(got));
			uint8_t wantLast = OldCell(w & 0xFF, w >> 8, pbit, want);
			uint8_t gotLast = HiresRender::DrawCell(w & 0xFF, w >> 8, pbit, got);
			if (memcmp(want, got, 14) != 0 || wantLast != gotLast || got[14] != 0xAB)
			{
				if (bad < 5)
					printf("  mismatch: bytes %02X %02X, left dot %d\n", w & 0xFF, w >> 8, pbit);
				bad++;
			}
		}
	expect(bad == 0, "every cell matches the old renderer, and nothing past its 14 bytes is written");
}

int main()
{
	TestEveryCell();
	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
```

- [ ] **Step 2: Create `tests/host/run-hires-tests.sh`** (LF, `chmod +x`)

```bash
#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-hires-tests.sh
#  Module : Builds and runs the host-side hires renderer test
#           (tests/host/hires_test.cpp plus the real
#           src/AppleII/HiresRender.cpp). No shim needed.
# ============================================================
#
# Usage:  tests/host/run-hires-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building hires_test"
g++ -std=gnu++17 -O1 -Wall \
  -I "$ROOT/src/AppleII" \
  -o "$BUILD/hires_test" \
  "$HERE/hires_test.cpp" "$ROOT/src/AppleII/HiresRender.cpp"

"$BUILD/hires_test"
```

- [ ] **Step 3: Run to verify it fails** — `tests/host/run-hires-tests.sh`. Expected: `HiresRender.h: No such file or directory`.

- [ ] **Step 4: Write `src/AppleII/HiresRender.h`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HiresRender.h
 *  Module : Hires dots to framebuffer bytes through a lookup table.
 *           A dot's colour depends on the dot, the dot to its left,
 *           its column parity and its byte's bit 7; the table holds,
 *           for every byte, both parities and both left neighbours,
 *           the 7 framebuffer bytes that byte draws. No Arduino
 *           dependency: tests/host/hires_test.cpp.
 * ============================================================
*/

#ifndef HIRES_RENDER_H
#define HIRES_RENDER_H

#include <cstdint>

namespace HiresRender
{
	// Palette index for (odd dot * 8 + colour set * 4 + dot * 2 + left dot):
	// black, green, violet, white / black, orange, blue, white, and the
	// same with green/violet and orange/blue swapped on odd dots.
	extern const uint8_t COLOR[16];

	// Fills the table (8 KB). Call once before DrawCell.
	void Init();

	// Draws the cell of screen bytes b0 (left) and b1, whose first dot is on
	// an even column, into out[0..13], one framebuffer byte per dot. pbit is
	// the last dot of the cell to the left (0 at the screen edge). Returns
	// this cell's last dot: the next cell's pbit.
	uint8_t DrawCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out);
}

#endif
```

- [ ] **Step 5: Write `src/AppleII/HiresRender.cpp`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HiresRender.cpp
 *  Module : Hires lookup table, see HiresRender.h.
 * ============================================================
*/

#include "HiresRender.h"
#include <cstring>

namespace HiresRender
{

const uint8_t COLOR[16] =
{
	0, 12, 3, 15, 0, 9, 6, 15,
	0, 3, 12, 15, 0, 6, 9, 15
};

// [parity of the byte's first dot][left dot][byte] -> its 7 dots, padded
// to 8 so every entry is aligned
static uint8_t table[2][2][256][8];

void Init()
{
	for (int parity = 0; parity < 2; parity++)
		for (int left = 0; left < 2; left++)
			for (int b = 0; b < 256; b++)
			{
				int colorSet = (b >> 7) * 4;
				int pbit = left;
				int odd = parity ? 8 : 0;
				for (int i = 0; i < 7; i++)
				{
					int dot = (b >> i) & 1;
					table[parity][left][b][i] = COLOR[odd + colorSet + dot * 2 + pbit] * 0x11;
					pbit = dot;
					odd = odd ? 0 : 8;
				}
				table[parity][left][b][7] = 0;
			}
}

uint8_t DrawCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out)
{
	// b0's 7 dots start on an even column, so b1's start on an odd one
	memcpy(out, table[0][pbit][b0], 7);
	memcpy(out + 7, table[1][(b0 >> 6) & 1][b1], 7);
	return (b1 >> 6) & 1;
}

}
```

Convert to CRLF: `perl -pi -e 's/\r?\n/\r\n/' src/AppleII/HiresRender.h src/AppleII/HiresRender.cpp`

- [ ] **Step 6: Run to verify it passes** — `tests/host/run-hires-tests.sh`. Expected: `PASS 1 checks`.

- [ ] **Step 7: Use it in `AppleVideo`** — in `AppleVideo.cpp`:
  - add `#include "HiresRender.h"`;
  - delete the `hiresColor` table and its comment (lines 82-90). Keep the `A2_*` defines, which the text and overlay still use;
  - in `Create()`, add `HiresRender::Init();` after `font.Create();`;
  - replace `RenderHires()` with:

```cpp
void AppleVideo::RenderHires(Memory& mem, int page, int lines)
{
	WORD base = page * 0x2000;

	for (int line = 0; line < lines; line++)
	{
		// one framebuffer byte per Apple dot
		uint8_t* out = vga->row(SCREEN_Y0 + line) + (SCREEN_X0 >> 1);
		const BYTE* src = mem.ram + base + offsetHGR[line];

		// for every 14 horizontal dots
		for (int col = 0; col < SCREENTEXT_X; col += 2)
		{
			WORD word = ((WORD)src[col + 1] << 8) | src[col];
			if (HiResCache[line][col] == word)
				continue;
			HiResCache[line][col] = word;

			BYTE pbit = HiresRender::DrawCell(src[col], src[col + 1], previousBit[line][col], out + col * 7);

			// the next cell's first dot takes its colour from our last one
			if (col < 37 && previousBit[line][col + 2] != pbit)
			{
				previousBit[line][col + 2] = pbit;
				HiResCache[line][col + 2] = -1;
			}
		}
	}
}
```

Then update the `AppleVideo.cpp` file header's module line to mention that hires draws through `HiresRender`'s table.

- [ ] **Step 8: All suites, build, board** — `for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done && tools/build-dev.sh build/dev && tools/flash-parts.sh build/dev`. Ask the user to confirm that the Karateka title and story screens look the same as `pictures/v030-karateka-*.png`, with the same colours and fringes. Check that the `[mem] internal free` boot line has dropped by no more than about 8 KB against the baseline.

- [ ] **Step 9: PERF check** — PERF build as in Task 2 Step 8. Expected: `render avg/max` during the Karateka hires screens is lower than after Task 3. Record the results.

- [ ] **Step 10: Commit**

```bash
git add src/AppleII/HiresRender.h src/AppleII/HiresRender.cpp src/AppleII/AppleVideo.cpp tests/host/hires_test.cpp tests/host/run-hires-tests.sh docs/appleII-improvements.md
git commit -m "Video: draw hires through a lookup table, checked against the old renderer

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: 32-bit cycle budget and compile-time CPU trace

**Files:**
- Modify: `src/AppleII/AppleCpu.h`, `src/AppleII/AppleCpu.cpp`, `src/AppleII/AppleCpu65C02.cpp`

**Interfaces:**
- Produces: `typedef int32_t Cycles;` in `AppleCpu.h`; `int CPU::Run(Memory& mem, Cycles cycle)`. `CPU::tick` and `CurrentTick()` stay `long long`, because they count from reset and would overflow 32 bits after 35 minutes. `CPU_TRACE` macro (default 0).

- [ ] **Step 1: The type** — in `AppleCpu.h`, after the `#include` lines, add:

```cpp
// A Run() call's cycle budget and what is left of it. A budget is at most a
// few hundred thousand cycles, so 32 bits are plenty; the ESP32 is a 32-bit
// CPU and a 64-bit count cost it on every bus access. The running total
// since reset (tick) stays 64-bit.
typedef int32_t Cycles;

// CPU_TRACE 1: print every instruction when enableLog is set (host debugging)
#ifndef CPU_TRACE
#define CPU_TRACE 0
#endif
```

- [ ] **Step 2: Mechanical rename of the reference parameters** (sed keeps CRLF intact):

```bash
sed -i -E 's/long long ?& ?cycle/Cycles\& cycle/g' src/AppleII/AppleCpu.h src/AppleII/AppleCpu.cpp src/AppleII/AppleCpu65C02.cpp
grep -n "long long" src/AppleII/AppleCpu.h src/AppleII/AppleCpu.cpp src/AppleII/AppleCpu65C02.cpp
```

Expected remaining hits: `tick`, `CurrentTick`, `runCycle`, `runStartTick`, `runBudget`, `Run(`, the locals in `Run`, and the `fwrite`/`fread` of `tick`.

- [ ] **Step 3: Hand edits**
  - `AppleCpu.h`: `long long* runCycle;` → `Cycles* runCycle;`, `long long  runBudget;` → `Cycles runBudget;`, and `int Run(Memory& mem, long long cycle);` → `int Run(Memory& mem, Cycles cycle);`. Keep `runStartTick` and `tick` as `long long`.
  - `AppleCpu.cpp`: `int CPU::Run(Memory &mem, long long _cycle)` → `int CPU::Run(Memory &mem, Cycles _cycle)`; inside it, `long long cycle = _cycle;` → `Cycles cycle = _cycle;` and `long long prevcycle = cycle;` → `Cycles prevcycle = cycle;`.
  - `AppleCpu.cpp`: wrap the `if (enableLog) { … }` block after `lastInst = inst;` in `#if CPU_TRACE` / `#endif`.
  - `Apple2Machine.cpp`: in `Run()`, change `cpu.Run(mem, cycle);` to `cpu.Run(mem, (Cycles)cycle);`.

- [ ] **Step 4: CPU suites** — `tests/host/run-cpu-tests.sh`. Expected: both Klaus Dormann suites `PASS`. Then run all suites, and build: `tools/build-dev.sh build/dev`. Expected: no warnings about narrowing in `AppleCpu*`.

- [ ] **Step 5: PERF check** — PERF build (MAX speed). Expected: `speed %` at the BASIC prompt before the mount and the Karateka motor-on time are equal or better (≤ 5% worse counts as noise; a larger loss means revert this task and record why). Record the results.

- [ ] **Step 6: Commit**

```bash
git add src/AppleII/AppleCpu.h src/AppleII/AppleCpu.cpp src/AppleII/AppleCpu65C02.cpp src/AppleII/Apple2Machine.cpp docs/appleII-improvements.md
git commit -m "CPU: 32-bit cycle budget, instruction trace compiled out

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Denibblize tracks in `DskImage`

**Files:**
- Modify: `src/AppleII/DskImage.h`, `src/AppleII/DskImage.cpp`
- Modify: `tests/host/dsk_test.cpp`

**Interfaces:**
- Produces:
  - `int DskImage::DenibblizeTrack(const uint8_t* nib, int track, uint8_t sectors[SECTORS][SECTOR_BYTES])` returns a bitmask of the physical sectors decoded (`0xFFFF` means all).
  - `bool DskImage::DenibblizeTrackToImage(const uint8_t* nib, int track, SectorOrder order, uint8_t* out)` writes `TRACK_BYTES` in image order and returns false, leaving `out` untouched, unless all 16 sectors decode. It is not reentrant (static scratch).
  - `ImageType` gains `IMAGE_HDV` (`.hdv`) and `IMAGE_2MG` (`.2mg`).

- [ ] **Step 1: Write the failing tests** — in `dsk_test.cpp`:
  - delete the test's own decoder: `readTable`, `BuildReadTable()`, `struct TrackReader` and `DecodeTrack()`, plus the `BuildReadTable();` call in `main`;
  - replace every call `DecodeTrack(` with `DenibblizeTrack(`;
  - in `TestTypes()` add:

```cpp
	expect(TypeFromPath("/HD/Total.HDV") == IMAGE_HDV, "type .HDV");
	expect(TypeFromPath("/x.2mg") == IMAGE_2MG, "type .2mg");
```

  - add this test and call it from `main` after `TestSynthetic(...)`:

```cpp
static void TestDenibblizeToImage()
{
	std::vector<uint8_t> image(IMAGE_BYTES);
	for (int i = 0; i < IMAGE_BYTES; i++)
		image[i] = (uint8_t)(i * 11 + (i >> 9));
	std::vector<uint8_t> nib = NibblizeLikeTheCard(image.data(), ORDER_PRODOS);

	std::vector<uint8_t> track(TRACK_BYTES);
	expect(DenibblizeTrackToImage(nib.data() + 3 * NIB_TRACK, 3, ORDER_PRODOS, track.data()),
	       "track 3 decodes to image order");
	expect(memcmp(track.data(), image.data() + 3 * TRACK_BYTES, TRACK_BYTES) == 0,
	       "track 3 matches the image");

	// break one data nibble: that sector fails its read, so the whole
	// track is refused and the output left alone
	uint8_t* t0 = nib.data();
	int at = -1;
	for (int i = 0; i + 2 < NIB_TRACK; i++)
		if (t0[i] == 0xD5 && t0[i + 1] == 0xAA && t0[i + 2] == 0xAD) { at = i + 10; break; }
	expect(at > 0, "found a data field on track 0");
	t0[at] = 0x00;                               // not a valid disk nibble
	std::vector<uint8_t> untouched(TRACK_BYTES, 0xEE);
	expect(!DenibblizeTrackToImage(t0, 0, ORDER_PRODOS, untouched.data()), "a broken sector refuses the track");
	bool same = true;
	for (uint8_t b : untouched)
		same &= (b == 0xEE);
	expect(same, "a refused track leaves the output untouched");
}
```

- [ ] **Step 2: Run to verify it fails** — `tests/host/run-dsk-tests.sh`. Expected: compile errors: `DenibblizeTrack`, `DenibblizeTrackToImage`, `IMAGE_HDV` and `IMAGE_2MG` not declared.

- [ ] **Step 3: Header** — in `DskImage.h`, change the enum to `enum ImageType { IMAGE_NONE, IMAGE_NIB, IMAGE_DOS, IMAGE_PRODOS, IMAGE_HDV, IMAGE_2MG };`. Update the `TypeFromPath` comment to `// From the file extension, case-insensitive: .nib, .dsk/.do, .po, .hdv, .2mg.` and add:

```cpp
	// Reads a nibble track back the way RWTS does, as a ring: every sector
	// whose address field names 'track' and whose data field checksums, into
	// sectors[physical sector]. Returns a bitmask of the physical sectors
	// found (0xFFFF: all 16).
	int DenibblizeTrack(const uint8_t* nib, int track, uint8_t sectors[SECTORS][SECTOR_BYTES]);

	// The whole track back into image order, TRACK_BYTES at out. False, with
	// out untouched, unless all 16 sectors decode. Not reentrant.
	bool DenibblizeTrackToImage(const uint8_t* nib, int track, SectorOrder order, uint8_t* out);
```

- [ ] **Step 4: Implementation** — in `DskImage.cpp`, in `TypeFromPath` add before `return IMAGE_NONE;`:

```cpp
	if (EndsWith(path, ".hdv"))
		return IMAGE_HDV;
	if (EndsWith(path, ".2mg"))
		return IMAGE_2MG;
```

and before the namespace's closing `}` add (this is the decoder moved from `dsk_test.cpp`):

```cpp
// disk nibble -> 6-bit value, 0xFF for a byte that is not a disk nibble
static uint8_t readTable[256];
static bool readTableBuilt = false;

static void BuildReadTable()
{
	if (readTableBuilt)
		return;
	memset(readTable, 0xFF, sizeof(readTable));
	for (int i = 0; i < 64; i++)
		readTable[WRITE_TABLE[i]] = i;
	readTableBuilt = true;
}

// A track read as a ring, from pos on
struct TrackReader
{
	const uint8_t* nib;
	int pos;
	uint8_t Next() { uint8_t v = nib[pos]; pos = (pos + 1) % NIB_TRACK; return v; }
	uint8_t Get44() { uint8_t a = Next(); uint8_t b = Next(); return ((a << 1) | 1) & b; }
	bool At(uint8_t a, uint8_t b, uint8_t c) const
	{
		return nib[pos] == a && nib[(pos + 1) % NIB_TRACK] == b && nib[(pos + 2) % NIB_TRACK] == c;
	}
};

int DenibblizeTrack(const uint8_t* nib, int track, uint8_t sectors[SECTORS][SECTOR_BYTES])
{
	BuildReadTable();
	int found = 0;
	for (int start = 0; start < NIB_TRACK; start++)
	{
		TrackReader r = { nib, start };
		if (!r.At(0xD5, 0xAA, 0x96))
			continue;
		r.Next(); r.Next(); r.Next();
		uint8_t vol = r.Get44(), trk = r.Get44(), sec = r.Get44(), sum = r.Get44();
		if ((vol ^ trk ^ sec) != sum || trk != track || sec >= SECTORS)
			continue;

		// the data field follows within a few dozen nibbles
		int n = 0;
		while (n < 64 && !r.At(0xD5, 0xAA, 0xAD))
		{
			r.Next();
			n++;
		}
		if (n == 64)
			continue;
		r.Next(); r.Next(); r.Next();

		uint8_t six[342];
		uint8_t prev = 0;
		bool bad = false;
		for (int i = 0; i < 342; i++)
		{
			uint8_t v = readTable[r.Next()];
			if (v == 0xFF) { bad = true; break; }
			prev ^= v;
			six[i] = prev;
		}
		if (bad || (readTable[r.Next()] ^ prev) != 0)
			continue;

		for (int i = 0; i < SECTOR_BYTES; i++)
		{
			int aux = six[i % 86] >> (2 * (i / 86));
			sectors[sec][i] = (six[86 + i] << 2) | ((aux & 1) << 1) | ((aux >> 1) & 1);
		}
		found |= 1 << sec;
	}
	return found;
}

bool DenibblizeTrackToImage(const uint8_t* nib, int track, SectorOrder order, uint8_t* out)
{
	// static: 4K is too much for the emulation task's 8K stack
	static uint8_t sectors[SECTORS][SECTOR_BYTES];
	if (DenibblizeTrack(nib, track, sectors) != 0xFFFF)
		return false;
	for (int p = 0; p < SECTORS; p++)
		memcpy(out + LogicalSector(p, order) * SECTOR_BYTES, sectors[p], SECTOR_BYTES);
	return true;
}
```

Also extend the file header's module comment with: "Also reads tracks back into sectors, for writing a changed disk back to its image."

- [ ] **Step 5: Run to verify it passes** — `tests/host/run-dsk-tests.sh`. Expected: `PASS`. Run all suites.

- [ ] **Step 6: Commit**

```bash
git add src/AppleII/DskImage.h src/AppleII/DskImage.cpp tests/host/dsk_test.cpp
git commit -m "DskImage: denibblize tracks back to sectors; .hdv/.2mg image types

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Disk II write-back to the image file

**Files:**
- Modify: `src/Tools/FileSystem.h`
- Modify: `tests/host/shim/SD.h`
- Modify: `src/AppleII/DiskIICard.h`, `src/AppleII/DiskIICard.cpp`
- Create: `tests/host/disk_test.cpp`, `tests/host/run-disk-tests.sh`

**Interfaces:**
- Consumes: `DskImage::DenibblizeTrackToImage`, `DskImage::ImageType` (Task 6).
- Produces:
  - `static bool FileSystem::WriteAt(const char* path, size_t offset, const uint8_t* data, size_t len)`
  - `void DiskIICard::Flush(int drive)` (public)
  - `FloppyDrive::type` (`DskImage::ImageType`) and `FloppyDrive::dirtyTracks` (`uint64_t`, bit per track)
  - the shim gains `SD.open(path, "r+")`, `File::seek(size_t)`, positional `File::write`, `File::flush()`

- [ ] **Step 1: Shim support** — in `tests/host/shim/SD.h`, replace the `File` constructor, `write`, and `HostSD::open`, and add `seek` and `flush`:

```cpp
	// truncate: "w" empties the file; atEnd: "a" writes after what is there.
	// "r+" does neither: it reads and overwrites in place.
	File(const char* p, bool write, bool truncate, bool atEnd)
		: path(p), opened(true), writing(write), pos(0)
	{
		if (truncate)
			HostFiles()[path].clear();
		if (atEnd)
			pos = HostFiles()[path].size();
	}
```

```cpp
	size_t write(const unsigned char* buf, size_t len)
	{
		if (!opened || !writing)
			return 0;
		std::string& data = HostFiles()[path];
		if (data.size() < pos + len)
			data.resize(pos + len);
		data.replace(pos, len, (const char*)buf, len);
		pos += len;
		return len;
	}

	bool seek(size_t p)
	{
		if (!opened)
			return false;
		pos = p;
		return true;
	}

	void flush() {}
```

```cpp
	File open(const char* path, const char* mode = FILE_READ)
	{
		bool update = (mode[0] == 'r' && mode[1] == '+');
		bool write = update || mode[0] == 'w' || mode[0] == 'a';
		// reading, and updating in place, need the file to exist already
		if ((!write || update) && !HostFiles().count(path))
			return File();
		return File(path, write, mode[0] == 'w', mode[0] == 'a');
	}
```

Update the shim header comment to mention "r+", seek and positional writes. Run `tests/host/run-ssc-tests.sh`. Expected: still `PASS`, because the SSC's print capture uses `"a"`/`"w"`.

- [ ] **Step 2: `FileSystem::WriteAt`** — add to `src/Tools/FileSystem.h` after `ReadFile`:

```cpp
    // Overwrites len bytes at offset in an existing file without truncating
    // it. False when the file cannot be opened for update (missing, card
    // read-only or gone) or not every byte was written.
    static bool WriteAt(const char *path, size_t offset, const uint8_t *data, size_t len)
    {
        File file = SD.open(path, "r+");
        if (!file)
        {
            LOGF("- failed to open file for update: %s\n", path);
            return false;
        }
        bool ok = file.seek(offset) && file.write(data, len) == len;
        file.close();
        return ok;
    }
```

- [ ] **Step 3: Write the failing test** — `tests/host/disk_test.cpp`:

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : disk_test.cpp
 *  Module : Host-side Disk II card test: tracks written through
 *           the data latch reach the image file on the (shimmed) SD
 *           card when the motor stops or the disk is ejected, for
 *           .dsk, .po and .nib; undecodable tracks and failing
 *           writes are handled; reset stops the motor.
 *           Driven by tests/host/run-disk-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>
#include <vector>

#include "DiskIICard.h"

using namespace DskImage;

// registers, as Apple2Device::SoftSwitch passes them for slot 6
#define R_MOTOR_OFF 0x8
#define R_MOTOR_ON  0x9
#define R_SHIFT     0xC
#define R_LOAD      0xD
#define R_READ      0xE
#define R_WRITE     0xF

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

static std::string SectorImage(int seed)
{
	std::string s(IMAGE_BYTES, '\0');
	for (int i = 0; i < IMAGE_BYTES; i++)
		s[i] = (char)((i * 13 + seed) ^ (i >> 8));
	return s;
}

// Writes one whole track through the data latch the way RWTS does - load
// the latch, then shift it out - once round from wherever the head is.
static void WriteTrack(DiskIICard& card, const uint8_t* nib)
{
	card.Io(R_WRITE, 0, true);
	for (int i = 0; i < NIB_TRACK; i++)
	{
		card.Io(R_LOAD, nib[i], true);
		card.Io(R_SHIFT, 0, false);
	}
	card.Io(R_READ, 0, false);
}

// Track 0 of 'from', nibblized in the given order
static std::vector<uint8_t> Track0(const std::string& from, SectorOrder order)
{
	std::vector<uint8_t> nib(NIB_TRACK);
	NibblizeTrack((const uint8_t*)from.data(), 0, order, nib.data());
	return nib;
}

static void TestSectorWriteBack(const char* path, SectorOrder order, const char* label)
{
	HostFiles().clear();
	std::string a = SectorImage(1), b = SectorImage(2);
	HostFiles()[path] = a;
	DiskIICard card;
	expect(card.Mount(path, 0), std::string(label) + ": mounts");

	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(b, order).data());
	expect(HostFiles()[path] == a, std::string(label) + ": nothing written while the motor runs");
	card.Io(R_MOTOR_OFF, 0, false);

	const std::string& now = HostFiles()[path];
	expect(now.size() == (size_t)IMAGE_BYTES, std::string(label) + ": file keeps its size");
	expect(now.compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0, std::string(label) + ": track 0 saved");
	expect(now.compare(TRACK_BYTES, std::string::npos, a, TRACK_BYTES, std::string::npos) == 0,
	       std::string(label) + ": other tracks untouched");
}

static void TestNibWriteBack()
{
	HostFiles().clear();
	HostFiles()["/t.nib"] = std::string(NIB_BYTES, (char)0xFF);
	DiskIICard card;
	expect(card.Mount("/t.nib", 0), ".nib: mounts");
	// a raw track, not a DOS one: a .nib keeps it anyway
	std::vector<uint8_t> raw(NIB_TRACK);
	for (int i = 0; i < NIB_TRACK; i++)
		raw[i] = 0x96 + (i % 0x69);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, raw.data());
	card.Io(R_MOTOR_OFF, 0, false);
	const std::string& now = HostFiles()["/t.nib"];
	expect(now.compare(0, NIB_TRACK, std::string(raw.begin(), raw.end())) == 0, ".nib: raw track saved");
	expect(now.size() == (size_t)NIB_BYTES, ".nib: file keeps its size");
}

static void TestReadsDoNotWrite()
{
	HostFiles().clear();
	std::string a = SectorImage(3);
	HostFiles()["/r.dsk"] = a;
	DiskIICard card;
	card.Mount("/r.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	for (int i = 0; i < 3 * NIB_TRACK; i++)
		card.Io(R_SHIFT, 0, false);
	card.Io(R_MOTOR_OFF, 0, false);
	expect(HostFiles()["/r.dsk"] == a, "reading alone leaves the file as it was");
}

static void TestUndecodableTrackNotSaved()
{
	HostFiles().clear();
	std::string a = SectorImage(4);
	HostFiles()["/u.dsk"] = a;
	DiskIICard card;
	card.Mount("/u.dsk", 0);
	std::vector<uint8_t> sync(NIB_TRACK, 0xFF);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, sync.data());
	card.Io(R_MOTOR_OFF, 0, false);
	expect(HostFiles()["/u.dsk"] == a, "a track with no sectors is not written to a .dsk");
	expect(card.Io(R_READ, 0, false) == 0, "and the drive stays writable");
}

static void TestWriteFailureProtects()
{
	HostFiles().clear();
	HostFiles()["/w.dsk"] = SectorImage(5);
	DiskIICard card;
	card.Mount("/w.dsk", 0);
	HostFiles().erase("/w.dsk");              // the card was pulled
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(SectorImage(6), ORDER_DOS).data());
	card.Io(R_MOTOR_OFF, 0, false);
	expect(card.Io(R_READ, 0, false) == 0x80, "a failed save turns the drive write-protected");
}

static void TestUnmountFlushes()
{
	HostFiles().clear();
	std::string a = SectorImage(7), b = SectorImage(8);
	HostFiles()["/e.dsk"] = a;
	DiskIICard card;
	card.Mount("/e.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(b, ORDER_DOS).data());
	card.Unmount(0);                          // ejected with the motor still on
	expect(HostFiles()["/e.dsk"].compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0,
	       "ejecting saves the changed track");
}

int main()
{
	TestSectorWriteBack("/t.dsk", ORDER_DOS, ".dsk");
	TestSectorWriteBack("/t.po", ORDER_PRODOS, ".po");
	TestNibWriteBack();
	TestReadsDoNotWrite();
	TestUndecodableTrackNotSaved();
	TestWriteFailureProtects();
	TestUnmountFlushes();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
```

- [ ] **Step 4: Create `tests/host/run-disk-tests.sh`** (LF, `chmod +x`)

```bash
#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-disk-tests.sh
#  Module : Builds and runs the host-side Disk II card test
#           (tests/host/disk_test.cpp plus the real DiskIICard.cpp
#           and DskImage.cpp) against the Arduino/SD shims.
# ============================================================
#
# Usage:  tests/host/run-disk-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building disk_test"
g++ -std=gnu++17 -O1 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/disk_test" \
  "$HERE/disk_test.cpp" \
  "$ROOT/src/AppleII/DiskIICard.cpp" "$ROOT/src/AppleII/DskImage.cpp"

"$BUILD/disk_test"
```

- [ ] **Step 5: Run to verify it fails** — `tests/host/run-disk-tests.sh`. Expected: it builds; `FAIL` on "track 0 saved", ".nib: raw track saved", "a failed save turns the drive write-protected" and "ejecting saves the changed track". The other checks pass, because today nothing is ever written. If the shim or `DiskIICard.h` fails to compile, fix the includes first; that is not the failure this step is looking for.

- [ ] **Step 6: `DiskIICard.h`** — in `struct FloppyDrive` add the members, and reset them in `Reset()`:

```cpp
	DskImage::ImageType type;   // how 'data' maps back to the file
	uint64_t dirtyTracks;       // bit t: track t written since the last Flush
```

```cpp
		type = DskImage::IMAGE_NONE;
		dirtyTracks = 0;
```

In `class DiskIICard`, public section, below `Unmount`:

```cpp
	// Writes the tracks changed since the last flush back to the image file.
	// Called when the motor stops, the drive is deselected, and on eject.
	void Flush(int drive);
```

- [ ] **Step 7: `DiskIICard.cpp`**
  - `case 0xC`: in the writing branch, replace `disk[currentDrive].data[idx] = dLatch;` with:

```cpp
			{
				disk[currentDrive].data[idx] = dLatch;                                  // writing
				disk[currentDrive].dirtyTracks |= 1ULL << disk[currentDrive].track;
			}
```

  - `case 0x8`: after `disk[currentDrive].motorOn = false;` add `Flush(currentDrive);`.
  - `setDrv()`: as its first line add `Flush(!drv);                                                            // leaving that drive: save what it wrote`.
  - `InsertFloppy()`: replace the lines `// For now, proceed in write-disabled mode` and `disk[drv].readOnly = false;	// read only` with:

```cpp
	// writable: changed tracks go back to the file (Flush)
	disk[drv].readOnly = false;
	disk[drv].type = type;
	disk[drv].dirtyTracks = 0;
```

  - `Mount()`: add `Flush(drive);` as the first statement after the range check, so a replaced disk saves first. `Unmount()`: add `Flush(drive);` before `disk[drive].Reset();`.
  - Add `Flush`:

```cpp
// A sector image takes a track back only when all 16 of its sectors still
// decode; one that does not (copy protection, a half-formatted track) stays
// in memory, and the log says so. If the card will not take the write at
// all, the drive turns write-protected, so DOS reports it instead of later
// writes being lost without a word.
void DiskIICard::Flush(int drv)
{
	FloppyDrive& d = disk[drv];
	if (!d.dirtyTracks || !d.filename[0])
		return;

	static uint8_t trackImage[DskImage::TRACK_BYTES];
	for (int t = 0; t < DskImage::TRACKS; t++)
	{
		if (!(d.dirtyTracks & (1ULL << t)))
			continue;
		const BYTE* nib = d.data + t * DskImage::NIB_TRACK;
		bool ok;
		if (d.type == DskImage::IMAGE_NIB)
			ok = FileSystem::WriteAt(d.filename, (size_t)t * DskImage::NIB_TRACK, nib, DskImage::NIB_TRACK);
		else
		{
			DskImage::SectorOrder order = (d.type == DskImage::IMAGE_PRODOS) ? DskImage::ORDER_PRODOS : DskImage::ORDER_DOS;
			if (!DskImage::DenibblizeTrackToImage(nib, t, order, trackImage))
			{
				LOGF("[disk] %s: track %d does not decode, not saved\n", d.filename, t);
				continue;
			}
			ok = FileSystem::WriteAt(d.filename, (size_t)t * DskImage::TRACK_BYTES, trackImage, DskImage::TRACK_BYTES);
		}
		if (!ok)
		{
			LOGF("[disk] cannot write %s: drive %d is now write-protected\n", d.filename, drv + 1);
			d.readOnly = true;
			break;
		}
	}
	d.dirtyTracks = 0;
}
```

  - Update the file header's module comment: "…the shift/load data latch at registers $C-$F; tracks written are saved back to the image file when the motor stops or the disk is ejected."

- [ ] **Step 8: Run to verify it passes** — `tests/host/run-disk-tests.sh`. Expected: `PASS`. Then run all suites.

- [ ] **Step 9: Board check (needs the user)** — build and flash `build/dev`. Put a **copy** of a DOS 3.3 `.dsk` on the SD card. Boot it, then run `10 PRINT "SAVED"` / `SAVE TEST`. Eject it in the Supervisor, remount it, boot, and check that `CATALOG` lists TEST and `RUN TEST` prints SAVED. Repeat with a `.nib` copy. Then take the SD card out while a disk is mounted and `SAVE` again: expected `WRITE PROTECTED` and a `[disk] cannot write` log line.

- [ ] **Step 10: Commit**

```bash
git add src/Tools/FileSystem.h tests/host/shim/SD.h src/AppleII/DiskIICard.h src/AppleII/DiskIICard.cpp tests/host/disk_test.cpp tests/host/run-disk-tests.sh
git commit -m "Disk II: save written tracks back to .nib/.dsk/.po images

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Motor control like the hardware ($CFFF hack out, reset stops the drive)

**Files:**
- Modify: `src/AppleII/Apple2Device.cpp` (the `case 0xCFFF:` block near line 247)
- Modify: `src/AppleII/AppleMem.cpp` (`CxAccess`, lines 242-244)
- Modify: `src/AppleII/DiskIICard.h`, `src/AppleII/DiskIICard.cpp`
- Modify: `src/AppleII/Apple2Machine.cpp` (`WarmReset`)
- Modify: `tests/host/disk_test.cpp`

**Interfaces:**
- Consumes: `DiskIICard::Flush` (Task 7).
- Produces: `void DiskIICard::ResetLine()`, which stops both motors, leaves write mode and flushes. It is called from `DiskIICard::Reset()` and from `Apple2Machine::WarmReset()`. `DiskIICard::MotorOff()` is removed.

On the real card the RESET line clears the 9334 control latch, so the motors stop. Nothing else stops them, and `$CFFF` only releases the slots' `$C800` ROMs.

- [ ] **Step 1: Write the failing test** — add to `disk_test.cpp` and call it from `main`:

```cpp
static void TestResetLineStopsMotor()
{
	HostFiles().clear();
	std::string a = SectorImage(9), b = SectorImage(10);
	HostFiles()["/m.dsk"] = a;
	DiskIICard card;
	card.Mount("/m.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	expect(card.MotorOn(), "motor on");
	WriteTrack(card, Track0(b, ORDER_DOS).data());
	card.ResetLine();
	expect(!card.MotorOn(), "Ctrl-Reset stops the motor");
	expect(HostFiles()["/m.dsk"].compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0,
	       "Ctrl-Reset saves what was written");

	card.Io(R_MOTOR_ON, 0, false);
	card.Reset();
	expect(!card.MotorOn(), "power-up reset stops the motor");
	expect(card.HasFloppy(0), "and keeps the disk inserted");
}
```

- [ ] **Step 2: Run to verify it fails** — `tests/host/run-disk-tests.sh`. Expected: compile error, `'class DiskIICard' has no member named 'ResetLine'`.

- [ ] **Step 3: Implement**
  - `DiskIICard.h`: delete `void MotorOff() { disk[currentDrive].motorOn = false; }` and add below `Reset()`:

```cpp
	// The RESET line (Ctrl-Reset too): clears the card's control latch, so
	// both motors stop and the drive leaves write mode. The head stays put.
	void ResetLine();
```

  - `DiskIICard.cpp`: add

```cpp
void DiskIICard::ResetLine()
{
	for (int drv = 0; drv < 2; drv++)
	{
		disk[drv].motorOn = false;
		disk[drv].writeMode = false;
		Flush(drv);
	}
}
```

    and call `ResetLine();` as the first statement of `DiskIICard::Reset()`. This is safe in the constructor, because `EjectAll()` runs first and `Flush` does nothing without a filename.
  - `Apple2Device.cpp`: delete the whole `case 0xCFFF:` block and the two comment lines above it.
  - `AppleMem.cpp` `CxAccess`: delete the three lines `// on the ][+ this is also where the Disk II motor is stopped`, `if (device)` and `device->SoftSwitch(this, address, value, write);`.
  - `Apple2Machine.cpp` `WarmReset()`: after `mem.ResetSwitches();` add:

```cpp
	// the RESET line reaches the Disk II too: a program that hung with the
	// drive running stops here, and so does fast-disk mode
	device.disk6.ResetLine();
```

  Check that nothing else used the removed method: `grep -rn "MotorOff" src/` prints nothing.

- [ ] **Step 4: Run to verify it passes** — `tests/host/run-disk-tests.sh`, then all suites, then `tools/build-dev.sh build/dev`. Expected: all PASS, and the firmware builds.

- [ ] **Step 5: Board check** — PERF build: the Karateka load completes, and the motor-on time is within 5% of Task 5's. Normal build (needs the user): Super Serial Card in slot 2, boot DOS 3.3, `CATALOG`, `PR#2`, `LIST`, `PR#0`, `CATALOG` again. Everything works and the disk stops afterwards (log capture with the card off: no stuck `motor ON`). Ctrl-F12 while a disk loads: the motor stops and FPS returns to ~60.

- [ ] **Step 6: Commit**

```bash
git add src/AppleII/Apple2Device.cpp src/AppleII/AppleMem.cpp src/AppleII/DiskIICard.h src/AppleII/DiskIICard.cpp src/AppleII/Apple2Machine.cpp tests/host/disk_test.cpp docs/appleII-improvements.md
git commit -m "Disk II: RESET stops the motor; \$CFFF no longer does

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: `HardDiskCard` — a ProDOS block device

**Files:**
- Create: `src/AppleII/HardDiskCard.h`, `src/AppleII/HardDiskCard.cpp` (CRLF)
- Create: `tests/host/hdd_test.cpp`, `tests/host/run-hdd-tests.sh` (LF)

**Interfaces:**
- Consumes: `Card` (`Card.h`), `FileSystem::Open`, `DskImage::TypeFromPath`/`IMAGE_HDV`/`IMAGE_2MG`/`IMAGE_PRODOS`/`IMAGE_BYTES` (Task 6), the shim's `"r+"`/`seek`/`flush` (Task 7).
- Produces:
  - `class CardBus { virtual BYTE Read(WORD); virtual void Write(WORD, BYTE); }`
  - `class HardDiskCard : public Card` with `Configure(int slot, CardBus* bus)`, `Io`, `SlotRom`, `static bool IsHardDiskImage(const char* path)`, `bool Mount(const char* path)`, `void Unmount()`, `bool HasImage() const`, `const char* ImageName() const`, `uint16_t Blocks() const`
  - constants `ERR_NONE=0x00`, `ERR_IO=0x27`, `ERR_NO_DEVICE=0x28`, `ERR_WRITE_PROTECT=0x2B`
  - firmware layout: ID bytes at `$Cn01/03/05/07` = `$20/$00/$03/$3C`; boot code at `$Cn08`; driver entry at `$Cn30`; `$CnFC-FD` = block count; `$CnFE` = `$07`; `$CnFF` = `$30`. Registers: write reg 0 executes the command in ZP `$42-$47`; reg 1 = result; regs 2/3 = block count lo/hi.

- [ ] **Step 1: Write the failing test** — `tests/host/hdd_test.cpp`:

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : hdd_test.cpp
 *  Module : Host-side hard disk card test: firmware ID bytes, the
 *           ProDOS STATUS/READ/WRITE commands against a fake bus and
 *           the shimmed SD card, .2mg headers, error codes, and which
 *           images the card takes. Driven by tests/host/run-hdd-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <string>

#include "HardDiskCard.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

struct FakeBus : public CardBus
{
	BYTE mem[0x10000];
	FakeBus() { memset(mem, 0, sizeof(mem)); }
	BYTE Read(WORD addr) override { return mem[addr]; }
	void Write(WORD addr, BYTE value) override { mem[addr] = value; }
};

static const int BLOCKS = 1600;              // an 800K .po

static std::string BlockImage(int blocks)
{
	std::string s((size_t)blocks * 512, '\0');
	for (size_t i = 0; i < s.size(); i++)
		s[i] = (char)((i / 512) * 7 + (i % 512));
	return s;
}

// The ProDOS command block in zero page, then the driver's STA to register 0
static BYTE Command(HardDiskCard& card, FakeBus& bus, BYTE cmd, BYTE unit, WORD buf, WORD block)
{
	bus.mem[0x42] = cmd;
	bus.mem[0x43] = unit;
	bus.mem[0x44] = buf & 0xFF;
	bus.mem[0x45] = buf >> 8;
	bus.mem[0x46] = block & 0xFF;
	bus.mem[0x47] = block >> 8;
	card.Io(0x0, 0, true);
	return card.Io(0x1, 0, false);
}

static void TestTypes()
{
	HostFiles().clear();
	HostFiles()["/small.po"] = std::string(143360, '\0');
	HostFiles()["/big.po"] = std::string(819200, '\0');
	expect(HardDiskCard::IsHardDiskImage("/a.hdv"), ".hdv is a hard disk");
	expect(HardDiskCard::IsHardDiskImage("/b.2MG"), ".2mg is a hard disk");
	expect(HardDiskCard::IsHardDiskImage("/big.po"), "an 800K .po is a hard disk");
	expect(!HardDiskCard::IsHardDiskImage("/small.po"), "a 140K .po is a floppy");
	expect(!HardDiskCard::IsHardDiskImage("/c.dsk"), ".dsk is a floppy");
	expect(!HardDiskCard::IsHardDiskImage("/missing.po"), "a .po that cannot be opened is not taken");
}

static void TestFirmware()
{
	HostFiles().clear();
	HostFiles()["/hd.hdv"] = BlockImage(BLOCKS);
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);
	expect(card.SlotRom() == NULL, "no image: no firmware, the boot scan passes the slot by");
	expect(card.Mount("/hd.hdv"), "mounts a .hdv");
	BYTE* rom = card.SlotRom();
	expect(rom != NULL, "image: firmware present");
	if (!rom)
		return;
	expect(rom[1] == 0x20 && rom[3] == 0x00 && rom[5] == 0x03, "autostart boot ID bytes");
	expect(rom[7] == 0x3C, "a block device, not SmartPort");
	expect(rom[0xFF] == 0x30, "driver entry at $Cn30");
	expect(rom[0xFE] == 0x07, "status byte: read, write, status; one volume");
	expect(rom[0xFC] == (BLOCKS & 0xFF) && rom[0xFD] == (BLOCKS >> 8), "block count");
	expect(rom[0x30] == 0x8D && rom[0x31] == 0xF0 && rom[0x32] == 0xC0, "driver: STA $C0F0 for slot 7");
	expect(rom[0x0D] == 0x70 && rom[0x22] == 0x70, "boot: unit and X are slot 7 * 16");
	card.Unmount();
	expect(card.SlotRom() == NULL && !card.HasImage(), "unmounted: firmware gone");
}

static void TestCommands()
{
	HostFiles().clear();
	std::string image = BlockImage(BLOCKS);
	HostFiles()["/hd.po"] = image;
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);
	card.Mount("/hd.po");

	expect(Command(card, bus, 0, 0x70, 0, 0) == HardDiskCard::ERR_NONE, "STATUS ok");
	expect(card.Io(0x2, 0, false) == (BLOCKS & 0xFF) && card.Io(0x3, 0, false) == (BLOCKS >> 8),
	       "STATUS: block count in registers 2/3");

	expect(Command(card, bus, 1, 0x70, 0x2000, 5) == HardDiskCard::ERR_NONE, "READ block 5 ok");
	expect(memcmp(bus.mem + 0x2000, image.data() + 5 * 512, 512) == 0, "READ: block 5 in memory");

	for (int i = 0; i < 512; i++)
		bus.mem[0x4000 + i] = (BYTE)(0xA5 ^ i);
	expect(Command(card, bus, 2, 0x70, 0x4000, 7) == HardDiskCard::ERR_NONE, "WRITE block 7 ok");
	expect(memcmp(HostFiles()["/hd.po"].data() + 7 * 512, bus.mem + 0x4000, 512) == 0, "WRITE: block 7 in the file");
	expect(HostFiles()["/hd.po"].size() == image.size(), "WRITE: file keeps its size");

	expect(Command(card, bus, 1, 0x70, 0x2000, BLOCKS) == HardDiskCard::ERR_IO, "block past the end: I/O error");
	expect(Command(card, bus, 1, 0xF0, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "drive 2: no device");
	expect(Command(card, bus, 1, 0x60, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "another slot: no device");
	expect(Command(card, bus, 9, 0x70, 0x2000, 0) == HardDiskCard::ERR_IO, "unknown command: I/O error");

	card.Unmount();
	expect(Command(card, bus, 1, 0x70, 0x2000, 0) == HardDiskCard::ERR_NO_DEVICE, "no image: no device");
}

static std::string TwoImg(uint32_t format, uint32_t flags, const std::string& data)
{
	std::string h(64, '\0');
	memcpy(&h[0], "2IMG", 4);
	auto le32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; i++) h[at + i] = (char)(v >> (8 * i)); };
	le32(0x0C, format);
	le32(0x10, flags);
	le32(0x14, (uint32_t)(data.size() / 512));
	le32(0x18, 64);
	le32(0x1C, (uint32_t)data.size());
	return h + data;
}

static void Test2mg()
{
	HostFiles().clear();
	std::string data = BlockImage(280);
	HostFiles()["/p.2mg"] = TwoImg(1, 0, data);
	HostFiles()["/dos.2mg"] = TwoImg(0, 0, data);
	HostFiles()["/locked.2mg"] = TwoImg(1, 0x80000000u, data);
	HostFiles()["/junk.2mg"] = std::string(1024, 'x');
	FakeBus bus;
	HardDiskCard card;
	card.Configure(7, &bus);

	expect(card.Mount("/p.2mg") && card.Blocks() == 280, ".2mg: mounts, 280 blocks");
	expect(Command(card, bus, 1, 0x70, 0x2000, 0) == HardDiskCard::ERR_NONE
	       && memcmp(bus.mem + 0x2000, data.data(), 512) == 0, ".2mg: block 0 is after the header");
	expect(!card.Mount("/dos.2mg"), ".2mg in DOS order is refused");
	expect(!card.Mount("/junk.2mg"), "a file without the 2IMG header is refused");
	expect(card.Mount("/locked.2mg"), "a locked .2mg mounts");
	expect(Command(card, bus, 2, 0x70, 0x2000, 0) == HardDiskCard::ERR_WRITE_PROTECT, "and refuses writes");
}

int main()
{
	TestTypes();
	TestFirmware();
	TestCommands();
	Test2mg();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
```

- [ ] **Step 2: Create `tests/host/run-hdd-tests.sh`** (LF, `chmod +x`) — the same as `run-disk-tests.sh`, with the name/comments changed and this compile line:

```bash
echo "==> Building hdd_test"
g++ -std=gnu++17 -O1 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/hdd_test" \
  "$HERE/hdd_test.cpp" \
  "$ROOT/src/AppleII/HardDiskCard.cpp" "$ROOT/src/AppleII/DskImage.cpp"

"$BUILD/hdd_test"
```

(with the header `File : run-hdd-tests.sh` / `Module : Builds and runs the host-side hard disk card test (tests/host/hdd_test.cpp plus the real HardDiskCard.cpp and DskImage.cpp) against the Arduino/SD shims.`)

- [ ] **Step 3: Run to verify it fails** — `tests/host/run-hdd-tests.sh`. Expected: `HardDiskCard.h: No such file or directory`.

- [ ] **Step 4: Write `src/AppleII/HardDiskCard.h`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HardDiskCard.h
 *  Module : A ProDOS block device, after AppleWin's Harddisk.cpp:
 *           one hard disk image (.hdv, .2mg, or a .po bigger than a
 *           floppy) read and written 512 bytes at a time straight
 *           from the SD card, with no disk-byte emulation. The
 *           256-byte firmware is a ProDOS driver and a boot routine
 *           that both hand the work to Io(): the driver leaves its
 *           command block in zero page $42-$47 and writes register 0,
 *           and the transfer is done before the next instruction.
 * ============================================================
*/

#ifndef HARD_DISK_CARD_H
#define HARD_DISK_CARD_H

#include "Card.h"
#include "../Tools/FileSystem.h"

// What the card's transfers need of the machine: the Apple's memory as the
// CPU sees it at that moment.
class CardBus
{
public:
	virtual ~CardBus() {}
	virtual BYTE Read(WORD addr) = 0;
	virtual void Write(WORD addr, BYTE value) = 0;
};

class HardDiskCard : public Card
{
public:
	static const int BLOCK_BYTES = 512;
	static const int MAX_BLOCKS  = 65535;            // ProDOS's 32 MB

	// ProDOS error codes, returned in A with carry set
	static const BYTE ERR_NONE          = 0x00;
	static const BYTE ERR_IO            = 0x27;
	static const BYTE ERR_NO_DEVICE     = 0x28;
	static const BYTE ERR_WRITE_PROTECT = 0x2B;

	HardDiskCard();

	// The firmware carries the slot number: call before the card goes in.
	void Configure(int slot, CardBus* bus);

	BYTE Io(int reg, BYTE value, bool write) override;
	// NULL with no image, so the autostart ROM's boot scan passes it by
	BYTE* SlotRom() override;

	// The images this card takes rather than the Disk II: .hdv, .2mg, and a
	// .po that is not exactly 140K.
	static bool IsHardDiskImage(const char* path);

	bool Mount(const char* path);
	void Unmount();
	bool HasImage() const { return filename[0] != '\0'; }
	const char* ImageName() const { return filename; }
	uint16_t Blocks() const { return blocks; }

private:
	BYTE rom[256];
	int slot;
	CardBus* bus;
	File file;                   // open for as long as the image is mounted
	char filename[400];
	uint32_t dataOffset;         // 64 after a .2mg header, else 0
	uint16_t blocks;
	bool readOnly;
	BYTE result;                 // register 1: the last command's error code
	BYTE block[BLOCK_BYTES];

	void BuildRom();
	BYTE Execute();
};

#endif
```

- [ ] **Step 5: Write `src/AppleII/HardDiskCard.cpp`**

```cpp
/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HardDiskCard.cpp
 *  Module : ProDOS block device card, see HardDiskCard.h.
 * ============================================================
*/

#include "HardDiskCard.h"
#include "DskImage.h"

static const int DRIVER = 0x30;      // driver entry, $Cn30

HardDiskCard::HardDiskCard()
	: slot(7), bus(NULL), dataOffset(0), blocks(0), readOnly(false), result(ERR_NONE)
{
	filename[0] = '\0';
	BuildRom();
}

void HardDiskCard::Configure(int s, CardBus* b)
{
	slot = s;
	bus = b;
	BuildRom();
}

// The firmware, for slot n:
//   $Cn00  LDX #$20 / LDY #$00 / LDX #$03 / LDX #$3C
//          the ID bytes the autostart ROM ($Cn01/03/05) and ProDOS ($Cn07,
//          not $00: a plain block device, not SmartPort) look for
//   $Cn08  boot: READ block 0 of unit $n0 to $0800, then JMP $0801 with X =
//          $n0 as boot code expects; on an error JMP $E000 (BASIC)
//   $Cn30  driver: STA $C080+n0 runs the command; A = error code, X/Y =
//          block count (for STATUS); CMP #1 sets carry on an error
//   $CnFC  block count, $CnFE status byte, $CnFF driver entry
void HardDiskCard::BuildRom()
{
	const BYTE s16 = (BYTE)(slot << 4);
	const BYTE io  = (BYTE)(0x80 + s16);         // $C0x0: register 0
	const BYTE cn  = (BYTE)(0xC0 + slot);
	const BYTE code[] =
	{
		0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0xA2, 0x3C,   // $00 ID bytes
		0xA9, 0x01, 0x85, 0x42,                           // $08 LDA #1 STA $42   READ
		0xA9, s16,  0x85, 0x43,                           // $0C LDA #$n0 STA $43 unit
		0xA9, 0x00, 0x85, 0x44, 0x85, 0x46, 0x85, 0x47,   // $10 buffer lo, block 0
		0xA9, 0x08, 0x85, 0x45,                           // $18 buffer $0800
		0x20, DRIVER, cn,                                 // $1C JSR driver
		0xB0, 0x05,                                       // $1F BCS fail
		0xA2, s16,                                        // $21 LDX #$n0
		0x4C, 0x01, 0x08,                                 // $23 JMP $0801
		0x4C, 0x00, 0xE0,                                 // $26 fail: JMP $E000
	};
	const BYTE driver[] =
	{
		0x8D, io, 0xC0,                                   // STA $C0x0  run it
		0xAD, (BYTE)(io + 1), 0xC0,                       // LDA $C0x1  error code
		0xAE, (BYTE)(io + 2), 0xC0,                       // LDX $C0x2  blocks lo
		0xAC, (BYTE)(io + 3), 0xC0,                       // LDY $C0x3  blocks hi
		0xC9, 0x01,                                       // CMP #1     carry = error
		0x60,                                             // RTS
	};
	memset(rom, 0, sizeof(rom));
	memcpy(rom, code, sizeof(code));
	memcpy(rom + DRIVER, driver, sizeof(driver));
	rom[0xFC] = blocks & 0xFF;
	rom[0xFD] = blocks >> 8;
	rom[0xFE] = 0x07;                                     // status, read, write; 1 volume
	rom[0xFF] = DRIVER;
}

BYTE* HardDiskCard::SlotRom()
{
	return HasImage() ? rom : NULL;
}

BYTE HardDiskCard::Io(int reg, BYTE value, bool write)
{
	switch (reg)
	{
		case 0x0:
			if (write)
				result = Execute();
			break;
		case 0x1: return result;
		case 0x2: return blocks & 0xFF;
		case 0x3: return blocks >> 8;
	}
	return 0;
}

BYTE HardDiskCard::Execute()
{
	if (!bus)
		return ERR_IO;
	BYTE cmd  = bus->Read(0x42);
	BYTE unit = bus->Read(0x43);
	WORD buf  = bus->Read(0x44) | (bus->Read(0x45) << 8);
	uint32_t blk = bus->Read(0x46) | (bus->Read(0x47) << 8);

	// one drive: drive 1 of our own slot
	if (!HasImage() || (unit & 0x80) || ((unit >> 4) & 7) != slot)
		return ERR_NO_DEVICE;
	if (cmd == 0)                                          // STATUS
		return readOnly ? ERR_WRITE_PROTECT : ERR_NONE;
	if (cmd == 3)                                          // FORMAT: nothing to lay down
		return readOnly ? ERR_WRITE_PROTECT : ERR_NONE;
	if ((cmd != 1 && cmd != 2) || blk >= blocks)
		return ERR_IO;

	uint32_t pos = dataOffset + blk * BLOCK_BYTES;
	if (cmd == 1)                                          // READ
	{
		if (!file.seek(pos) || (int)file.read(block, BLOCK_BYTES) != BLOCK_BYTES)
			return ERR_IO;
		for (int i = 0; i < BLOCK_BYTES; i++)
			bus->Write((WORD)(buf + i), block[i]);
		return ERR_NONE;
	}

	if (readOnly)                                          // WRITE
		return ERR_WRITE_PROTECT;
	for (int i = 0; i < BLOCK_BYTES; i++)
		block[i] = bus->Read((WORD)(buf + i));
	if (!file.seek(pos) || file.write(block, BLOCK_BYTES) != (size_t)BLOCK_BYTES)
		return ERR_IO;
	file.flush();                                          // a power cut keeps the block
	return ERR_NONE;
}

static uint32_t Le32(const BYTE* p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool HardDiskCard::IsHardDiskImage(const char* path)
{
	DskImage::ImageType type = DskImage::TypeFromPath(path);
	if (type == DskImage::IMAGE_HDV || type == DskImage::IMAGE_2MG)
		return true;
	if (type != DskImage::IMAGE_PRODOS)
		return false;
	File f = FileSystem::Open(path);
	if (!f)
		return false;
	bool big = f.size() != (size_t)DskImage::IMAGE_BYTES;
	f.close();
	return big;
}

bool HardDiskCard::Mount(const char* path)
{
	Unmount();

	// writable if the card lets us, read-only otherwise
	bool ro = false;
	File f = SD.open(path, "r+");
	if (!f)
	{
		f = FileSystem::Open(path);
		ro = true;
	}
	if (!f)
	{
		LOGF("[hdd] cannot open %s\n", path);
		return false;
	}

	uint32_t size = f.size(), offset = 0, length = size;
	if (DskImage::TypeFromPath(path) == DskImage::IMAGE_2MG)
	{
		// 2IMG header: format at $0C (1 = ProDOS order), flags at $10
		// (bit 31: locked), data offset at $18, data length at $1C
		BYTE h[64];
		if (size < 64 || (int)f.read(h, 64) != 64 || memcmp(h, "2IMG", 4) != 0)
		{
			LOGF("[hdd] %s: not a 2IMG file\n", path);
			f.close();
			return false;
		}
		offset = Le32(h + 0x18);
		length = Le32(h + 0x1C);
		if (Le32(h + 0x0C) != 1 || offset + length > size)
		{
			LOGF("[hdd] %s: only ProDOS-order .2mg images\n", path);
			f.close();
			return false;
		}
		if (Le32(h + 0x10) & 0x80000000u)
			ro = true;
	}
	if (length < (uint32_t)BLOCK_BYTES || length % BLOCK_BYTES)
	{
		LOGF("[hdd] %s: not a whole number of 512-byte blocks\n", path);
		f.close();
		return false;
	}

	file = f;
	dataOffset = offset;
	uint32_t n = length / BLOCK_BYTES;
	blocks = (uint16_t)(n > (uint32_t)MAX_BLOCKS ? MAX_BLOCKS : n);
	readOnly = ro;
	snprintf(filename, sizeof(filename), "%s", path);
	BuildRom();
	LOGF("[hdd] %s: %u blocks%s\n", path, (unsigned)blocks, ro ? ", read-only" : "");
	return true;
}

void HardDiskCard::Unmount()
{
	if (HasImage())
		file.close();
	filename[0] = '\0';
	blocks = 0;
	dataOffset = 0;
	readOnly = false;
	BuildRom();
}
```

Convert to CRLF: `perl -pi -e 's/\r?\n/\r\n/' src/AppleII/HardDiskCard.h src/AppleII/HardDiskCard.cpp`

Note: in the shim, `File::read` takes `unsigned char*`/`int`; on the ESP32 it is `read(uint8_t*, size_t)`, which is why the code casts the returns to `int`. If the device build complains that `File f = …; file = f;` is ambiguous, use `file = f;` exactly as written, since ESP32 `fs::File` is copy-assignable.

- [ ] **Step 6: Run to verify it passes** — `tests/host/run-hdd-tests.sh`. Expected: `PASS`. Then run all suites, and `tools/build-dev.sh build/dev`. The card isn't wired in yet, but the new file must compile for the ESP32.

- [ ] **Step 7: Commit**

```bash
git add src/AppleII/HardDiskCard.h src/AppleII/HardDiskCard.cpp tests/host/hdd_test.cpp tests/host/run-hdd-tests.sh
git commit -m "HardDiskCard: ProDOS block device for .hdv/.2mg/large .po images

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Hard disk in slot 7 — machine, Supervisor and settings

**Files:**
- Modify: `src/AppleII/Apple2Device.h` (include + member next to `disk6`)
- Modify: `src/AppleII/Apple2Machine.h`, `src/AppleII/Apple2Machine.cpp`
- Modify: `src/Tools/Settings.h`
- Modify: `src/Supervisor/Supervisor.cpp` (`Select()` lines 857-864, `ChooseMachine()` lines 916-917)
- Modify: `ESP32-VGA_AppleII_Emulator.ino` (disk remount block in `setup()`, lines 189-201)

**Interfaces:**
- Consumes: `HardDiskCard`, `CardBus` (Task 9).
- Produces: `Apple2Device::hdd7`; `class MemoryBus : public CardBus`; `bool Apple2Machine::MountHardDisk(const char*)`; `void Apple2Machine::UnmountHardDisk()`; `Settings::LoadHardDisk()`/`SaveHardDisk(const char*)`.
- UI behaviour: picking a hard-disk image in the Supervisor mounts it straight away (there is no drive picker). Picking the image that is already mounted ejects it. Status texts: `HARD DISK MOUNTED (PICK AGAIN TO EJECT)`, `HARD DISK EJECTED`, `LOAD FAILED`.

- [ ] **Step 1: Device and machine wiring**
  - `Apple2Device.h`: `#include "HardDiskCard.h"`; below `SuperSerialCard ssc;` add `HardDiskCard hdd7;               // slot 7, shows up once an image is mounted`.
  - `Apple2Machine.h`: above `class Apple2Machine` add

```cpp
// The hard disk card's view of memory: whatever the CPU sees right now.
class MemoryBus : public CardBus
{
public:
	explicit MemoryBus(Memory& m) : mem(m) {}
	BYTE Read(WORD addr) override { return mem.ReadByte(addr); }
	void Write(WORD addr, BYTE value) override { mem.WriteByte(addr, value); }
private:
	Memory& mem;
};
```

    Add the member `MemoryBus bus;` **after** `const char* bootNote;`. Add the public methods `bool MountHardDisk(const char* path);` and `void UnmountHardDisk();`.
  - `Apple2Machine.cpp` constructor: change the initialiser to `: profile(p), bootNote(""), bus(mem)`, and after the `device.slots[6] = …` line add:

```cpp
	// slot 7: the autostart ROM scans down from here, so a mounted hard disk
	// boots before the floppies, as on a real machine with one fitted
	device.hdd7.Configure(7, &bus);
	device.slots[7] = &device.hdd7;
```

  - `Apple2Machine.cpp`: add

```cpp
bool Apple2Machine::MountHardDisk(const char* path)
{
	bool ok = device.hdd7.Mount(path);
	// the card shows or hides its firmware with the image
	mem.Remap();
	return ok;
}

void Apple2Machine::UnmountHardDisk()
{
	device.hdd7.Unmount();
	mem.Remap();
}
```

  - Check that the Super Serial Card can't claim slot 7: `grep -n "slot >= 1 && slot <= 2" src/Supervisor/Supervisor.cpp` shows its picker offers slots 1–2, and `Apple2Device::SetSerialSlot` refuses an occupied slot.

- [ ] **Step 2: Settings** — add to `Settings.h` above `private:`:

```cpp
	// Hard disk image in slot 7, "" when none: saved across a machine switch
	static String LoadHardDisk()
	{
		Preferences p;
		if (!p.begin(NS, true))
			return String();
		String path = p.getString("hd", "");
		p.end();
		return path;
	}

	static bool SaveHardDisk(const char* path)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		p.putString("hd", path ? path : "");
		p.end();
		return true;
	}
```

- [ ] **Step 3: Remount after a machine switch** — in `setup()`, after the floppy `for` loop and before `if (remounted)`:

```cpp
    String hd = Settings::LoadHardDisk();
    if (hd.length() > 0)
    {
        Settings::SaveHardDisk("");
        remounted |= machine->MountHardDisk(hd.c_str());
    }
```

- [ ] **Step 4: Supervisor** — add `#include "../AppleII/HardDiskCard.h"` if `Supervisor.cpp` doesn't already see it through `Apple2Machine.h`. In `Select()`, replace `mode = PICK_DRIVE;` (the line after the two `snprintf(pickPath…)` calls) with:

```cpp
		// a hard disk has one place to go, slot 7: no drive to pick
		if (HardDiskCard::IsHardDiskImage(pickPath))
		{
			if (strcmp(machine->device.hdd7.ImageName(), pickPath) == 0)
			{
				machine->UnmountHardDisk();
				SetStatus("HARD DISK EJECTED");
			}
			else if (machine->MountHardDisk(pickPath))
				SetStatus("HARD DISK MOUNTED (PICK AGAIN TO EJECT)");
			else
				SetStatus("LOAD FAILED");
			return;
		}
		mode = PICK_DRIVE;
```

In `ChooseMachine()`, after the two `Settings::SaveDisk` lines, add `Settings::SaveHardDisk(machine->device.hdd7.ImageName());`. Update the `Supervisor.cpp` file header's module line to mention `.hdv/.2mg` hard disks.

- [ ] **Step 5: Build and run the suites** — all host suites PASS; `tools/build-dev.sh build/dev` builds.

- [ ] **Step 6: Board check (needs the user)** — flash. Put a ProDOS 8 hard-disk image on the SD card, for example a 32 MB `.hdv` with ProDOS and a few programs, and a **copy** of an 800K `.po`.
  1. The image appears in the Supervisor list. Pick it: the status line says `HARD DISK MOUNTED (PICK AGAIN TO EJECT)`.
  2. RESET from the Supervisor: the machine boots ProDOS from slot 7. `CAT /` in BASIC.SYSTEM lists the volume.
  3. Save a file (`SAVE TEST`), eject the image by picking it again, remount it and reboot: `TEST` is still there.
  4. Pick it again → `HARD DISK EJECTED`; RESET boots the floppy (or BASIC) as before.
  5. Switch machine model with a hard disk mounted: after the restart it is mounted again.
  6. With a floppy in drive 1 and no hard disk, boot behaviour is unchanged.
  7. PERF build: the Karateka numbers are unchanged, because the Disk II path is untouched.

- [ ] **Step 7: Commit**

```bash
git add src/AppleII/Apple2Device.h src/AppleII/Apple2Machine.h src/AppleII/Apple2Machine.cpp src/Tools/Settings.h src/Supervisor/Supervisor.cpp ESP32-VGA_AppleII_Emulator.ino docs/appleII-improvements.md
git commit -m "Hard disk in slot 7: mount .hdv/.2mg/large .po from the Supervisor

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Documentation

**Files:**
- Modify: `README.md`, `docs/ARCHITECTURE.md`, `docs/BUILD_AND_RELEASE.md`

- [ ] **Step 1: README**
  - Features: add "Runs at a real Apple II's 60.05 Hz by default (F4: MAX); disk loads still run flat out"; "Disks you write to are saved back to the image on the SD card (.nib, .dsk, .do, .po)"; "ProDOS hard disks (.hdv, .2mg, 800K+ .po) in slot 7, straight from the SD card".
  - Usage: how to mount or eject a hard disk (pick it; pick it again to eject); boot order (a hard disk in slot 7 boots first).
  - Tests list (around line 172): add `run-pacer-tests.sh`, `run-hires-tests.sh`, `run-disk-tests.sh`, `run-hdd-tests.sh` with one-line descriptions, in the same style as the existing rows.
  - A caution line: back up disk images before letting programs write to them.
- [ ] **Step 2: ARCHITECTURE.md** — add short sections:
  - "Frame pacing": what `FramePacer` decides, the 17,030 cycles per frame, 1X/MAX, fast disk, frame skipping, and that FPS counts drawn frames.
  - "Rendering": no periodic repaint; `Reset()` leaves every cell dirty; the hires lookup table.
  - "Disk II write-back": dirty tracks, when `Flush` runs, `.dsk` tracks that don't decode, write-protect on a failed write, and the RESET line versus `$CFFF`.
  - "HardDiskCard": the firmware layout table from Task 9, the zero-page command block, the registers, and the `.2mg` header fields used.
- [ ] **Step 3: BUILD_AND_RELEASE.md** — in the pre-release checklist (around line 226), replace the per-suite lines with "`for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done` — every suite must PASS". Add a "Measuring speed" subsection covering `tools/build-dev.sh build/perf -DPERF_TRACE=1`, `tools/flash-parts.sh`, `tools/capture-log.py`, what the summary lines mean, and the 5% noise rule.
- [ ] **Step 4: Commit**

```bash
git add README.md docs/ARCHITECTURE.md docs/BUILD_AND_RELEASE.md
git commit -m "Docs: frame pacing, speed key, disk write-back, hard disk card, perf tooling

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Releasing (version bump in `src/Version.h`, PR, tag, GitHub release) follows `docs/BUILD_AND_RELEASE.md` and is done separately when the user asks.

---

## Deferred (considered, not in this plan)

- **Cycle-timed disk reads** (advancing the disk position by elapsed cycles / 32, as AppleWin does). This is more accurate for timing-sensitive loaders, but it makes RWTS sector searches wait for real rotation, so loads get slower on this CPU. Revisit only for a specific program that needs it, and behind an option.
- **WOZ 1/2 images.** These need a bit-level shift register and quarter-track stepping. It's a milestone of its own, for copy-protected originals.
- **Dirty-page tracking for video memory.** After Task 3, scanning an unchanged screen is already cheap. Measure `render avg` on a static screen before building it.
- **Floating bus** (AppleWin's `MemReadFloatingBus`). A few ][+ games sync to the video scanner with it. Pick this up with a game that shows the need.
- **Hard-disk eject button in the Supervisor.** For now, picking the mounted image again ejects it. A proper button needs the Supervisor button row to change.

## Results

Fill in after each measured task (MAX speed for PERF rows).

| After task | Idle FPS | speed % (idle) | Karateka load ms | Motor-on ms | render avg / max µs | Internal free / largest block |
|---|---|---|---|---|---|---|
| 0 (baseline, unpaced) | 34 (68,200 cyc/pass ≈ 227%) | n/a | 22,623 (90 motor events) | 17,331 | idle 522 / 6,300; Karateka 4,186 / 18,672 | 84,252 / 63,476 (PERF build, no SSC) |
| 2 (paced; PERF = MAX) | 34 (1X: 61) | 221 (1X: 100) | 22,601 (1X: 22,667) | 17,443 (1X: 19,372) | Karateka 4,194 / 18,667 | 84,228 / 63,476 |
| 3 | 34 | 223 | 22,571 | 17,429 | idle 317 / 350 (was 522 / 6,300); Karateka 3,740 / 18,301 | — |
| 4 | 34 | 226 | 22,553 | 17,411 | idle 322 / 354; Karateka 1,957 / 7,772 | 76,036 / 63,476 |
| 5 | 40 | 266 | 19,353 (same 98 motor events/PCs) | 14,992 | — | 76,036 / 63,476 |
| 8 | 40 | 271 | 19,102 (same 98 motor events/PCs) | 14,761 | — | 67,588 / 59,380 (−16.7 KB vs baseline: hires table + two track buffers) |
| 10 | 40 | 272 | 19,104 (same 98 motor events) | 14,758 | — | 67,588 / 59,380 |
