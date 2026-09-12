"""Read-only TuLED SE 1.5.97 HandlePolicy code audit, not a game patch.

Reads only the verified vtable and bounded code from Skyrim's loaded image.
No engine calls, actor/heap reads, suspension, injection or process writes.
"""
import ctypes as c
from ctypes import wintypes as w
import importlib.util
import json
from pathlib import Path
import struct
import sys

spec = importlib.util.spec_from_file_location("face_audit", Path(__file__).with_name("audit-native-face-code.py"))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
assert audit.file_version() == [1, 5, 97, 0]
pe = audit.PE(audit.GAME.read_bytes())
table = audit.address_library()[271953]  # VTABLE_SkyrimScript::HandlePolicy SE ID
kernel = c.WinDLL("kernel32", use_last_error=True)
psapi = c.WinDLL("psapi", use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
kernel.ReadProcessMemory.argtypes = [w.HANDLE, w.LPCVOID, w.LPVOID, c.c_size_t, c.POINTER(c.c_size_t)]
psapi.EnumProcessModules.argtypes = [w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD)]
process = kernel.OpenProcess(0x410, False, int(sys.argv[1]))
if not process:
    raise c.WinError(c.get_last_error())
try:
    name, length = c.create_unicode_buffer(32768), w.DWORD(32768)
    assert kernel.QueryFullProcessImageNameW(process, 0, name, c.byref(length))
    assert Path(name.value) == audit.GAME
    modules, needed = (w.HMODULE * 4096)(), w.DWORD()
    assert psapi.EnumProcessModules(process, modules, c.sizeof(modules), c.byref(needed))
    assert 0 < needed.value <= c.sizeof(modules)
    base = modules[0]

    def read(rva, count):
        assert 0 <= rva < rva + count <= pe.image_size and 0 < count <= 0x4000
        data, received = c.create_string_buffer(count), c.c_size_t()
        assert kernel.ReadProcessMemory(process, base+rva, data, count, c.byref(received))
        assert received.value == count
        return data.raw

    functions = []
    for slot, label in [(1,"HandleIsType"), (2,"IsHandleObjectAvailable"),
                        (3,"EmptyHandle"), (4,"GetHandleForObject"),
                        (5,"HasParent"), (6,"GetParentHandle"),
                        (8,"GetObjectForHandle"), (11,"ConvertHandleToString")]:
        rva = struct.unpack_from("<Q", pe.data, pe.raw(table)+slot*8)[0]-pe.image_base
        assert struct.unpack("<Q", read(table+slot*8,8))[0] == base+rva
        try:
            fragments = pe.function_fragments(rva)
        except (ValueError, AssertionError):
            fragments = [(rva,rva+64)]  # labelled bounded leaf window, not inferred extent
        for index,(start,end) in enumerate(fragments):
            assert any(va <= start < end <= va+size and flags & 0x20000000
                       for va,size,_,_,flags in pe.sections)
            functions.append(dict(name=f"{label}_{index}",rva=start,end=end,hex=read(start,end-start).hex()))
    output = Path("build/audits/vm-handle-policy-20260911")
    output.mkdir(parents=True, exist_ok=True)
    (output / "capture.json").write_text(json.dumps(dict(base=base,table=table,functions=functions),indent=2))
    audit.disassemble(functions,base,output)
    print(f"Captured {len(functions)} bounded policy code fragments; output: {output}")
finally:
    kernel.CloseHandle(process)
