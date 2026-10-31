#!/usr/bin/env python3
# speedcheck.py — check emulated CPU speed against wall-clock time.
#
# Reads /api/status speed.* and a guest 60 Hz tick counter twice, SECONDS
# apart. Real hardware advances the counter 59.94 ticks per second, so
# ticks / (59.94 * wall) is the real-time ratio, independent of the
# emulator's own cycle counter.
#
#   python3 tools/perf/speedcheck.py [--ip 192.168.50.104] [--seconds 30]
#        [--counter basic|ADDR] [--len 2]
#
# basic = Color BASIC TIMER at $0112-$0113 (16-bit, big-endian; it wraps at
# 65536 ticks, about 18 minutes, which this script handles once).
import argparse
import json
import time
import urllib.request

NTSC_FIELD_HZ = 14318180 / 912 / 262   # 59.94 Hz


def get(ip, path):
    with urllib.request.urlopen(f"http://{ip}{path}", timeout=5) as r:
        return json.load(r)


def read_counter(ip, addr, length):
    d = get(ip, f"/api/mem?addr={addr}&len={length}&space=cpu")
    return int(d["data"], 16)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", default="192.168.50.104")
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--counter", default="basic")
    ap.add_argument("--len", type=int, default=2)
    a = ap.parse_args()
    addr = 0x0112 if a.counter == "basic" else int(a.counter, 0)

    c0, t0 = read_counter(a.ip, addr, a.len), time.monotonic()
    time.sleep(a.seconds)
    c1, t1 = read_counter(a.ip, addr, a.len), time.monotonic()
    sp = get(a.ip, "/api/status")["speed"]

    ticks = (c1 - c0) % (1 << (8 * a.len))
    wall = t1 - t0
    ratio = ticks / (NTSC_FIELD_HZ * wall)
    print(f"counter ${addr:04X}: {ticks} ticks in {wall:.2f} s "
          f"-> {ticks / wall:.2f} Hz, real-time ratio {ratio:.3f}")
    print(f"speed: {sp['mhz']:.4f} MHz (target {sp['target_mhz']:.4f}, "
          f"ratio {sp['ratio']:.3f}), fps {sp['fps']}, rate {sp['rate']}, "
          f"work {sp['frame_us_avg']}/{sp['frame_us_max']} us per frame")


if __name__ == "__main__":
    main()
