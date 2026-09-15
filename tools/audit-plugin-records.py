"""Read-only SE/AE plugin record inspection, including archive members.

This does not resolve a load order, execute scripts, extract to disk, or edit
plugins. Form IDs are file-local IDs, not runtime IDs.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zipfile
import zlib


def records(data, start=0, end=None, kinds=None):
    end = len(data) if end is None else end
    pos = start
    while pos < end:
        if pos + 24 > end:
            raise ValueError(f"Truncated record header at {pos}")
        kind = data[pos:pos + 4].decode("ascii")
        size, flags, form = struct.unpack_from("<III", data, pos + 4)
        stop = pos + size if kind == "GRUP" else pos + 24 + size
        if stop > end or stop < pos + 24:
            raise ValueError(f"Invalid {kind} extent at {pos}")
        if kind == "GRUP":
            yield from records(data, pos + 24, stop, kinds)
        else:
            if kinds is not None and kind not in kinds and kind != "TES4":
                yield kind, form, flags, []
                pos = stop
                continue
            payload = data[pos + 24:stop]
            if flags & 0x40000:
                expected, = struct.unpack_from("<I", payload)
                payload = zlib.decompress(payload[4:])
                if len(payload) != expected:
                    raise ValueError("Invalid compressed record size")
            yield kind, form, flags, list(subrecords(payload))
        pos = stop


def subrecords(data):
    pos = 0
    extended = None
    while pos < len(data):
        if pos + 6 > len(data):
            raise ValueError("Truncated subrecord")
        kind = data[pos:pos + 4].decode("ascii")
        size, = struct.unpack_from("<H", data, pos + 4)
        pos += 6
        if kind == "XXXX":
            if size != 4 or pos + 4 > len(data):
                raise ValueError("Invalid extended subrecord")
            extended, = struct.unpack_from("<I", data, pos)
            pos += 4
            continue
        if extended is not None:
            size, extended = extended, None
        if pos + size > len(data):
            raise ValueError("Subrecord exceeds record")
        yield kind, data[pos:pos + size]
        pos += size
    if extended is not None:
        raise ValueError("Dangling extended subrecord")


def string(data):
    return data.rstrip(b"\0").decode("utf-8", errors="replace")


def describe(record):
    kind, form, flags, fields = record
    out = {"type": kind, "form": f"{form:08X}", "flags": f"{flags:08X}"}
    for tag, value in fields:
        if tag in {"EDID", "FULL", "MAST", "MOD2", "MOD3", "MOD4", "MOD5"} or (kind == "TXST" and tag.startswith("TX")):
            out.setdefault(tag, []).append(string(value))
        elif tag in {"WNAM", "RNAM", "TPLT", "FTST", "NAM0", "NAM1", "NAM2", "NAM3", "ANAM", "MODL"} and len(value) == 4:
            out.setdefault(tag, []).append(f"{struct.unpack('<I', value)[0]:08X}")
        elif kind == "NPC_" and tag == "ACBS":
            out[tag] = value.hex()
            out["female"] = bool(struct.unpack_from("<I", value)[0] & 1)
            out["templateFlags"] = f"{struct.unpack_from('<H', value, 18)[0]:04X}"
        elif kind == "NPC_" and tag == "NAM7" and len(value) == 4:
            out["weight"] = struct.unpack("<f", value)[0]
    return out


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path")
    parser.add_argument("--member")
    parser.add_argument("--match", default="Eila")
    parser.add_argument("--types", default="NPC_,ARMO,ARMA,RACE,TXST")
    parser.add_argument("--forms", nargs="*", default=[])
    args = parser.parse_args()
    source = Path(args.path)
    if args.member:
        if source.suffix.lower() == ".zip":
            with zipfile.ZipFile(source) as archive:
                data = archive.read(args.member)
        else:
            data = subprocess.run(["tar", "-xOf", str(source), args.member],
                                  check=True, stdout=subprocess.PIPE).stdout
    else:
        data = source.read_bytes()
    items = list(records(data, kinds=set(args.types.split(","))))
    print(json.dumps({"source": str(source), "member": args.member,
                      "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "counts": dict(collections.Counter(r[0] for r in items)),
                      "header": describe(items[0])}, ensure_ascii=False))
    forms = {int(form, 16) for form in args.forms}
    for record in items:
        if record[0] not in args.types.split(","):
            continue
        info = describe(record)
        if record[1] in forms or args.match.lower() in json.dumps(info, ensure_ascii=False).lower():
            print(json.dumps(info, ensure_ascii=False))


if __name__ == "__main__":
    main()
