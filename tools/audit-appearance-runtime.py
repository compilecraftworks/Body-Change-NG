"""Read-only native face/callback audit of installed PE files and Address Libraries.

Writes bounded diagnostic disassembly under build/audits, never game files.
No process loading, injection or engine calls. Steam-packed code is NOT evidence
of its decoded instruction stream; the report explicitly exposes that limitation.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import importlib.util
import io
import json
import struct
from pathlib import Path

spec = importlib.util.spec_from_file_location("face_audit", Path(__file__).with_name("audit-native-face-code.py"))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def library(path):
    data = path.read_bytes()
    stream = io.BytesIO(data)
    def read(fmt):
        return struct.unpack("<" + fmt, stream.read(struct.calcsize("<" + fmt)))[0]
    assert read("I") in (1, 2)
    version = tuple(read("I") for _ in range(4))
    stream.read(read("I"))
    size, count = read("I"), read("I")
    assert size == 8
    def delta(kind, old):
        if kind == 0: return read("Q")
        if kind == 1: return old + 1
        if kind == 2: return old + read("B")
        if kind == 3: return old - read("B")
        if kind == 4: return old + read("H")
        if kind == 5: return old - read("H")
        if kind == 6: return read("H")
        if kind == 7: return read("I")
        raise ValueError(kind)
    result, ident, offset = {}, 0, 0
    for _ in range(count):
        code = read("B")
        ident = delta(code & 15, ident)
        high = code >> 4
        offset = delta(high & 7, offset // size if high & 8 else offset)
        if high & 8: offset *= size
        result[ident] = offset
    return version, result


def function_owner(pe, rva):
    start, length = pe.exceptions
    raw = pe.raw(start)
    rows = [struct.unpack_from("<III", pe.data, p) for p in range(raw,raw+length,12)]
    row = next(row for row in rows if row[0] <= rva < row[1])
    seen = set()
    while True:
        begin, _, unwind = row
        assert unwind not in seen
        seen.add(unwind)
        raw = pe.raw(unwind)
        if not (pe.data[raw] >> 3) & 4: return begin
        codes = pe.data[raw+2]
        row = struct.unpack_from("<III",pe.data,raw+4+((codes+1)&~1)*2)


def inspect(exe, db, output):
    data = exe.read_bytes()
    pe = audit.PE(data)
    version, mapping = library(db)
    target = mapping[24228 if version[1] == 5 else 24732]
    functions, callers = {}, []
    def capture(rva, label):
        begin, end = pe.function(rva)
        raw = pe.raw(begin)
        functions[begin] = {"name": label, "rva": begin, "end": end,
                            "hex": data[raw:raw + end - begin].hex()}
        return begin
    capture(target, "face_attach")
    for va, _, raw, length, flags in pe.sections:
        if not flags & 0x20000000: continue
        code = data[raw:raw + length]
        for i, op in enumerate(code[:-4]):
            if op != 0xE8: continue
            site = va + i
            if site + 5 + struct.unpack_from("<i", code, i + 1)[0] != target: continue
            try: begin = capture(site, "caller")
            except ValueError: continue
            callers.append({"site": hex(site), "begin": hex(begin), "offset": hex(site-begin),
                            "ids": [k for k,v in mapping.items() if v == begin]})
    output.mkdir(parents=True, exist_ok=True)
    report = {"exe": str(exe), "sha256": hashlib.sha256(data).hexdigest(),
              "version": version, "target": hex(target), "callers": callers,
              "note": "Disk bytes; packed instructions are not a live-code validation"}
    (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    audit.disassemble(list(functions.values()), pe.image_base, output)
    print(json.dumps(report, indent=2))


def inspect_live(exe, db, output, pid):
    """User-authorized main-menu audit: bounded executable image reads only."""
    disk = exe.read_bytes()
    pe = audit.PE(disk)
    version, mapping = library(db)
    assert exe.resolve() == Path(r"C:\TAKEALOOK\Stock Game\SkyrimSE.exe")
    assert version == (1, 6, 1170, 0)
    kernel = ct.WinDLL("kernel32", use_last_error=True)
    psapi = ct.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
    kernel.OpenProcess.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = (wt.HANDLE,)
    kernel.ReadProcessMemory.argtypes = (wt.HANDLE, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t))
    psapi.GetModuleFileNameExW.argtypes = (wt.HANDLE, wt.HMODULE, wt.LPWSTR, wt.DWORD)
    psapi.EnumProcessModules.argtypes = (wt.HANDLE, ct.POINTER(wt.HMODULE), wt.DWORD, ct.POINTER(wt.DWORD))
    process = kernel.OpenProcess(0x410, False, pid)
    assert process, ct.get_last_error()
    try:
        name = ct.create_unicode_buffer(32768)
        assert psapi.GetModuleFileNameExW(process, None, name, len(name))
        assert Path(name.value) == exe.resolve(), name.value
        modules = (wt.HMODULE * 1024)()
        needed = wt.DWORD()
        assert psapi.EnumProcessModules(process, modules, ct.sizeof(modules), ct.byref(needed))
        base = modules[0]
        def read(rva, size):
            assert 0 <= rva and rva + size <= pe.image_size
            result, count = ct.create_string_buffer(size), ct.c_size_t()
            assert kernel.ReadProcessMemory(process, base+rva, result, size, ct.byref(count)) and count.value == size
            return result.raw
        functions, callers = {}, []
        def capture(rva, label):
            owner = function_owner(pe,rva)
            if owner in functions: return owner
            for begin, end in pe.function_fragments(owner):
                functions[begin] = {"name": label, "rva": begin, "end": end,
                                    "hex": read(begin, end-begin).hex()}
            return owner
        target = mapping[24732]
        capture(target, "face_attach")
        # Temporary bounded text scan for one exact target. Do not retain the
        # entire image, heap or player data; save only the four caller ranges.
        for va, length, _, _, flags in pe.sections:
            if not flags & 0x20000000: continue
            assert length < 32*1024*1024
            code = read(va, length)
            for i, op in enumerate(code[:-4]):
                if op != 0xE8: continue
                site = va+i
                if site + 5 + struct.unpack_from("<i", code, i+1)[0] != target: continue
                try: begin = capture(site, "face_caller")
                except ValueError: continue
                callers.append({"site": hex(site), "begin": hex(begin), "offset": hex(site-begin),
                                "ids": [k for k,v in mapping.items() if v == begin]})
        # Versioned handle-policy vtable (official CommonLib AE ID) and the
        # material virtual implementations used by the face adapter.
        import re
        header = Path("third_party/CommonLibSSE-NG/include/RE/Offsets_VTABLE.h").read_text()
        for name, slots in (("SkyrimScript__HandlePolicy", [1,3,4]),
                            ("BSLightingShaderMaterialFacegen", [8,9]),
                            ("BSLightingShaderMaterialBase", [8,9])):
            match = re.search(r"VTABLE_"+name+r"\{\s*REL::VariantID\(\d+,\s*(\d+)", header)
            assert match, name
            table = mapping[int(match[1])]
            for slot in slots:
                pointer = struct.unpack("<Q", read(table+slot*8,8))[0]
                rva = pointer-base
                assert 0 <= rva < pe.image_size, (name, slot, hex(pointer))
                capture(rva, name+"_slot_"+str(slot))
        output.mkdir(parents=True, exist_ok=True)
        report = {"exe": str(exe), "sha256": hashlib.sha256(disk).hexdigest(), "version": version,
                  "pid": pid, "base": hex(base), "target": hex(target), "callers": callers,
                  "stage": "user-confirmed-main-menu", "access": "0x410 read-only", "functions": list(functions.values())}
        (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        audit.disassemble(list(functions.values()), base, output)
        print(json.dumps({k:v for k,v in report.items() if k != "functions"}, indent=2))
    finally:
        kernel.CloseHandle(process)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("library", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--main-menu-pid", type=int)
    args = parser.parse_args()
    if args.main_menu_pid:
        inspect_live(args.exe, args.library, args.output, args.main_menu_pid)
    else:
        inspect(args.exe, args.library, args.output)
