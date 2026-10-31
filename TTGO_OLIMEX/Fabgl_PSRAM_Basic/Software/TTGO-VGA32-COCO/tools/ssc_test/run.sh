#!/bin/bash
# Build and run the host-side tests for the Speech/Sound Cartridge port.
# usage: tools/ssc_test/run.sh
#
# VCC_SSC   VCC's ssc/ directory; when present the port is also run side by
#           side with VCC's original cores (default: the copy on this Pi).
# SSC_ROMS  directory holding pic-7040-510.bin and sp0256-al2.bin; when both
#           are present HELLO is spoken through the real firmware and written
#           to $OUT/ssc_hello.wav (default: /roms).
set -e
D=$(cd "$(dirname "$0")" && pwd)
SSC="$D/../../src/core/ssc"
OUT=${TMPDIR:-/tmp}/ssc_test
VCC_SSC=${VCC_SSC:-/home/pi/proyectos/coco-emu-other-projects-source/VCC/ssc}
SSC_ROMS=${SSC_ROMS:-/roms}
mkdir -p "$OUT"
FLAGS="-std=gnu++11 -O2 -Wall -Wextra -I$D/stub"
PORT="$SSC/tms7000.cpp $SSC/sp0256.cpp $SSC/ay8913.cpp"

DEFS=""
OBJS=""
if [ -f "$VCC_SSC/ssc_core.cpp" ]; then
    for f in ssc_core tms7000 sp0256 ay8913; do
        g++ -std=c++17 -O2 -w -Dssc=vcc_ssc -c "$VCC_SSC/$f.cpp" -o "$OUT/vcc_$f.o"
        OBJS="$OBJS $OUT/vcc_$f.o"
    done
    DEFS="-DHAVE_VCC_SSC -I$VCC_SSC"
else
    echo "VCC sources not found at $VCC_SSC: comparison skipped"
fi

# The VCC headers need C++17; our files are built as C++11 where VCC is not involved.
g++ ${FLAGS/gnu++11/gnu++17} $DEFS -o "$OUT/chips_test" "$D/chips_test.cpp" $PORT ${OBJS:+$OUT/vcc_tms7000.o}
"$OUT/chips_test"
# SSC_BATCH=1 runs the chips one scanline at a time, so the port can be
# compared with VCC tick for tick; the second build is the firmware's batching.
g++ ${FLAGS/gnu++11/gnu++17} $DEFS -DSSC_BATCH=1 -o "$OUT/glue_test" "$D/glue_test.cpp" "$SSC/ssc.cpp" $PORT $OBJS
g++ ${FLAGS/gnu++11/gnu++17} -o "$OUT/glue_test_batch" "$D/glue_test.cpp" "$SSC/ssc.cpp" $PORT
# The device build: the chips on their own thread (FreeRTOS stubs), under
# ThreadSanitizer to catch races between the two sides.
g++ ${FLAGS/gnu++11/gnu++17} -DSSC_CORE0 -O1 -g -fsanitize=thread -pthread \
    -o "$OUT/glue_test_core0" "$D/glue_test.cpp" "$SSC/ssc.cpp" $PORT
"$OUT/glue_test"
"$OUT/glue_test_batch"
"$OUT/glue_test_batch" random
TSAN_OPTIONS=halt_on_error=1 "$OUT/glue_test_core0"
TSAN_OPTIONS=halt_on_error=1 "$OUT/glue_test_core0" random
(cd "$OUT" && "$OUT/glue_test" "$SSC_ROMS/pic-7040-510.bin" "$SSC_ROMS/sp0256-al2.bin")
echo "ssc_test: all passed"
