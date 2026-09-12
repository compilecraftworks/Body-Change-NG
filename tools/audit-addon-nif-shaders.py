"""Read-only SSE NIF shader inventory (20.2.0.7 / user 12 / BS 100 only).

Layout reference: https://github.com/niftools/nifxml/blob/master/nif.xml
Reads declared block boundaries; does not guess offsets by searching strings.
No mesh editing, conversion, game access, or runtime dependencies.
This inventories shader blocks, not the MO2 winning-file graph or ESP routing.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        if count < 0 or self.pos + count > len(self.data):
            raise ValueError('Truncated NIF field')
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def number(self, fmt='I'):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))[0]

    def count(self, maximum=100000):
        value = self.number()
        if value > maximum:
            raise ValueError('NIF count exceeds audit bound')
        return value

    def string(self):
        return self.take(self.count(65536)).decode('utf-8', errors='replace')


def inspect(data):
    if len(data) > 64 * 1024 * 1024:
        raise ValueError('NIF exceeds 64 MiB audit bound')
    r = Reader(data)
    line = data.find(b'\n', 0, 100)
    if line < 0 or r.take(line + 1) != b'Gamebryo File Format, Version 20.2.0.7\n':
        raise ValueError('Not an SSE NIF header')
    version, endian, user, count, bs = (r.number(), r.number('B'), r.number(), r.count(), r.number())
    if (version, endian, user, bs) != (0x14020007, 1, 12, 100):
        raise ValueError(f'Unsupported NIF version {(version, endian, user, bs)}')
    for _ in range(3):
        r.take(r.number('B'))
    types = [r.string() for _ in range(r.number('H'))]
    indices = [r.number('H') for _ in range(count)]
    sizes = [r.count(64 * 1024 * 1024) for _ in range(count)]
    string_count = r.count()
    r.count(65536)  # maximum string length
    strings = [r.string() for _ in range(string_count)]
    for _ in range(r.count()):
        r.number()
    blocks = []
    for index, size in zip(indices, sizes):
        if index >= len(types):
            raise ValueError('Invalid block type')
        blocks.append((types[index], r.take(size)))
    roots = [r.number('i') for _ in range(r.count())]
    if r.pos != len(data) or any(root < -1 or root >= count for root in roots):
        raise ValueError('Invalid footer or unconsumed data')
    shapes = []
    shape_kinds = {'BSTriShape', 'BSSubIndexTriShape', 'BSDynamicTriShape'}
    for block_index, (kind, raw) in enumerate(blocks):
        if kind not in shape_kinds:
            continue
        shape = Reader(raw)
        name = shape.number('i')
        for _ in range(shape.count()):
            shape.number('i')
        shape.number('i')  # controller
        shape.number()  # flags
        shape.take(12 + 36 + 4)  # translation, rotation, scale
        shape.number('i')  # collision object
        shape.take(16)  # bounding sphere
        shape.number('i')  # skin instance
        shader_index = shape.number('i')
        shape.number('i')  # alpha property
        if name < -1 or name >= len(strings):
            raise ValueError('Invalid shape name reference')
        if shader_index < -1 or shader_index >= count:
            raise ValueError('Invalid shape shader reference')
        shapes.append(dict(index3D=len(shapes), block=block_index,
                           name=strings[name] if name >= 0 else '',
                           shaderBlock=shader_index))
    result = []
    for index, (kind, raw) in enumerate(blocks):
        if kind != 'BSLightingShaderProperty':
            continue
        shader = Reader(raw)
        shader_type, name = shader.number(), shader.number('i')
        for _ in range(shader.count()):
            shader.number('i')
        shader.number('i')  # controller
        flags1, flags2 = shader.number(), shader.number()
        shader.take(16)  # UV offset and scale
        texture_index = shader.number('i')
        textures = []
        if texture_index != -1:
            if not 0 <= texture_index < count or blocks[texture_index][0] != 'BSShaderTextureSet':
                raise ValueError('Invalid shader texture reference')
            tr = Reader(blocks[texture_index][1])
            textures = [tr.string() for _ in range(tr.count(32))]
            if tr.pos != len(tr.data):
                raise ValueError('Texture block not fully consumed')
        if name < -1 or name >= len(strings):
            raise ValueError('Invalid shader name reference')
        owners = [dict(index3D=shape['index3D'], block=shape['block'], name=shape['name'])
                  for shape in shapes if shape['shaderBlock'] == index]
        result.append(dict(block=index, name=strings[name] if name >= 0 else '', owners=owners,
                           shaderType=shader_type, flags1=f'{flags1:08X}', flags2=f'{flags2:08X}',
                           nativeSkinFlag=bool(flags1 & (0x200000 | 0x400)), textures=textures))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='+', type=Path)
    args = parser.parse_args()
    rows = []
    for path in args.paths:
        try:
            raw = path.read_bytes()
            rows.append(dict(path=str(path), sha256=hashlib.sha256(raw).hexdigest(), shaders=inspect(raw)))
        except (ValueError, OSError) as error:
            rows.append(dict(path=str(path), error=str(error)))
    print(json.dumps(rows, ensure_ascii=False, indent=2))
    raise SystemExit(any('error' in row for row in rows))
