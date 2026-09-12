#include "BodyChangeNG/RaceMenuFormDeleteGuard.h"
#include "BodyChangeNG/RaceMenuFormDeletePolicy.h"
#include "BodyChangeNG/FormDeleteCallbackPattern.h"
#include "BodyChangeNG/PeImageFile.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>

namespace
{
    using Erase = void(*)(void*,std::uint32_t);
    Erase g_armorErase{}, g_nodeErase{};
    void* g_override{};
    void* g_module{};
    const char* g_status="not installed";
#ifdef BODY_CHANGE_NG_GUARD_PROBE
    std::atomic<std::uint64_t> g_skipped{},g_forwarded{},g_lastSkipped{};
#endif

    // Fixed storage only. No VM/actor lookup, PersistHandle, allocation, locks,
    // logger, geometry pointers, timers or restoration queue in this callback.
    __declspec(noinline) void OnFormDelete(std::uint64_t handle)
    {
        if (!bcn::racemenu_form_delete::IsDirectFormHandle(handle)) {
#ifdef BODY_CHANGE_NG_GUARD_PROBE
            g_skipped.fetch_add(1,std::memory_order_relaxed);
            g_lastSkipped.store(handle,std::memory_order_relaxed);
#endif
            return;
        }
#ifdef BODY_CHANGE_NG_GUARD_PROBE
        g_forwarded.fetch_add(1,std::memory_order_relaxed);
#endif
        const auto formID=static_cast<std::uint32_t>(handle);
        g_armorErase(g_override,formID);
        g_nodeErase(g_override,formID);
    }

    bool Readable(std::uintptr_t address,std::size_t count)
    {
        if (count>UINTPTR_MAX-address) return false;
        const auto end=address+count;
        while (address<end) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)) ||
                info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
                !(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|
                    PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
            const auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
            if (next<=address) return false;
            address=next;
        }
        return true;
    }

    bool ReadFile(HMODULE module,std::vector<unsigned char>& file)
    {
        std::array<wchar_t,32768> path{};
        const auto length=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
        if (!length || length>=path.size()) return false;
        std::ifstream input(std::filesystem::path{path.data()},std::ios::binary|std::ios::ate);
        if (!input || input.tellg()<=0 || input.tellg()>64*1024*1024) return false;
        file.resize(static_cast<std::size_t>(input.tellg()));
        input.seekg(0);
        return static_cast<bool>(input.read(reinterpret_cast<char*>(file.data()),static_cast<std::streamsize>(file.size())));
    }

    bool IsOverrideObject(const bcn::code_image::File& file,std::uintptr_t base,std::uint32_t object)
    {
        if (!file.HasFlags(object,8,IMAGE_SCN_MEM_WRITE) || !Readable(base+object,8)) return false;
        std::uintptr_t vtable{};
        std::memcpy(&vtable,reinterpret_cast<void*>(base+object),8);
        if (vtable<base+8 || vtable-base>=file.size ||
            !file.HasFlags(static_cast<std::uint32_t>(vtable-base-8),16,IMAGE_SCN_MEM_READ) ||
            !Readable(vtable-8,16)) return false;
        std::uintptr_t locator{};
        std::memcpy(&locator,reinterpret_cast<void*>(vtable-8),8);
        if (locator<base || locator-base>=file.size || !Readable(locator,24)) return false;
        const auto disk=file.Bytes(static_cast<std::uint32_t>(locator-base),24);
        if (disk.size()!=24 || std::memcmp(reinterpret_cast<void*>(locator),disk.data(),24) ||
            bcn::code_image::File::Read<std::uint32_t>(disk,0)!=1U ||
            bcn::code_image::File::Read<std::uint32_t>(disk,4)!=0U ||
            bcn::code_image::File::Read<std::uint32_t>(disk,20)!=locator-base) return false;
        const auto type=*bcn::code_image::File::Read<std::uint32_t>(disk,12);
        constexpr char name[]=".?AVOverrideInterface@@";
        if (type>file.size || file.size-type<16+sizeof(name)) return false;
        const auto descriptor=file.Bytes(type+16,sizeof(name));
        return descriptor.size()==sizeof(name) && !std::memcmp(descriptor.data(),name,sizeof(name)) &&
            Readable(base+type+16,sizeof(name)) && !std::memcmp(reinterpret_cast<void*>(base+type+16),name,sizeof(name));
    }
}

namespace bcn::racemenu_form_delete
{
    bool Install(void* module,bool verifiedHandleLayout) noexcept
    {
        if (!verifiedHandleLayout || !module) { g_status="runtime/handle layout not verified; unchanged"; return false; }
        if (g_module) return g_module==module;
        try {
            std::vector<unsigned char> bytes;
            if (!ReadFile(static_cast<HMODULE>(module),bytes)) { g_status="module file unavailable; unchanged"; return false; }
            const auto file=bcn::code_image::File::Parse(bytes);
            if (!file) { g_status="invalid PE image; unchanged"; return false; }
            const auto base=reinterpret_cast<std::uintptr_t>(module);
            std::optional<CallbackPlan> selected;
            for (const auto& function:file->functions) {
                if (function.end-function.begin!=42) continue;
                const auto code=file->Bytes(function.begin,42);
                const auto plan=DecodeNarrowingCallback(code,function.begin,file->size);
                if (!plan || !IsOverrideObject(*file,base,plan->object) ||
                    !file->IsFunction(plan->armorErase) || !file->IsFunction(plan->nodeErase)) continue;
                if (selected) { g_status="ambiguous narrowing callbacks; unchanged"; return false; }
                selected=plan;
            }
            if (!selected) {
                // Later official RaceMenu removed the callback. Old full-width
                // callbacks are safe and deliberately do not match either.
                g_status="no recognized narrowing FormDelete callback (removed/full-width/unknown); normal API unchanged";
                return false;
            }
            const auto plan=*selected;
            const auto original=file->Bytes(plan.entry,42);
            if (!Readable(base+plan.entry,42) || std::memcmp(reinterpret_cast<void*>(base+plan.entry),original.data(),42)) {
                g_status="callback code differs; unchanged"; return false;
            }
            for (const auto rva:{plan.armorErase,plan.nodeErase}) {
                const auto disk=file->Bytes(rva,32);
                if (disk.size()!=32 || !Readable(base+rva,32) || std::memcmp(reinterpret_cast<void*>(base+rva),disk.data(),32)) {
                    g_status="original erase entry differs; unchanged"; return false;
                }
            }
            std::array<unsigned char,14> patch{0xFF,0x25,0,0,0,0};
            const auto target=reinterpret_cast<std::uintptr_t>(&OnFormDelete);
            std::memcpy(patch.data()+6,&target,sizeof(target));
            auto* entry=reinterpret_cast<void*>(base+plan.entry);
            DWORD protection{};
            if (!VirtualProtect(entry,patch.size(),PAGE_EXECUTE_READWRITE,&protection)) {
                g_status="callback protection failed; unchanged"; return false;
            }
            g_override=reinterpret_cast<void*>(base+plan.object);
            g_armorErase=reinterpret_cast<Erase>(base+plan.armorErase);
            g_nodeErase=reinterpret_cast<Erase>(base+plan.nodeErase);
            // Startup only. Compiler-generated unwind metadata; no relocated
            // prologue, anonymous executable trampoline or foreign file edit.
            std::memcpy(entry,patch.data(),patch.size());
            const bool flushed=FlushInstructionCache(GetCurrentProcess(),entry,patch.size())!=0;
            DWORD unused{};
            const bool restored=VirtualProtect(entry,patch.size(),protection,&unused)!=0;
            if (!flushed || !restored) {
                DWORD ignored{};
                if (!restored || VirtualProtect(entry,patch.size(),PAGE_EXECUTE_READWRITE,&ignored)) {
                    std::memcpy(entry,original.data(),patch.size());
                    FlushInstructionCache(GetCurrentProcess(),entry,patch.size());
                    VirtualProtect(entry,patch.size(),protection,&unused);
                    g_status="installation finalization failed; callback rolled back";
                    return false;
                }
                g_status="installed; WARNING instruction-cache flush failed";
            } else g_status="installed: verified narrowing callback; direct Form deletes only";
            g_module=module;
            return true;
        } catch (...) { g_status="validation failed; unchanged"; return false; }
    }
    const char* Status() noexcept { return g_status; }
#ifdef BODY_CHANGE_NG_GUARD_PROBE
    Stats Statistics() noexcept
    {
        return {g_skipped.load(std::memory_order_relaxed),g_forwarded.load(std::memory_order_relaxed),
                g_lastSkipped.load(std::memory_order_relaxed)};
    }
#endif
}
