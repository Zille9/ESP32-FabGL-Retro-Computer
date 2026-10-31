#!/bin/bash
# Build and run the host-side GIME RAM-size test.
# usage: tools/gime_ram_test/run.sh
set -e
D=$(cd "$(dirname "$0")" && pwd)
OUT=${TMPDIR:-/tmp}/gime_ram_test
g++ -std=gnu++11 -O2 -Wall -Wno-unused-function -o "$OUT" "$D/gime_ram_test.cpp"
"$OUT" "$@"
