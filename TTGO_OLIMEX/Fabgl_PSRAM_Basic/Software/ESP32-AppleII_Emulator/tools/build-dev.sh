#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : build-dev.sh
#  Module : Development build into a scratch directory, optionally
#           with extra preprocessor defines (e.g. -DPERF_TRACE=1).
#           Release builds use tools/build-firmware.sh instead.
# ============================================================
#
# Usage:  tools/build-dev.sh <out dir> [-DNAME=VALUE ...]
#
set -euo pipefail

OUT="${1:?usage: tools/build-dev.sh <out dir> [-DNAME=VALUE ...]}"
shift
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FQBN="esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"

mkdir -p "$OUT"
if [ $# -gt 0 ]; then
  arduino-cli compile --clean --fqbn "$FQBN" \
    --build-property "compiler.cpp.extra_flags=$*" \
    --output-dir "$OUT" "$SKETCH_DIR"
else
  arduino-cli compile --clean --fqbn "$FQBN" --output-dir "$OUT" "$SKETCH_DIR"
fi
