"""Read-only Address Library coverage; RVAs are not executable-code proof."""
import importlib.util
from pathlib import Path
import json

spec = importlib.util.spec_from_file_location('appearance', Path(__file__).with_name('audit-appearance-runtime.py'))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

ROOTS = [Path(r'D:\TuLED13E\File Mod Skyrim SE\mods\Address Library for SKSE Plugins\SKSE\Plugins'),
         Path(r'C:\TAKEALOOK\mods\Address Library for SKSE Plugins\SKSE\Plugins')]
REFERENCES = {
    'se': [0x1CCD50,0x12CF480,0x2D1980,0xC61A30,0x2D2074,0x2D11A0,0x2D2230,0x1CB555,0x1CC735],
    'ae': [0x219510,0x14B7920,0x3270A0,0xD27520,0x327784,0x3267F0,0x326680,0x217DC8,0x218EF3],
}
if __name__ == '__main__':
    for key, root, name in [('se', ROOTS[0], 'version-1-5-97-0.bin'),
                            ('ae', ROOTS[1], 'versionlib-1-6-1170-0.bin')]:
        version, mapping = audit.library(root / name)
        print(key, version)
        for rva in REFERENCES[key]:
            near = max(value for value in mapping.values() if value <= rva)
            ids = [ident for ident, value in mapping.items() if value == near]
            print(f'  {rva:08X}: ids={ids} offset=0x{rva-near:X}')
        for site in REFERENCES[key][-2:]:
            for path in (Path(__file__).resolve().parents[1]/'build/audits/addon-txst').glob('*/addon-code.json'):
                record=json.loads(path.read_text())
                if record['runtime'] != '.'.join(map(str,version)): continue
                if not any(f['rva'] <= site < f['end'] for f in record['functions']): continue
                text=path.with_name('native-face-code.disasm.txt').read_text().splitlines()
                token=f'{record["imageBase"]+site:016X}:'
                line=next((i for i,line in enumerate(text) if token in line),None)
                if line is not None:
                    print(path.parent.name, '\n'.join(text[line-10:line+6]))
                    break
