"""Render read-native-texture-code.ps1 JSON with the pinned MSVC dumpbin.

Generates a non-runnable diagnostic .bin PE container for captured code only.
No external disassembler/library download; no process access or engine writes.
"""
import json
import struct
import subprocess
import sys
from pathlib import Path

capture = json.load(sys.stdin)
assert capture['runtime'] == '1.5.97'
pages = {}
for func in capture['functions']:
    data = bytes.fromhex(func['hex'])
    assert len(data) == 384 and 0 <= func['rva'] < 0x4000000
    for index, value in enumerate(data):
        rva = func['rva'] + index
        pages.setdefault(rva & ~0xfff, bytearray(0x1000))[rva & 0xfff] = value
pages = sorted(pages.items())
header = bytearray(0x400)
def put(fmt, offset, *values): struct.pack_into('<' + fmt, header, offset, *values)
put('H', 0, 0x5a4d)
put('I', 0x3c, 0x80)
put('IHHIIIHH', 0x80, 0x4550, 0x8664, len(pages), 0, 0, 0, 0xf0, 0x2022)
optional = 0x98
put('H', optional, 0x20b)
put('QII', optional + 24, capture['imageBase'], 0x1000, 0x200)
put('II', optional + 56, pages[-1][0] + 0x1000, 0x400)
put('H', optional + 68, 3)
for index, (rva, data) in enumerate(pages):
    pos = optional + 0xf0 + 40 * index
    assert pos + 40 <= len(header)
    header[pos:pos + 8] = (f'.tx{index}'.encode()).ljust(8, b'\0')
    put('IIII', pos + 8, 0x1000, rva, 0x1000, 0x400 + index * 0x1000)
    put('I', pos + 36, 0x60000020)
output = Path(__file__).resolve().parents[1] / 'build' / 'audits' / 'native-texture-code.bin'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(header + b''.join(data for _, data in pages))
dumpbin = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe')
for func in capture['functions']:
    start = capture['imageBase'] + func['rva']
    print(func['name'], hex(start), flush=True)
    subprocess.run([str(dumpbin), '/DISASM', f'/RANGE:0x{start:X},0x{start + 383:X}', str(output)], check=True)
