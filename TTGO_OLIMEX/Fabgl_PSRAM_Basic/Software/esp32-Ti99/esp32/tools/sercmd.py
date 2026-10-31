#!/usr/bin/env python3
"""Send commands to the emulator's serial console and print its output.

usage: sercmd.py <seconds-to-listen> [cmd | sleep:<secs>] ...
Opening the port resets the board, so start with sleep:4 and do a whole test in one call.
"""
import sys
import time
import serial

secs = float(sys.argv[1])
s = serial.Serial()
s.port = '/dev/ttyACM0'
s.baudrate = 115200
s.timeout = 0.2
s.dtr = False
s.rts = False
s.open()


def pump(duration):
    end = time.time() + duration
    while time.time() < end:
        d = s.read(4096)
        if d:
            sys.stdout.write(d.decode('utf-8', 'replace'))
            sys.stdout.flush()


for c in sys.argv[2:]:
    if c.startswith("sleep:"):
        pump(float(c[6:]))
        continue
    s.write((c + "\n").encode())
    pump(0.5)
pump(secs)
