#!/bin/bash
# Reset the CoCo, inject a tokenized BASIC program and auto-RUN it.
# usage: COCO_HOST=http://<ip> runbas.sh prog.bas
# Tokenizer + auto-run recipe: the basic-coder skill (references/injection.md).
H=${COCO_HOST:?set COCO_HOST=http://<board ip>}
TOK=${BASIC_TOKENIZER:-$(dirname "$0")/../../../.claude/skills/basic-coder/scripts/basic_tokenizer.py}
J=$(python3 "$TOK" "$1" --base 0x2601) || exit 1
HEX=$(echo "$J" | python3 -c 'import json,sys;print(json.load(sys.stdin)["program_hex"].replace(" ",""))')
END=$(echo "$J" | python3 -c 'import json,sys;print(json.load(sys.stdin)["end"][2:])')
curl -s -m5 -X POST $H/api/reset >/dev/null; sleep 4
w(){ curl -s -m5 -X POST $H/api/mem -d "addr=$1&data=$2" >/dev/null; }
curl -s -m5 -X POST $H/api/pause >/dev/null
w 0x2601 $HEX; w 0x2600 00; w 0x0019 2601; w 0x001B $END; w 0x001D $END; w 0x001F $END
w 0x02DD 8E00; w 0x00A6 02DC          # pre-crunched RUN in LINBUF, CHARAD = LINBUF-1
curl -s -m5 -X POST $H/api/registers -d "pc=0xADC0" >/dev/null
curl -s -m5 -X POST $H/api/resume >/dev/null
echo "injected $1 end=$END"
