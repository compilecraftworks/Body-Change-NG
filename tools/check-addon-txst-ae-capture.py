"""Verify the recorded 1170 code evidence, not in-game functionality.

Offline only: never opens a game process. Requires the ae-full audit capture.
"""
import json
from pathlib import Path
import struct


def main():
    path = Path(__file__).resolve().parents[1] / 'build/audits/addon-txst/ae-full/addon-code.json'
    capture = json.loads(path.read_text(encoding='utf-8'))
    if capture['runtime'] != '1.6.1170.0' or capture['processAccessMask'] != '0x410':
        raise RuntimeError('Wrong capture scope')

    def read(rva, size):
        for function in capture['functions']:
            if function['rva'] <= rva and rva + size <= function['end']:
                raw = bytes.fromhex(function['hex'])
                offset = rva - function['rva']
                return raw[offset:offset + size]
        raise RuntimeError(f'Uncaptured range: {rva:X}')

    checks = {
        0x277692: '4c8bbcf730010000',  # ARMA sex-specific TXST
        0x277782: '4c897c2420',        # fifth argument
        0x2134F4: '48897e10',          # BIPOBJECT.skinTexture
        0x21370F: '4b3b8c2cd0130000', # model reuse comparison
        0x213725: '4b39442c10',        # item reuse comparison
        0x213730: '4b8b442c28',        # new TXST
        0x213735: '4b89842cd8130000', # buffered TXST copy
        0x2195B4: '48a900002000',      # FaceGenRGBTint flag
        0x21964E: '48a900040000',      # FaceGen flag
        0x2195E9: '4c8b7c0118',        # first shader TXST read
        0x21968D: '488b5c0118',        # second shader TXST read
        0x219630: 'ff5340',            # material texture load virtual call
    }
    for rva, expected in checks.items():
        if read(rva, len(bytes.fromhex(expected))).hex() != expected:
            raise RuntimeError(f'Byte evidence mismatch: {rva:X}')
    branches = {
        0x277791: (0xE8, 0x213210),
        0x21376A: (0xE8, 0x218120),
        0x217DC8: (0xE8, 0x219510),
        0x218EF3: (0xE9, 0x219510),
    }
    for rva, (opcode, target) in branches.items():
        raw = read(rva, 5)
        if raw[0] != opcode or rva + 5 + struct.unpack_from('<i', raw, 1)[0] != target:
            raise RuntimeError(f'Branch evidence mismatch: {rva:X}')
    print(f'PASS: {len(checks) + len(branches)} AE capture checks (not gameplay tests)')


if __name__ == '__main__':
    main()
