# Building and Publishing Firmware

How to produce flashable firmware for the ESP32-TTGO-VGA Apple II emulator and
publish it on GitHub. This is the reference for future "build the firmware and
post it" requests — follow it end to end.

## Prerequisites

| Requirement | Version used | Notes |
|---|---|---|
| [arduino-cli](https://arduino.github.io/arduino-cli/) | 1.3.1 | must be on `PATH` |
| `esp32:esp32` core | 2.0.17 | **2.0.x only** — the 3.x core changes APIs this sketch relies on |
| FabGL library | 1.0.9 | user library |
| Python 3 | any | only used to run the core's bundled `esptool.py` |
| `gh` (GitHub CLI) | any | only needed for publishing a release |

Install the core and library if a machine doesn't have them:

```bash
arduino-cli core install esp32:esp32@2.0.17
arduino-cli lib install FabGL@1.0.9
```

## 1. Build

One command produces everything:

```bash
tools/build-firmware.sh
```

It compiles with the release FQBN, merges the four flash images into a single
file, drops the `.elf`/`.map` build intermediates, builds the
[ESP32_Bootloader](#esp32_bootloader-build) flavour, and writes `SHA256SUMS`.
Output goes to `build/` (git-ignored — binaries are published as release assets,
not committed).

### Versioning

`FW_VERSION_STR` in [`src/Version.h`](../src/Version.h) is the single source of
truth for the release version. `tools/build-firmware.sh` parses it out of that
header to name the output files, and the supervisor's ABOUT page displays
it on screen, so the firmware always reports the version it was built as.
**Bump it there for a new release**, then use the same number for the git tag.
For a throwaway build, override it without editing anything:

```bash
FW_VERSION=0.3.0-rc1 tools/build-firmware.sh
```

Artifacts in `build/` (at `FW_VERSION=0.2.0`):

| File | Purpose |
|---|---|
| `ESP32-AppleII-v0.2.0.bin` | **the one to publish** — full image, flash at offset `0x0` |
| `ESP32-AppleII-v0.2.0-app.bin` | application only, flash at `0x10000` |
| `ESP32-VGA_AppleII_Emulator.ino.bootloader.bin` | bootloader, `0x1000` |
| `ESP32-VGA_AppleII_Emulator.ino.partitions.bin` | partition table, `0x8000` |
| `boot_app0.bin` | OTA selector, `0xe000` |
| `sdcard/AppleII/firmware.bin` | ESP32_Bootloader build — bare app image for the SD card |
| `sdcard/AppleII/version.txt` | ESP32_Bootloader version string |
| `SHA256SUMS` | checksums for all of the above |

The two publishable images carry the version; the bootloader, partition table
and OTA selector are version-neutral and keep the names arduino-cli and the
esp32 core give them.

### ESP32_Bootloader build

[ESP32_Bootloader](https://github.com/ESP-WORKS/ESP32_Bootloader) lives in the
`factory` partition and flashes an app from the SD card into `ota_0`
(`0x130000`, 2816 KB). `tools/package-bootloader.sh` builds that flavour on its
own; `build-firmware.sh` runs it as well:

```bash
tools/package-bootloader.sh [version]   # -> build/sdcard/AppleII/{firmware.bin,version.txt}
```

- The build target is selected by `BUILD_TARGET` in
  [`src/BuildConfig.h`](../src/BuildConfig.h). The default is standalone, and the
  script passes `-DBUILD_TARGET=1` through **both** `compiler.c.extra_flags` and
  `compiler.cpp.extra_flags`. No source edit is needed.
- In that build [`src/Tools/Bootloader.h`](../src/Tools/Bootloader.h) erases
  `otadata` as the first statement of `setup()`, so the next power-up goes back
  to the bootloader menu. The machine switch's restart first selects the running
  partition again (`esp_ota_set_boot_partition`), so it comes back into the
  emulator instead of the menu. In the standalone build both are compiled out.
- The FQBN, including `PartitionScheme=huge_app`, is the same as the standalone
  one. An app image carries no partition table, and runs at any 64K-aligned
  offset. **Do not add a `partitions.csv`** to make it "match" the bootloader.
- `firmware.bin` must be the bare app image. The script refuses anything that
  does not start with `0xE9` or does not fit `ota_0`.
- `version.txt` defaults to `AppleII_<git describe --tags --always --dirty>`.
  **The bootloader only reflashes when `version.txt` changes**, so a rebuild
  with an unchanged version silently keeps running the old app. That is why a
  release is tagged *before* it is built (see the checklist). A `-dirty` version
  prints a warning and must never be released.

To check that the flag reached the code, the bootloader build's `setup()` must
start by calling `esp_partition_find_first` and `esp_partition_erase_range`,
and the standalone one must not call them:

```bash
TC=$(ls -d ~/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/*/bin | tail -1)
ELF=build/bootloader/ESP32-VGA_AppleII_Emulator.ino.elf   # package-bootloader.sh alone keeps it
addr=$($TC/xtensa-esp32-elf-nm "$ELF" | awk '$3=="_Z5setupv"{print $1}')
$TC/xtensa-esp32-elf-objdump -d --start-address=0x$addr \
  --stop-address=$((0x$addr + 0xc0)) "$ELF" | grep -oE "call8?\s+\S+ <[^>]+>"
```

On hardware, with the bootloader flashed and `AppleII/` on the card:

1. Pick **AppleII**: it flashes, then boots the ][+ (the serial log says
   `(ESP32_Bootloader build)`)
2. Power-cycle: you must land in the **bootloader menu**. If the emulator starts
   instead, the `otadata` erase did not run
3. Pick it again: it boots with no reflash
4. **MACHINE** switch: it restarts straight into the other model, not the menu
5. Change only `version.txt`: it reflashes

### Doing it by hand

If you'd rather not use the script:

```bash
# compile (--clean: see the partition table note below)
arduino-cli compile --clean --fqbn "esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app" \
  --output-dir ./build .

# merge (paths depend on the installed core version)
python3 ~/.arduino15/packages/esp32/tools/esptool_py/4.5.1/esptool.py \
  --chip esp32 merge_bin -o build/ESP32-AppleII-v0.2.0.bin \
  --flash_mode dio --flash_freq keep --flash_size 4MB \
  0x1000  build/ESP32-VGA_AppleII_Emulator.ino.bootloader.bin \
  0x8000  build/ESP32-VGA_AppleII_Emulator.ino.partitions.bin \
  0xe000  ~/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/partitions/boot_app0.bin \
  0x10000 build/ESP32-VGA_AppleII_Emulator.ino.bin
```

The FQBN options are load-bearing: `PSRAM=enabled` (FabGL needs it) and
`PartitionScheme=huge_app` (the sketch does not fit the default 1.2 MB app
partition scheme's headroom expectations used by this project).

**There must be no `partitions.csv` in the sketch folder.** The esp32 core uses
one instead of the `PartitionScheme` in the FQBN, and keeps a copy in the build
cache that outlives the file — hence `--clean`. Releases up to 0.2.1 were built
with such a table (a 928K app and no `nvs` partition, so settings could not be
saved). From 0.3.0 the image uses the `huge_app` layout, so anyone upgrading
from 0.2.x must flash the full merged image at `0x0`, not just the app. To check
which table a build really produced:

```bash
python3 ~/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/gen_esp32part.py \
  build/ESP32-VGA_AppleII_Emulator.ino.partitions.bin    # must list nvs at 0x9000
```

## 2. Flash locally to test before publishing

```bash
arduino-cli board list                                   # find the port
arduino-cli upload --fqbn "esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app" \
  -p /dev/ttyUSB0 .
arduino-cli monitor -p /dev/ttyUSB0 -c 115200
```

The port is `/dev/ttyACM0` on some boards depending on the USB-serial chip.

To verify the merged image specifically (what users will actually flash):

```bash
python3 ~/.arduino15/packages/esp32/tools/esptool_py/4.5.1/esptool.py \
  --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 build/ESP32-AppleII-v0.2.0.bin
```

## 3. Publish to the remote repository

Binaries are **not** committed to git — they bloat history and can't be
rewritten. Publish them as GitHub Release assets instead.

```bash
# make sure the source that produced the binary is pushed first
git push origin HEAD

# tag and publish — keep the tag in step with FW_VERSION in the build script
VERSION=v0.2.0
git tag -a "$VERSION" -m "Firmware $VERSION"
git push origin "$VERSION"

gh release create "$VERSION" \
  --title "Firmware $VERSION" \
  --notes "Apple II emulator firmware for ESP32-TTGO-VGA (VGA32 v1.4)." \
  "build/ESP32-AppleII-$VERSION.bin" \
  "build/ESP32-AppleII-$VERSION-app.bin" \
  build/sdcard/AppleII/firmware.bin \
  build/sdcard/AppleII/version.txt \
  build/SHA256SUMS
```

To update an existing release's assets instead of making a new one:

```bash
gh release upload v0.2.0 build/ESP32-AppleII-v0.2.0.bin --clobber
```

## 4. Flashing instructions for end users

Worth pasting into the release notes:

```bash
# esptool (pip install esptool)
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 ESP32-AppleII-v0.2.0.bin
```

Or drop the merged `.bin` at offset `0x0` into
[ESP Web Tools / esptool-js](https://espressif.github.io/esptool-js/) in a
Chrome-based browser — no toolchain needed.

For ESP32_Bootloader users: put `firmware.bin` and `version.txt` in an
`AppleII/` folder at the SD card root, next to `/roms` and the disk images.

## Measuring speed

A measurement build logs what the normal one keeps quiet, and ignores the saved
Super Serial Card (which would mute the log):

```bash
tools/build-dev.sh build/perf -DPERF_TRACE=1          # MAX speed; add -DPERF_SPEED=0 for 1X
tools/flash-parts.sh build/perf                         # four parts at their offsets: NVS survives
tools/capture-log.py 80                                 # resets the board, prints and summarises the log
tools/flash-parts.sh build/dev                          # put a normal build back (tools/build-dev.sh build/dev)
```

20 s after boot the `PERF_TRACE` build mounts `/karateka.nib` (or
`/AppleII/karateka.nib`) in drive 1 and resets. The summary gives FPS (frames
drawn per second), `speed %` (against a real Apple II), the Karateka load time
(mount to last motor off) and total motor-on time; `[perf] render avg/max` lines
give the time spent drawing. Same motor events at the same PCs mean identical
emulated behaviour. Differences under ~5% between builds are layout noise.

## Checklist for a release

1. Bump `FW_VERSION_STR` in `src/Version.h`
2. `for t in tests/host/run-*-tests.sh; do "$t" || exit 1; done` — every host
   suite (CPU, keyboard layouts, sector images, Disk II write-back, hard disk
   card, frame pacing, hires renderer, joystick, serial card) must PASS
3. Measure speed (below) and compare with the previous release
4. Build a test image and flash it to hardware. Confirm it boots to BASIC, the F1
   supervisor opens (check **ABOUT** reports the version you just bumped), and
   F2 toggles FPS. With the //e ROMs in `/roms`, switch to the //e in
   **MACHINE**: it must restart into the "Apple //e" screen, and `PR#3` must
   give 80 columns; switch back to the ][+. Run the ESP32_Bootloader hardware
   checks above as well
5. Commit the source, then tag it with the same version (`git tag -a vX.Y.Z`)
6. `tools/build-firmware.sh` **after** tagging, so `version.txt` is exactly
   `AppleII_vX.Y.Z` and not `-dirty` — clean build, no warnings that matter
7. Push the commit and the tag, then `gh release create` with the merged binary,
   the `-app.bin`, `firmware.bin`, `version.txt` and `SHA256SUMS`
