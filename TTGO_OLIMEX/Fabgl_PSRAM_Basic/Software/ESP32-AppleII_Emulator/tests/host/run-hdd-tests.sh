#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-hdd-tests.sh
#  Module : Builds and runs the host-side hard disk card test
#           (tests/host/hdd_test.cpp plus the real HardDiskCard.cpp
#           and DskImage.cpp) against the Arduino/SD shims.
# ============================================================
#
# Usage:  tests/host/run-hdd-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building hdd_test"
g++ -std=gnu++17 -O1 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/hdd_test" \
  "$HERE/hdd_test.cpp" \
  "$ROOT/src/AppleII/HardDiskCard.cpp" "$ROOT/src/AppleII/DskImage.cpp"

"$BUILD/hdd_test"
