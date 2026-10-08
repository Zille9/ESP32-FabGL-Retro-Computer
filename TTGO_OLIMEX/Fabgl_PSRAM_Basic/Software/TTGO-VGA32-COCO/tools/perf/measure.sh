#!/bin/bash
# Print the latest perf_probe block and save core + framebuffer screenshots.
# usage: COCO_HOST=http://<ip> measure.sh <name> [prog.bas]
#   needs PERF_PROBE_ENABLED 1 and seriallog.py writing $PERF_LOG
#   WAIT=<s> settle time (default 15), OUT=<dir> screenshot dir (default ./shots)
D=$(dirname "$0"); H=${COCO_HOST:?set COCO_HOST=http://<board ip>}
LOG=${PERF_LOG:-/tmp/coco_serial.log}; OUT=${OUT:-shots}; mkdir -p "$OUT"
[ -n "$2" ] && "$D/runbas.sh" "$2"
sleep ${WAIT:-15}
echo "== $1"; tail -c 4000 "$LOG" | tr -d '\r' | grep -a -A6 "^--- perf" | tail -7
curl -s -m15 "$H/api/screenshot.png" -o "$OUT/$1.png"
curl -s -m15 "$H/api/screenshot.png?fb=1" -o "$OUT/$1_fb.png"
