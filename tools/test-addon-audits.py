"""Offline tests for the genital audit tools; no game process access."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import unittest


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


nif = load('audit-addon-nif-shaders')
refresh = load('check-addon-native-refresh-capture')
ownership = load('check-native-addon-ownership')


def u32(value):
    return struct.pack('<I', value)


def string(value):
    raw = value.encode('utf-8')
    return u32(len(raw)) + raw


def fixture(flags=0x82601303, texture_index=1, root=0):
    # Shader prefix only: this tool inventories texture references, not all
    # type-dependent shader payloads or the validity of a renderable mesh.
    shader = struct.pack('<IiIiII4fi', 5, -1, 0, -1, flags, 0x02008001,
                         0, 0, 1, 1, texture_index)
    texture = u32(2) + string('textures\\example\\malegenitals.dds') + string('')
    header = b'Gamebryo File Format, Version 20.2.0.7\n'
    header += struct.pack('<IBIII', 0x14020007, 1, 12, 2, 100) + b'\0\0\0'
    header += struct.pack('<H', 2) + string('BSLightingShaderProperty') + string('BSShaderTextureSet')
    header += struct.pack('<HHII', 0, 1, len(shader), len(texture))
    header += u32(0) * 3  # string count, max length, groups
    return header + shader + texture + u32(1) + struct.pack('<i', root)


def owned_shape_fixture():
    # Minimal block prefixes used by the read-only inventory. The geometry is
    # deliberately non-renderable; it only proves the native ARMA MODS key
    # material (0-based index3D + exact shape name + shader owner) is decoded.
    shape = struct.pack('<iIiI', 0, 0, -1, 0)
    shape += b'\0' * (12 + 36 + 4)
    shape += struct.pack('<i', -1) + b'\0' * 16
    shape += struct.pack('<iii', -1, 1, -1)
    shader = struct.pack('<IiIiII4fi', 5, -1, 0, -1, 0x82601303, 0x02008001,
                         0, 0, 1, 1, 2)
    texture = u32(2) + string('textures\\actors\\character\\female\\femalebody_etc_v2_1.dds') + string('')
    header = b'Gamebryo File Format, Version 20.2.0.7\n'
    header += struct.pack('<IBIII', 0x14020007, 1, 12, 3, 100) + b'\0\0\0'
    header += struct.pack('<H', 3)
    header += string('BSTriShape') + string('BSLightingShaderProperty') + string('BSShaderTextureSet')
    header += struct.pack('<HHHIII', 0, 1, 2, len(shape), len(shader), len(texture))
    header += u32(1) + u32(len('3BA_Vagina')) + string('3BA_Vagina') + u32(0)
    return header + shape + shader + texture + u32(1) + struct.pack('<i', 0)


class NifAuditTests(unittest.TestCase):
    def test_skin_channels(self):
        row, = nif.inspect(fixture())
        self.assertTrue(row['nativeSkinFlag'])
        self.assertEqual(row['textures'], ['textures\\example\\malegenitals.dds', ''])

    def test_not_every_shader_is_skin(self):
        row, = nif.inspect(fixture(flags=1))
        self.assertFalse(row['nativeSkinFlag'])

    def test_shape_owner_supplies_native_alternate_texture_key(self):
        row, = nif.inspect(owned_shape_fixture())
        self.assertEqual(row['owners'], [{
            'index3D': 0,
            'block': 0,
            'name': '3BA_Vagina',
        }])

    def test_absent_texture_set(self):
        row, = nif.inspect(fixture(texture_index=-1))
        self.assertEqual(row['textures'], [])

    def test_bad_references(self):
        for data in (fixture(texture_index=2), fixture(texture_index=0), fixture(root=2)):
            with self.subTest(data=data[-20:]), self.assertRaises(ValueError):
                nif.inspect(data)

    def test_truncations_and_trailing_data(self):
        data = fixture()
        for length in range(len(data)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                nif.inspect(data[:length])
        with self.assertRaises(ValueError):
            nif.inspect(data + b'\0')

    def test_other_version_rejected(self):
        data = fixture().replace(u32(100), u32(130), 1)
        with self.assertRaises(ValueError):
            nif.inspect(data)


class RefreshEvidenceTests(unittest.TestCase):
    runtime = 'ae'
    helper_rva = 0x2180E0
    mutation_rvas = (0x213794, 0x21377B + 2, 0x6E3E4C + 6)

    @classmethod
    def setUpClass(cls):
        path = Path(__file__).resolve().parents[1] / f'build/audits/addon-txst/{cls.runtime}-native-txst-refresh/addon-code.json'
        if not path.exists():
            raise unittest.SkipTest('Read-only runtime capture not present; NOT a runtime pass')
        cls.capture = json.loads(path.read_text(encoding='utf-8'))

    def test_full_capture(self):
        self.assertEqual(refresh.verify(self.capture), 27)

    def test_wrong_runtime_rejected(self):
        capture = copy.deepcopy(self.capture)
        capture['runtime'] = '1.5.97.0' if self.runtime == 'ae' else '1.6.1170.0'
        with self.assertRaises(ValueError):
            refresh.verify(capture)

    def test_missing_function_rejected(self):
        capture = copy.deepcopy(self.capture)
        capture['functions'] = [f for f in capture['functions'] if f['rva'] != self.helper_rva]
        with self.assertRaises(ValueError):
            refresh.verify(capture)

    def test_broken_post_reuse_route_rejected(self):
        # Test call removal, wrong global-byte target, and wrong flag value.
        for rva in self.mutation_rvas:
            capture = copy.deepcopy(self.capture)
            for function in capture['functions']:
                if function['rva'] <= rva < function['end']:
                    raw = bytearray.fromhex(function['hex'])
                    raw[rva - function['rva']] ^= 1
                    function['hex'] = raw.hex()
                    break
            with self.subTest(rva=hex(rva)), self.assertRaises(ValueError):
                refresh.verify(capture)


class SERefreshEvidenceTests(RefreshEvidenceTests):
    runtime = 'se'
    helper_rva = 0x1CB860
    mutation_rvas = (0x1C71A4, 0x1C718B + 2, 0x6510CA + 6)


class OwnershipEvidenceTests(unittest.TestCase):
    def test_both_runtime_pins(self):
        for runtime in ('se', 'ae'):
            self.assertEqual(ownership.verify(runtime, ownership.captures(runtime)), 15)

    def test_missing_evidence_fails(self):
        for runtime in ('se', 'ae'):
            with self.assertRaises(ValueError):
                ownership.verify(runtime, [])

    def test_wrong_runtime_fails(self):
        with self.assertRaises(ValueError):
            ownership.verify('se', ownership.captures('ae'))

    def test_changed_reference_operation_fails(self):
        for runtime, rva in (('se', 0x12CF4FC), ('ae', 0x14B799C)):
            records = ownership.captures(runtime)
            for record in records:
                for function in record['functions']:
                    if function['rva'] <= rva < function['end']:
                        data = bytearray.fromhex(function['hex'])
                        data[rva - function['rva']] ^= 1
                        function['hex'] = data.hex()
            with self.assertRaises(ValueError):
                ownership.verify(runtime, records)


if __name__ == '__main__':
    unittest.main()
