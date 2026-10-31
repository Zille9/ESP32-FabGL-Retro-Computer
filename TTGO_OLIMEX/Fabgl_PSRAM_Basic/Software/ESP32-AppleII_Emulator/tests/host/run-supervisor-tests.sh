#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-supervisor-tests.sh
#  Module : Builds and runs the host-side test of the supervisor's
#           menu logic (tests/host/supervisor_test.cpp against
#           src/Supervisor/SupervisorLogic.h, which needs neither
#           Arduino nor FabGL).
# ============================================================
#
# Usage:  tests/host/run-supervisor-tests.sh
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"

mkdir -p "$BUILD"

echo "==> Building supervisor_test"
g++ -std=gnu++17 -O1 -Wall -I "$ROOT/src/Supervisor" \
  -o "$BUILD/supervisor_test" "$HERE/supervisor_test.cpp"

"$BUILD/supervisor_test"
