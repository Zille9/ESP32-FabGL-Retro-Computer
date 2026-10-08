#!/bin/bash
# Build and flash a dev image, optionally with extra -D flags, and restart the
# serial logger. Refuses to continue unless the upload verified.
# usage: flash.sh ["-DPERF_PROBE_ENABLED=1 -DFOO=2"]
set -e
D=$(cd "$(dirname "$0")" && pwd); SK=$(cd "$D/../.." && pwd)
F=esp32:esp32:esp32wrover:PartitionScheme=huge_app
B=${BUILD_DIR:-/tmp/coco_build_perf}; LOG=${PERF_LOG:-/tmp/coco_serial.log}; PORT=${PORT:-/dev/ttyACM0}
# EXTRA_PROPS: extra --build-property arguments (experiments only)
arduino-cli compile --fqbn $F --build-path "$B" --build-property "compiler.cpp.extra_flags=$1" $EXTRA_PROPS "$SK" 2>&1 | grep -E "error|Sketch uses" 
pkill -f '^python3 .*[s]eriallog.py' || true; sleep 1
# Capture the whole log: grep -q on the pipe would close it at the first
# "verified" (bootloader segment) and kill esptool before the app is written.
UP=$(arduino-cli upload --fqbn $F --input-dir "$B" -p $PORT "$SK" 2>&1) || true
[ "$(echo "$UP" | grep -c "Hash of data verified")" -ge 4 ] && echo "$UP" | grep -q "Leaving" \
    || { echo "UPLOAD FAILED"; echo "$UP" | tail -5; exit 1; }
: > "$LOG"; (setsid nohup python3 "$D/seriallog.py" "$LOG" $PORT >/dev/null 2>&1 &)
sleep ${BOOT_WAIT:-16}; echo "flashed [$1]"
