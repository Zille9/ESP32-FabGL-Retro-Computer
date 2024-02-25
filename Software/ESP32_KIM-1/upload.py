#
# Sends paper tape data to '/dev/ttyUSB0'
# Usage: python3 upload.py path/to/file.ptf
#

import time
import sys

# paper tape
paper = ''

try:
    # read file
    with open(sys.argv[1]) as f:
        paper = f.read()

except:
    print('Usage: python3 send.py path/to/file.ptf');

# write to serial port
with open('/dev/ttyUSB0', 'w') as f:
    for c in paper:
        print(c, end='')
        f.write(c)
        if c == '\n': f.write('\r')
        time.sleep(0.01)
        
