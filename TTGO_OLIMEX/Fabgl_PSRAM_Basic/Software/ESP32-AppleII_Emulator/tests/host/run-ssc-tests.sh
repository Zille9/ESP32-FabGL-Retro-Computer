#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-ssc-tests.sh
#  Module : Builds the host-side Super Serial Card test
#           (tests/host/ssc_test.cpp plus the real
#           src/AppleII/SuperSerialCard.cpp and RomLoader.cpp)
#           against the Arduino and SD shims, and runs it.
# ============================================================
#
# Usage:  tests/host/run-ssc-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building ssc_test"
g++ -std=gnu++17 -O1 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/ssc_test" \
  "$HERE/ssc_test.cpp" \
  "$ROOT/src/AppleII/SuperSerialCard.cpp" "$ROOT/src/AppleII/RomLoader.cpp"

"$BUILD/ssc_test"
