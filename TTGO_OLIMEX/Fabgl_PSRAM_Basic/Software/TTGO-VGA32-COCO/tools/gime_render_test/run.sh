#!/bin/bash
# Build and run the host-side GIME renderer equivalence test.
# usage: tools/gime_render_test/run.sh [trials]
set -e
D=$(cd "$(dirname "$0")" && pwd)
OUT=${TMPDIR:-/tmp}/gime_render_test
g++ -std=gnu++11 -O2 -Wall -Wno-unused-function -o "$OUT" "$D/gime_render_test.cpp"
"$OUT" "$@"
