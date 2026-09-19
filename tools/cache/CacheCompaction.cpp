#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "CacheCompaction.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    struct Close { void operator()(void* h) const { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
    using Handle = std::unique_ptr<void, Close>;
    std::wstring Lower(std::wstring s)
    { for (auto& c : s) if (c >= L'A' && c <= L'Z') c += L'a' - L'A'; return s; }
    std::wstring Name(const fs::path& p) { return Lower(p.filename().wstring()); }
    bool Plain(const fs::path& p, bool directory)
    {
        const auto a = GetFileAttributesW(p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_REPARSE_POINT) &&
            bool(a & FILE_ATTRIBUTE_DIRECTORY) == directory;
    }
    bool RootSafe(const fs::path& root)
    {
        if (!root.is_absolute() || root != root.lexically_normal() || Name(root) != L"cache" ||
            Name(root.parent_path()) != L"bodychangeng" || Name(root.parent_path().parent_path()) != L"textures") return false;
        for (auto p = root; p.has_relative_path(); p = p.parent_path()) if (!Plain(p, true)) return false;
        return Plain(root, true);
    }
    bool HashDirectory(const fs::path& p)
    {
        const auto s = p.filename().wstring();
        return s.size() == 16 && std::ranges::all_of(s, [](wchar_t c) {
            return (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f');
        });
    }
    bool StagingName(const fs::path& p)
    {
        const auto name = Name(p);
        const auto digits = [](std::wstring_view s) {
            return !s.empty() && std::ranges::all_of(s, [](wchar_t c) { return c >= L'0' && c <= L'9'; });
        };
        if (const auto at = name.rfind(L".dds.bcng-prepare-"); at != std::wstring::npos && at > 0) {
            const auto suffix = std::wstring_view(name).substr(at + 18);
            const auto dash = suffix.find(L'-');
            return dash != suffix.npos && digits(suffix.substr(0, dash)) && digits(suffix.substr(dash + 1));
        }
        if (const auto at = name.rfind(L".dds.bcng-link-"); at != std::wstring::npos && at > 0) {
            const auto suffix = std::wstring_view(name).substr(at + 15);
            return suffix.size() == 32 && std::ranges::all_of(suffix, [](wchar_t c) {
                return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f');
            });
        }
        return false;
    }
    Handle Open(const fs::path& p, bool replace = false)
    {
        // Deny writers. Publication handles share DELETE for atomic rename;
        // scanning handles deny deletion too. Existing writers are skipped.
        auto h = CreateFileW(p.c_str(), GENERIC_READ,
            FILE_SHARE_READ | (replace ? FILE_SHARE_DELETE : 0), nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        return Handle(h == INVALID_HANDLE_VALUE ? nullptr : h);
    }
    using Identity = std::array<DWORD, 3>;
    Identity Id(const BY_HANDLE_FILE_INFORMATION& i)
    { return {i.dwVolumeSerialNumber, i.nFileIndexHigh, i.nFileIndexLow}; }
    std::uint64_t Size(const BY_HANDLE_FILE_INFORMATION& i)
    { return (std::uint64_t(i.nFileSizeHigh) << 32) | i.nFileSizeLow; }
    std::uint64_t Time(const BY_HANDLE_FILE_INFORMATION& i)
    { return (std::uint64_t(i.ftLastWriteTime.dwHighDateTime) << 32) | i.ftLastWriteTime.dwLowDateTime; }
    bool Info(HANDLE h, BY_HANDLE_FILE_INFORMATION& i)
    { return GetFileInformationByHandle(h, &i) && !(i.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY)); }
    bool Rewind(HANDLE h) { return SetFilePointerEx(h, {}, nullptr, FILE_BEGIN); }
    using Digest = std::array<unsigned char, 32>;
    std::optional<Digest> Hash(HANDLE h, const std::function<bool()>& allowed)
    {
        if (!Rewind(h)) return {};
        BCRYPT_ALG_HANDLE alg{};
        BCRYPT_HASH_HANDLE hash{};
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
        struct Cleanup { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h;
            ~Cleanup() { if (h) BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a, 0); } } cleanup{alg, hash};
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) return {};
        std::array<unsigned char, 256 * 1024> buffer;
        for (;;) {
            DWORD count{};
            if (!allowed() || !ReadFile(h, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return {};
            if (!count) break;
            if (BCryptHashData(hash, buffer.data(), count, 0) < 0) return {};
        }
        Digest result;
        if (BCryptFinishHash(hash, result.data(), DWORD(result.size()), 0) < 0) return {};
        return result;
    }
    bool Equal(HANDLE a, HANDLE b, const std::function<bool()>& allowed)
    {
        if (!Rewind(a) || !Rewind(b)) return false;
        std::array<unsigned char, 64 * 1024> x, y;
        for (;;) {
            DWORD nx{}, ny{};
            if (!allowed() || !ReadFile(a, x.data(), DWORD(x.size()), &nx, nullptr) ||
                !ReadFile(b, y.data(), DWORD(y.size()), &ny, nullptr) || nx != ny) return false;
            if (!nx) return true;
            if (!std::equal(x.begin(), x.begin() + nx, y.begin())) return false;
        }
    }
    struct File { fs::path path; BY_HANDLE_FILE_INFORMATION info; std::uint64_t allocation{}; };
    bool Same(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b)
    { return Id(a) == Id(b) && Size(a) == Size(b) && Time(a) == Time(b); }
    bool Replace(const fs::path& root, const File& anchor, const File& victim,
        const std::function<bool()>& allowed, std::uint64_t& reclaimed)
    {
        if (!allowed() || !RootSafe(root) || !Plain(anchor.path.parent_path(), true) ||
            !Plain(victim.path.parent_path(), true) || !Plain(anchor.path.parent_path().parent_path(), true) ||
            !Plain(victim.path.parent_path().parent_path(), true)) return false;
        auto a = Open(anchor.path, true), b = Open(victim.path, true);
        BY_HANDLE_FILE_INFORMATION ai{}, bi{};
        if (!a || !b || !Info(a.get(), ai) || !Info(b.get(), bi) || !Same(ai, anchor.info) ||
            !Same(bi, victim.info) || Id(ai) == Id(bi) || Size(ai) != Size(bi) || Time(ai) != Time(bi) ||
            !Equal(a.get(), b.get(), allowed)) return false;
        std::array<unsigned char, 16> random{};
        if (BCryptGenRandom(nullptr, random.data(), DWORD(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
        std::wstring suffix = L".bcng-link-";
        for (auto c : random) { suffix += L"0123456789abcdef"[c >> 4]; suffix += L"0123456789abcdef"[c & 15]; }
        const fs::path temporary = victim.path.wstring() + suffix;
        if (!CreateHardLinkW(temporary.c_str(), anchor.path.c_str(), nullptr)) return false;
        struct RemoveTemporary { fs::path p; ~RemoveTemporary() { DeleteFileW(p.c_str()); } } cleanup{temporary};
        // Never unlink the public DDS first. If linking/renaming fails, its
        // previous bytes and path remain available. Only our random temp dies.
        auto now = Open(victim.path, true);
        Handle linked(CreateFileW(temporary.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        BY_HANDLE_FILE_INFORMATION ni{}, li{};
        if (!allowed() || !linked || linked.get() == INVALID_HANDLE_VALUE || !Info(linked.get(), li) || !Same(li, ai) ||
            !now || !Info(now.get(), ni) || !Same(ni, victim.info)) return false;
        const auto name = victim.path.wstring();
        // Include the terminator too: the Win32 DOS-to-NT path conversion may
        // inspect the string before forwarding FileNameLength to the kernel.
        std::vector<unsigned char> buffer(offsetof(FILE_RENAME_INFO, FileName) + (name.size() + 1) * sizeof(wchar_t));
        auto& rename = *reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
        // Windows 10+ POSIX replacement retains the old file for our open
        // read handles and atomically changes the name. No write-denying
        // validation handle needs to be dropped before the operation.
        rename.Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
        rename.FileNameLength = DWORD(name.size() * sizeof(wchar_t));
        std::copy(name.begin(), name.end(), rename.FileName);
        if (!SetFileInformationByHandle(linked.get(), FileRenameInfoEx, &rename, DWORD(buffer.size()))) return false;
        auto published = Open(victim.path, true);
        BY_HANDLE_FILE_INFORMATION pi{};
        if (!published || !Info(published.get(), pi) || !Same(pi, ai)) return false;
        if (ni.nNumberOfLinks == 1) reclaimed += victim.allocation;
        return true;
    }
}

namespace bcn::cache_compaction
{
    bool GameRunning()
    {
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snapshot || snapshot.get() == INVALID_HANDLE_VALUE) return true;
        PROCESSENTRY32W entry{sizeof(entry)};
        if (!Process32FirstW(snapshot.get(), &entry)) return true;
        do { if (Lower(entry.szExeFile) == L"skyrimse.exe") return true; }
        while (Process32NextW(snapshot.get(), &entry));
        return GetLastError() != ERROR_NO_MORE_FILES;
    }

    Result Run(const fs::path& root, bool apply, const std::function<bool()>& allowed)
    {
        Result result;
        if (!RootSafe(root) || !allowed()) return result;
        result.validRoot = true;
        std::vector<File> files;
        std::vector<File> staging;
        std::map<Identity, std::size_t> aliases;
        std::error_code error;
        for (const auto* space : {L"skin", L"skin-face", L"futanari", L"tint"}) {
            const auto dir = root / space;
            if (!fs::exists(dir, error)) { if (error) ++result.failures; error.clear(); continue; }
            if (!Plain(dir, true)) { ++result.skipped; continue; }
            fs::directory_iterator end;
            for (fs::directory_iterator keys(dir, error); !error && keys != end; keys.increment(error)) {
                if (!allowed()) return result;
                if (!HashDirectory(keys->path()) || !Plain(keys->path(), true)) { ++result.skipped; continue; }
                std::error_code inner;
                for (fs::directory_iterator it(keys->path(), inner); !inner && it != end; it.increment(inner)) {
                    if (!allowed() || files.size() + staging.size() >= 100000) return result;
                    const auto temporary = StagingName(it->path());
                    if ((!temporary && Lower(it->path().extension().wstring()) != L".dds") || !Plain(it->path(), false)) { ++result.skipped; continue; }
                    auto h = Open(it->path());
                    BY_HANDLE_FILE_INFORMATION info{};
                    FILE_STANDARD_INFO standard{};
                    if (!h || !Info(h.get(), info) || (!temporary && !Size(info)) ||
                        !GetFileInformationByHandleEx(h.get(), FileStandardInfo, &standard, sizeof(standard))) { ++result.skipped; continue; }
                    (temporary ? staging : files).push_back({it->path(), info, std::uint64_t(standard.AllocationSize.QuadPart)});
                    if (!temporary) ++aliases[Id(info)];
                }
                if (inner) ++result.failures;
            }
            if (error) { ++result.failures; error.clear(); }
        }
        result.files = files.size();
        result.stagingFiles = staging.size();
        for (const auto& file : staging) {
            if (!allowed()) return result;
            if (file.info.nNumberOfLinks == 1) result.reclaimableBytes += file.allocation;
            if (!apply) continue;
            if (!RootSafe(root) || !Plain(file.path.parent_path(), true) || !Plain(file.path.parent_path().parent_path(), true)) { ++result.failures; continue; }
            // These exact temporary names are never handed to Skyrim. Acquire
            // exclusive access so no active preparation/editor can lose them.
            Handle h(CreateFileW(file.path.c_str(), DELETE | FILE_READ_ATTRIBUTES, 0, nullptr,
                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            BY_HANDLE_FILE_INFORMATION info{};
            FILE_DISPOSITION_INFO remove{TRUE};
            if (!h || h.get() == INVALID_HANDLE_VALUE || !Info(h.get(), info) || !Same(info, file.info) ||
                !SetFileInformationByHandle(h.get(), FileDispositionInfo, &remove, sizeof(remove))) { ++result.failures; continue; }
            ++result.stagingRemoved;
            if (info.nNumberOfLinks == 1) result.reclaimedBytes += file.allocation;
        }
        // External hard links often point to mutable source packs. Do not
        // couple another pack to those bytes; they consume no extra copy now.
        std::map<std::pair<std::uint64_t, std::uint64_t>, std::vector<File>> buckets;
        for (const auto& file : files) {
            if (aliases.at(Id(file.info)) != file.info.nNumberOfLinks) { ++result.skipped; continue; }
            buckets[{Size(file.info), Time(file.info)}].push_back(file);
        }
        for (const auto& [key, bucket] : buckets) {
            if (bucket.size() < 2) continue;
            std::map<Digest, File> anchors;
            std::map<Identity, Digest> hashes;
            std::map<Identity, std::size_t> replaced;
            for (const auto& file : bucket) {
                if (!allowed()) return result;
                auto h = Open(file.path);
                BY_HANDLE_FILE_INFORMATION info{};
                if (!h || !Info(h.get(), info) || !Same(info, file.info)) { ++result.skipped; continue; }
                auto prior = hashes.find(Id(info));
                const auto digest = prior == hashes.end() ? Hash(h.get(), allowed) : std::optional<Digest>(prior->second);
                if (!digest) { ++result.failures; continue; }
                hashes.insert_or_assign(Id(info), *digest);
                const auto [anchor, inserted] = anchors.try_emplace(*digest, file);
                if (inserted || Id(anchor->second.info) == Id(info)) continue;
                auto a = Open(anchor->second.path);
                if (!a || !Equal(a.get(), h.get(), allowed)) { ++result.skipped; continue; }
                ++result.candidates;
                if (++replaced[Id(info)] == aliases.at(Id(info))) result.reclaimableBytes += file.allocation;
                a.reset(); h.reset();
                if (apply) {
                    if (Replace(root, anchor->second, file, allowed, result.reclaimedBytes)) ++result.linked;
                    else ++result.failures;
                }
            }
        }
        result.complete = true;
        return result;
    }
}
