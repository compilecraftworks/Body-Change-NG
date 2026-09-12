"""Bounded read-only overlay-registry snapshot of the scoped TuLED process.

Pinned UBE 2.0 U.0.7 data layout, validated from installed Add/Get disassembly.
Only QUERY_INFORMATION | VM_READ; no injection, suspension or remote calls.
Reads player overlay registry keys and their corresponding StringTable buckets.
An observation can race with the game; repeated snapshots are evidence, not locks.
"""
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import struct
import sys
from datetime import datetime, timezone

from importlib.util import spec_from_file_location, module_from_spec
spec = spec_from_file_location("face_audit", Path(__file__).with_name("audit-native-face-code.py"))
audit = module_from_spec(spec)
spec.loader.exec_module(audit)
assert audit.file_version() == [1, 5, 97, 0]
disk_path = Path(r"D:\TuLED13E\File Mod Skyrim SE\mods\[LED]UBE 2.0\SKSE\Plugins\skee64.dll")
disk = disk_path.read_bytes()
assert hashlib.sha256(disk).hexdigest() == "283ea6f0df6234b5636d6b03445a57c90369514e61ec3b02da07f731fcf3469b"
pe = audit.PE(disk)
kernel = c.WinDLL("kernel32", use_last_error=True)
psapi = c.WinDLL("psapi", use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
kernel.ReadProcessMemory.argtypes = [w.HANDLE, w.LPCVOID, w.LPVOID, c.c_size_t, c.POINTER(c.c_size_t)]
psapi.EnumProcessModules.argtypes = [w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD)]
psapi.GetModuleFileNameExW.argtypes = [w.HANDLE, w.HMODULE, w.LPWSTR, w.DWORD]
handle = kernel.OpenProcess(0x410, False, int(sys.argv[1]))
if not handle:
    raise c.WinError(c.get_last_error())
try:
    path, length = c.create_unicode_buffer(32768), w.DWORD(32768)
    assert kernel.QueryFullProcessImageNameW(handle, 0, path, c.byref(length))
    assert Path(path.value) == audit.GAME
    modules, needed = (w.HMODULE * 4096)(), w.DWORD()
    assert psapi.EnumProcessModules(handle, modules, c.sizeof(modules), c.byref(needed))
    assert needed.value <= c.sizeof(modules)
    loaded = []
    for module in modules[:needed.value // c.sizeof(w.HMODULE)]:
        name = c.create_unicode_buffer(32768)
        assert psapi.GetModuleFileNameExW(handle, module, name, 32768)
        loaded.append((module, Path(name.value).name))
    base = next(address for address, name in loaded if name.lower() == "skee64.dll")

    def read(address, size):
        assert 0x10000 <= address < 0x7fffffffffff and 0 < size <= 0x20000
        buffer, count = c.create_string_buffer(size), c.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buffer, size, c.byref(count)) or count.value != size:
            raise RuntimeError(f"Incomplete read {address:X}+{size:X}")
        return buffer.raw

    def integer(address, fmt="Q"):
        return struct.unpack("<"+fmt, read(address, struct.calcsize(fmt)))[0]

    def fixed(address):
        raw = read(address, 40)
        length, capacity, hashed = struct.unpack_from("<QQQ", raw, 16)
        assert length <= 4096 and length <= capacity < 0x100000
        value = read(struct.unpack_from("<Q", raw)[0], length) if length and capacity >= 16 else raw[:length]
        return dict(text=value.decode("utf-8", errors="replace"), hash=hex(hashed), pointer=hex(address))

    def chain(head, count, maximum):
        assert count <= maximum
        current = integer(head)
        seen = set()
        while current != head:
            assert current not in seen and len(seen) < count
            seen.add(current)
            yield current
            current = integer(current)
        assert len(seen) == count

    # The loaded PE layout and key function entries must match this diagnostic.
    assert integer(base + 0x180490 + 11*8) == base + 0x87c70
    assert integer(base + 0x180490 + 30*8) == base + 0x88a40
    assert integer(base+0x3c, "I") == struct.unpack_from("<I", disk, 0x3c)[0]
    objects = []
    for rva, size, _, _, characteristics in pe.sections:
        if characteristics & 0x80000000 and size <= 0x20000:
            raw = read(base+rva, size)
            needle = struct.pack("<Q", base+0x180490)
            objects.extend(base+rva+i for i in range(0, len(raw)-8, 8) if raw[i:i+8] == needle)
    assert len(objects) == 1, objects
    api = objects[0]
    actor_head, actor_count = integer(api+0x68), integer(api+0x70)
    player = next((row for row in chain(actor_head, actor_count, 4096) if integer(row+0x10, "I") == 0x14), None)
    result = dict(time=datetime.now(timezone.utc).isoformat(), pid=int(sys.argv[1]), skeeBase=hex(base), interface=hex(api), actorCount=actor_count,
                  playerPresent=player is not None, nodes=[], functions={})
    # Search BCNG's small writable sections only for its cached interface tuple.
    # Output just matching pointers/version fields, never the surrounding bytes.
    bcng_base = next(address for address,name in loaded if name.lower()=="bodychangeng.dll")
    bcng_disk = Path(r"D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll").read_bytes()
    assert hashlib.sha256(bcng_disk).hexdigest() in {
        "54c037855d1e3c68e51d3844583144e5bca41c35149c919da304746765f04d10", # successful face trial
        "3ca3d4439bc1d59f66841b3936cd5fb0604b48717da02cb5c672ab9fb49ade03", # observational overlay trace
    }
    bcng_pe = audit.PE(bcng_disk)
    result["bcngInterfaceCandidates"]=[]
    for rva,size,_,_,flags in bcng_pe.sections:
        if flags & 0x80000000 and size<=0x20000:
            data=read(bcng_base+rva,size)
            for i in range(0,len(data)-32,8):
                overlay,override,abi,ov,rv=struct.unpack_from("<QQB3xII",data,i)
                if abi==1 and ov==1 and rv==1 and overlay>0x10000 and override>0x10000:
                    vt=integer(override)
                    result["bcngInterfaceCandidates"].append(dict(address=hex(bcng_base+rva+i),
                        overlay=hex(overlay),override=hex(override),vtable=hex(vt),
                        sameOverride=override==api,add=hex(integer(vt+11*8)),get=hex(integer(vt+30*8))))
    for name, rva in [("AddNode",0x87c70), ("GetNode",0x88a40), ("Intern",0xe0540),
                      ("RemoveNode",0x8a770), ("RemoveNodeName",0x8a460),
                      ("RemoveReferenceNodes",0x89310), ("RemoveAllNodes",0x8e8d0), ("Revert",0x8e780)]:
        result["functions"][name] = dict(rva=hex(rva), bytes=read(base+rva,32).hex(),
            matchesDisk=read(base+rva,32) == disk[pe.raw(rva):pe.raw(rva)+32])
    # Full registry-method text range, not just entry prologues. x64 .text
    # here uses RIP-relative code: report differences without interpreting them.
    start, size = 0x87000, 0x8200
    live, original = read(base+start,size), disk[pe.raw(start):pe.raw(start)+size]
    changed=[i for i in range(size) if live[i]!=original[i]]
    groups=[]
    for i in changed:
        if not groups or i-groups[-1][-1]>8: groups.append([])
        groups[-1].append(i)
    result["registryTextDifferences"]=[dict(rva=hex(start+g[0]),
        live=live[g[0]:g[-1]+1].hex(),disk=original[g[0]:g[-1]+1].hex()) for g in groups]
    # Candidate relative call/jump references from the pinned disk image.
    # These are byte candidates and must be confirmed at instruction boundaries.
    targets={0x89310,0x89490,0x8e8d0,0x8e780}
    result["clearCallerCandidates"]=[]
    for rva,size,raw,rawsize,flags in pe.sections:
        if not flags & 0x20000000: continue
        code=disk[raw:raw+rawsize]
        for i in range(len(code)-5):
            if code[i] not in (0xe8,0xe9): continue
            target=rva+i+5+struct.unpack_from('<i',code,i+1)[0]
            if target in targets:
                result["clearCallerCandidates"].append(dict(rva=hex(rva+i),target=hex(target),
                    liveMatchesDisk=read(base+rva+i,5)==code[i:i+5]))

    if player:
        for female in [False, True]:
            registration = player+0x18+(72 if female else 0)
            head, count = integer(registration+0x10), integer(registration+0x18)
            for row in chain(head, count, 1024):
                name = fixed(integer(row+0x10))
                if not any(name["text"].lower().startswith(prefix) for prefix in
                           ["body [ovl", "face [ovl", "hands [ovl", "feet [ovl"]):
                    continue
                values = []
                tree, size = integer(row+0x28), integer(row+0x30)
                assert size <= 256
                stack, visited = [integer(tree+8)], set()
                while stack:
                    item = stack.pop()
                    if integer(item+0x19,"B"): continue
                    assert item not in visited and len(visited) < size
                    visited.add(item)
                    key, typ, index = struct.unpack("<HBb",read(item+0x20,4))
                    value = dict(key=key,type=typ,index=index,data=hex(integer(item+0x28)))
                    if typ == 2: value["string"] = fixed(integer(item+0x30))
                    values.append(value)
                    stack.extend([integer(item), integer(item+0x10)])
                assert len(visited) == size
                control = integer(row+0x18)
                # Locate only this name's StringTable bucket, not the whole table.
                hashed = int(name["hash"],16)
                mask, buckets = integer(base+0x1dd0c0), integer(base+0x1dd0a8)
                assert mask < 0x100000 and (mask & (mask+1)) == 0
                first,last = struct.unpack("<QQ",read(buckets+(hashed & mask)*16,16))
                sentinel = integer(base+0x1dd098)
                matches=[]
                if last != sentinel:
                    current=last
                    for _ in range(128):
                        entry = fixed(current+0x10)
                        if entry["text"].lower() == name["text"].lower():
                            ptr,ctrl=integer(current+0x38),integer(current+0x40)
                            matches.append(dict(pointer=hex(ptr),control=hex(ctrl),
                                strong=integer(ctrl+8,"i") if ctrl else 0, sameIdentity=ptr==int(name["pointer"],16)))
                        if current==first: break
                        current=integer(current+8)
                    else: raise AssertionError("bucket too long")
                result["nodes"].append(dict(female=female,name=name,control=hex(control),
                    strong=integer(control+8,"i"),values=values,stringTable=matches))
    result["actorHeaderStable"] = (actor_head,actor_count)==(integer(api+0x68),integer(api+0x70))
    print(json.dumps(result,ensure_ascii=False,indent=2))
finally:
    kernel.CloseHandle(handle)
