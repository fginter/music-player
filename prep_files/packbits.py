#Processes the little-endian wave data as follows:
#1)shift right four bits
#2)prepend with 0011 (D/A header)

import struct
import sys

DAHeader=0x3000  #0011 0000 0000 0000
while True:
    twobytes=sys.stdin.read(2)
    if len(twobytes)==0:
        break
    elif len(twobytes)!=2:
        assert False
    sample=struct.unpack("<H",twobytes)[0] #unsigned 2-byte, little endian
    #divide by 4
    sample=sample >> 4
    sample=sample | DAHeader
    twobytes=struct.pack(">H",sample) #Big endian -> precisely how the data enters the D/A converter
    sys.stdout.write(twobytes)
sys.stdout.flush()

    

