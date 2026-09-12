"""Read-only audit of installed SE 1.5.97 Copy/duplicate virtuals.

Address Library format decoding follows vendored REL/IDDB.cpp. No process
attachment, engine writes, or binary patching. Prints table/function RVAs.
Steam's on-disk code may be packed: pointer equality, not code bytes, is audited.
"""
import argparse
import io
import struct
from pathlib import Path


def audit(executable, library):
    stream = io.BytesIO(Path(library).read_bytes())
    def read(fmt):
        return struct.unpack('<' + fmt, stream.read(struct.calcsize('<' + fmt)))[0]
    assert read('I') == 1, 'This audit requires SE Address Library format 1'
    version = [read('I') for _ in range(4)]
    assert version == [1, 5, 97, 0], version
    stream.read(read('I'))
    pointer_size, count = read('I'), read('I')
    def delta(kind, previous):
        if kind == 0: return read('Q')
        if kind == 1: return previous + 1
        if kind == 2: return previous + read('B')
        if kind == 3: return previous - read('B')
        if kind == 4: return previous + read('H')
        if kind == 5: return previous - read('H')
        if kind == 6: return read('H')
        if kind == 7: return read('I')
        raise ValueError(kind)
    mapping, ident, offset = {}, 0, 0
    for _ in range(count):
        code = read('B')
        ident = delta(code & 15, ident)
        high = code >> 4
        offset = delta(high & 7, offset // pointer_size if high & 8 else offset)
        if high & 8: offset *= pointer_size
        mapping[ident] = offset
    data = Path(executable).read_bytes()
    def at(fmt, pos): return struct.unpack_from('<' + fmt, data, pos)[0]
    pe = at('I', 0x3c)
    base = at('Q', pe + 24 + 24)
    section_table = pe + 24 + at('H', pe + 20)
    sections = []
    for i in range(at('H', pe + 6)):
        pos = section_table + 40 * i
        sections.append((at('I', pos+12), at('I', pos+16), at('I', pos+20)))
    def raw(rva):
        for va, size, start in sections:
            if va <= rva < va+size: return start+rva-va
        raise ValueError(hex(rva))
    base_copy = at('Q',raw(mapping[231469])+47*8)-base
    for name, ident, slots in [
        ('TESForm',231469,[9,47]), ('ARMO',234078,[9,47]),
        ('ARMA',234039,[9,47]), ('TXST',236685,[9,47]),
        ('FLST',236407,[9,47]), ('ModelSwap',231588,[3]),
        ('TXST-paths',236686,[0x25,0x26,0x27]), ('TESTexture',231661,[5,6])]:
        print(name, 'vtable RVA', hex(mapping[ident]))
        for slot in slots:
            rva = at('Q',raw(mapping[ident])+slot*8)-base
            print(' slot',hex(slot),'RVA',hex(rva),
                'inherits TESForm::Copy' if slot == 47 and rva == base_copy else '')
            if name in ['ARMA','TXST','FLST'] and slot == 47:
                assert rva == base_copy


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('library')
    args = parser.parse_args()
    audit(args.executable,args.library)
