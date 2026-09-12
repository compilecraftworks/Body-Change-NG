"""Read-only MSVC RTTI/vtable inspection of an installed RaceMenu DLL."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
def at(fmt, pos): return struct.unpack_from('<'+fmt, data, pos)[0]
pe = at('I',0x3c)
base = at('Q',pe+48)
sections = []
start = pe+24+at('H',pe+20)
for i in range(at('H',pe+6)):
    p = start+40*i
    sections.append((at('I',p+12),at('I',p+16),at('I',p+20)))
def rva(pos):
    for va,size,raw in sections:
        if raw <= pos < raw+size: return va+pos-raw
    raise ValueError(pos)
needle = b'.?AVOverrideInterface@@'
pos = data.find(needle)
assert pos >= 16
td = rva(pos-16)
print('type descriptor',hex(td),'image base',hex(base))
pattern = struct.pack('<I',td)
pos = 0
while True:
    pos = data.find(pattern,pos+1)
    if pos < 0: break
    col = pos-12
    if col < 0 or at('I',col) != 1 or at('I',col+20) != rva(col) or at('I',col+4) != 0: continue
    print('COL',hex(rva(col)),'offset',at('I',col+4))
    vt = data.find(struct.pack('<Q',base+rva(col)))
    if vt < 0: continue
    print('vtable',hex(rva(vt+8)))
    for i in range(35): print(hex(i),hex(at('Q',vt+8+i*8)))
for argument in sys.argv[2:]:
    address = int(argument,16)
    for va,size,raw in sections:
        if va <= address < va+size:
            print('extra vtable',hex(address))
            for i in range(4): print(hex(i),hex(at('Q',raw+address-va+i*8)))
