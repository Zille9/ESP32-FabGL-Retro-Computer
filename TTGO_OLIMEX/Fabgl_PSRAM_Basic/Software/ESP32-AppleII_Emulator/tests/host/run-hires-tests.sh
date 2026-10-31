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
