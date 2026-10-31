#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : package-bootloader.sh
#  Module : Builds the firmware for ESP32_Bootloader
#           (https://github.com/ESP-WORKS/ESP32_Bootloader) with
#           -DBUILD_TARGET=1 and stages it for the SD card as
#           <out>/sdcard/AppleII/{firmware.bin,version.txt}.
#           The standalone build is tools/build-firmware.sh, which
#           also runs this script.
# ============================================================
#
# Usage:  tools/package-bootloader.sh [version]
#   version defaults to AppleII_<git describe --tags --always --dirty>.
#   The bootloader reflashes only when version.txt changes, so every distinct
#   binary needs a distinct version: tag a release BEFORE building it.
#   OUT_DIR=<dir> overrides the output root (default: build).
#
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SKETCH_DIR"

# ---- per-project settings --------------------------------------------------
MENU_NAME="AppleII"                     # SD folder name = bootloader menu entry
NAME="ESP32-VGA_AppleII_Emulator"       # arduino-cli names its outputs after the sketch
# Same board options as the standalone build (build-firmware.sh). The
# partition scheme only matters for the standalone image: at runtime the app
# sees the bootloader's partition table, and an app image runs at any 64K-
# aligned offset, ota_0 included.
FQBN="esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
# -----------------------------------------------------------------------------

OUT_DIR="${OUT_DIR:-$SKETCH_DIR/build}"
BUILD_DIR="$OUT_DIR/bootloader"
STAGE_DIR="$OUT_DIR/sdcard/$MENU_NAME"
# ota_0 in the ESP32_Bootloader partition table (2816 KB); BOOTLOADER_OTA0_MAX_BYTES
OTA0_MAX_BYTES=$((0x2C0000))

VERSION="${1:-${MENU_NAME}_$(git describe --tags --always --dirty 2>/dev/null || echo dev)}"

command -v arduino-cli >/dev/null || { echo "error: arduino-cli not found in PATH"; exit 1; }

echo "==> Building $MENU_NAME for ESP32_Bootloader (version $VERSION)"
rm -rf "$BUILD_DIR"
# --clean: the build cache is shared with the standalone build and keyed on the
# sketch, not on the flags.
# --port none: never probe a board; packaging must work with it unplugged.
# The core has separate C and C++ hooks; set both so .c files see the define.
arduino-cli compile --clean \
  --fqbn "$FQBN" \
  --port none \
  --output-dir "$BUILD_DIR" \
  --build-property "compiler.cpp.extra_flags=-DBUILD_TARGET=1" \
  --build-property "compiler.c.extra_flags=-DBUILD_TARGET=1" \
  "$SKETCH_DIR"

APP_BIN="$BUILD_DIR/$NAME.ino.bin"
[ -f "$APP_BIN" ] || { echo "error: app image not found: $APP_BIN"; exit 1; }

# The bootloader wants the BARE app image. Every ESP32 app image starts with
# 0xE9; a merged flash image starts with 0xFF padding instead.
MAGIC=$(head -c1 "$APP_BIN" | od -An -tx1 | tr -d ' \n')
if [ "$MAGIC" != "e9" ]; then
  echo "error: $APP_BIN starts with 0x$MAGIC, not 0xE9 - not a bare app image"
  exit 1
fi

SIZE=$(stat -c%s "$APP_BIN")
if [ "$SIZE" -gt "$OTA0_MAX_BYTES" ]; then
  echo "error: app image is $((SIZE / 1024)) KB; ota_0 holds $((OTA0_MAX_BYTES / 1024)) KB"
  exit 1
fi

case "$VERSION" in
  *-dirty) echo "warning: version '$VERSION' has uncommitted changes - do not release it" >&2 ;;
esac

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp "$APP_BIN" "$STAGE_DIR/firmware.bin"
printf '%s\n' "$VERSION" > "$STAGE_DIR/version.txt"

echo
echo "==> Packaged: $STAGE_DIR"
echo "    firmware.bin  $((SIZE / 1024)) KB of $((OTA0_MAX_BYTES / 1024)) KB"
echo "    version.txt   $VERSION"
echo
echo "Copy the folder to the SD card root:  cp -r $STAGE_DIR /media/\$USER/<SD>/"
