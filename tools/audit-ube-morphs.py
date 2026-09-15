"""Read-only UBE OSP / generated body TRI / preset XML audit.

Body TRI layout: BodySlide-and-Outfit-Studio/src/files/TriFile.cpp.
This checks assets and reconstruction math, NOT execution of the BCNG DLL.
Pass the installed SliderSets directory and generated meshes/!UBE directory.
Optional --presets directories are read recursively; nothing is rewritten.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import xml.etree.ElementTree as ET


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        if count < 0 or self.pos + count > len(self.data):
            raise ValueError('Truncated body TRI')
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def number(self, fmt):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))[0]

    def name(self):
        return self.take(self.number('B')).decode('utf-8')


def tri_names(path):
    raw = path.read_bytes()
    if len(raw) > 128 * 1024 * 1024:
        raise ValueError('Body TRI exceeds audit bound')
    reader = Reader(raw)
    if reader.take(4) != b'PIRT':
        raise ValueError('Not BodySlide PIRT body morph data')
    result = {'position': set(), 'uv': set()}
    for kind, stride in [('position', 8), ('uv', 6)]:
        if kind == 'uv' and reader.pos == len(raw):
            break
        count = reader.number('H')
        if count > 1024:
            raise ValueError('Unexpected TRI shape count')
        for _ in range(count):
            reader.name()
            for _ in range(reader.number('H')):
                name = reader.name()
                scale = reader.number('f')
                if not math.isfinite(scale):
                    raise ValueError('Non-finite TRI scale')
                vertices = reader.number('H')
                reader.take(vertices * stride)
                if vertices:
                    result[kind].add(name.casefold())
    if reader.pos != len(raw):
        raise ValueError('Unconsumed TRI bytes')
    return result, hashlib.sha256(raw).hexdigest()


def default(name, large):
    name = name.casefold()
    return 1.0 if name == 'nipplesshowup' or (name == 'skinnymorph' and not large) else 0.0


def audit(osp_root, meshes, preset_roots):
    all_names, build_only, rows, baselines = set(), set(), [], {}
    cases = 0
    for part, subfolder, output in [
        ('Body', 'Body', 'femalebody_tangent.tri'),
        ('Hands', 'Hands', 'femalehands_tangent.tri'),
        ('Feet', 'Feet', 'femalefeet_tangent.tri'),
        ('Preview', None, None),
    ]:
        path = osp_root / f'UBE SE 2.0 Release {part}.osp'
        xml = ET.parse(path).getroot().find('SliderSet')
        assert xml is not None
        names, zaps, defaults = set(), set(), []
        for slider in xml.findall('Slider'):
            name = slider.get('name')
            assert name and slider.get('invert', 'false').lower() == 'false', 'Audit new inverted UBE slider'
            low, high = (float(slider.get(side, '0')) / 100 for side in ('small', 'big'))
            assert low == default(name, False) and high == default(name, True), f'Changed build baseline: {name}'
            baselines[name.casefold()] = (low, high)
            if low or high:
                defaults.append({'name': name, 'small': low, 'big': high})
            if slider.get('zap', 'false') == 'true' and slider.get('uv', 'false') != 'true':
                zaps.add(name.casefold())
                continue
            names.add(name.casefold())
            for target in (-1.5, -.5, 0, .25, 1, 1.5, 2.5):
                for weight in (0, .25, .5, .73, 1):
                    baseline = low + (high - low) * weight
                    delta_low, delta_high = target - low, target + .3 - high
                    got = baseline + delta_low + (delta_high - delta_low) * weight
                    assert math.isclose(got, target + .3 * weight, abs_tol=1e-6)
                    cases += 1
        all_names |= names
        build_only |= zaps
        row = {'part': part, 'osp_sliders': len(xml.findall('Slider')), 'runtime_names': len(names),
               'nonzero_defaults': defaults, 'build_only_zaps': sorted(zaps),
               'osp_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
        if output:
            tri, sha = tri_names(meshes / subfolder / output)
            available = tri['position'] | tri['uv']
            row.update(tri_sha256=sha, position=len(tri['position']), uv=len(tri['uv']),
                       missing_from_tri=sorted(names - available), extra_tri_names=sorted(available - names))
        rows.append(row)
    presets = []
    for root in preset_roots:
        for path in sorted(root.rglob('*.xml')):
            xml = ET.parse(path).getroot()
            for preset in xml.findall('Preset'):
                if 'ube' not in (' '.join([preset.get('set', '')] +
                                         [g.get('name', '') for g in preset.findall('Group')])).casefold():
                    continue
                values, differences, unsupported = [], [], set()
                for slider in preset.findall('SetSlider'):
                    name = slider.get('name', '')
                    value = float(slider.get('value', '0')) / 100
                    values.append(value)
                    large = slider.get('size') == 'big'
                    if name.casefold() not in all_names and value != 0:
                        unsupported.add(name)
                    baseline = default(name, large)
                    if baseline:
                        differences.append({'name': name, 'size': slider.get('size'),
                                            'xml': value, 'runtime_delta': value - baseline})
                presets.append({'name': preset.get('name'), 'set': preset.get('set'),
                                'entries': len(values), 'empty_zeroed': not values,
                                'range': [min(values), max(values)] if values else [],
                                'changed_values': differences,
                                'nonzero_outside_body_hands_feet': sorted(unsupported)})
    return {'note': 'Read-only asset/math audit; not a compiled-plugin or in-game test',
            'parts': rows, 'unique_runtime_names': len(all_names), 'math_cases': cases, 'presets': presets}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('osp', type=Path)
    parser.add_argument('meshes', type=Path)
    parser.add_argument('--presets', action='append', type=Path, default=[])
    args = parser.parse_args()
    print(json.dumps(audit(args.osp, args.meshes, args.presets), ensure_ascii=False, indent=2))
