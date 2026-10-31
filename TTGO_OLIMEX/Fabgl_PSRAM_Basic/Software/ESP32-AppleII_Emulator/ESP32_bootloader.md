# Adding ESP32_Bootloader Support to an ESP32 Emulator Project

A reusable spec for making any ESP32 firmware (emulator or otherwise) loadable
from the SD-card menu of
[ESP32_Bootloader](https://github.com/ESP-WORKS/ESP32_Bootloader), while keeping
the normal standalone USB-flashed build as the default.

Distilled from the TTGOVGA32intosh integration (commit `d8d109e`), which is
worked through as a reference example at the end.

---

## 1. How the bootloader works

ESP32_Bootloader (by fg1998) turns a TTGO VGA32 v1.4, or any ESP32, into an
SD-card emulator loader. It is flashed once, then lives in the `factory`
partition. On power-up it:

1. Shows a splash screen and checks the SD card.
2. Looks for `firmware.bin` + `version.txt`:
   - **Single-emulator mode** — both files in the SD root: boot that one, no menu.
   - **Multi-emulator mode** — otherwise: scan every root folder containing both
     files and show a menu (PS/2 UP/DOWN/ENTER). The **folder name is the menu
     entry**, and the list is sorted by folder name.
3. Compares the selected `version.txt` with the version it last flashed:
   - **Same** → boot straight into the app already in `ota_0`.
   - **Different** → flash `firmware.bin` into `ota_0`, then boot it.

Once the app is running, it owns the device until the next power cycle — at
which point it must hand control back (see §3).

### Partition table (fixed by the bootloader)

```csv
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   0x5000
otadata,  data, ota,     0xe000,   0x2000
factory,  app,  factory, 0x10000,  0x120000
ota_0,    app,  ota_0,   0x130000, 0x2C0000
spiffs,   data, spiffs,  0x3F0000, 0x10000
```

| Partition | Offset     | Size    | Role                                  |
| --------- | ---------- | ------- | ------------------------------------- |
| `nvs`     | `0x9000`   | 20 KB   | Non-volatile storage, **shared** with the bootloader |
| `otadata` | `0xE000`   | 8 KB    | Boot selector — which app runs next   |
| `factory` | `0x10000`  | 1152 KB | The bootloader itself                 |
| `ota_0`   | `0x130000` | **2816 KB** | Where your app is flashed         |
| `spiffs`  | `0x3F0000` | 64 KB   | Reserved                              |

Your project does **not** ship or replicate this table. It is already on the
device, and your app runs against it.

---

## 2. Compatibility checklist — read before starting

Most emulators work unchanged apart from §3. Check these first, because they
are what actually breaks:

| Check | Requirement | Why |
| --- | --- | --- |
| **App image size** | ≤ **2816 KB** (`0x2C0000`) | That is all of `ota_0`. |
| **Own partitions** | Must not rely on partitions the table above lacks | At runtime your app sees the bootloader's table, not your build's. A large SPIFFS/LittleFS/FAT partition, a custom data partition, or a second OTA slot **does not exist** here. The `spiffs` partition is only 64 KB. |
| **OTA self-update** | Will not work; disable or ignore it | There is only one OTA slot, and the bootloader owns the boot selection. (ESPectrum's self-update is broken for this reason.) |
| **NVS usage** | Fits in 20 KB alongside the bootloader; use your own namespace | `nvs` is shared. Settings are also lost on a full flash erase. |
| **WiFi** | Test it explicitly | The bootloader README lists PocketTRS as incompatible, due to "WiFi conflicts and partition differences". |
| **Where data lives** | ROMs and disk images should be on the SD card | That is the natural fit: the bootloader's card is also your app's card. |

To measure the app image size, see §6 — it is the bare `.bin`, not a merged
image.

### Why no partition scheme change is needed

An ESP32 app image carries **no partition table**. Its flash-resident code and
data segments are mapped through the MMU at whatever 64 KB-aligned offset the
image was flashed to, and both `0x10000` (standalone) and `0x130000` (`ota_0`)
are 64 KB-aligned. So **the same app binary runs at either offset**, and you
can keep your standalone build's partition scheme exactly as it is (for example,
Arduino's `huge_app`).

> **Warning:** do not add a `partitions.csv` to an Arduino sketch root to "match
> the bootloader". arduino-esp32 will pick it up for **every** build, including
> the standalone one, and your standalone firmware will then ship with the wrong
> layout.

---

## 3. The one required code change

The bootloader flashes your app and sets `otadata` to boot it. Left alone, the
ESP32 would then boot straight into your app on every power-up, and the
bootloader menu would become unreachable.

The fix is to **erase `otadata` as the very first thing the app does**. With
`otadata` blank, the ROM falls back to the `factory` partition — the bootloader —
on the next power-up.

```cpp
#include "esp_ota_ops.h"
#include "esp_partition.h"

static void bootloader_release_otadata(void) {
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (!otadata) {
        return;
    }
    esp_partition_erase_range(otadata, 0, otadata->size);
}

void setup() {
    bootloader_release_otadata();   // first statement, before Serial/display/etc.
    // ... rest of the original setup()
}
```

- **Arduino:** put the call at the top of `setup()`.
- **ESP-IDF:** put it at the top of `app_main()`; these are IDF APIs, so the same
  code applies. The bootloader README notes some IDF projects may not need the
  change if they manage `otadata` themselves — check what yours does.

This is **safe in a standalone layout**. A layout like `huge_app` has no
`otadata` partition, so `esp_partition_find_first()` returns `NULL` and the
function does nothing. A bootloader build flashed over USB still runs normally.

The erase is still worth gating behind a build flag (§4). That keeps the default
build byte-for-byte unchanged and makes the intent explicit — and in any layout
that *does* have `otadata`, the erase would change boot behaviour.

---

## 4. Build-target switch

Use a single compile-time macro with the standalone build as the default,
overridable from the command line so a bootloader build needs **no source edit**.

### `src/config.h` (or wherever your project keeps build config)

```c
#ifndef MYPROJECT_CONFIG_H
#define MYPROJECT_CONFIG_H

// BUILD_TARGET_STANDALONE  Normal USB flash; the app owns the whole device.
// BUILD_TARGET_BOOTLOADER  Launched from ota_0 by ESP32_Bootloader:
//                          https://github.com/ESP-WORKS/ESP32_Bootloader
#define BUILD_TARGET_STANDALONE 0
#define BUILD_TARGET_BOOTLOADER 1

#ifndef BUILD_TARGET
#define BUILD_TARGET BUILD_TARGET_STANDALONE
#endif

// Size of ota_0 in the ESP32_Bootloader partition table (2816 KB).
#define BOOTLOADER_OTA0_MAX_BYTES 0x2C0000

#endif
```

> **Note:** give the header a **project-specific include guard**. Generic guards
> like `CONFIG_H` collide easily. In TTGOVGA32intosh, a pre-existing
> `user_config.h` already used `CONFIG_H`, so a second `config.h` with the same
> guard would have been silently skipped.

### Gate the code from §3

```cpp
#include "config.h"

#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
#include "esp_ota_ops.h"
#include "esp_partition.h"

static void bootloader_release_otadata(void) { /* as in §3 */ }
#endif

void setup() {
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
    bootloader_release_otadata();
#endif
    // ...
}
```

### Selecting the target per toolchain

**arduino-cli**

```bash
arduino-cli compile --fqbn "<your fqbn>" --port none --output-dir build/bootloader \
    --build-property compiler.cpp.extra_flags=-DBUILD_TARGET=1 \
    --build-property compiler.c.extra_flags=-DBUILD_TARGET=1
```

`--port none` lets this run with the board unplugged; see the comment in the §6
template.

> **Warning:** the ESP32 core has **separate** hooks for C and C++
> (`compiler.c.extra_flags`, `compiler.cpp.extra_flags`; both empty by default in
> core 2.0.x). Setting only the C++ one means `.c` files never see the define and
> silently build as standalone. Set both unless you are certain the gated code
> is only in `.cpp` files.

**PlatformIO** — add an environment that extends your existing one:

```ini
[env:bootloader]
extends = env:myboard
build_flags =
    ${env:myboard.build_flags}
    -DBUILD_TARGET=1
```

```bash
pio run -e bootloader      # -> .pio/build/bootloader/firmware.bin
```

**ESP-IDF (CMake)** — add a define in `main/CMakeLists.txt`, driven by a cache
variable:

```cmake
if(BUILD_FOR_BOOTLOADER)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BUILD_TARGET=1)
endif()
```

```bash
idf.py -DBUILD_FOR_BOOTLOADER=1 build
```

---

## 5. Versioning

`version.txt` is not decorative: **it is the only thing the bootloader uses to
decide whether to reflash.**

| Situation | Result |
| --- | --- |
| New binary, **new** `version.txt` | Reflashed ✅ |
| New binary, **same** `version.txt` | ❌ **Not reflashed** — the old app keeps running, silently |
| Same binary, new `version.txt` | Reflashed (harmless, just slower) |

### Rules

1. **Every distinct binary gets a distinct version string.** Otherwise testing a
   rebuild silently runs the previous build.
2. **Derive it from git** so it is automatic and traceable:
   ```bash
   git describe --tags --always --dirty
   # v1.2.0                  exactly on the v1.2.0 tag
   # v1.2.0-3-g1a2b3c4       3 commits past v1.2.0, at 1a2b3c4
   # v1.2.0-3-g1a2b3c4-dirty uncommitted changes — never release this
   ```
   `--dirty` matters: without it, two different uncommitted builds of the same
   commit share a version and the second is never flashed.
3. **Allow a manual override** (for example, the script's first argument) for
   one-off test builds.
4. **Keep it one line of plain text**, with no need for spaces or special
   characters. The bootloader README's examples use a name-and-version style,
   such as `ESPectrum_1.4.5` or `CPCESP.0.85`.

### Release flow — tag first, then build

Build *after* tagging, so `git describe` returns exactly the tag and the binary,
the version string and the release all agree:

```bash
git commit ...                       # 1. commit the code
git tag -a v1.2.0 -m "..."           # 2. tag it
tools/package-bootloader.sh          # 3. build -> version.txt = "v1.2.0"
git push origin main v1.2.0          # 4. push commit and tag
gh release create v1.2.0 ...         # 5. publish (§7)
```

> **Lesson learned:** in TTGOVGA32intosh the release was built *before* the
> commit, so `version.txt` said `v0.1.0-1-g4e0b57b` (describing the *previous*
> commit), while the code actually lived in `d8d109e`. It worked, but the
> version string pointed at the wrong commit. Tagging first avoids this.

Do **not** use a `git describe` string such as `v1.2.0-3-g1a2b3c4` as a release
tag name. It encodes a specific commit, and becomes false as soon as the tag is
placed on any other commit.

---

## 6. Packaging script

A script turns "build for the bootloader" into one command and catches the two
mistakes that brick the workflow: an oversized image, and shipping a **merged**
flash image instead of the bare app image.

### What the bootloader needs

```
/SD card root
└── <MenuName>/              folder name = menu entry
    ├── firmware.bin         BARE app image (not merged)
    └── version.txt          one line; changing it triggers a reflash
```

### Which build output is `firmware.bin`

| Toolchain | Bare app image | ⚠️ Not this |
| --- | --- | --- |
| arduino-cli | `<output-dir>/<Sketch>.ino.bin` | `*.merged.bin`, `*.bootloader.bin`, `*.partitions.bin` |
| PlatformIO | `.pio/build/<env>/firmware.bin` | `bootloader.bin`, `partitions.bin` |
| ESP-IDF | `build/<project>.bin` | `build/bootloader/bootloader.bin`, `build/partition_table/*.bin` |

**Sanity check:** a bare ESP32 app image starts with the magic byte **`0xE9`**.
A merged image starts with `0xFF` padding, with the second-stage bootloader at
`0x1000` and the app further in, so byte 0 is not `0xE9`.

### Template: `tools/package-bootloader.sh`

Fill in the settings block. Everything else is generic.

```bash
#!/usr/bin/env bash
#
# Build this project for ESP32_Bootloader and stage it for the SD card.
#   https://github.com/ESP-WORKS/ESP32_Bootloader
#
# Usage: tools/package-bootloader.sh [version]
#   version defaults to `git describe --tags --always --dirty`.
# Output: build/sdcard/<MENU_NAME>/{firmware.bin,version.txt}

set -euo pipefail
cd "$(dirname "$0")/.."

# ---- per-project settings --------------------------------------------------
readonly MENU_NAME="MyEmulator"      # SD folder name = bootloader menu entry
readonly TOOLCHAIN="arduino-cli"     # arduino-cli | platformio | esp-idf
readonly SKETCH="MyEmulator"         # arduino-cli: sketch (.ino) name
readonly FQBN=""                     # arduino-cli: empty = read sketch.yaml
readonly PIO_ENV="bootloader"        # platformio: env that sets -DBUILD_TARGET=1
readonly IDF_PROJECT="my_emulator"   # esp-idf: project() name in CMakeLists.txt
# -----------------------------------------------------------------------------

# ota_0 size in the ESP32_Bootloader partition table (2816 KB).
readonly OTA0_MAX_BYTES=$((0x2C0000))
readonly STAGE_DIR="build/sdcard/$MENU_NAME"

version="${1:-$(git describe --tags --always --dirty 2>/dev/null || echo dev)}"

echo "==> Building $MENU_NAME for ESP32_Bootloader (version $version)"

case "$TOOLCHAIN" in
arduino-cli)
    fqbn="${FQBN:-$(sed -n 's/^default_fqbn:[[:space:]]*//p' sketch.yaml 2>/dev/null)}"
    if [ -z "$fqbn" ]; then
        echo "error: set FQBN or add default_fqbn to sketch.yaml" >&2
        exit 1
    fi
    rm -rf build/bootloader
    # --port none: if sketch.yaml sets a default_port, arduino-cli probes it for
    # board metadata even on a plain compile, and fails when the board is
    # unplugged ("Error getting port metadata: port not found"). --fqbn alone
    # does NOT stop that; any explicit --port overrides the default, and compile
    # never opens it. Packaging should not need the hardware attached.
    # Separate C and C++ hooks: set both so .c files see the define too.
    arduino-cli compile \
        --fqbn "$fqbn" \
        --port none \
        --output-dir build/bootloader \
        --build-property compiler.cpp.extra_flags=-DBUILD_TARGET=1 \
        --build-property compiler.c.extra_flags=-DBUILD_TARGET=1
    app_bin="build/bootloader/$SKETCH.ino.bin"
    ;;
platformio)
    pio run -e "$PIO_ENV"
    app_bin=".pio/build/$PIO_ENV/firmware.bin"
    ;;
esp-idf)
    idf.py -DBUILD_FOR_BOOTLOADER=1 build
    app_bin="build/$IDF_PROJECT.bin"
    ;;
*)
    echo "error: unknown TOOLCHAIN '$TOOLCHAIN'" >&2
    exit 1
    ;;
esac

if [ ! -f "$app_bin" ]; then
    echo "error: app image not found: $app_bin" >&2
    exit 1
fi

# Must be the bare app image. Every ESP32 image starts with 0xE9; a merged
# flash image starts with 0xFF padding instead.
magic=$(head -c1 "$app_bin" | od -An -tx1 | tr -d ' \n')
if [ "$magic" != "e9" ]; then
    echo "error: $app_bin starts with 0x$magic, not 0xE9 - not a bare app image" >&2
    exit 1
fi

size=$(stat -c%s "$app_bin")
if [ "$size" -gt "$OTA0_MAX_BYTES" ]; then
    echo "error: app image is $((size / 1024)) KB; ota_0 holds $((OTA0_MAX_BYTES / 1024)) KB" >&2
    exit 1
fi

case "$version" in
*-dirty) echo "warning: version '$version' has uncommitted changes - do not release it" >&2 ;;
esac

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp "$app_bin" "$STAGE_DIR/firmware.bin"
printf '%s\n' "$version" > "$STAGE_DIR/version.txt"

echo
echo "==> Packaged: $STAGE_DIR"
echo "    firmware.bin  $((size / 1024)) KB of $((OTA0_MAX_BYTES / 1024)) KB"
echo "    version.txt   $version"
echo
echo "Copy the folder to the SD card root:  cp -r $STAGE_DIR /media/\$USER/<SD>/"
```

Then:

```bash
chmod +x tools/package-bootloader.sh
bash -n tools/package-bootloader.sh       # syntax check
```

> **Note:** on macOS, `stat -c%s` is `stat -f%z`. Swap it if you package there.

### Repository hygiene

- Keep build output out of git: ignore `build/` (and `.pio/` for PlatformIO).
- If the repo's `.gitignore` is an **allowlist** (`/*` followed by `!` entries),
  add `!/tools/` or the script will never be committed.

---

## 7. Verification

### 7.1 Prove the flag reached the code — before you touch hardware

The failure mode is silent: if the define never reached the file, you get a
standalone build under a bootloader name. Check the linked binary directly.

**Arduino / PlatformIO** — disassemble `setup()` in both builds:

```bash
TC=~/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/*/bin   # adjust for PlatformIO
ELF=build/bootloader/MySketch.ino.elf

addr=$($TC/xtensa-esp32-elf-nm "$ELF" | awk '$3=="_Z5setupv"{print $1}')
$TC/xtensa-esp32-elf-objdump -d --start-address=0x$addr \
    --stop-address=$((0x$addr + 0xc0)) "$ELF" | grep -oE "call8?\s+\S+ <[^>]+>"
```

Expected:

- **Bootloader build** — the first two calls are `esp_partition_find_first` and
  `esp_partition_erase_range`, before `HardwareSerial::begin` or anything else.
- **Standalone build** — neither call appears in `setup()`.

> **Note:** grepping `nm` output for `esp_partition_*` symbols is **not** enough
> on its own: the core links them into standalone builds too. And the helper
> itself usually has no symbol of its own, because it is `static` and gets
> inlined. Check the calls inside `setup()`.

### 7.2 Check the artifacts

```bash
f=build/sdcard/MyEmulator/firmware.bin
head -c1 "$f" | od -An -tx1          # e9
stat -c%s "$f"                       # <= 2883584 (0x2C0000)
cat build/sdcard/MyEmulator/version.txt
```

Also exercise the script's guards once, by feeding them a file starting with
`0x00` and a file 1 byte over the limit. Both must be rejected.

### 7.3 On hardware

Do these in order, with the bootloader already flashed:

| # | Action | Expected |
| --- | --- | --- |
| 1 | Put `<MenuName>/` on the card, power on | Entry appears in the menu |
| 2 | Select it | "Flashing" progress, then the app boots |
| 3 | Power-cycle | **Back at the bootloader menu** — this proves the `otadata` erase works |
| 4 | Select it again | Boots **without** reflashing (version unchanged) |
| 5 | Change `version.txt` only, power on, select | Reflashes, then boots |
| 6 | Flash the **standalone** build over USB | Boots directly, with no bootloader involved |

If step 3 lands in the app instead of the menu, the `otadata` erase didn't run —
go back to 7.1.

---

## 8. Releases

Publish both flavours from one tag, so users of either flow are covered:

| Asset | Audience | Notes |
| --- | --- | --- |
| `firmware.bin` | ESP32_Bootloader | Keep this exact name: users drop it straight onto the card |
| `version.txt` | ESP32_Bootloader | Keep this exact name |
| `<Name>-<ver>-standalone-merged.bin` | USB flash | Complete image, flashed at `0x0` |
| `<Name>-<ver>-standalone-app.bin` | USB flash | App only, at `0x10000` (optional) |
| `SHA256SUMS.txt` | Everyone | `sha256sum -c --ignore-missing SHA256SUMS.txt` |

### Build the standalone merged image (Arduino core 2.0.x)

```bash
ET=~/.arduino15/packages/esp32/tools/esptool_py/*/esptool.py
B=build/standalone
python3 $ET --chip esp32 merge_bin -o MyEmulator-v1.2.0-standalone-merged.bin \
    --flash_mode dio --flash_freq 40m --flash_size 4MB \
    0x1000  $B/MySketch.ino.bootloader.bin \
    0x8000  $B/MySketch.ino.partitions.bin \
    0xe000  ~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin \
    0x10000 $B/MySketch.ino.bin
```

These are the same offsets `arduino-cli upload` uses. esptool may warn that it
won't change the flash frequency of a hash-protected bootloader image; the
image's own setting is kept, which is fine.

Verify the layout — expected bytes are `ff`, `e9`, `aa` and `e9`:

```bash
M=MyEmulator-v1.2.0-standalone-merged.bin
for off in 0x0 0x1000 0x8000 0x10000; do
  printf "%-8s %s\n" $off "$(dd if=$M bs=1 skip=$((off)) count=1 2>/dev/null | od -An -tx1)"
done
# 0x0 ff (padding)   0x1000 e9 (bootloader)   0x8000 aa (partition table)   0x10000 e9 (app)
```

### Publish

```bash
gh release create v1.2.0 -R <owner>/<repo> \
    --title "MyEmulator v1.2.0" --notes-file NOTES.md \
    build/sdcard/MyEmulator/firmware.bin \
    build/sdcard/MyEmulator/version.txt \
    MyEmulator-v1.2.0-standalone-merged.bin \
    SHA256SUMS.txt
```

> **Warning:** always pass **`-R <owner>/<repo>`** in a fork. With both `origin`
> and `upstream` remotes and no `gh` default set, `gh` can resolve to the
> **upstream** repo, and the release fails or lands in the wrong place. To fix it
> once, run `gh repo set-default <owner>/<repo>`.

Release notes should tell users to put `firmware.bin` and `version.txt` inside a
`<MenuName>/` folder on the card root, alongside the app's own SD files (ROMs,
disk images).

---

## 9. Using firmware you don't build yourself

For a third-party emulator released only as a merged image, extract the app
image yourself. It is usually at `0x10000`; the bootloader README cites `0x40000`
for MSPX and CPC.

Find the `0xE9` offsets. The **first** is the second-stage bootloader, so skip
it; the **second** is the app:

```bash
python3 - <<'PY'
d = open('firmware_merged.bin', 'rb').read()
for off in (0x0, 0x1000, 0x8000, 0xe000, 0x10000, 0x40000, 0x90000, 0xa0000):
    if off < len(d):
        print(f'0x{off:05X}: 0x{d[off]:02X}')
PY
python3 -c "d=open('firmware_merged.bin','rb').read(); open('firmware.bin','wb').write(d[0x10000:])"
```

That firmware still needs the §3 `otadata` erase to return to the menu; without
it, it boots straight in every time. Some projects publish a bootloader-ready
image directly — for example, ESPectrum's `.upg` renamed to `firmware.bin`.

---

## 10. Integration checklist

- [ ] §2 compatibility: image ≤ 2816 KB, no dependency on missing partitions, no OTA self-update, NVS fits in 20 KB
- [ ] `config.h` with `BUILD_TARGET`, standalone default, project-specific include guard
- [ ] `otadata` erase as the **first** statement of `setup()` / `app_main()`, gated on `BUILD_TARGET`
- [ ] Toolchain override wired (arduino-cli: **both** `c` and `cpp` `extra_flags`; PlatformIO: `[env:bootloader]`; IDF: CMake define)
- [ ] No root `partitions.csv` added; standalone partition scheme unchanged
- [ ] `tools/package-bootloader.sh`: size guard, `0xE9` guard, `git describe --dirty` version, staged `<MenuName>/` folder
- [ ] `build/` ignored; `tools/` tracked (check for an allowlist `.gitignore`)
- [ ] §7.1 disassembly: erase calls present in the bootloader build, absent in standalone
- [ ] §7.3 hardware: power-cycle returns to the menu; unchanged version skips the reflash
- [ ] Tag **before** building a release; publish both flavours plus `SHA256SUMS.txt` with `gh ... -R`
- [ ] README section: how to build and install for the bootloader

---

## Reference example: TTGOVGA32intosh

A Macintosh Plus emulator for the TTGO VGA32 (Arduino, arduino-cli), and the
first project integrated this way.

| Item | Where |
| --- | --- |
| Build switch | `src/config.h` — `BUILD_TARGET`, `BOOTLOADER_OTA0_MAX_BYTES` |
| `otadata` erase | `src/main.cpp` — `bootloader_release_otadata()`, first call in `setup()` |
| Packaging | `tools/package-bootloader.sh` → `build/sdcard/TTGOVGA32intosh/` |
| Standalone scheme | `huge_app`, unchanged (`sketch.yaml`) |
| App image size | 876 KB of the 2816 KB `ota_0` |
| Commit | `d8d109e` — *feat: add ESP32_Bootloader build target* |
| Release | `v0.1.0-1-g4e0b57b`, with `firmware.bin`, `version.txt`, standalone merged and app images, and `SHA256SUMS.txt` |

Differences from this spec, which came from lessons learned there:

- Its script sets only `compiler.cpp.extra_flags`. That is fine because the
  gated code is in `main.cpp`, but it would miss `.c` files.
- It uses `git describe --tags --always` without `--dirty`.
- Its release was built before tagging, so the version string names the
  previous commit (§5).
- Its script originally relied on `--fqbn` alone to avoid the port probe. That
  only appeared to work because the board happened to be plugged in, and it was
  corrected to `--port none`.
