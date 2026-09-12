"""Offline validation of native genital TXST ownership evidence and C++ pins.

Reads saved executable-code captures only. This is not an engine, renderer,
save/load, leak-detector or in-game integration test.
"""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FOLDERS = {
    'se': ('se-txst-loader-lifetime', 'se-txst-refcount',
           'se-material-texture-ownership', 'se-txst-constructor', 'se-txst-factory'),
    'ae': ('ae-txst-loader-lifetime', 'ae-txst-refcount', 'ae-txst-constructor'),
}


def captures(runtime):
    return [json.loads((ROOT / 'build/audits/addon-txst' / folder / 'addon-code.json').read_text())
            for folder in FOLDERS[runtime]]


def verify(runtime, records):
    expected = '1.5.97.0' if runtime == 'se' else '1.6.1170.0'
    if not records or any(r['runtime'] != expected or r['processAccessMask'] != '0x410'
                          for r in records):
        raise ValueError('Wrong capture scope')
    if len({r['exeSha256'] for r in records}) != 1:
        raise ValueError('Mixed executable identities')

    def read(rva, size):
        for record in records:
            for f in record['functions']:
                data = bytes.fromhex(f['hex'])
                if len(data) != f['end'] - f['rva']:
                    raise ValueError('Truncated function capture')
                if f['rva'] <= rva and rva + size <= f['end']:
                    return data[rva - f['rva']:rva - f['rva'] + size]
        raise ValueError(f'Uncaptured code {rva:X}')

    # Independently decoded actual instructions, not identical offsets assumed
    # between SE and AE: +0x30 subobject, +0x78 material owner, +8 refcount.
    semantics = ({
        0x2D1985: '488d4130',
        0x12CF4FC: 'f0ff4708', 0x12CF500: '48897b78',
        0x12CF50C: 'f00fc14108', 0x12CF519: 'ff5008',
        0x2D126E: 'f041ff4638',
        0x2D2074: '4883e930e9c3000000',
        0xC61A35: '488b01ba0100000048ff20',
    } if runtime == 'se' else {
        0x3270A2: '488d4130',
        0x14B799C: 'f0ff4708', 0x14B79A0: '48897b78',
        0x14B79AE: 'f00fc14108', 0x14B79BB: 'ff5008',
        0x3268EE: 'f0ff4738',
        0x327784: '4883e930e903000000',
        0xD27525: '488b01ba0100000048ff20',
    })
    for rva, encoded in semantics.items():
        if read(rva, len(bytes.fromhex(encoded))).hex() != encoded:
            raise ValueError(f'Ownership instruction mismatch {rva:X}')

    source = (ROOT / 'src/BodyChangeNG/NativeAddonRuntime.h').read_text()
    table = source.split(f'{runtime}OwnershipCode{{', 1)[1].split('};', 1)[0]
    pins = re.findall(r'CodeFingerprint\{\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+)ULL', table)
    if len(pins) != 7:
        raise ValueError('Incomplete runtime fingerprint table')
    for rva, size, expected_hash in pins:
        digest = 14695981039346656037
        for byte in read(int(rva, 16), int(size, 16)):
            digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        if digest != int(expected_hash, 16):
            raise ValueError(f'C++ code pin disagrees with capture at {rva}')
    return len(semantics) + len(pins)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', choices=('se', 'ae'), required=True)
    args = parser.parse_args()
    print(f'{args.runtime}: {verify(args.runtime, captures(args.runtime))} ownership/pin checks passed (offline only)')
