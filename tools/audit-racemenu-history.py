"""Compare BCNG's used virtual prefixes with pinned official RaceMenu history.

Read-only: takes an existing official-source git checkout. This verifies order
and argument count, not binary ABI equivalence or in-game rendering.
"""
import argparse
import re
import subprocess
from pathlib import Path


def class_body(source, name):
    match = re.search(r'\bclass\s+' + re.escape(name) + r'\b[^;{]*\{', source)
    if not match: raise ValueError(name)
    start, depth = match.end(), 1
    for end in range(start, len(source)):
        if source[end] == '{': depth += 1
        elif source[end] == '}': depth -= 1
        if not depth: return source[start:end]
    raise ValueError('unclosed '+name)


def methods(source, name):
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    body = class_body(source, name)
    result, depth = [], 0
    for match in re.finditer(r'[{}]|\bvirtual\b[^;{}]+', body):
        token = match.group()
        if token == '{': depth += 1
        elif token == '}': depth -= 1
        elif depth == 0:
            method = re.search(r'(\w+)\s*\((.*?)\)', token, re.S)
            if method:
                key, params = method.groups()
                if key in ['GetVersion','Revert']: continue # inherited slots
                key = key.removeprefix('Reserved')
                result.append((key, 0 if not params.strip() else len(params.split(',')), token.strip()))
    return result


def main(checkout):
    def canonical_type(value):
        value = re.sub(r'=.*', '', value).strip().replace('RE::','')
        value = re.sub(r'(?<=[\s*&])([A-Za-z_]\w*)$',
            lambda m: m[0] if m[0] in ['char','bool','float','void'] else '',value).strip()
        replacements = {'UInt32':'u32','skee_u32':'u32','std::uint32_t':'u32',
            'UInt16':'u16','skee_u16':'u16','std::uint16_t':'u16',
            'UInt8':'u8','skee_u8':'u8','std::uint8_t':'u8',
            'SInt32':'i32','skee_i32':'i32','std::int32_t':'i32',
            'UInt64':'u64','skee_u64':'u64','std::size_t':'u64',
            'BSFixedString':'LegacyNodeName','OverrideVariant':'LegacyOverrideVariant'}
        for key,val in replacements.items(): value = re.sub(r'\b'+re.escape(key)+r'\b',val,value)
        if '*' not in value and '&' not in value: value = value.removeprefix('const ')
        return re.sub(r'\s+','',value)
    def parameters(method):
        params = re.search(r'\((.*?)\)',method[2],re.S).group(1)
        return [canonical_type(p) for p in params.split(',')] if params.strip() else []
    def result_type(method):
        return canonical_type(re.search(r'virtual\s+(.+?)\s+\w+\s*\(',method[2],re.S).group(1))
    def git(*args):
        return subprocess.check_output(['git','-c','safe.directory='+checkout,
            '-C',checkout,*args],text=True,encoding='utf-8')
    overlay = Path('src/BodyChangeNG/RaceMenuOverlay.cpp').read_text(encoding='utf-8')
    overlay += Path('src/BodyChangeNG/RaceMenuOverrideABI.h').read_text(encoding='utf-8')
    morph = Path('src/BodyChangeNG/RaceMenuBodyMorph.cpp').read_text(encoding='utf-8')
    # Include the intervening legacy refactors and GOG header, not just the
    # release labels on either side of the v1/public-v2 transition.
    commits = [
        '8f58265b7019c162dec7a6199a20721e66398214',
        '19fc48cb452c7a03bc742d01de079d31fc92198b',
        '86c890a7f89a3d31f3f80362b0b44f1d83d76fe3',
        '7ffff9afb35ce9cae5e26f8bdbf86367b75e29b8',
        '87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc',
        '867458e0d6a9f45dea5599bf694d6198332e3b0b',
        '4cf6d628e7a74e1204fc5bdc0fc4885de6481a03',
        'e779c68ce5449f713c7b551c7b7173208e7f83c0',
        'c1b408f363d645d354a8b548d064b3429ba1da5d',
        '8adc4b635ef2c6eef5befef3578ecabfb059a5ee',
        '7694eabfdf9e675cc28a8524afecb96aa5cd8a4b',
        '348607e9ae5f360ccab0d623e8b0d8f42e586fa6',
        '9ebcb733e17be695f994cd2e9cc383043446bc02']
    # Cover every intervening interface-header change on the pinned official lineage.
    changed = git('log', '--format=%H', '8f58265^..9ebcb733', '--',
        'skee/IPluginInterface.h', 'skee/OverlayInterface.h', 'skee/OverrideInterface.h',
        'skee64/IPluginInterface.h', 'skee64/OverlayInterface.h', 'skee64/OverrideInterface.h').splitlines()
    commits = list(dict.fromkeys(commits + changed))
    for commit in commits:
        files = git('ls-tree','-r','--name-only',commit).splitlines()
        header = next(p for p in files if p.endswith('/IPluginInterface.h'))
        prefix = header.rsplit('/',1)[0]
        public = git('show',commit+':'+header)
        modern = 'class IOverrideInterface' in public
        full = git('rev-parse',commit).strip()
        print(full, git('show','-s','--format=%s',commit).strip(), 'v2' if modern else 'v1')
        for kind, local_name, upstream_name in [
            ('Overlay','IOverlayInterfaceV2' if modern else 'IOverlayInterfaceV1',
                'IOverlayInterface' if modern else 'OverlayInterface'),
            ('Override','IOverrideInterfaceV2' if modern else 'IOverrideInterfaceV1',
                'IOverrideInterface' if modern else 'OverrideInterface'),
            ('BodyMorph','IBodyMorphInterface','IBodyMorphInterface')]:
            local = methods(morph if kind == 'BodyMorph' else overlay,local_name)
            original = public if modern or kind == 'BodyMorph' else git('show',commit+':'+prefix+'/'+kind+'Interface.h')
            upstream = methods(original,upstream_name)
            expected = [m[0] for m in upstream[:len(local)]]
            observed = [m[0] for m in local]
            assert observed == expected, (commit,kind,observed,expected)
            # Reserved/unused local entries intentionally have no typed args.
            for l,u in zip(local,upstream):
                if 'Reserved' not in l[2]:
                    assert parameters(l) == parameters(u), (commit,kind,l[0],parameters(l),parameters(u))
                    assert result_type(l) == result_type(u), (commit,kind,l[0],result_type(l),result_type(u))
            print(' ',kind, len(local),'prefix slots match; called parameters/returns match')
            if kind == 'Override':
                for m in upstream:
                    if m[0] in ['AddNodeOverride','GetNodeProperty']:
                        print('  ', ' '.join(m[2].split()))
        if modern:
            for visitor in ['GetVariant','SetVariant']:
                local, upstream = methods(overlay,visitor),methods(public,visitor)
                assert [(m[0],parameters(m),result_type(m)) for m in local] == [
                    (m[0],parameters(m),result_type(m)) for m in upstream], (commit,visitor)
            print('  v2 GetVariant/SetVariant virtual contracts match')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('checkout')
    main(parser.parse_args().checkout)
