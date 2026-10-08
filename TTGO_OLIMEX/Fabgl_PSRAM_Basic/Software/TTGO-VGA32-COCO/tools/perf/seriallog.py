#!/usr/bin/env python3
"""Append the board's serial output to a file (perf_probe dumps land here).

usage: seriallog.py <logfile> [port]
Opening the port resets the board once. Stop this before `arduino-cli upload`
(a held port makes the upload fail with "Wrong boot mode").
"""
import sys
import serial

s = serial.Serial()
s.port = sys.argv[2] if len(sys.argv) > 2 else '/dev/ttyACM0'
s.baudrate = 115200
s.timeout = 0.5
s.dtr = False
s.rts = False
s.open()
with open(sys.argv[1], 'ab') as f:
    while True:
        d = s.read(4096)
        if d:
            f.write(d)
            f.flush()
