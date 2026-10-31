#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-disk-tests.sh
#  Module : Builds and runs the host-side Disk II card test
#           (tests/host/disk_test.cpp plus the real DiskIICard.cpp
#           and DskImage.cpp) against the Arduino/SD shims.
# ============================================================
#
# Usage:  tests/host/run-disk-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building disk_test"
g++ -std=gnu++17 -O1 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/disk_test" \
  "$HERE/disk_test.cpp" \
  "$ROOT/src/AppleII/DiskIICard.cpp" "$ROOT/src/AppleII/DskImage.cpp"

"$BUILD/disk_test"
