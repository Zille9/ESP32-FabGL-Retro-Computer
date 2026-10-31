#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-dsk-tests.sh
#  Module : Builds the host-side sector image test
#           (tests/host/dsk_test.cpp plus the real
#           src/AppleII/DskImage.cpp) and runs it against the
#           sample .nib images in data/.
# ============================================================
#
# Usage:  tests/host/run-dsk-tests.sh
#
# DskImage.cpp has no FabGL or Arduino dependency, so no shim is needed.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building dsk_test"
g++ -std=gnu++17 -O1 -Wall \
  -I "$ROOT/src/AppleII" \
  -o "$BUILD/dsk_test" \
  "$HERE/dsk_test.cpp" "$ROOT/src/AppleII/DskImage.cpp"

"$BUILD/dsk_test" "$ROOT/data"
