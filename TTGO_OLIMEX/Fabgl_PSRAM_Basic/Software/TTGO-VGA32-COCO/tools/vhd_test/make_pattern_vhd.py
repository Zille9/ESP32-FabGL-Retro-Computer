#!/usr/bin/env python3
"""Write a 1 MB pattern image for port_test.py: 4,096 sectors, each starting
with its own 24-bit LSN, the rest (LSN + byte index) & 0xFF.
Usage: make_pattern_vhd.py [TEST.VHD]"""
import sys

out = sys.argv[1] if len(sys.argv) > 1 else "TEST.VHD"
data = bytearray()
for n in range(4096):
    s = bytearray(((n + i) & 0xFF) for i in range(256))
    s[0:3] = n.to_bytes(3, "big")
    data += s
open(out, "wb").write(data)
print(out, len(data), "bytes")
