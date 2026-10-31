#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : build-firmware.sh
#  Module : Builds release firmware with arduino-cli, merges the
#           bootloader / partition table / boot_app0 / app images
#           into one flashable file at offset 0x0, and writes
#           SHA256 checksums. Output lands in build/, with the
#           final image named ESP32-AppleII-v<version>.bin. Then
#           builds the ESP32_Bootloader flavour too, via
#           tools/package-bootloader.sh, into build/sdcard/AppleII/.
# ============================================================
#
# Usage:  tools/build-firmware.sh [output-dir]
#
# src/Version.h is the single source of truth for the released firmware
# version; bump FW_VERSION_STR there for a new release, and the ABOUT page in
# the supervisor menu and this script stay in step. Override for a one-off
# build with:
#   FW_VERSION=0.3.0-rc1 tools/build-firmware.sh
#
set -euo pipefail

FQBN="esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$SKETCH_DIR/build}"
NAME="ESP32-VGA_AppleII_Emulator"       # arduino-cli names its outputs after the sketch
VERSION_H="$SKETCH_DIR/src/Version.h"
FW_VERSION="${FW_VERSION:-$(sed -n 's/.*FW_VERSION_STR[[:space:]]*"\([^"]*\)".*/\1/p' "$VERSION_H")}"
[ -n "$FW_VERSION" ] || { echo "error: could not read FW_VERSION_STR from $VERSION_H"; exit 1; }
RELEASE_NAME="ESP32-AppleII-v$FW_VERSION"

command -v arduino-cli >/dev/null || { echo "error: arduino-cli not found in PATH"; exit 1; }

echo "==> Cleaning $OUT_DIR"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"       # later steps cd elsewhere

echo "==> Building ESP32-AppleII v$FW_VERSION"
echo "==> Compiling ($FQBN)"
# --clean: the esp32 core copies the partition table into the cached build
# folder and keeps using that copy, so a stale one would outlive any change
# to the partition scheme. Releases always build from scratch.
arduino-cli compile --clean --fqbn "$FQBN" --output-dir "$OUT_DIR" "$SKETCH_DIR"

# esptool and boot_app0 ship with the installed esp32 core; resolve whatever
# version is present rather than hard-coding one.
ESPTOOL="$(ls -1 "$HOME"/.arduino15/packages/esp32/tools/esptool_py/*/esptool.py 2>/dev/null | sort -V | tail -1)"
BOOT_APP0="$(ls -1 "$HOME"/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin 2>/dev/null | sort -V | tail -1)"
[ -n "$ESPTOOL" ]   || { echo "error: esptool.py not found under ~/.arduino15"; exit 1; }
[ -n "$BOOT_APP0" ] || { echo "error: boot_app0.bin not found under ~/.arduino15"; exit 1; }

cp "$BOOT_APP0" "$OUT_DIR/boot_app0.bin"

echo "==> Merging into single-file image: $RELEASE_NAME.bin"
python3 "$ESPTOOL" --chip esp32 merge_bin \
  -o "$OUT_DIR/$RELEASE_NAME.bin" \
  --flash_mode dio --flash_freq keep --flash_size 4MB \
  0x1000  "$OUT_DIR/$NAME.ino.bootloader.bin" \
  0x8000  "$OUT_DIR/$NAME.ino.partitions.bin" \
  0xe000  "$OUT_DIR/boot_app0.bin" \
  0x10000 "$OUT_DIR/$NAME.ino.bin"

# The .elf and .map are large build intermediates, not release artifacts.
rm -f "$OUT_DIR/$NAME.ino.elf" "$OUT_DIR/$NAME.ino.map"

# Give the app-only image the same versioned identity as the merged one; the
# remaining pieces (bootloader, partition table, boot_app0) are version-neutral
# and keep the names arduino-cli and the core give them.
mv "$OUT_DIR/$NAME.ino.bin" "$OUT_DIR/$RELEASE_NAME-app.bin"

# The SD-card flavour for ESP32_Bootloader: the same sources built with
# -DBUILD_TARGET=1. Its version.txt comes from git describe, so tag a release
# before running this.
echo "==> ESP32_Bootloader build"
OUT_DIR="$OUT_DIR" "$SKETCH_DIR/tools/package-bootloader.sh"
rm -rf "$OUT_DIR/bootloader"            # build intermediates; the package is in sdcard/

echo "==> Checksums"
# firmware.bin and version.txt are listed without their folder: they are
# release assets too, and get downloaded side by side with the rest.
( cd "$OUT_DIR" && sha256sum ./*.bin > SHA256SUMS )
( cd "$OUT_DIR/sdcard/AppleII" && sha256sum firmware.bin version.txt >> "$OUT_DIR/SHA256SUMS" )
cat "$OUT_DIR/SHA256SUMS"

echo
echo "Firmware v$FW_VERSION ready in $OUT_DIR"
echo "  flash $RELEASE_NAME.bin at offset 0x0"
echo "  or copy $OUT_DIR/sdcard/AppleII/ to the SD card root for ESP32_Bootloader"
ls -la "$OUT_DIR" "$OUT_DIR"/sdcard/*
