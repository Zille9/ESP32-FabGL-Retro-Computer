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
