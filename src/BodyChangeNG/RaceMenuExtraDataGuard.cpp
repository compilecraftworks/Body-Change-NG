#include "BodyChangeNG/RaceMenuExtraDataGuard.h"
#include "BodyChangeNG/PeImageFile.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>

namespace
{
    constexpr std::size_t kFileSize = 2007552;
    constexpr std::uint32_t kComparator = 0xF2390;
    constexpr std::array<std::uint8_t, 32> kSha256{
        0x28,0x3e,0xa6,0xf0,0xdf,0x62,0x34,0xb5,0x63,0x6d,0x6b,0x03,0x44,0x5a,0x57,0xc9,
        0x03,0x69,0x51,0x4e,0x61,0xec,0x3b,0x02,0xda,0x07,0xf7,0x31,0xfc,0xf3,0x46,0x9b
    };
    constexpr std::array<std::uint8_t, 16> kOriginal{
        0x4c,0x8b,0x01,0x48,0x8b,0x02,0x41,0x8b,0x48,0x10,0x2b,0x48,0x10,0x8b,0xc1,0xc3
    };
    void* g_installedModule{};
    const char* g_status = "not installed";

    bool Readable(std::uintptr_t address, std::size_t count)
    {
        if (!address || count > UINTPTR_MAX - address) return false;
        const auto end = address + count;
        while (address < end) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) ||
                info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                !(info.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                    PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
            const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            if (next <= address) return false;
            address = next;
        }
        return true;
    }

    bool MatchesFile(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() != kFileSize) return false;
        std::array<std::uint8_t, 32> hash{};
        // Windows' SHA-256 pseudo-handle needs no allocated provider/hash state.
        const auto status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
            const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()),
            hash.data(), static_cast<ULONG>(hash.size()));
        return status >= 0 && hash == kSha256;
    }

    bool InstallImage(void* module, std::span<const std::uint8_t> bytes, bool isSkyrim1597)
    {
        if (!isSkyrim1597 || !module) {
            g_status = "not the Skyrim 1.5.97 target; unchanged"; return false;
        }
        if (!MatchesFile(bytes)) {
            g_status = "not the pinned UBE SE DLL; unchanged"; return false;
        }
        const auto file = bcn::code_image::File::Parse(bytes);
        if (!file) { g_status = "invalid PE image; unchanged"; return false; }
        const auto base = reinterpret_cast<std::uintptr_t>(module);
        if (file->size > UINTPTR_MAX - base || !Readable(base, 0x400) ||
            std::memcmp(module, bytes.data(), 0x400)) {
            g_status = "loaded image header differs; unchanged"; return false;
        }

        std::array<std::uint8_t, 14> patch{0xFF, 0x25, 0, 0, 0, 0};
        const auto target = reinterpret_cast<std::uintptr_t>(&bcn::racemenu_extra_data::CompareExtraData);
        static_assert(sizeof(target) == 8);
        std::memcpy(patch.data() + 6, &target, sizeof(target));
        auto* entry = reinterpret_cast<void*>(base + kComparator);
        if (g_installedModule == module) {
            const bool intact = Readable(base + kComparator, patch.size()) &&
                !std::memcmp(entry, patch.data(), patch.size());
            g_status = intact ? "already installed: full-width ExtraData ordering" :
                                "installed comparator was changed externally; left unchanged";
            return intact;
        }
        if (g_installedModule) { g_status = "another module already handled; unchanged"; return false; }

        // These are verified RVA ranges of this exact digest, not version-based
        // offsets for other RaceMenu builds. No inline search/parallel morphing
        // code is replaced. Refuse a foreign hook at the comparator or caller.
        struct Range { std::uint32_t begin, length; };
        for (const auto range : {Range{kComparator, 16}, Range{0xF23A0, 0x1B8},
                                 Range{0x84BA, 0x55}, Range{0x5FC0, 6}}) {
            const auto disk = file->Bytes(range.begin, range.length);
            if (disk.size() != range.length ||
                !file->HasFlags(range.begin, range.length, IMAGE_SCN_MEM_EXECUTE) ||
                !Readable(base + range.begin, range.length) ||
                std::memcmp(reinterpret_cast<void*>(base + range.begin), disk.data(), range.length)) {
                g_status = "live comparator/caller code differs; unchanged"; return false;
            }
        }
        if (std::memcmp(entry, kOriginal.data(), kOriginal.size())) {
            g_status = "unrecognized comparator; unchanged"; return false;
        }
        DWORD protection{};
        if (!VirtualProtect(entry, patch.size(), PAGE_EXECUTE_READWRITE, &protection)) {
            g_status = "comparator protection failed; unchanged"; return false;
        }
        // Startup only, before geometry jobs. Replace one complete leaf entry
        // with a tail jump to compiler-owned code. No executable allocation,
        // relocated prologue, callback state, actor cache or reference lifetime.
        std::memcpy(entry, patch.data(), patch.size());
        const bool flushed = FlushInstructionCache(GetCurrentProcess(), entry, patch.size()) != 0;
        DWORD unused{};
        const bool restored = VirtualProtect(entry, patch.size(), protection, &unused) != 0;
        if (!flushed || !restored) {
            DWORD ignored{};
            if (!restored || VirtualProtect(entry, patch.size(), PAGE_EXECUTE_READWRITE, &ignored)) {
                std::memcpy(entry, kOriginal.data(), patch.size());
                const bool rollbackFlushed = FlushInstructionCache(GetCurrentProcess(), entry, patch.size()) != 0;
                const bool rollbackProtected = VirtualProtect(entry, patch.size(), protection, &unused) != 0;
                g_status = rollbackFlushed && rollbackProtected ? "finalization failed; comparator rolled back" :
                    "ERROR rollback finalization failed; restart required";
                return false;
            }
            // The replacement remains live, so do not claim it was removed.
            g_installedModule = module;
            g_status = "installed; WARNING instruction-cache finalization failed; restart required";
            return true;
        }
        g_installedModule = module;
        g_status = "installed: pinned UBE SE full-width ExtraData ordering";
        return true;
    }
}

namespace bcn::racemenu_extra_data
{
    int CompareExtraData(const void* left, const void* right) noexcept
    {
        // Original qsort ABI: pointers to NiExtraData* entries; the verified
        // flat layout stores each interned name pointer at +0x10. memcpy avoids
        // a fabricated C++ object type/alias. No allocation or retained pointers.
        const std::byte* a{};
        const std::byte* b{};
        std::uintptr_t aName{}, bName{};
        std::memcpy(&a, left, sizeof(a));
        std::memcpy(&b, right, sizeof(b));
        std::memcpy(&aName, a + 0x10, sizeof(aName));
        std::memcpy(&bName, b + 0x10, sizeof(bName));
        return CompareNameAddresses(aName, bName);
    }

    bool Install(void* module, bool isSkyrim1597) noexcept
    {
        if (!isSkyrim1597 || !module) {
            g_status = "not the Skyrim 1.5.97 target; unchanged"; return false;
        }
        try {
            std::array<wchar_t, 32768> path{};
            const auto length = GetModuleFileNameW(static_cast<HMODULE>(module), path.data(),
                static_cast<DWORD>(path.size()));
            if (!length || length >= path.size()) { g_status = "module path unavailable; unchanged"; return false; }
            std::ifstream input(std::filesystem::path{path.data()}, std::ios::binary | std::ios::ate);
            if (!input || input.tellg() != static_cast<std::streamoff>(kFileSize)) {
                g_status = "not the pinned UBE SE DLL size; unchanged"; return false;
            }
            std::vector<std::uint8_t> bytes(kFileSize);
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
                g_status = "module file unavailable; unchanged"; return false;
            }
            return InstallImage(module, bytes, isSkyrim1597);
        } catch (...) { g_status = "validation failed; unchanged"; return false; }
    }

    const char* Status() noexcept { return g_status; }
#ifdef BODY_CHANGE_NG_EXTRA_DATA_TEST
    bool InstallTestImage(void* image, std::span<const std::uint8_t> file, bool isSkyrim1597) noexcept
    {
        try { return InstallImage(image, file, isSkyrim1597); }
        catch (...) { g_status = "test validation failed; unchanged"; return false; }
    }
#endif
}
