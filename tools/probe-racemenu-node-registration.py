"""Isolated-process probe of the installed UBE 2.0 U.0.7 node registry.

Not an SKSE plugin and never attaches to Skyrim. Only the audited string
interning and Add/Get node-registry methods are called, with a synthetic reference.
No rendering, save/load serialization, or SKSEPlugin_Load calls. The audited
FormDelete callback can be tested separately with --form-delete-repro; in this
exact image it only calls two local override-map erasers. Exact DLL hash required:
these two methods use FormID, not engine/VM handles, in this particular binary.
"""
import ctypes as c
import hashlib
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1]).resolve()
blob = path.read_bytes()
assert hashlib.sha256(blob).hexdigest() == "283ea6f0df6234b5636d6b03445a57c90369514e61ec3b02da07f731fcf3469b"
pe = struct.unpack_from("<I", blob, 0x3c)[0]
section_table = pe + 24 + struct.unpack_from("<H", blob, pe + 20)[0]
sections = []
for i in range(struct.unpack_from("<H", blob, pe + 6)[0]):
    pos = section_table + i * 40
    name = blob[pos:pos+8].rstrip(b"\0")
    virtual_size, rva, raw_size, raw = struct.unpack_from("<4I", blob, pos+8)
    sections.append((name, rva, virtual_size, raw_size, raw))

# Normal OS loading runs the library's own CRT/global container constructors.
# SKSEPlugin_Load is deliberately NOT called. This is a disposable host process.
library = c.WinDLL(str(path))
base = library._handle
trace = None
if len(sys.argv) > 2 and not sys.argv[2].startswith("--"):
    # Optional removal-call observer: same core compiled into the trial BCNG.
    trace = c.WinDLL(str(Path(sys.argv[2]).resolve()))
    trace.TraceInstall.argtypes = [c.c_void_p]
    trace.TraceInstall.restype = c.c_bool
    trace.TraceStatus.restype = c.c_char_p
    trace.TraceRecordSize.restype = c.c_size_t
    trace.TraceDropped.restype = c.c_uint64
    # Wrong module must fail closed, without modifying that module.
    assert not trace.TraceInstall(trace._handle)
    assert trace.TraceInstall(base), trace.TraceStatus()
    assert trace.TraceInstall(base)  # idempotent: never hook our hook
vtable = base + 0x180490
objects = []
for name, rva, size, _, _ in sections:
    if name != b".data":
        continue
    memory = c.string_at(base+rva, size)
    needle = struct.pack("<Q", vtable)
    for offset in range(0, len(memory)-8, 8):
        if memory[offset:offset+8] == needle:
            objects.append(base+rva+offset)
assert len(objects) == 1, objects
api = objects[0]
slots = (c.c_void_p * 31).from_address(vtable)
assert slots[11] == base + 0x87c70 and slots[30] == base + 0x88a40

class Variant(c.Structure):
    _fields_ = [("key", c.c_uint16), ("type", c.c_uint8),
                ("index", c.c_int8), ("data", c.c_uint64),
                ("string", c.c_uint64), ("control", c.c_uint64)]
assert c.sizeof(Variant) == 32
add = c.WINFUNCTYPE(None, c.c_void_p, c.c_void_p, c.c_bool, c.c_char_p,
                   c.POINTER(Variant))(slots[11])
get = c.WINFUNCTYPE(c.c_void_p, c.c_void_p, c.c_void_p, c.c_bool, c.c_char_p,
                   c.c_uint16, c.c_uint8)(slots[30])
actor = c.create_string_buffer(0x100)
struct.pack_into("<I", actor, 0x14, 0x14)
names = [b"Face [Ovl0]", b"Body [Ovl1]", b"Hands [Ovl0]", b"Feet [Ovl0]"]

class SharedString(c.Structure):
    _fields_ = [("string", c.c_void_p), ("control", c.c_void_p)]

class FixedString(c.Structure):
    _fields_ = [("buffer", c.c_char * 16), ("length", c.c_uint64),
                ("capacity", c.c_uint64), ("hash", c.c_uint64)]

intern = c.WINFUNCTYPE(c.c_void_p, c.c_void_p, c.POINTER(SharedString),
                      c.POINTER(FixedString))(base + 0xe0540)

def intern_path(path):
    owner = c.create_string_buffer(path)
    fixed = FixedString(length=len(path), capacity=max(16, len(path)))
    c.c_void_p.from_address(c.addressof(fixed)).value = c.addressof(owner)
    value = 14695981039346656037
    for byte in path.lower():
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    fixed.hash = value
    result = SharedString()
    intern(base + 0x1dd090, c.byref(result), c.byref(fixed))
    assert result.string and result.control
    return result

def release_string(value):
    # Single-threaded disposable host; exact _Ref_count_base offsets and
    # virtual destructors verified in this DLL. Never used by production code.
    control = value.control
    strong = c.c_int32.from_address(control + 8)
    strong.value -= 1
    if strong.value == 0:
        virtuals = (c.c_void_p * 2).from_address(c.c_void_p.from_address(control).value)
        destroy, delete = virtuals[0], virtuals[1]
        c.WINFUNCTYPE(None, c.c_void_p)(destroy)(control)
        weak = c.c_int32.from_address(control + 12)
        weak.value -= 1
        if weak.value == 0:
            c.WINFUNCTYPE(None, c.c_void_p)(delete)(control)

for iteration in range(1000):
    for name in names:
        path = b"textures\\bcng-probe\\" + name + str(iteration % 7).encode() + b".dds"
        resource = intern_path(path)
        texture = Variant(key=9, type=2, index=0, string=resource.string, control=resource.control)
        add(api, actor, True, name, c.byref(texture))
        release_string(resource)
        tint = Variant(key=7, type=3, index=-1, data=iteration)
        alpha = Variant(key=8, type=4, index=-1, data=0x3f800000)
        add(api, actor, True, name, c.byref(tint))
        add(api, actor, True, name, c.byref(alpha))
        # Fresh name buffers, just like separate UI selections/queries.
        for key, expected in [(7, iteration), (8, 0x3f800000), (9, None)]:
            pointer = get(api, actor, True, c.create_string_buffer(name), key, 0 if key == 9 else 255)
            assert pointer, (iteration, name, key, "missing")
            result = Variant.from_address(pointer)
            assert result.key == key and (expected is None or result.data == expected), (iteration, name, key)
            if key == 9:
                fixed = FixedString.from_address(result.string)
                pointer = c.c_void_p.from_address(result.string).value if fixed.capacity >= 16 else result.string
                assert c.string_at(pointer, fixed.length) == path
print("PASS: actual pinned DLL retained/replaced texture and scalar keys on all 4 nodes for 1000 iterations", flush=True)

if "--form-delete-repro" in sys.argv:
    # Observed full handles from the 2026-09-11 07:40 TuLED trace. They are
    # NOT the player's logged VM handle (0000FFFF00000014), yet the installed
    # callback at 50AA0 narrows them to FormID32 and erases the player's map.
    # This is a reproduction of the provider bug, NOT a repair or game test.
    assert trace is None, "run the cause reproducer without observer hooks"
    delete_callback = c.WINFUNCTYPE(None, c.c_uint64)(base + 0x50aa0)
    prefixes = (0x20002, 0x20004, 0x20009, 0x2000f, 0x20010,
                0x20014, 0x20015, 0x20017, 0x2001b, 0x20021)
    refs = []
    for ident in (0x14, 0x813ba, 0xff002e75):
        ref = c.create_string_buffer(0x100)
        struct.pack_into("<I", ref, 0x14, ident)
        refs.append((ident, ref))
    probes = 0
    for target_id, target in refs:
        for prefix in prefixes:
            for ident, ref in refs:
                for female in (False, True):
                    for name in names:
                        tint = Variant(key=7, type=3, index=-1, data=ident)
                        alpha = Variant(key=8, type=4, index=-1, data=0x3f800000)
                        add(api, ref, female, name, c.byref(tint))
                        add(api, ref, female, name, c.byref(alpha))
            delete_callback((prefix << 32) | target_id)
            for ident, ref in refs:
                for female in (False, True):
                    for name in names:
                        for key in (7, 8):
                            present = bool(get(api, ref, female, name, key, 255))
                            assert present == (ident != target_id), (
                                hex(prefix), hex(target_id), hex(ident), female, name, key)
            probes += 1
    print(f"CONFIRMED PROVIDER BUG: {probes} full-handle callbacks erased all four"
          " nodes for the matching low32 actor (both sexes), while other actors"
          " were unchanged. No BCNG/game/other mods loaded.", flush=True)

if "--form-delete-guard" in sys.argv:
    assert trace is None, "guard test runs without observer hooks"
    guard = c.WinDLL(str(Path(sys.argv[sys.argv.index("--form-delete-guard")+1]).resolve()))
    guard.GuardInstall.argtypes = [c.c_void_p,c.c_bool]
    guard.GuardInstall.restype = c.c_bool
    guard.GuardStatus.restype = c.c_char_p
    class GuardStats(c.Structure):
        _fields_ = [("skipped",c.c_uint64),("forwarded",c.c_uint64),("lastSkipped",c.c_uint64)]
    guard.GuardStats.argtypes = [c.POINTER(GuardStats)]
    def stats():
        result=GuardStats()
        guard.GuardStats(c.byref(result))
        return result
    before=c.string_at(base+0x50aa0,42)
    assert not guard.GuardInstall(base,False)  # wrong runtime: no write
    assert c.string_at(base+0x50aa0,42)==before
    assert not guard.GuardInstall(guard._handle,True)  # wrong SHA: no write
    assert c.string_at(base+0x50aa0,42)==before
    # A conflicting entry patch must be refused before modifying ANY bytes.
    kernel=c.WinDLL("kernel32",use_last_error=True)
    kernel.VirtualProtect.argtypes=[c.c_void_p,c.c_size_t,c.c_uint32,c.POINTER(c.c_uint32)]
    protection=c.c_uint32()
    assert kernel.VirtualProtect(base+0x50aa0,42,0x40,c.byref(protection))
    c.c_ubyte.from_address(base+0x50aa0).value=0xCC
    assert not guard.GuardInstall(base,True)
    assert c.string_at(base+0x50aa0,42)==b'\xcc'+before[1:]
    c.memmove(base+0x50aa0,before,42)
    unused=c.c_uint32()
    assert kernel.VirtualProtect(base+0x50aa0,42,protection.value,c.byref(unused))
    assert guard.GuardInstall(base,True),guard.GuardStatus()
    installed=c.string_at(base+0x50aa0,42)
    assert installed[:6]==b'\xff\x25\0\0\0\0' and installed[14:]==before[14:]
    assert guard.GuardInstall(base,True) and c.string_at(base+0x50aa0,42)==installed
    delete_callback=c.WINFUNCTYPE(None,c.c_uint64)(base+0x50aa0)
    # Concrete v1 Add/Get armor overloads: same seven leading arguments as
    # the audited header, FormID32 reads verified in the installed entry code.
    armor=c.create_string_buffer(0x100)
    addon=c.create_string_buffer(0x100)
    struct.pack_into('<I',armor,0x14,0x1234)
    struct.pack_into('<I',addon,0x14,0x5678)
    armor_add=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool,c.c_void_p,c.c_void_p,c.c_char_p,c.POINTER(Variant))(slots[9])
    armor_get=c.WINFUNCTYPE(c.c_void_p,c.c_void_p,c.c_void_p,c.c_bool,c.c_void_p,c.c_void_p,c.c_char_p,c.c_uint16,c.c_uint8)(slots[29])
    references=[]
    for ident in (0x14,0x813ba,0xff002e75):
        reference=c.create_string_buffer(0x100)
        struct.pack_into('<I',reference,0x14,ident)
        references.append((ident,reference))
    def fill_refs():
        for ident,reference in references:
            for female in (False,True):
                armor_value=Variant(key=7,type=3,index=-1,data=ident)
                armor_add(api,reference,female,armor,addon,b'ArmorProbe',c.byref(armor_value))
                for name in names:
                    texture_path=b'textures\\bcng-probe\\'+str(ident).encode()+name+b'.dds'
                    resource=intern_path(texture_path)
                    texture=Variant(key=9,type=2,index=0,string=resource.string,control=resource.control)
                    add(api,reference,female,name,c.byref(texture))
                    release_string(resource)
                    for key,kind,value in [(7,3,ident),(8,4,0x3f800000)]:
                        scalar=Variant(key=key,type=kind,index=-1,data=value)
                        add(api,reference,female,name,c.byref(scalar))
    def check_refs(deleted=None):
        for ident,reference in references:
            for female in (False,True):
                armor_pointer=armor_get(api,reference,female,armor,addon,b'ArmorProbe',7,255)
                assert bool(armor_pointer)==(ident!=deleted),('armor',ident,deleted)
                if armor_pointer: assert Variant.from_address(armor_pointer).data==ident
                for name in names:
                    for key in (7,8,9):
                        pointer=get(api,reference,female,name,key,0 if key==9 else 255)
                        assert bool(pointer)==(ident!=deleted),(hex(ident),female,name,key,deleted)
                        if pointer:
                            result=Variant.from_address(pointer)
                            if key in (7,8): assert result.data==(ident if key==7 else 0x3f800000)
                            else:
                                fixed=FixedString.from_address(result.string)
                                string_pointer=c.c_void_p.from_address(result.string).value if fixed.capacity>=16 else result.string
                                assert c.string_at(string_pointer,fixed.length)==b'textures\\bcng-probe\\'+str(ident).encode()+name+b'.dds'
    fill_refs()
    for _ in range(100):
        for ident,_ in references:
            for prefix in (0x20002,0x20004,0x20009,0x2000f,0x20010,
                           0x20014,0x20015,0x20017,0x2001b,0x20021,0x1,0xfffe,0x10001):
                delete_callback((prefix<<32)|ident)
            check_refs()  # 3 actors, both sexes, every texture/tint/alpha still present
    assert stats().skipped==3900 and stats().forwarded==0
    for ident,_ in references:
        fill_refs()
        delete_callback(0xffff00000000|ident)
        check_refs(deleted=ident)
    assert stats().forwarded==3
    # These are original explicit APIs, NOT subject to the callback filter.
    key_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool,c.c_char_p,c.c_uint16,c.c_uint8)(slots[28])
    node_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool,c.c_char_p)(slots[27])
    actor_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p)(slots[26])
    all_remove=c.WINFUNCTYPE(None,c.c_void_p)(slots[25])
    revert=c.WINFUNCTYPE(None,c.c_void_p)(slots[2])
    for call in [lambda:key_remove(api,references[0][1],True,names[0],7,255),
                 lambda:node_remove(api,references[0][1],True,names[0]),
                 lambda:actor_remove(api,references[0][1]),lambda:all_remove(api),lambda:revert(api)]:
        fill_refs()
        call()
        assert not get(api,references[0][1],True,names[0],7,255)
    # Revert clears; repopulating simulates a new session's loaded registration.
    # This does not call a real Skyrim serialization/load path.
    fill_refs()
    delete_callback(0x0002001500000014)
    check_refs()
    from concurrent.futures import ThreadPoolExecutor
    def concurrent_guard_test(ident):
        ref=c.create_string_buffer(0x100)
        struct.pack_into('<I',ref,0x14,ident)
        for iteration in range(500):
            value=Variant(key=7,type=3,index=-1,data=iteration)
            add(api,ref,True,names[0],c.byref(value))
            delete_callback(0x0002001500000000|ident)
            assert get(api,ref,True,names[0],7,255)
            delete_callback(0x0000ffff00000000|ident)
            assert not get(api,ref,True,names[0],7,255)
        return True
    previous=stats()
    with ThreadPoolExecutor(max_workers=4) as pool:
        assert all(pool.map(concurrent_guard_test,(0x30,0x31,0x32,0x33)))
    final=stats()
    assert final.skipped-previous.skipped==2000 and final.forwarded-previous.forwarded==2000
    print('PASS: guard blocked 3900 parent-collision callbacks; 3-actor/two-sex/four-area and armor keys preserved; genuine Form deletes clear BOTH armor/node maps; explicit reset and Revert unaffected; 2000 concurrent preserve/delete cycles; wrong runtime/image/conflicting bytes rejected; idempotent install',flush=True)

if trace:
    class Record(c.Structure):
        _fields_ = [("sequence",c.c_uint64),("tick",c.c_uint64),("handle",c.c_uint64),
                    ("thread",c.c_uint32),("kind",c.c_uint32),("actor",c.c_uint32),
                    ("female",c.c_uint32),("key",c.c_uint32),("index",c.c_uint32),
                    ("frames",c.c_uint32),("node",c.c_char*96),("stack",c.c_uint64*24)]
    assert c.sizeof(Record)==trace.TraceRecordSize()
    trace.TracePop.argtypes=[c.POINTER(Record)]
    trace.TracePop.restype=c.c_bool
    def pop_all():
        records=[]
        while True:
            record=Record()
            if not trace.TracePop(c.byref(record)): break
            records.append(record)
        return records
    assert not pop_all(), "register/replace must not invoke deletion hooks"
    other=c.create_string_buffer(0x100)
    struct.pack_into('<I',other,0x14,0x15)
    name=b'Body [Ovl1]'
    value=Variant(key=7,type=3,index=-1,data=0x123456)
    key_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool,c.c_char_p,c.c_uint16,c.c_uint8)(slots[28])
    node_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool,c.c_char_p)(slots[27])
    actor_remove=c.WINFUNCTYPE(None,c.c_void_p,c.c_void_p)(slots[26])
    all_remove=c.WINFUNCTYPE(None,c.c_void_p)(slots[25])
    papyrus_all_remove=c.WINFUNCTYPE(None,c.c_void_p)(base+0x9bca0)
    revert=c.WINFUNCTYPE(None,c.c_void_p)(slots[2])
    def fill():
        add(api,actor,True,name,c.byref(value))
        add(api,other,True,name,c.byref(value))
    kinds=set()
    for _ in range(100):
        for kind,call in [(3,lambda:key_remove(api,actor,True,name,7,255)),
                          (2,lambda:node_remove(api,actor,True,name)),
                          (0,lambda:actor_remove(api,actor)),
                          (1,lambda:all_remove(api)),
                          (1,lambda:papyrus_all_remove(None)),
                          (1,lambda:revert(api))]:
            fill()
            call()
            assert not get(api,actor,True,name,7,255), 'observer prevented native deletion'
            if kind!=1:
                assert get(api,other,True,name,7,255), 'observer changed another actor'
            records=pop_all()
            assert len(records)==1 and records[0].kind==kind, [(r.kind,r.actor) for r in records]
            record=records[0]
            assert record.frames>1 and record.thread and record.tick
            if kind==1:
                # Proves we captured the actual original SKEE caller, not only
                # the diagnostic's wrapper or a synthetic return address.
                assert any(address-base in (0x8e8ef,0x9bcbc,0x8e7e1)
                           for address in record.stack[:record.frames]), list(map(hex,record.stack[:record.frames]))
            if kind!=1: assert record.actor==0x14
            if kind in (2,3): assert record.node==name and record.female==1
            if kind==3: assert record.key==7 and record.index==255
            kinds.add(kind)
    assert kinds=={0,1,2,3} and trace.TraceDropped()==0
    # Overflow must drop trace records, not change native operations or block.
    for _ in range(300):
        fill()
        key_remove(api,actor,True,name,7,255)
        assert not get(api,actor,True,name,7,255)
        assert get(api,other,True,name,7,255)
    assert len(pop_all())==256 and trace.TraceDropped()==44
    print('PASS: 600 deletion-path probes (vtable, Papyrus inline clear, Revert), stack/argument capture, actor isolation, idempotence, wrong-image rejection, 300 bounded-overflow probes',flush=True)
    from concurrent.futures import ThreadPoolExecutor
    def concurrent_probe(ident):
        reference=c.create_string_buffer(0x100)
        struct.pack_into('<I',reference,0x14,0x30+ident)
        scalar=Variant(key=7,type=3,index=-1,data=ident)
        for _ in range(500):
            add(api,reference,True,name,c.byref(scalar))
            key_remove(api,reference,True,name,7,255)
            assert not get(api,reference,True,name,7,255)
        return True
    before=trace.TraceDropped()
    with ThreadPoolExecutor(max_workers=4) as pool:
        assert all(pool.map(concurrent_probe,range(4)))
    captured=pop_all()
    assert len(captured)+trace.TraceDropped()-before==2000
    assert all(r.kind==3 and 0x30<=r.actor<0x34 and r.key==7 for r in captured)
    print('PASS: 2000 concurrent native deletes on four distinct actors; bounded trace drops accounted for; no lost original operations',flush=True)
