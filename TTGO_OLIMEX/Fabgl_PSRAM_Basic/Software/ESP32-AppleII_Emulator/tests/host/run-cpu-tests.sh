#!/usr/bin/env bash
#
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : run-cpu-tests.sh
#  Module : Builds the host-side CPU test (tests/host/cpu_test.cpp
#           plus the real src/AppleII/AppleCpu*.cpp) with the host
#           g++ and runs Klaus Dormann's functional test images
#           against it: the NMOS 6502 suite, and the 65C02 suite
#           with the Rockwell bit instructions enabled.
# ============================================================
#
# Usage:  tests/host/run-cpu-tests.sh
#
# The test images are GPL-3 and are not committed; they are fetched from a
# pinned commit into tests/host/.cache/ and checked against known SHA-256s.
# The prebuilt 65C02 image expects Rockwell RMB/SMB/BBR/BBS, hence the
# cmos-rockwell variant; the enhanced IIe itself runs with them as NOPs.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CACHE="$HERE/.cache"
BUILD="$HERE/build"

KLAUS_COMMIT="7954e2dbb49c469ea286070bf46cdd71aeb29e4b"
KLAUS_URL="https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/$KLAUS_COMMIT/bin_files"

# image  sha256  success-trap  cpu-variant
TESTS=(
  "6502_functional_test.bin        fa12bfc761e6f9057e4cc01a665a7b800ff01ae91f598af1e39a1201d01953fd 3469 nmos"
  "65C02_extended_opcodes_test.bin 10a2a07fa240666fa610c46accebe8d42b1000feef3aae619da15a8d152869b2 24f1 cmos-rockwell"
)

mkdir -p "$CACHE" "$BUILD"

for t in "${TESTS[@]}"; do
  set -- $t
  if [ ! -f "$CACHE/$1" ]; then
    echo "==> Fetching $1"
    curl -fsSL -o "$CACHE/$1" "$KLAUS_URL/$1"
  fi
  echo "$2  $CACHE/$1" | sha256sum -c --quiet - || { echo "error: $1 checksum mismatch"; exit 1; }
done

echo "==> Building cpu_test"
g++ -std=gnu++17 -O2 -Wall -Wno-unused-variable \
  -I "$HERE/shim" -I "$ROOT/src/AppleII" \
  -o "$BUILD/cpu_test" \
  "$HERE/cpu_test.cpp" "$ROOT/src/AppleII/AppleCpu.cpp" "$ROOT/src/AppleII/AppleCpu65C02.cpp"

status=0
for t in "${TESTS[@]}"; do
  set -- $t
  # the core logs every BRK and unknown opcode; keep only the verdict
  "$BUILD/cpu_test" "$CACHE/$1" "$3" "$4" | grep -E '^(PASS|FAIL)' || status=1
done
exit $status
