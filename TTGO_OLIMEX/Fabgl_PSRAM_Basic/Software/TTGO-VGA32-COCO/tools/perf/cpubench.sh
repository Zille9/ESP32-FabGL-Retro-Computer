#!/bin/bash
# CPU-core benchmark: frame / cpu_run / render_scn (us per frame) per scenario.
# usage: COCO_HOST=http://<ip> cpubench.sh [label]     (probe build + seriallog.py)
D=$(dirname "$0"); H=${COCO_HOST:?set COCO_HOST}; LOG=${PERF_LOG:-/tmp/coco_serial.log}
row(){ sleep ${2:-12}; tail -c 3000 "$LOG" | tr -d '\r' | grep -a -A3 "^--- perf" | tail -4 | \
  awk -v n="$1" '/avg frame/{f=$7} /cpu_run/{c=$2} /render_scn/{r=$2} END{printf "%-9s frame %6d  cpu %6d  render %5d\n", n, f, c, r}'; }
inject(){ curl -s -m5 -X POST $H/api/reset >/dev/null; sleep 4
  curl -s -m5 -X POST $H/api/inject -d "addr=0x4000&data=$1&pc=0x4000&resume=1" >/dev/null; }
echo "== ${1:-run}"
curl -s -m5 -X POST $H/api/reset >/dev/null; row idle 10
inject 20FE; row "bra*" 8                                  # BRA * : one instruction, no misses
inject "$(printf '12%.0s' $(seq 1 4093))7E4000"; row nopsled 8   # 4093 NOPs + JMP $4000
for p in pm4 w80 hs2; do "$D/runbas.sh" "$D/bench/$p.bas" >/dev/null; row $p ${WAITBAS:-30}; done
