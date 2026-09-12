"""Offline relocation coverage and saved-code verification, NOT 12-game playtests."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('generate', Path(__file__).with_name('generate-addon-relocation-patterns.py'))
generate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate)

def verify():
    generated = generate.generate()
    assert (ROOT/'src/BodyChangeNG/NativeAddonPatterns.h').read_text().strip() == generated['header'].strip()
    needed = {}
    for branch, callers in [('se', [15535, 15546]), ('ae', [15712, 15722])]:
        ids = set(callers)
        for proof in generated['evidence']:
            if proof['branch'] != branch: continue
            ids.add(proof['id'])
            ids.update(operand[2] for operand in proof['operands'] if operand[2] != generate.SLEEP_IMPORT)
        needed[branch] = ids
    versions = [(1,5,97,0)] + [(1,6,patch,0) for patch in
        [317,318,323,342,353,629,640,659,1130,1170,1179]]
    coverage = []
    for version in versions:
        branch = 'se' if version[1] == 5 else 'ae'
        filename = ('version-' if branch == 'se' else 'versionlib-') + '-'.join(map(str,version)) + '.bin'
        path = next(root/filename for root in generate.mapping.ROOTS if (root/filename).is_file())
        observed, mapping = generate.mapping.audit.library(path)
        assert observed == version
        missing = sorted(ident for ident in needed[branch] if not (0 < mapping.get(ident, 0) < 0xFFFFFFFF))
        assert not missing, (version, missing)
        coverage.append(dict(runtime='.'.join(map(str,version)), addressLibrary=str(path),
            sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            resolvedIDs=len(needed[branch]), executableCodeVerified=version in [(1,5,97,0),(1,6,1170,0)]))
    source = (ROOT/'src/BodyChangeNG/RelocatedCodePattern.h').read_text()
    def prefix(name):
        text = source.split(name+'{',1)[1].split('};',1)[0]
        return bytes(int(value,16) for value in re.findall(r'\\x([0-9A-F]{2})',text))
    sites = []
    for branch in ['se','ae']:
        version = '1.5.97.0' if branch == 'se' else '1.6.1170.0'
        visitor = generate.mapping.REFERENCES[branch][0]
        captured = generate.ownership.captures(branch)[0]
        pe = generate.mapping.audit.audit.PE(Path(captured['exe']).read_bytes())
        filename = ('version-' if branch == 'se' else 'versionlib-') + version.replace('.','-') + '.bin'
        _, ids = generate.mapping.audit.library(next(root/filename for root in generate.mapping.ROOTS if (root/filename).exists()))
        for index, site in enumerate(generate.mapping.REFERENCES[branch][-2:]):
            callerID = ([15535,15546] if branch == 'se' else [15712,15722])[index]
            start, length = pe.exceptions
            raw = pe.raw(start)
            owner = next(row for row in (struct.unpack_from('<III',pe.data,pos)
                for pos in range(raw,raw+length,12)) if row[0] <= ids[callerID] < row[1])
            assert owner[0] == ids[callerID] and owner[1] - owner[0] <= 0x4000 and owner[0] <= site < owner[1]
            before = prefix('directVisitorPrefix' if index == 0 else branch+'RecursiveVisitorPrefix')
            opcode = 0xE8 if index == 0 else 0xE9
            evidence = None
            for path in (ROOT/'build/audits/addon-txst').glob('*/addon-code.json'):
                record = json.loads(path.read_text())
                if record['runtime'] != version: continue
                for function in record['functions']:
                    if not (function['rva'] <= site-len(before) and site+5 <= function['end']): continue
                    data = bytes.fromhex(function['hex'])
                    offset = site-function['rva']
                    assert data[offset-len(before):offset] == before and data[offset] == opcode
                    assert site+5+struct.unpack_from('<i', data, offset+1)[0] == visitor
                    evidence = dict(runtime=version, site=hex(site), capture=str(path))
                    break
                if evidence: break
            assert evidence is not None
            sites.append(evidence)
    return dict(limitation='Only two runtimes have actual executable-code captures; other rows prove ID coverage only.',
        libraries=coverage, savedCallsites=sites, ownershipPatterns=len(generated['evidence']))

if __name__ == '__main__':
    print(json.dumps(verify(), indent=2))
