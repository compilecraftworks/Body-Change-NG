"""Read-only TOFU/TuLED SE/AE form-lifetime code capture at the main menu.

Reads only fixed module vtables and bounded executable code windows. Never
reads actor/heap/save data, executes engine functions, suspends or writes the
process. Extra callees must first be identified in the captured disassembly.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import importlib.util
import json
from pathlib import Path
import struct


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


audit = module("form_code", "audit-native-face-code.py")
owners = module("form_addresses", "audit-appearance-runtime.py")
PACKS = {
    "tofu": (r"D:\TOFU\Stock Game\SkyrimSE.exe",
             r"D:\TOFU\MO2\mods\Address Library for SKSE Plugins\SKSE\Plugins"),
    "tuled": (r"D:\TuLED\STOCK GAME\Skyrim Special Edition\SkyrimSE.exe",
              r"D:\TuLED\File Mod Skyrim SE\mods\Address Library for SKSE Plugins\SKSE\Plugins"),
}


def main(args):
    executable, libraries = PACKS[args.pack]
    audit.GAME = Path(executable)
    runtime = tuple(audit.file_version())
    profiles = {
        (1, 5, 97, 0): ("version-1-5-97-0.bin", (231469, 234078, 234039, 236685, 236407)),
        (1, 6, 1170, 0): ("versionlib-1-6-1170-0.bin", (187895, 189542, 189517, 191208, 190933)),
    }
    if runtime not in profiles:
        raise RuntimeError("No verified vtable profile for this runtime")
    library, table_ids = profiles[runtime]
    version, mapping = owners.library(Path(libraries) / library)
    if version != runtime:
        raise RuntimeError("Address Library version mismatch")
    disk = audit.GAME.read_bytes()
    pe = audit.PE(disk)
    kernel = ct.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    kernel.OpenProcess.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = [wt.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ct.POINTER(wt.DWORD)]
    kernel.ReadProcessMemory.argtypes = [wt.HANDLE, wt.LPCVOID, wt.LPVOID, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    psapi = ct.WinDLL("psapi", use_last_error=True)
    psapi.EnumProcessModules.argtypes = [wt.HANDLE, ct.POINTER(wt.HMODULE), wt.DWORD, ct.POINTER(wt.DWORD)]
    handle = kernel.OpenProcess(0x410, False, args.pid)  # QUERY_INFORMATION | VM_READ only
    if not handle:
        raise ct.WinError(ct.get_last_error())
    try:
        path, length = ct.create_unicode_buffer(32768), wt.DWORD(32768)
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, ct.byref(length)):
            raise ct.WinError(ct.get_last_error())
        if Path(path.value).resolve() != audit.GAME.resolve():
            raise RuntimeError("Not the scoped game process")
        modules, needed = (wt.HMODULE * 4096)(), wt.DWORD()
        if not psapi.EnumProcessModules(handle, modules, ct.sizeof(modules), ct.byref(needed)):
            raise ct.WinError(ct.get_last_error())
        if not 0 < needed.value <= ct.sizeof(modules):
            raise RuntimeError("Invalid module list")
        base = modules[0]

        def read(rva, size, code):
            if size <= 0 or size > 1024 or not any(
                va <= rva < rva + size <= va + span and
                (bool(flags & 0x20000000) if code else
                 bool(flags & 0x40000000) and not bool(flags & 0x80000000))
                for va, span, _, _, flags in pe.sections):
                raise RuntimeError("Read is not a bounded code/readonly-vtable window")
            buffer, count = ct.create_string_buffer(size), ct.c_size_t()
            if not kernel.ReadProcessMemory(handle, base + rva, buffer, size, ct.byref(count)):
                raise ct.WinError(ct.get_last_error())
            if count.value != size:
                raise RuntimeError("Incomplete module read")
            return buffer.raw

        functions, tables = {}, []

        def capture(rva, name):
            if rva in functions:
                return
            if len(functions) >= 40:
                raise RuntimeError("Capture limit exceeded")
            functions[rva] = dict(name=name, rva=rva, end=rva + 1024,
                                  boundedWindow=True, hex=read(rva, 1024, True).hex())

        for name, ident in zip(("TESForm", "ARMO", "ARMA", "TXST", "FLST"), table_ids):
            vtable = mapping[ident]
            data = read(vtable, 0x30 * 8, False)
            for slot, method in [(0, "destructor"), (9, "duplicate"), (0x2f, "copy")]:
                rva = struct.unpack_from("<Q", data, slot * 8)[0] - base
                if not 0 < rva < pe.image_size:
                    raise RuntimeError("Vtable target is outside the verified game image")
                tables.append(dict(type=name, tableId=ident, slot=slot, method=method, targetRva=rva))
                capture(rva, name + "_" + method)
        if len(args.callee) > 16:
            raise RuntimeError("Too many identified callees")
        for rva in args.callee:
            capture(rva, f"IdentifiedCallee_{rva:08X}")
        output = Path(__file__).resolve().parents[1] / "build/audits/form-lifetime" / args.label
        output.mkdir(parents=True, exist_ok=True)
        result = dict(runtime=".".join(map(str, runtime)), executable=str(audit.GAME),
                      exeSha256=hashlib.sha256(disk).hexdigest(), processId=args.pid,
                      processAccessMask="0x410", imageBase=base, vtables=tables,
                      functions=list(functions.values()))
        (output / "form-code.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        audit.disassemble(result["functions"], base, output)
        for row in tables:
            print(row["type"], row["method"], hex(row["targetRva"]))
        print(output)
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--pack", choices=PACKS, default="tofu")
    parser.add_argument("--callee", type=lambda value: int(value, 16), action="append", default=[])
    parser.add_argument("--label", default="initial")
    args = parser.parse_args()
    if not args.label or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in args.label):
        parser.error("label must be a simple lowercase directory name")
    main(args)
