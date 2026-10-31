#!/usr/bin/env bash
#
# Build the Classic99 ESP32 firmware for release.
#
#   standalone  Flashed over USB; the firmware owns the whole board.
#   bootloader  Loaded from the SD card menu of ESP32_Bootloader:
#               https://github.com/ESP-WORKS/ESP32_Bootloader
#
# Usage: tools/build.sh [all|standalone|bootloader] [version]
#   The default is "all". The version defaults to CLASSIC99_ESP32_VERSION from
#   src/version.h; builds that are not a clean tagged commit get the commit id
#   added, and uncommitted builds a hash of the firmware as well.
#
# Output, in build/:
#   standalone/Classic99-<version>-standalone-merged.bin   whole flash, write at 0x0
#   standalone/Classic99-<version>-standalone-app.bin      application only, write at 0x10000
#   sdcard/Classic99/firmware.bin + version.txt            copy the folder to the SD card root
#   SHA256SUMS.txt

set -euo pipefail
cd "$(dirname "$0")/.."

readonly NAME="Classic99"            # SD folder name = ESP32_Bootloader menu entry
readonly ENV_STANDALONE="ttgo-vga32"
readonly ENV_BOOTLOADER="bootloader"
# ota_0 size in the ESP32_Bootloader partition table (BOOTLOADER_OTA0_MAX_BYTES in src/config.h).
readonly OTA0_MAX_BYTES=$((0x2C0000))
readonly OUT_DIR="build"

target="${1:-all}"
case "$target" in
all|standalone|bootloader) ;;
*)
    echo "usage: tools/build.sh [all|standalone|bootloader] [version]" >&2
    exit 1
    ;;
esac

PIO="$(command -v pio || true)"
[ -n "$PIO" ] || PIO="$HOME/.platformio/penv/bin/pio"
if [ ! -x "$PIO" ]; then
    echo "error: PlatformIO (pio) not found" >&2
    exit 1
fi
PIO_CORE="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"

base_version="$(sed -n 's/^#define CLASSIC99_ESP32_VERSION "\(.*\)"/\1/p' src/version.h)"
if [ -z "$base_version" ]; then
    echo "error: CLASSIC99_ESP32_VERSION not found in src/version.h" >&2
    exit 1
fi

# version_for <app image>: the string that names this exact binary.
# ESP32_Bootloader reflashes only when version.txt changes, so two different
# binaries must never share a version.
version_for() {
    if [ -n "${2:-}" ]; then
        printf '%s\n' "$2"
        return
    fi
    local v="$base_version" commit dirty=""
    commit="$(git rev-parse --short HEAD 2>/dev/null || true)"
    [ -z "$(git status --porcelain -- . 2>/dev/null)" ] || dirty=1
    if [ -z "$commit" ]; then
        v="$v-$(sha256sum "$1" | cut -c1-8)"
    elif [ -n "$dirty" ]; then
        v="$v-$commit-dirty-$(sha256sum "$1" | cut -c1-8)"
    elif ! git describe --tags --exact-match HEAD >/dev/null 2>&1; then
        v="$v-$commit"
    fi
    printf '%s\n' "$v"
}

# check_app <app image>: must be a bare application image (they start with 0xE9;
# a merged flash image starts with 0xFF padding).
check_app() {
    if [ ! -f "$1" ]; then
        echo "error: app image not found: $1" >&2
        exit 1
    fi
    local magic
    magic=$(head -c1 "$1" | od -An -tx1 | tr -d ' \n')
    if [ "$magic" != "e9" ]; then
        echo "error: $1 starts with 0x$magic, not 0xE9 - not a bare app image" >&2
        exit 1
    fi
}

warn_dirty() {
    case "$1" in
    *-dirty*) echo "warning: version '$1' has uncommitted changes - do not release it" >&2 ;;
    esac
}

build_standalone() {
    local b=".pio/build/$ENV_STANDALONE" out="$OUT_DIR/standalone"
    echo "==> Building $NAME standalone"
    "$PIO" run -e "$ENV_STANDALONE"
    check_app "$b/firmware.bin"
    local version size
    version="$(version_for "$b/firmware.bin" "${1:-}")"
    warn_dirty "$version"

    local esptool="$PIO_CORE/packages/tool-esptoolpy/esptool.py"
    local boot_app0="$PIO_CORE/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    local merged="$out/$NAME-$version-standalone-merged.bin"
    rm -rf "$out"
    mkdir -p "$out"
    cp "$b/firmware.bin" "$out/$NAME-$version-standalone-app.bin"
    # Same offsets as "pio run -t upload".
    "$PIO_CORE/penv/bin/python" "$esptool" --chip esp32 merge_bin -o "$merged" \
        --flash_mode dio --flash_freq 40m --flash_size 4MB \
        0x1000 "$b/bootloader.bin" \
        0x8000 "$b/partitions.bin" \
        0xe000 "$boot_app0" \
        0x10000 "$b/firmware.bin" >/dev/null
    size=$(stat -c%s "$b/firmware.bin")

    echo
    echo "==> Standalone: $out"
    echo "    $(basename "$merged")  whole flash, write at 0x0"
    echo "    $NAME-$version-standalone-app.bin  $((size / 1024)) KB, write at 0x10000"
    echo "    Flash: esptool.py --chip esp32 write_flash 0x0 $merged"
    echo
}

build_bootloader() {
    local app=".pio/build/$ENV_BOOTLOADER/firmware.bin" stage="$OUT_DIR/sdcard/$NAME"
    echo "==> Building $NAME for ESP32_Bootloader"
    "$PIO" run -e "$ENV_BOOTLOADER"
    check_app "$app"
    local size version
    size=$(stat -c%s "$app")
    if [ "$size" -gt "$OTA0_MAX_BYTES" ]; then
        echo "error: app image is $((size / 1024)) KB; ota_0 holds $((OTA0_MAX_BYTES / 1024)) KB" >&2
        exit 1
    fi
    version="$(version_for "$app" "${1:-}")"
    warn_dirty "$version"

    rm -rf "$stage"
    mkdir -p "$stage"
    cp "$app" "$stage/firmware.bin"
    printf '%s_%s\n' "$NAME" "$version" > "$stage/version.txt"

    echo
    echo "==> ESP32_Bootloader: $stage"
    echo "    firmware.bin  $((size / 1024)) KB of $((OTA0_MAX_BYTES / 1024)) KB"
    echo "    version.txt   $(cat "$stage/version.txt")"
    echo "    Copy the folder to the SD card root:  cp -r $stage /media/\$USER/<SD>/"
    echo
}

case "$target" in
all)
    build_standalone "${2:-}"
    build_bootloader "${2:-}"
    ;;
standalone) build_standalone "${2:-}" ;;
bootloader) build_bootloader "${2:-}" ;;
esac

(cd "$OUT_DIR" && find . -type f ! -name SHA256SUMS.txt -printf '%P\n' | sort | xargs sha256sum > SHA256SUMS.txt)
echo "==> Checksums: $OUT_DIR/SHA256SUMS.txt"
