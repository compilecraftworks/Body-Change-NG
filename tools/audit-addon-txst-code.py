"""Read-only TuLED 1.5.97 / TAKEALOOK 1.6.1170 armor/TXST audit.

Captures executable function ranges, never actor/heap/save data. No injection,
engine calls, process suspension or writes. Additional RVAs must be callees or
callers manually identified in the captured armor path. Not a runtime hook.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

spec = importlib.util.spec_from_file_location(
    "face_audit", Path(__file__).with_name("audit-native-face-code.py"))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
owner_spec = importlib.util.spec_from_file_location(
    "appearance_audit", Path(__file__).with_name("audit-appearance-runtime.py"))
owners = importlib.util.module_from_spec(owner_spec)
owner_spec.loader.exec_module(owners)


def main(args):
    expected = (1, 5, 97, 0)
    ident = 17361
    library = audit.LIBRARY
    if args.runtime == "takealook":
        expected = (1, 6, 1170, 0)
        ident = 17759
        audit.GAME = Path(r"C:\TAKEALOOK\Stock Game\SkyrimSE.exe")
        library = Path(r"C:\TAKEALOOK\mods\Address Library for SKSE Plugins\SKSE\Plugins\versionlib-1-6-1170-0.bin")
    if tuple(audit.file_version()) != expected:
        raise RuntimeError("Installed executable does not match the scoped runtime")
    disk = audit.GAME.read_bytes()
    pe = audit.PE(disk)
    version, mapping = owners.library(library)
    if version != expected:
        raise RuntimeError("Address Library does not match the scoped runtime")
    target = mapping[ident]  # CommonLib TESObjectARMA::InitWornArmorAddon
    kernel = ct.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    kernel.OpenProcess.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = [wt.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ct.POINTER(wt.DWORD)]
    kernel.ReadProcessMemory.argtypes = [wt.HANDLE, wt.LPCVOID, wt.LPVOID, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    psapi = ct.WinDLL("psapi", use_last_error=True)
    psapi.EnumProcessModules.argtypes = [wt.HANDLE, ct.POINTER(wt.HMODULE), wt.DWORD, ct.POINTER(wt.DWORD)]
    handle = kernel.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ct.WinError(ct.get_last_error())
    try:
        path, length = ct.create_unicode_buffer(32768), wt.DWORD(32768)
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, ct.byref(length)):
            raise ct.WinError(ct.get_last_error())
        if Path(path.value) != audit.GAME:
            raise RuntimeError("Not the scoped game process")
        modules, needed = (wt.HMODULE * 4096)(), wt.DWORD()
        if not psapi.EnumProcessModules(handle, modules, ct.sizeof(modules), ct.byref(needed)):
            raise ct.WinError(ct.get_last_error())
        if not 0 < needed.value <= ct.sizeof(modules):
            raise RuntimeError("Invalid module listing")
        base = modules[0]

        def read_code(rva, size):
            if not any(va <= rva < rva + size <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections):
                raise RuntimeError("Only bounded executable-section reads allowed")
            data, read = ct.create_string_buffer(size), ct.c_size_t()
            if not kernel.ReadProcessMemory(handle, base + rva, data, size, ct.byref(read)):
                raise ct.WinError(ct.get_last_error())
            if read.value != size:
                raise RuntimeError("Incomplete code read")
            return data.raw

        functions = {}
        def capture(rva, label):
            root = owners.function_owner(pe, rva)
            for begin, end in pe.function_fragments(root):
                if begin in functions:
                    continue
                if len(functions) >= 64:
                    raise RuntimeError("Function capture limit exceeded")
                functions[begin] = dict(name=label, ownerRva=root, rva=begin, end=end,
                                       hex=read_code(begin, end-begin).hex())

        capture(target, f"TESObjectARMA_InitWornArmorAddon_ID{ident}")
        if len(args.callee) > 16:
            raise RuntimeError("Too many additional functions")
        for rva in args.callee:
            capture(rva, f"ArmorPath_ManuallySelected_{rva:08X}")
        # Known call targets can be short leaf/thunk functions without unwind
        # entries. This is a bounded CODE window, not a guessed whole function.
        # Caller must first identify the target in captured disassembly.
        if len(args.leaf) > 8:
            raise RuntimeError("Too many leaf-code windows")
        for rva in args.leaf:
            if rva not in functions:
                if len(functions) >= 64:
                    raise RuntimeError("Function capture limit exceeded")
                functions[rva] = dict(name=f"ArmorPath_LeafWindow_{rva:08X}",
                    ownerRva=None, rva=rva, end=rva + 128, boundedWindow=True,
                    hex=read_code(rva, 128).hex())
        callers = []
        if len(args.caller_target) > 16:
            raise RuntimeError("Too many caller targets")
        if args.find_callers:
            caller_targets = {target, *args.caller_target}
            for caller_target in args.caller_target:
                capture(caller_target, f"ArmorPath_CallerTarget_{caller_target:08X}")
            executable = [(va, length) for va, length, _, _, flags in pe.sections
                          if flags & 0x20000000]
            if sum(length for _, length in executable) > 0x2000000:
                raise RuntimeError("Executable scan exceeds 32 MiB limit")
            for va, length in executable:
                raw = read_code(va, length)
                offset = raw.find(b"\xe8")
                while 0 <= offset < len(raw)-4:
                    dest = va + offset + 5 + struct.unpack_from("<i", raw, offset+1)[0]
                    if dest in caller_targets:
                        # Byte matches require subsequent instruction-boundary review.
                        callers.append(va + offset)
                        capture(va + offset, f"ArmorPath_CallerCandidate_{va+offset:08X}_To_{dest:08X}")
                    offset = raw.find(b"\xe8", offset+1)
        output = Path(__file__).resolve().parents[1] / "build/audits/addon-txst" / args.label
        output.mkdir(parents=True, exist_ok=True)
        result = dict(runtime=".".join(map(str, expected)), exe=str(audit.GAME),
                      processId=args.pid, imageBase=base,
                      processAccessMask="0x410", exeSha256=hashlib.sha256(disk).hexdigest(),
                      rootRva=target, additionalCallerTargets=args.caller_target,
                      callerCandidates=callers, functions=list(functions.values()))
        (output / "addon-code.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        audit.disassemble(result["functions"], base, output)
        for f in functions.values():
            print(f"{f['name']} {f['rva']:08X}..{f['end']:08X}")
        print(output)
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--runtime", choices=("tuled", "takealook"), default="tuled")
    parser.add_argument("--callee", type=lambda s: int(s, 16), action="append", default=[])
    parser.add_argument("--leaf", type=lambda s: int(s, 16), action="append", default=[],
                        help="Previously identified leaf/thunk target: read 128 code bytes, not a whole-function assertion")
    parser.add_argument("--find-callers", action="store_true")
    parser.add_argument("--caller-target", type=lambda s: int(s, 16), action="append", default=[],
                        help="Known armor-path function RVA whose direct callers should be captured")
    parser.add_argument("--label", default="initial")
    args = parser.parse_args()
    if not args.label or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in args.label):
        parser.error("label must be a simple lowercase directory name")
    main(args)
