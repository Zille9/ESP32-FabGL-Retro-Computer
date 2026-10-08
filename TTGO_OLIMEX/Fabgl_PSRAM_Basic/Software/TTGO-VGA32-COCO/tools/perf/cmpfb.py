#!/usr/bin/env python3
"""Check the display framebuffer against the core's output for each scenario.

usage: cmpfb.py <shots dir> <name>...
<name>.png is /api/screenshot.png (core RGB565 line output); <name>_fb.png is
/api/screenshot.png?fb=1 (what FabGL scans out). After RGB222 truncation and
the HAL's centring / pixel-doubling they must match exactly.
"""
import sys
from PIL import Image

def q(im):
    return im.convert("RGB").point(lambda v: v >> 6)

bad = 0
for n in sys.argv[2:]:
    core = q(Image.open(f"{sys.argv[1]}/{n}.png"))
    fb = q(Image.open(f"{sys.argv[1]}/{n}_fb.png"))
    w, h = core.size
    if w * 2 == fb.size[0]:
        core = core.resize((w * 2, h), Image.NEAREST)
        w *= 2
    xo, yo = (fb.size[0] - w) // 2, (fb.size[1] - h) // 2
    reg = fb.crop((xo, yo, xo + w, yo + h))
    diff = sum(1 for a, b in zip(core.getdata(), reg.getdata()) if a != b)
    bad += diff
    print(f"{n}: core {w}x{h} at fb ({xo},{yo}): {diff} differing pixels")
sys.exit(1 if bad else 0)
