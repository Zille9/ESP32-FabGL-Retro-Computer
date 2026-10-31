#!/usr/bin/env python3
"""Port-level tests of the virtual hard disk ($FF80-$FF86) over the debug API.
Writes through /api/inject go through machine_write(), as CPU stores do.

Mount the pattern image (make_pattern_vhd.py) on HD0 first, with the hard disk
on, and keep the OSD closed. Uses RAM at $5000 and $6000. LSN 100 is written
and then restored.

Usage: COCO_DEBUG_HOST=http://<board> port_test.py TEST.VHD"""
import json, os, sys, urllib.request, urllib.parse

HOST = os.environ.get("COCO_DEBUG_HOST", "http://coco3.local").rstrip("/")
IMG = sys.argv[1]           # host copy of the image mounted on HD0
ref = open(IMG, "rb").read()

def post(path, **kw):
    data = urllib.parse.urlencode(kw).encode()
    return json.load(urllib.request.urlopen(HOST + path, data, timeout=15))

def get(path, **kw):
    return json.load(urllib.request.urlopen(HOST + path + "?" + urllib.parse.urlencode(kw), timeout=15))

def poke(addr, hexdata):
    post("/api/inject", addr=addr, data=hexdata)

def peek(addr, n):
    return bytes.fromhex(get("/api/mem", addr=addr, len=n)["data"])

def cmd(c, lsn=None, buf=None):
    if buf is not None: poke(0xFF84, "%04X" % buf)
    if lsn is not None: poke(0xFF80, "%06X" % lsn)
    poke(0xFF83, "%02X" % c)
    return peek(0xFF83, 1)[0]

fails = 0
def check(name, ok, extra=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + (" " + extra if extra else ""))
    if not ok: fails += 1

BUF = 0x5000
def expect_sector(n):
    if n * 256 >= len(ref): return bytes(256)
    return ref[n * 256:(n + 1) * 256]

# Reads
for lsn in (0, 1, 2047, 4095):
    poke(BUF, "AA" * 256)
    st = cmd(0, lsn, BUF)
    got = peek(BUF, 256)
    check("read LSN %d" % lsn, st == 0 and got == expect_sector(lsn), "status=%02X" % st)

# Past end of image: zeros
poke(BUF, "AA" * 256)
st = cmd(0, 4096, BUF)
check("read past end zero-fills", st == 0 and peek(BUF, 256) == bytes(256), "status=%02X" % st)

# Registers read back
regs = peek(0xFF80, 7)
check("registers read back", regs[0:3] == bytes([0, 0x10, 0]) and regs[4:6] == bytes([0x50, 0]), regs.hex())

# Invalid command
check("invalid command -> FE", cmd(9) == 0xFE)

# Drive select: 2 ignored, 1 (empty) -> no disk
poke(0xFF86, "02")
check("$FF86=2 ignored", peek(0xFF86, 1)[0] == 0)
poke(0xFF86, "01")
check("empty drive -> 04", cmd(0, 0, BUF) == 0x04)
poke(0xFF86, "00")

# Write LSN 100 with a new pattern, read it back via a different buffer
pat = bytes((0xC3 ^ i) & 0xFF for i in range(256))
poke(0x6000, pat.hex())
st = cmd(1, 100, 0x6000)
check("write LSN 100", st == 0, "status=%02X" % st)
poke(BUF, "00" * 256)
st = cmd(0, 100, BUF)
check("read back LSN 100", st == 0 and peek(BUF, 256) == pat)

# Neighbours untouched
for lsn in (99, 101):
    cmd(0, lsn, BUF)
    check("neighbour LSN %d intact" % lsn, peek(BUF, 256) == expect_sector(lsn))

# Flush clears LSN and buffer address
st = cmd(2)
regs = peek(0xFF80, 7)
check("flush ok, clears LSN/buffer", st == 0 and regs[0:3] == bytes(3) and regs[4:6] == bytes(2), regs.hex())

# Restore LSN 100 so the image matches the host copy again
poke(0x6000, ref[100 * 256:101 * 256].hex())
check("restore LSN 100", cmd(1, 100, 0x6000) == 0)
cmd(2)

print("FAILURES:", fails)
sys.exit(1 if fails else 0)
