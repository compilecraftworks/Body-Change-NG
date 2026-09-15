"""Read-only MO2 loose-plugin override trace for explicitly named form IDs.

Resolves enabled loose plugin providers in modlist priority order. Does not
execute patchers, inspect a save, start the game, or write into MO2. Runtime
changes and redirected/custom mod directories are outside this audit.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys
import zlib

spec = importlib.util.spec_from_file_location(
    "record_audit", Path(__file__).with_name("audit-plugin-records.py"))
record_audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(record_audit)


def read_exact(stream, count):
    result = stream.read(count)
    if len(result) != count:
        raise ValueError("Truncated plugin")
    return result


def canonical(form, masters, plugin):
    index = form >> 24
    names = masters + [plugin]
    return f"{names[index].lower()}|{form & 0xffffff:06X}" if index < len(names) else f"unresolved|{form:08X}"


def selected_records(path, targets):
    with path.open("rb") as stream:
        header = read_exact(stream, 24)
        if header[:4] != b"TES4":
            raise ValueError(f"Not a TES4 plugin: {path}")
        payload = read_exact(stream, struct.unpack_from("<I", header, 4)[0])
        masters = [record_audit.string(value) for tag, value in record_audit.subrecords(payload) if tag == "MAST"]
        plugin = path.name
        # Entire unrelated top-level groups are skipped, not decompressed.
        def walk(end):
            while stream.tell() < end:
                start = stream.tell()
                header = read_exact(stream, 24)
                kind = header[:4].decode("ascii")
                size, flags, form = struct.unpack_from("<III", header, 4)
                stop = start + size if kind == "GRUP" else start + 24 + size
                if stop > end or stop < start + 24:
                    raise ValueError("Record/group outside parent")
                if kind == "GRUP":
                    label, group_type = header[8:12], struct.unpack_from("<I", header, 12)[0]
                    if group_type != 0 or label in {b"NPC_", b"ARMO", b"ARMA", b"RACE", b"TXST"}:
                        yield from walk(stop)
                elif canonical(form, masters, plugin) in targets:
                    payload = read_exact(stream, size)
                    if flags & 0x40000:
                        expected, = struct.unpack_from("<I", payload)
                        payload = zlib.decompress(payload[4:])
                        if expected != len(payload):
                            raise ValueError("Bad decompression size")
                    record = (kind, form, flags, list(record_audit.subrecords(payload)))
                    info = record_audit.describe(record)
                    info["canonical"] = canonical(form, masters, plugin)
                    info["provider"] = str(path)
                    for tag in ("TPLT", "WNAM", "RNAM", "FTST", "ANAM", "MODL", "NAM0", "NAM1"):
                        if tag in info:
                            info[tag + "Resolved"] = [canonical(int(value, 16), masters, plugin) for value in info[tag]]
                    yield info
                stream.seek(stop)
        yield from walk(path.stat().st_size)


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mo2", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--target", action="append", required=True, help="Plugin.esp|local hex ID")
    args = parser.parse_args()
    root = Path(args.mo2)
    profile = root / "profiles" / args.profile
    mods = [line[1:].strip() for line in (profile / "modlist.txt").read_text(encoding="utf-8-sig").splitlines() if line.startswith("+")]
    dirs = [root / "overwrite"] + [root / "mods" / name for name in mods] + [Path(args.data)]
    providers = {}
    missing_dirs = []
    for folder in dirs:
        if not folder.is_dir():
            missing_dirs.append(str(folder))
            continue
        for candidate in folder.iterdir():
            if candidate.is_file() and candidate.suffix.lower() in {".esm", ".esp", ".esl"}:
                providers.setdefault(candidate.name.lower(), candidate)
    raw_plugins = (profile / "plugins.txt").read_text(encoding="utf-8-sig").splitlines()
    plugins = [line[1:].strip() for line in raw_plugins if line.startswith("*")]
    order = list(dict.fromkeys(name.lower() for name in
        ["Skyrim.esm", "Update.esm", "Dawnguard.esm", "HearthFires.esm", "Dragonborn.esm"] + plugins))
    targets = {f"{plugin.lower()}|{int(form, 16):06X}" for plugin, form in (target.split("|", 1) for target in args.target)}
    missing_plugins = []
    winners = {}
    count = 0
    for plugin in order:
        path = providers.get(plugin)
        if path is None:
            missing_plugins.append(plugin)
            continue
        for info in selected_records(path, targets):
            count += 1
            print(json.dumps({"override": info}, ensure_ascii=False))
            winners[info["canonical"]] = info
    print(json.dumps({"summary": {"listedPlugins": len(order), "matchedOverrides": count,
        "missingPlugins": missing_plugins, "missingDirectories": missing_dirs,
        "unresolvedTargets": sorted(targets - winners.keys()), "winners": winners}}, ensure_ascii=False))


if __name__ == "__main__":
    main()
