#!/usr/bin/env bash
#
# Build TTGOVGA32intosh for ESP32_Bootloader and package it for the SD card.
#
#   https://github.com/ESP-WORKS/ESP32_Bootloader
#
# The bootloader lives in the `factory` partition and flashes app firmware into
# `ota_0` (0x130000, 2816 KB) from the SD card. It needs two files per
# emulator, in a folder named after the menu entry:
#
#   /<FolderName>/firmware.bin   the raw app image (NOT a merged flash image)
#   /<FolderName>/version.txt    a version string; changing it triggers a reflash
#
# Usage:
#   tools/package-bootloader.sh [version]
#
# `version` defaults to `git describe`. Output lands in build/sdcard/ ready to
# copy to the root of the SD card.

set -euo pipefail

cd "$(dirname "$0")/.."

readonly SKETCH="TTGOVGA32intosh"
readonly MENU_NAME="$SKETCH"
# ota_0 in the ESP32_Bootloader partition table; keep in sync with
# BOOTLOADER_OTA0_MAX_BYTES in src/config.h.
readonly OTA0_MAX_BYTES=$((0x2C0000))

readonly BUILD_DIR="build/bootloader"
readonly STAGE_DIR="build/sdcard/$MENU_NAME"

version="${1:-$(git describe --tags --always 2>/dev/null || echo "dev")}"

# sketch.yaml also sets a default_port, and arduino-cli probes it for board
# metadata even on a plain compile -- which fails when no board is plugged in
# ("Error getting port metadata: port not found"). Passing --fqbn does not stop
# that; any explicit --port overrides the default, and compile never opens it,
# hence --port none below. Packaging for the SD card should not need the
# hardware attached.
fqbn=$(sed -n 's/^default_fqbn:[[:space:]]*//p' sketch.yaml)
if [ -z "$fqbn" ]; then
    echo "error: could not read default_fqbn from sketch.yaml" >&2
    exit 1
fi

echo "==> Building $SKETCH for ESP32_Bootloader (version $version)"
echo "    fqbn $fqbn"

# BUILD_TARGET=1 is BUILD_TARGET_BOOTLOADER; src/config.h leaves it overridable
# so no source edit is needed for a bootloader build.
rm -rf "$BUILD_DIR"
arduino-cli compile \
    --fqbn "$fqbn" \
    --port none \
    --output-dir "$BUILD_DIR" \
    --build-property compiler.cpp.extra_flags=-DBUILD_TARGET=1

readonly APP_BIN="$BUILD_DIR/$SKETCH.ino.bin"

if [ ! -f "$APP_BIN" ]; then
    echo "error: expected app image not found: $APP_BIN" >&2
    exit 1
fi

# The bootloader flashes this straight into ota_0, so it must be the bare app
# image. A merged flash image starts with the second-stage bootloader, and the
# giveaway is the magic byte: every ESP32 image starts with 0xE9, but a merged
# one has the app further in (see the README's offset-hunting snippet).
magic=$(head -c1 "$APP_BIN" | od -An -tx1 | tr -d ' \n')
if [ "$magic" != "e9" ]; then
    echo "error: $APP_BIN does not start with 0xE9 (got 0x$magic)." >&2
    echo "       That is not a valid ESP32 app image." >&2
    exit 1
fi

size=$(stat -c%s "$APP_BIN")
if [ "$size" -gt "$OTA0_MAX_BYTES" ]; then
    echo "error: app image is $((size / 1024)) KB, over the $((OTA0_MAX_BYTES / 1024)) KB ota_0 limit." >&2
    echo "       ESP32_Bootloader cannot flash it." >&2
    exit 1
fi

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp "$APP_BIN" "$STAGE_DIR/firmware.bin"
printf '%s\n' "$version" > "$STAGE_DIR/version.txt"

echo
echo "==> Packaged for ESP32_Bootloader"
echo "    firmware.bin  $((size / 1024)) KB of $((OTA0_MAX_BYTES / 1024)) KB ota_0"
echo "    version.txt   $version"
echo
echo "Copy the folder to the root of the SD card:"
echo "    cp -r $STAGE_DIR /media/\$USER/<SD>/"
echo
echo "Then power-cycle the board and pick \"$MENU_NAME\" from the bootloader menu."
