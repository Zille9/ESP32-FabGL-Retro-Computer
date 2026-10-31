#!/bin/bash
# Build and run the host-side CPU core tests (MC6809 and HD6309 builds).
# usage: tools/cpu_test/run.sh [fuzz_iterations]
#
# Set XROAR_SRC to an XRoar source tree to also compare the HD6309 against
# XRoar's hd6309.c, instruction by instruction:
#   XROAR_SRC=~/src/xroar tools/cpu_test/run.sh
set -e
D=$(cd "$(dirname "$0")" && pwd)
CORE="$D/../../src/core"
OUT=${TMPDIR:-/tmp}/cpu_test
mkdir -p "$OUT"
DEFS=""
OBJS=""
if [ -n "$XROAR_SRC" ]; then
    gcc -std=gnu11 -O2 -w -I"$XROAR_SRC" -I"$XROAR_SRC/src" -I"$XROAR_SRC/portalib" -I"$D" \
        -c "$D/xroar_oracle.c" -o "$OUT/xroar_oracle.o"
    DEFS="-DHAVE_XROAR_ORACLE"
    OBJS="$OUT/xroar_oracle.o"
fi
g++ -std=gnu++11 -O2 -Wall -Wno-unused-function $DEFS -I"$D/stub" -I"$D" \
    -o "$OUT/cpu_test" "$D/cpu_test.cpp" "$CORE/mc6809.cpp" "$CORE/hd6309.cpp" $OBJS
"$OUT/cpu_test" "$@"
