#!/usr/bin/env python3
"""Build an RGB-DOS test VHD (see docs/vhd.md). RS-DOS drive n starts at LSN
$5A000 + n*630 (RGB-DOS 'CHANGES FOR EMULATOR BY R.GAULT SEP 2012'). Drives 4
and 5 are formatted and drive 4 holds an ASCII BASIC program, so DIR 4 and
LOAD"HELLO:4" work whether or not DRIVE OFF was typed. 95 MB, mostly zeros.
Usage: mkrgbvhd.py RGBTEST.VHD"""
import sys
OFFSET, DSK = 0x5A000, 630
def blank_disk():
    d = bytearray(b'\xff' * DSK * 256)
    fat = 17 * 18 + 1                       # track 17 sector 2
    d[fat*256:fat*256+256] = b'\xff' * 68 + bytes(188)
    return d
def add_ascii_basic(d, name, ext, text):
    data = text.encode()
    assert len(data) <= 256                 # one sector, granule 0
    d[0:len(data)] = data                   # granule 0 = track 0 sectors 1-9 = LSN 0
    fat = (17 * 18 + 1) * 256
    d[fat + 0] = 0xC1                       # last granule, 1 sector used
    e = (17 * 18 + 2) * 256                 # first directory sector (T17 S3)
    ent = name.ljust(8).encode() + ext.ljust(3).encode() + bytes([0x00, 0xFF, 0x00]) \
          + len(data).to_bytes(2, 'big') + bytes(16)
    d[e:e+32] = ent
out = sys.argv[1]
d4 = blank_disk()
add_ascii_basic(d4, "HELLO", "BAS", '10 PRINT "HELLO FROM THE VHD!"\r20 PRINT "RGB-DOS DRIVE 4 WORKS"\r')
d5 = blank_disk()
with open(out, "wb") as f:
    f.truncate((OFFSET + 4 * DSK) * 256)    # zeros up to drive 4
    f.seek((OFFSET + 4 * DSK) * 256)
    f.write(d4); f.write(d5)
print(out, (OFFSET + 6 * DSK) * 256, "bytes")
