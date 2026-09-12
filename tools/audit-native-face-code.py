"""Read-only SE 1.5.97 face-code capture; never injects or writes game memory.

Only the scoped TuLED executable is accepted. Reads bounded executable function
ranges and explicit small leaf-code windows. Optional scans cover a 20 KiB NPC
code window or executable sections (32 MiB maximum) for three exact face-map/
attachment CALL targets; the latter retains only bounded candidate functions.
Never reads actor objects, heap, or save data.
Output under build/audits is diagnostic data, not a runnable game patch.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess

GAME = Path(r"D:\TuLED13E\STOCK GAME\Skyrim Special Edition\SkyrimSE.exe")
LIBRARY = Path(r"D:\TuLED13E\File Mod Skyrim SE\mods\Address Library for SKSE Plugins\SKSE\Plugins\version-1-5-97-0.bin")
DUMPBIN = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe")
# Official RaceMenu 0.4.16, 87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc,
# skee/SKEEHooks.cpp. Labels describe upstream names, not verified semantics.
TARGETS = {
    "UpdateHeadState": 0x362E90,
    "UpdateHeadState_Caller1": 0x363F20,
    "UpdateHeadState_Caller2": 0x363000,
    "GetHeadModel_PreprocessedHeads": 0x363DE0,
    "RegenerateHead_PrepareHeadPartForShaders": 0x3D2A60,
    # Direct call at SE BSLightingShaderProperty::CreateClone + 0x9F.
    "BSLightingShaderProperty_CloneMembers": 0x12C4A50,
    # Direct call at the lighting property's CloneMembers + 0x10.
    "BSShaderProperty_CloneMembers": 0x12910F0,
    # Direct material manager call at BSShaderProperty_CloneMembers + 0x6D.
    "MaterialManager_CloneOrShare": 0x130BB80,
}


def address_library():
    # Format 1, as decoded by vendored REL/IDDB.cpp and the existing
    # audit-native-form-vtables.py. Never use AE IDs in this SE-only audit.
    stream = io.BytesIO(LIBRARY.read_bytes())
    def read(fmt):
        return struct.unpack("<" + fmt, stream.read(struct.calcsize("<" + fmt)))[0]
    assert read("I") == 1
    assert [read("I") for _ in range(4)] == [1, 5, 97, 0]
    stream.read(read("I"))
    pointer_size, count = read("I"), read("I")
    assert pointer_size == 8
    def delta(kind, previous):
        if kind == 0: return read("Q")
        if kind == 1: return previous + 1
        if kind == 2: return previous + read("B")
        if kind == 3: return previous - read("B")
        if kind == 4: return previous + read("H")
        if kind == 5: return previous - read("H")
        if kind == 6: return read("H")
        if kind == 7: return read("I")
        raise ValueError(kind)
    mapping, ident, offset = {}, 0, 0
    for _ in range(count):
        code = read("B")
        ident = delta(code & 15, ident)
        high = code >> 4
        offset = delta(high & 7, offset // pointer_size if high & 8 else offset)
        if high & 8: offset *= pointer_size
        mapping[ident] = offset
    return mapping


class PE:
    def __init__(self, data):
        self.data = data
        pe = self.u32(0x3C)
        assert data[pe:pe + 4] == b"PE\0\0"
        optional = pe + 24
        assert self.u16(optional) == 0x20B
        self.image_size = self.u32(optional + 56)
        self.image_base = struct.unpack_from("<Q", data, optional + 24)[0]
        self.exceptions = (self.u32(optional + 112 + 24), self.u32(optional + 116 + 24))
        self.sections = []
        start = optional + self.u16(pe + 20)
        for i in range(self.u16(pe + 6)):
            pos = start + 40 * i
            self.sections.append((self.u32(pos + 12), self.u32(pos + 8),
                                  self.u32(pos + 20), self.u32(pos + 16), self.u32(pos + 36)))

    def u16(self, pos):
        return struct.unpack_from("<H", self.data, pos)[0]

    def u32(self, pos):
        return struct.unpack_from("<I", self.data, pos)[0]

    def raw(self, rva):
        for va, _, raw, size, _ in self.sections:
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError(f"No file-backed section at {rva:X}")

    def function(self, rva):
        start, size = self.exceptions
        assert size % 12 == 0
        raw = self.raw(start)
        for pos in range(raw, raw + size, 12):
            begin, end, _ = struct.unpack_from("<III", self.data, pos)
            if begin <= rva < end:
                assert 0 < end - begin <= 0x10000
                assert any(va <= begin < end <= va + length and flags & 0x20000000
                           for va, length, _, _, flags in self.sections)
                return begin, end
        raise ValueError(f"No exception-table function at {rva:X}")

    def function_fragments(self, rva):
        start, size = self.exceptions
        raw = self.raw(start)
        rows = [struct.unpack_from("<III", self.data, pos) for pos in range(raw, raw + size, 12)]
        def root(row):
            seen = set()
            while True:
                begin, end, unwind = row
                assert unwind not in seen
                seen.add(unwind)
                pos = self.raw(unwind)
                if not (self.data[pos] >> 3) & 4:  # UNW_FLAG_CHAININFO
                    return begin
                codes = self.data[pos + 2]
                chain = pos + 4 + ((codes + 1) & ~1) * 2
                row = struct.unpack_from("<III", self.data, chain)
        assert any(begin == rva for begin, _, _ in rows), f"No exception-table function start at {rva:X}"
        fragments = [(begin, end) for begin, end, unwind in rows if root((begin, end, unwind)) == rva]
        assert fragments and sum(end - begin for begin, end in fragments) <= 0x10000
        # Revalidate each fragment against the executable section bounds.
        for begin, end in fragments:
            assert self.function(begin) == (begin, end)
        # Merge only contiguous fragments, never intervening unrelated bytes.
        merged = []
        for begin, end in sorted(fragments):
            if merged and merged[-1][1] == begin:
                merged[-1] = (merged[-1][0], end)
            else:
                merged.append((begin, end))
        return merged


def file_version():
    version = ct.WinDLL("version", use_last_error=True)
    version.GetFileVersionInfoSizeW.argtypes = [wt.LPCWSTR, ct.POINTER(wt.DWORD)]
    version.GetFileVersionInfoSizeW.restype = wt.DWORD
    version.GetFileVersionInfoW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID]
    version.VerQueryValueW.argtypes = [wt.LPCVOID, wt.LPCWSTR, ct.POINTER(wt.LPVOID), ct.POINTER(wt.UINT)]
    length = version.GetFileVersionInfoSizeW(str(GAME), None)
    assert length
    data = ct.create_string_buffer(length)
    assert version.GetFileVersionInfoW(str(GAME), 0, length, data)
    ptr, count = wt.LPVOID(), wt.UINT()
    assert version.VerQueryValueW(data, "\\VarFileInfo\\Translation", ct.byref(ptr), ct.byref(count))
    assert count.value >= 4
    language, codepage = struct.unpack("<HH", ct.string_at(ptr, 4))
    key = f"\\StringFileInfo\\{language:04x}{codepage:04x}\\FileVersion"
    assert version.VerQueryValueW(data, key, ct.byref(ptr), ct.byref(count))
    # Skyrim's fixed numeric resource may be 1.0.0.0; its FileVersion string
    # identifies the actual runtime (same field used by the earlier PS audit).
    return [int(part) for part in ct.wstring_at(ptr).strip().split(".")]


def disassemble(functions, base, output):
    # Each selected function is a separate diagnostic PE section. Keeping its
    # original RVA preserves relative call/jump destinations in disassembly.
    header_size = (0x188 + 40 * len(functions) + 0x1FF) & ~0x1FF
    header = bytearray(header_size)
    def put(fmt, pos, *values):
        struct.pack_into("<" + fmt, header, pos, *values)
    put("H", 0, 0x5A4D)
    put("I", 0x3C, 0x80)
    put("IHHIIIHH", 0x80, 0x4550, 0x8664, len(functions), 0, 0, 0, 0xF0, 0x2022)
    put("H", 0x98, 0x20B)
    put("QII", 0x98 + 24, base, 0x10, 0x10)
    put("II", 0x98 + 56, max(f["end"] for f in functions), header_size)
    put("H", 0x98 + 68, 3)
    chunks, offset = [], header_size
    for i, func in enumerate(functions):
        data = bytes.fromhex(func["hex"])
        padded = data + b"\0" * ((-len(data)) % 16)
        pos = 0x188 + 40 * i
        header[pos:pos + 8] = f".f{i}".encode().ljust(8, b"\0")
        put("IIII", pos + 8, len(data), func["rva"], len(padded), offset)
        put("I", pos + 36, 0x60000020)
        chunks.append(padded)
        offset += len(padded)
    container = output / "native-face-code.bin"
    container.write_bytes(header + b"".join(chunks))
    report = []
    for func in functions:
        start, end = base + func["rva"], base + func["end"] - 1
        report.append(f"\n{func['name']} RVA {func['rva']:08X}..{func['end']:08X}\n")
        result = subprocess.run([str(DUMPBIN), "/DISASM", f"/RANGE:0x{start:X},0x{end:X}", str(container)],
                                capture_output=True, check=True)
        report.append("\n".join(line for line in result.stdout.decode("utf-8", errors="replace").splitlines()
                                if line.strip()) + "\n")
    (output / "native-face-code.disasm.txt").write_text("".join(report), encoding="utf-8")


def main(pid, additional, find_callers=False, cache_refresh=False, attachment_boundary=False,
         main_menu=False, face_map_callers=False):
    assert not face_map_callers or (attachment_boundary and main_menu)
    cache_refresh = cache_refresh or attachment_boundary
    assert file_version() == [1, 5, 97, 0], "Only SE 1.5.97 is audited"
    disk = GAME.read_bytes()
    pe = PE(disk)
    kernel = ct.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    kernel.OpenProcess.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = [wt.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ct.POINTER(wt.DWORD)]
    kernel.ReadProcessMemory.argtypes = [wt.HANDLE, wt.LPCVOID, wt.LPVOID, ct.c_size_t, ct.POINTER(ct.c_size_t)]
    psapi = ct.WinDLL("psapi", use_last_error=True)
    psapi.EnumProcessModules.argtypes = [wt.HANDLE, ct.POINTER(wt.HMODULE), wt.DWORD, ct.POINTER(wt.DWORD)]
    handle = kernel.OpenProcess(0x410, False, pid)  # QUERY_INFORMATION | VM_READ only
    if not handle:
        raise ct.WinError(ct.get_last_error())
    try:
        path, length = ct.create_unicode_buffer(32768), wt.DWORD(32768)
        assert kernel.QueryFullProcessImageNameW(handle, 0, path, ct.byref(length))
        assert Path(path.value) == GAME, "Not the scoped TuLED executable"
        modules, needed = (wt.HMODULE * 4096)(), wt.DWORD()
        assert psapi.EnumProcessModules(handle, modules, ct.sizeof(modules), ct.byref(needed))
        assert 0 < needed.value <= ct.sizeof(modules)
        base = modules[0]
        targets = dict(TARGETS)
        mapping = address_library()
        if cache_refresh:
            # Exact SE IDs from the installed Address Library, not AE IDs.
            # 74039/74043 are direct targets in the previously captured face
            # loader. Keep unknown DB semantics out of the labels.
            for name, ident in [
                ("ModelDB_FaceLoaderCallee_ID74039", 74039),
                ("BSModelDB_Demand_ID74040", 74040),
                ("ModelDB_FaceResultCallee_ID74043", 74043),
                ("ModelDB_ResultInnerCallee_ID74090", 74090),
                ("NiObject_Clone_ID68836", 68836),
                ("AIProcess_Update3DModel_Impl_ID38404", 38404),
                ("Actor_DoReset3D_ID39181", 39181),
                ("TESNPC_HeadResetCallee_ID24214", 24214),
                ("TESNPC_FaceMapRemoveCallee_ID24292", 24292),
            ]:
                targets[name] = mapping[ident]
        # Exact SE/AE-flat virtual slots from vendored TESNPC.h and
        # BSLightingShaderProperty.h; resolve from the installed on-disk vtable.
        for name, ident, slot in [
            ("TESNPC_Clone3D", 241857, 0x4A),
            ("BSLightingShaderProperty_CreateClone", 304424, 0x17),
            ("BSLightingShaderMaterialBase_CopyMembers", 304554, 0x02),
            ("BSLightingShaderMaterialFacegen_CopyMembers", 304560, 0x02),
        ]:
            targets[name] = struct.unpack_from("<Q", disk, pe.raw(mapping[ident]) + slot * 8)[0] - pe.image_base
        if attachment_boundary:
            # Direct calls already observed inside the attach function. Read
            # the complete insert callee before assigning it a C++ prototype.
            # The NPC-keyed map is NOT assumed to be a disposable model cache:
            # its lookup/removal consumers may require the attached face.
            targets.update({
                "NPCFaceMap_InsertCallee_ABIUnverified": 0x36A880,
                "NPCFaceMap_Exists": 0x364120,
                "NPCFaceMap_Lookup": 0x3641A0,
                # Direct callee at HeadReset +0x7A; consumes the removed
                # node's NiPointer. Its exact cleanup semantics need reading.
                "NPCFaceMap_RemovedNodeConsumer_Unverified": 0xC61CE0,
                # Direct callees verified in the 2026-09-11 attachment capture.
                "FaceGenNiNode_CloneMembers": 0x3D8560,
                "FaceGenNiNode_Constructor": 0x3D8610,
                "LightingShader_SetupGeometryInner": 0x12C5540,
                "LightingMaterial_OnLoadTextureSetBase": 0x12CF480,
                "LightingMaterial_ClearTexturesBase": 0x12CF5B0,
                "RemovedNode_CollectionInsert": 0xC624C0,
                # User-confirmed main-menu CALL xref: BipedAnim's lookup
                # consumer at 1CBCA0 forwards the returned face here.
                "BipedAnim_FaceMapValueConsumer": 0x1CBD10,
            })
            for name, ident in [
                ("NiObject_Clone_DefaultProcess_ID68835", 68835),
                ("BSShaderProperty_SetMaterial_ID98897", 98897),
            ]:
                targets[name] = mapping[ident]
            for name, ident, slot in [
                ("BSFaceGenNiNode_CreateClone", 252410, 0x17),
                ("BSLightingShaderProperty_SetupGeometry", 304424, 0x27),
                ("FacegenMaterial_OnLoadTextureSet", 304560, 0x08),
                ("FacegenMaterial_ClearTextures", 304560, 0x09),
                ("Actor_GetFaceGenAnimationData", 260538, 0x63),
                ("Character_GetFaceNodeSkinned", 261397, 0x61),
                ("Character_GetFaceGenAnimationData", 261397, 0x63),
                ("BSFaceGenNiNode_FixSkinInstances", 252410, 0x3E),
            ]:
                targets[name] = struct.unpack_from("<Q", disk,
                    pe.raw(mapping[ident]) + slot * 8)[0] - pe.image_base
        for rva in additional:
            # Additional targets must be manually verified direct callees of
            # captured face functions, restricted to the face/NPC code region.
            assert 0x350000 <= rva < 0x400000
            targets[f"FaceCallee_{rva:08X}"] = rva
        caller_candidates = []
        if face_map_callers:
            # Scan executable bytes for callers of these already verified
            # face lifecycle functions. Never read heap, map, actor or save
            # data, and never retain the complete executable-section bytes.
            # Byte matches are NOT decoded instruction boundaries: capture
            # candidate functions for manual disassembly confirmation below.
            watched = {0x364120, 0x3641A0, 0x363F20}
            executable = [(va, size) for va, size, _, _, flags in pe.sections
                          if flags & 0x20000000]
            assert sum(size for _, size in executable) <= 0x2000000
            for start, size in executable:
                data, read = ct.create_string_buffer(size), ct.c_size_t()
                assert kernel.ReadProcessMemory(handle, base + start, data, size, ct.byref(read))
                assert read.value == size
                raw = data.raw
                offset = raw.find(b"\xe8")
                while 0 <= offset < len(raw) - 4:
                    target = start + offset + 5 + struct.unpack_from("<i", raw, offset + 1)[0]
                    if target in watched:
                        candidate = {"instructionCandidateRVA": start + offset,
                                     "targetRVA": target}
                        try:
                            begin, end = pe.function(start + offset)
                            candidate["fragmentBeginRVA"] = begin
                            targets[f"FaceMapCallerCandidate_{begin:08X}"] = begin
                        except ValueError:
                            candidate["noExceptionTableEntry"] = True
                        caller_candidates.append(candidate)
                    offset = raw.find(b"\xe8", offset + 1)
                del data, raw
            assert len(caller_candidates) <= 64, "Unexpectedly broad face-call scan"
        if find_callers:
            # Bounded executable-only NPC face region; no heap/actor/save reads.
            # Byte matches are candidates until checked at instruction boundaries
            # in the emitted disassembly, not proof of a real CALL instruction.
            start, end = 0x362000, 0x367000
            assert any(va <= start < end <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections)
            data, read = ct.create_string_buffer(end - start), ct.c_size_t()
            assert kernel.ReadProcessMemory(handle, base + start, data, end - start, ct.byref(read))
            assert read.value == end - start
            for offset, opcode in enumerate(data.raw[:-4]):
                if opcode != 0xE8:
                    continue
                target = start + offset + 5 + struct.unpack_from("<i", data.raw, offset + 1)[0]
                if target in (0x363DE0, 0x363F20, 0x3632D0, 0x388480):
                    begin, _ = pe.function(start + offset)
                    # A chained fragment can start inside the complete function.
                    # The caller is later captured explicitly after inspection.
                    caller_candidates.append({"instructionCandidateRVA": start + offset,
                                              "fragmentBeginRVA": begin, "targetRVA": target})
        functions = []
        for name, rva in targets.items():
            for i, (begin, end) in enumerate(pe.function_fragments(rva)):
                data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
                assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
                assert read.value == end - begin
                functions.append({"name": name if i == 0 else f"{name}_fragment{i}",
                                  "rva": begin, "end": end, "hex": data.raw.hex()})
        if attachment_boundary:
            # ID15576 is the already decoded BipedAnim consumer's direct
            # callee. No .pdata entry: an explicit 96-byte executable window,
            # not an inferred complete function or a heap/partition read.
            begin, end = mapping[15576], mapping[15577]
            assert (begin, end) == (0x1CDD40, 0x1CDDA0)
            assert any(va <= begin < end <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections)
            data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
            assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
            assert read.value == end - begin
            functions.append({"name": "DismemberPartitionUpdate_leaf_window", "rva": begin,
                              "end": end, "boundedWindowNotFunction": True, "hex": data.raw.hex()})
        # Direct callee of 3D2600 at +0x1DD. This leaf has no .pdata record.
        # A fixed 64-byte executable window, not an inferred function extent;
        # do not interpret alignment/following function bytes as the same body.
        begin, end = 0x3D3E90, 0x3D3ED0
        assert any(va <= begin < end <= va + length and flags & 0x20000000
                   for va, length, _, _, flags in pe.sections)
        data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
        assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
        assert read.value == end - begin
        functions.append({"name": "FaceTextureSetter_leaf_window", "rva": begin,
                          "end": end, "boundedWindowNotFunction": True, "hex": data.raw.hex()})
        if cache_refresh:
            # Same bounded NPC executable region as the prior CALL search.
            # Retain its bytes to inspect RIP-relative face-map references;
            # never read the map itself or any actor/heap data. This is a code
            # window containing multiple functions, not a claimed function.
            begin, end = 0x362000, 0x367000
            assert any(va <= begin < end <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections)
            data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
            assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
            assert read.value == end - begin
            functions.append({"name": "NPCFaceLifecycle_code_window", "rva": begin,
                              "end": end, "boundedWindowNotFunction": True, "hex": data.raw.hex()})
            # Four adjacent small SE leaf helpers, IDs 38867..38870, used
            # to set/read/reset/test AIProcess 3D-update flags. No heap reads.
            begin, end = mapping[38867], mapping[38870] + 0x20
            assert (begin, end) == (0x67E3B0, 0x67E450)
            assert any(va <= begin < end <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections)
            data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
            assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
            assert read.value == end - begin
            functions.append({"name": "AIProcess_UpdateFlags_leaf_window", "rva": begin,
                              "end": end, "boundedWindowNotFunction": True, "hex": data.raw.hex()})
            # The ID74090 direct callee at +0xC5 has no exception-table entry.
            # Scope a 128-byte code window up to the next mapped function;
            # do not infer an entire function's extent from that distance.
            begin, end = mapping[74092], mapping[74093]
            assert (begin, end) == (0xD314E0, 0xD31560)
            assert any(va <= begin < end <= va + length and flags & 0x20000000
                       for va, length, _, _, flags in pe.sections)
            data, read = ct.create_string_buffer(end - begin), ct.c_size_t()
            assert kernel.ReadProcessMemory(handle, base + begin, data, end - begin, ct.byref(read))
            assert read.value == end - begin
            functions.append({"name": "ModelDB_ExistingResult_leaf_window", "rva": begin,
                              "end": end, "boundedWindowNotFunction": True, "hex": data.raw.hex()})
        output = Path(__file__).resolve().parents[1] / "build" / "audits"
        if attachment_boundary and main_menu:
            output /= "face-attachment-main-menu"
        elif attachment_boundary:
            # Never overwrite earlier process captures while extending the
            # investigation. No hook installation or heap/map reads here.
            output /= "face-attachment-boundary"
        elif cache_refresh:
            # Do not overwrite the first audit's evidence.
            output /= "face-cache-refresh"
        output.mkdir(parents=True, exist_ok=True)
        capture = {"runtime": "1.5.97", "imageBase": base,
                   "processId": pid, "processAccessMask": "0x410",
                   "userReportedStage": "main-menu" if main_menu else "unspecified",
                   "auditProfile": "attachment-boundary" if attachment_boundary else
                       "cache-refresh" if cache_refresh else "face-construction",
                   "exeSha256": hashlib.sha256(disk).hexdigest(),
                   "lightingShaderNiRTTI_RVA": mapping[527752],
                   "callerCandidates": caller_candidates, "functions": functions}
        (output / "native-face-code.json").write_text(json.dumps(capture, indent=2), encoding="utf-8")
        disassemble(functions, base, output)
        for f in functions:
            print(f"{f['name']} RVA {f['rva']:08X}..{f['end']:08X} {f['end']-f['rva']} bytes")
        for candidate in caller_candidates:
            print("CALL candidate", {k: hex(v) for k, v in candidate.items()})
        print(output / "native-face-code.disasm.txt")
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--callee", action="append", default=[], type=lambda s: int(s, 16))
    parser.add_argument("--find-face-callers", action="store_true")
    parser.add_argument("--cache-refresh", action="store_true",
                        help="Read the scoped model DB/refresh functions and NPC code window")
    parser.add_argument("--attachment-boundary", action="store_true",
                        help="Also read native face-map insertion and clone/material boundaries (read-only)")
    parser.add_argument("--main-menu", action="store_true",
                        help="Record user-confirmed main-menu stage; separate attachment output")
    parser.add_argument("--face-map-callers", action="store_true",
                        help="Scan executable code for the three scoped face-map/attach CALL targets")
    args = parser.parse_args()
    main(args.pid, args.callee, args.find_face_callers, args.cache_refresh,
         args.attachment_boundary, args.main_menu, args.face_map_callers)
