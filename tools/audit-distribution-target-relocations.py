"""Read-only Address Library coverage audit for the distribution metadata guard.

Checks the 12 supported SE/AE databases; this is not an in-game ABI validation.
Uses the repository's existing binary database reader, with no downloads.
"""
import argparse
import importlib.util
from pathlib import Path
import re

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database_directory', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('appearance_audit', Path(__file__).with_name('audit-appearance-runtime.py'))
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    header = (root / 'third_party/CommonLibSSE-NG/include/RE/Offsets_RTTI.h').read_text(encoding='utf-8')
    ids = {}
    for name in ('BaseFormComponent','TESForm','TESFullName','TESFaction','TESRace','TESClass','BGSKeyword'):
        match = re.search(r'RTTI_' + name + r'\((\d+), (\d+),', header)
        assert match, name
        ids[name] = tuple(map(int, match.groups()))
    ids.update(RTDynamicCast=(102238,109689), formMap=(514351,400507),
               formMapLock=(514360,400517), dataHandler=(514141,400269))
    versions = [(1,5,97,0)] + [(1,6,patch,0) for patch in (317,318,323,342,353,629,640,659,1130,1170,1179)]
    for version in versions:
        branch = int(version[1] == 6)
        filename = ('versionlib-' if branch else 'version-') + '-'.join(map(str,version)) + '.bin'
        actual, mapping = audit.library(args.database_directory / filename)
        assert actual == version, (actual,version)
        missing = [name for name, pair in ids.items() if not mapping.get(pair[branch])]
        assert not missing, (version,missing)
        print('.'.join(map(str,version)), 'PASS', len(ids), 'relocations')

if __name__ == '__main__':
    main()
