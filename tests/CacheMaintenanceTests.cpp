#define NOMINMAX
#include "CacheCompaction.h"
#include "BodyChangeNG/CacheFilePublication.h"
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    void Check(bool yes, const char* what) { if (!yes) throw std::runtime_error(what); }
    std::string Read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
    struct Fixture {
        fs::path stage = fs::temp_directory_path() / (L"BCNG-CacheMaintenanceTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        fs::path root = stage / "textures" / "BodyChangeNG" / "Cache";
        Fixture() { Check(fs::create_directories(root), "new private fixture required"); }
        ~Fixture() {
            // Fixed owned fixture only, never the installed cache or a save.
            std::error_code e;
            if (fs::equivalent(stage.parent_path(), fs::temp_directory_path(), e) && !e && stage.filename().wstring().starts_with(L"BCNG-CacheMaintenanceTests-")) {
                for (fs::recursive_directory_iterator it(stage, e), end; !e && it != end; it.increment(e)) {
                    const auto attributes = GetFileAttributesW(it->path().c_str());
                    if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) && (attributes & FILE_ATTRIBUTE_READONLY))
                        SetFileAttributesW(it->path().c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
                }
                fs::remove_all(stage, e);
                if (e) std::cerr << "Fixture cleanup failed: " << e.message() << '\n';
            }
        }
        fs::path Write(std::wstring key, std::string data, std::wstring name = L"skin.dds", std::wstring space = L"skin") {
            const auto p = root / space / key / name;
            fs::create_directories(p.parent_path());
            std::ofstream f(p, std::ios::binary); f << data; f.close();
            return p;
        }
    };
    void Align(const fs::path& a, const fs::path& b) { fs::last_write_time(b, fs::last_write_time(a)); }
    std::size_t Temps(const fs::path& root) {
        std::size_t count{};
        for (const auto& p : fs::recursive_directory_iterator(root)) if (p.path().filename().wstring().find(L".bcng-") != std::wstring::npos) ++count;
        return count;
    }
}
int main() try
{
    Fixture f;
    const std::string bytes(32768, 'a');
    const auto a = f.Write(L"1111111111111111", bytes);
    const auto b = f.Write(L"2222222222222222", bytes, L"femalehead.dds", L"skin-face"); Align(a, b);
    const auto c = f.Write(L"3333333333333333", std::string(32768, 'b')); Align(a, c);
    const auto differentTime = f.Write(L"4444444444444444", bytes);
    fs::last_write_time(differentTime, fs::last_write_time(a) + std::chrono::seconds(10));
    const auto unknown = f.Write(L"not-a-cache-key", bytes); Align(a, unknown);
    const auto external = f.Write(L"5555555555555555", bytes); Align(a, external);
    const auto original = f.stage / "original.dds"; fs::create_hard_link(external, original);
    const auto readonly = f.Write(L"6666666666666666", bytes); Align(a, readonly);
    Check(SetFileAttributesW(readonly.c_str(), FILE_ATTRIBUTE_READONLY), "read-only fixture");
    const auto dry = bcn::cache_compaction::Run(f.root, false);
    Check(dry.validRoot && dry.complete && dry.candidates == 1 && dry.linked == 0, "dry run must find only the eligible duplicate");
    Check(!fs::equivalent(a, b) && Read(b) == bytes && Temps(f.stage) == 0, "dry run mutated files");
    Check(!bcn::cache_compaction::Run(f.stage, true).validRoot, "broad root accepted");
    Check(!bcn::cache_compaction::Run(f.root / ".." / "Cache", true).validRoot, "unresolved traversal accepted");
    Check(!bcn::cache_compaction::Run(f.root, true, [] { return false; }).complete, "cancellation ignored");
    const auto done = bcn::cache_compaction::Run(f.root, true);
    Check(done.complete && !done.failures && done.linked == 1 && done.reclaimedBytes > 0, "compaction failed");
    Check(fs::equivalent(a, b) && Read(a) == bytes && Read(b) == bytes, "DDS identity/bytes changed");
    Check(!fs::equivalent(a, c) && !fs::equivalent(a, external) && !fs::equivalent(a, differentTime), "unrelated files linked");
    Check(fs::equivalent(external, original) && Read(original) == bytes, "source pack changed");
    Check(!fs::equivalent(a, readonly) && !fs::equivalent(a, unknown), "unknown ownership changed");
    Check(Temps(f.stage) == 0, "temporary hard link leaked");
    const auto repeated = bcn::cache_compaction::Run(f.root, true);
    Check(!repeated.candidates && !repeated.linked && !repeated.failures, "repeat compaction not idempotent");
    // A writer/reader that denies delete must not turn replacement into a gap.
    const auto locked = f.Write(L"7777777777777777", bytes); Align(a, locked);
    HANDLE lock = CreateFileW(locked.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "lock fixture");
    const auto blocked = bcn::cache_compaction::Run(f.root, true);
    CloseHandle(lock);
    Check(blocked.failures == 1 && Read(locked) == bytes && !fs::equivalent(a, locked) && Temps(f.stage) == 0, "blocked replacement removed a DDS or left temp");
    const auto retry = bcn::cache_compaction::Run(f.root, true);
    Check(retry.linked == 1 && fs::equivalent(a, locked), "retry failed");
    const auto abandoned = f.Write(L"9999999999999999", "unfinished", L"body.dds.bcng-prepare-123-4");
    const auto abandonedLink = f.Write(L"9999999999999999", "unused", L"head.dds.bcng-link-0123456789abcdef0123456789abcdef");
    const auto userFile = f.Write(L"9999999999999999", "keep", L"body.dds.bcng-prepare-user-backup");
    const auto stagedScan = bcn::cache_compaction::Run(f.root, false);
    Check(stagedScan.stagingFiles == 2 && !stagedScan.stagingRemoved && fs::exists(abandoned), "staging dry run");
    const auto stagedClean = bcn::cache_compaction::Run(f.root, true);
    Check(stagedClean.stagingRemoved == 2 && !fs::exists(abandoned) && !fs::exists(abandonedLink) && Read(userFile) == "keep", "precise staging cleanup");
    fs::remove(userFile);
    // Never follow a directory symlink (when the host permits creating one).
    std::error_code error;
    const auto link = f.root / "tint" / "8888888888888888";
    fs::create_directories(link.parent_path());
    fs::create_directory_symlink(f.stage, link, error);
    if (!error) { Check(bcn::cache_compaction::Run(f.root, false).skipped > 0, "reparse not excluded"); fs::remove(link); }
    // Atomic publication keeps previous bytes on errors and unlinks only its
    // own unexposed staging file. New aliases still hard-link where possible.
    const auto source = f.stage / "source.dds", dest = f.stage / "published.dds";
    { std::ofstream(source) << "new"; std::ofstream(dest) << "old"; }
    Check(bcn::cache_files::Publish(source, dest, error) && fs::equivalent(source, dest), "publication did not preserve original hard-link behavior");
    Check(!bcn::cache_files::Publish(f.stage / "missing.dds", dest, error) && Read(dest) == "new" && Temps(f.stage) == 0, "missing source destroyed old alias");
    const auto replacement = f.stage / "replacement.dds"; { std::ofstream(replacement) << "replacement"; }
    lock = CreateFileW(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "publication lock");
    const auto success = bcn::cache_files::Publish(replacement, dest, error);
    CloseHandle(lock);
    Check(!success && Read(dest) == "new" && Read(source) == "new" && Temps(f.stage) == 0, "failed publication destroyed old or leaked temp");
    Check(bcn::cache_files::Publish(replacement, dest, error) && Read(dest) == "replacement" && Read(source) == "new", "publication retry/source isolation");
    // Read-only source copies remain intact; cleanup must not clear their
    // attributes through a hard link if destination replacement is blocked.
    SetFileAttributesW(replacement.c_str(), FILE_ATTRIBUTE_READONLY);
    lock = CreateFileW(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "read-only publication lock");
    Check(!bcn::cache_files::Publish(replacement, dest, error), "blocked read-only copy succeeded");
    CloseHandle(lock);
    Check((GetFileAttributesW(replacement.c_str()) & FILE_ATTRIBUTE_READONLY) && Temps(f.stage) == 0, "read-only source modified or private copy leaked");
    SetFileAttributesW(replacement.c_str(), FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(readonly.c_str(), FILE_ATTRIBUTE_NORMAL);
    for (int n = 0; n < 1000; ++n) {
        Check(!bcn::cache_files::Publish(f.stage / "missing.dds", dest, error), "unexpected missing publication");
    }
    Check(Temps(f.stage) == 0 && Read(dest) == "replacement", "1000 failure cleanup cycles");
    lock = CreateFileW(dest.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "stress lock");
    DWORD handlesBefore{}, handlesAfter{};
    Check(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore), "handle baseline");
    for (int n = 0; n < 1000; ++n) Check(!bcn::cache_files::Publish(source, dest, error), "blocked stress publication");
    Check(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) && handlesAfter == handlesBefore, "publication handle leak");
    CloseHandle(lock);
    Check(Temps(f.stage) == 0 && Read(dest) == "replacement", "1000 prepared-temp cleanup cycles");
    {
        Fixture bulk;
        std::vector<fs::path> anchors;
        for (unsigned group = 0; group < 12; ++group) {
            const std::string content(65536, char('a' + group));
            for (unsigned copy = 0; copy < 8; ++copy) {
                wchar_t key[17]{}; swprintf_s(key, L"%016X", group * 8 + copy + 1);
                const auto p = bulk.Write(key, content, L"피부 파일.dds");
                if (!copy) anchors.push_back(p); else Align(anchors.back(), p);
            }
        }
        const auto report = bcn::cache_compaction::Run(bulk.root, true);
        Check(report.complete && !report.failures && report.linked == 84, "multi-group compaction or Unicode names");
        for (unsigned group = 0; group < 12; ++group) {
            for (unsigned copy = 0; copy < 8; ++copy) {
                wchar_t key[17]{}; swprintf_s(key, L"%016X", group * 8 + copy + 1);
                const auto p = bulk.root / L"skin" / key / L"피부 파일.dds";
                Check(fs::equivalent(anchors[group], p) && Read(p) == std::string(65536, char('a' + group)), "published alias content/identity");
            }
        }
        const auto again = bcn::cache_compaction::Run(bulk.root, true);
        Check(again.complete && !again.candidates && !again.failures && !Temps(bulk.stage), "multi-group repeat/temp leak");
    }
    std::cout << "Cache maintenance: dry-run, bytes/paths, hard-link ownership, timestamps, cancellation, idempotence, locks, retry, atomic publication and 1000 failure cycles PASS\n";
    return 0;
}
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
