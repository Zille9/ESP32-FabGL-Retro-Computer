#!/usr/bin/env python3
# ============================================================
#        APPLE II Emulator for ESP32-TTGO-VGA
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
#   MIT License
# ============================================================
#  File   : capture-log.py
#  Module : Resets the board over USB serial, prints its log for a
#           while, then summarises it: FPS and speed % (the first 3 s
#           are dropped as boot noise) and, for a PERF_TRACE build,
#           the Karateka load time and total motor-on time.
# ============================================================
#
# Usage:  tools/capture-log.py [seconds=60] [port=/dev/ttyACM0]
import re
import sys
import time

import serial

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 60
port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM0"

s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = True                     # hold the ESP32 in reset
time.sleep(0.1)
s.rts = False

fps, speed = [], []
mount_ms = last_off_ms = on_since = None
motor_on_ms = 0
end = time.time() + secs
buf = b""
while time.time() < end:
    buf += s.read(4096)
    while b"\n" in buf:
        raw, buf = buf.split(b"\n", 1)
        line = raw.decode("ascii", "replace").rstrip()
        print(line, flush=True)
        m = re.search(r"FPS : (\d+)", line)
        if m:
            fps.append(int(m.group(1)))
        m = re.search(r"speed : (\d+)%", line)
        if m:
            speed.append(int(m.group(1)))
        m = re.search(r"\[perf\] mount ok at (\d+)", line)
        if m:
            mount_ms = int(m.group(1))
        m = re.search(r"\[perf\] motor ON at (\d+)", line)
        if m:
            on_since = int(m.group(1))
        m = re.search(r"\[perf\] motor off at (\d+)", line)
        if m:
            last_off_ms = int(m.group(1))
            if on_since is not None:
                motor_on_ms += last_off_ms - on_since
                on_since = None


def summary(name, values):
    steady = values[3:] or values
    if steady:
        print(f"== {name}: min {min(steady)} max {max(steady)} "
              f"mean {sum(steady) / len(steady):.1f} over {len(steady)} s")


summary("FPS", fps)
summary("speed %", speed)
if mount_ms is not None and last_off_ms is not None:
    print(f"== Karateka load: {last_off_ms - mount_ms} ms (mount -> last motor off), "
          f"motor on {motor_on_ms} ms")
