#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : flash-parts.sh
#  Module : Flashes a build's four parts at their own offsets, so
#           NVS (saved machine, keyboard layout, speed) survives.
#           The merged release image written at 0x0 wipes it.
# ============================================================
#
# Usage:  tools/flash-parts.sh <arduino-cli output dir> [port]
#
set -euo pipefail

DIR="${1:?usage: tools/flash-parts.sh <build dir> [port]}"
PORT="${2:-/dev/ttyACM0}"
ESPTOOL="$(ls -1 "$HOME"/.arduino15/packages/esp32/tools/esptool_py/*/esptool.py | sort -V | tail -1)"
BOOT_APP0="$(ls -1 "$HOME"/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin | sort -V | tail -1)"

python3 "$ESPTOOL" --chip esp32 --port "$PORT" --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq keep --flash_size keep \
  0x1000  "$DIR"/ESP32-VGA_AppleII_Emulator.ino.bootloader.bin \
  0x8000  "$DIR"/ESP32-VGA_AppleII_Emulator.ino.partitions.bin \
  0xe000  "$BOOT_APP0" \
  0x10000 "$DIR"/ESP32-VGA_AppleII_Emulator.ino.bin
