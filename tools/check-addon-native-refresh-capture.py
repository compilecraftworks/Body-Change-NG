"""Offline SE 1597 / AE 1170 native TXST refresh checks; NOT gameplay tests.

Checks the *post-reuse* branch too: treating the partition-only visit as the
entire reuse path incorrectly implies that a new mesh must be created.
Never opens a process or writes game data.
"""
import argparse
import json
from pathlib import Path
import struct


def verify(capture):
    if capture['runtime'] not in ('1.5.97.0', '1.6.1170.0') or capture['processAccessMask'] != '0x410':
        raise ValueError('Unexpected capture scope')

    def read(rva, size):
        for function in capture['functions']:
            raw = bytes.fromhex(function['hex'])
            if len(raw) != function['end'] - function['rva']:
                raise ValueError('Incomplete function capture')
            if function['rva'] <= rva and rva + size <= function['end']:
                offset = rva - function['rva']
                return raw[offset:offset + size]
        raise ValueError(f'Uncaptured range {rva:X}')

    checks = {
        0x726F3A: '84db',                 # updateWeight guard before RemoveAllParts
        0x72708C: 'ba17000000',           # normal reset sets flags including 0x2
        0x6E3DFD: '40f6c501',             # armor update gate
        0x6E3E3D: '40f6c502',             # native texture refresh flag
        0x213730: '4b8b442c28',           # current BIPOBJECT TXST
        0x213735: '4b89842cd8130000',     # buffered TXST updated before reuse
        0x213784: '4b8b542c30',           # current geometry supplied to visitor
        0x2180ED: '4883c110',             # BIPOBJECT array in visitor context
        0x2180F1: '4489442420',           # selected biped slot in context
        0x2195B4: '48a900002000',         # native skin flag branch one
        0x21964E: '48a900040000',         # native skin flag branch two
        0x2195E9: '4c8b7c0118',           # selected slot's TXST read
        0x21968D: '488b5c0118',           # second branch TXST read
        0x219630: 'ff5340',               # material texture-load virtual call
    }
    if capture['runtime'] == '1.5.97.0':
        # Independently decoded from se-native-txst-refresh, not an AE offset.
        checks = {
            0x69313A: '84db',
            0x69327E: 'ba17000000',
            0x65107B: '40f6c501',
            0x6510BB: '40f6c502',
            0x1C7140: '4b8b442c28',
            0x1C7145: '4b89842cd8130000',
            0x1C7194: '4b8b542c30',
            0x1CB86D: '4883c110',
            0x1CB871: '4489442420',
            0x1CCDF2: '48a900002000',
            0x1CCE8C: '48a900040000',
            0x1CCE27: '4c8b7c0118',
            0x1CCECB: '488b5c0118',
            0x1CCE6E: 'ff5340',
        }
    for rva, expected in checks.items():
        if read(rva, len(bytes.fromhex(expected))).hex() != expected:
            raise ValueError(f'Instruction mismatch {rva:X}')

    # opcode bytes, instruction length, target; includes conditional branches.
    branches = {
        0x726F3C: ('0f84', 6, 0x727068),  # false skips RemoveAllParts
        0x6E3E59: ('e8', 5, 0x2E8510),
        0x2E8540: ('e8', 5, 0x3BDD00),
        0x3BE272: ('e8', 5, 0x213580),
        0x213782: ('74', 2, 0x213799),    # zero flag skips texture refresh
        0x213794: ('e8', 5, 0x2180E0),    # nonzero flag visits reused geometry
        0x218107: ('e8', 5, 0x218EC0),
        0x218EF3: ('e9', 5, 0x219510),
        0x219620: ('e8', 5, 0x3270A0),
    }
    if capture['runtime'] == '1.5.97.0':
        branches = {
            0x69313C: ('0f84', 6, 0x693264),
            0x6510D7: ('e8', 5, 0x2943B0),
            0x2943E0: ('e8', 5, 0x364FF0),
            0x36533C: ('e8', 5, 0x1C6F90),
            0x1C7192: ('74', 2, 0x1C71A9),
            0x1C71A4: ('e8', 5, 0x1CB860),
            0x1CB887: ('e8', 5, 0x1CC700),
            0x1CC735: ('e9', 5, 0x1CCD50),
            0x1CCE5E: ('e8', 5, 0x2D1980),
        }
    for rva, (opcode, size, target) in branches.items():
        raw = read(rva, size)
        op = bytes.fromhex(opcode)
        displacement = int.from_bytes(raw[len(op):], 'little', signed=True)
        if not raw.startswith(op) or rva + size + displacement != target:
            raise ValueError(f'Branch mismatch {rva:X}')

    # Prove the flag producer and consumer refer to the SAME byte. These are
    # RIP-relative instructions; equal-looking disassembly names are not enough.
    globals_used = []
    flag_sites = (
        (0x6E3E4C, b'\xc6\x05', 7),  # set byte to 1 before armor update
        (0x6E3E6C, b'\x88\x05', 6),  # restore after armor update
        (0x21377B, b'\x80\x3d', 7),  # read after reused geometry's partition visit
    )
    if capture['runtime'] == '1.5.97.0':
        flag_sites = (
            (0x6510CA, b'\xc6\x05', 7),
            (0x6510EA, b'\x88\x05', 6),
            (0x1C718B, b'\x80\x3d', 7),
        )
    for rva, prefix, size in flag_sites:
        raw = read(rva, size)
        if not raw.startswith(prefix):
            raise ValueError(f'Flag instruction mismatch {rva:X}')
        globals_used.append(rva + size + struct.unpack_from('<i', raw, 2)[0])
    if len(set(globals_used)) != 1:
        raise ValueError('Refresh flag producer/consumer mismatch')
    if read(flag_sites[0][0] + 6, 1) != b'\x01' or read(flag_sites[2][0] + 6, 1) != b'\x00':
        raise ValueError('Wrong refresh flag value')
    return len(checks) + len(branches) + 4


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', choices=('se', 'ae'), default='ae')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1] / f'build/audits/addon-txst/{args.runtime}-native-txst-refresh/addon-code.json'
    capture = json.loads(path.read_text(encoding='utf-8'))
    print(f'PASS: {verify(capture)} native refresh evidence checks (not gameplay tests)')
