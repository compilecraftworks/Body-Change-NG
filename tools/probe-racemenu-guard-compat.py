"""Verify that the narrowing-callback guard leaves audited official SKEE unchanged.

Disposable process only. Loads pinned DLLs to initialize their CRT globals, but
never calls SKSEPlugin_Load, an engine function, or an in-game process. This is
not a rendering/persistence test. UBE's positive case is covered by the separate
probe-racemenu-node-registration.py script.
"""
import argparse
import ctypes as ct
import hashlib
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("face_audit", Path(__file__).with_name("audit-native-face-code.py"))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
PINS = {
    "official-se":"255c0db0ba5ff14640cc6d75ccddfb24474b498d35b77f3140ccb05eb80e7273",
    "official-ae":"5225e4e3b185e6fc57c8d31b0cedbe5a030a951d9a744d33071b64c45a38c208",
}


def main(args):
    guard = ct.WinDLL(str(args.probe.resolve()))
    guard.GuardInstall.argtypes = (ct.c_void_p,ct.c_bool)
    guard.GuardInstall.restype = ct.c_bool
    guard.GuardStatus.restype = ct.c_char_p
    for label,path in (("official-se",args.official_se),("official-ae",args.official_ae)):
        disk = path.read_bytes()
        digest = hashlib.sha256(disk).hexdigest()
        assert digest == PINS[label], (label,digest)
        library = ct.WinDLL(str(path.resolve()))
        pe = audit.PE(disk)
        sections = [(rva,size) for rva,size,_,_,flags in pe.sections if flags&0x20000000]
        assert sections and all(0<size<32*1024*1024 for _,size in sections)
        def code_hash():
            result = hashlib.sha256()
            for rva,size in sections: result.update(ct.string_at(library._handle+rva,size))
            return result.hexdigest()
        before = code_hash()
        assert not guard.GuardInstall(library._handle,True), label
        status = guard.GuardStatus().decode()
        assert status.startswith("no recognized narrowing FormDelete callback"), status
        assert code_hash() == before, "official executable code was modified"
        print("PASS:",label,digest,"no code mutation; normal API unchanged")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe",type=Path,required=True)
    parser.add_argument("--official-se",type=Path,required=True)
    parser.add_argument("--official-ae",type=Path,required=True)
    main(parser.parse_args())
