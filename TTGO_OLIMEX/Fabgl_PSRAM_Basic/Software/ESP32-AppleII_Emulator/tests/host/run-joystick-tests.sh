#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-joystick-tests.sh
#  Module : Builds the host-side joystick test
#           (tests/host/joystick_test.cpp plus the real
#           src/AppleII/Joystick.cpp) and runs it.
# ============================================================
#
# Usage:  tests/host/run-joystick-tests.sh
#
# Joystick.cpp has no FabGL or Arduino dependency, so no shim is needed.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building joystick_test"
g++ -std=gnu++17 -O1 -Wall \
  -I "$ROOT/src/AppleII" \
  -o "$BUILD/joystick_test" \
  "$HERE/joystick_test.cpp" "$ROOT/src/AppleII/Joystick.cpp"

"$BUILD/joystick_test"
