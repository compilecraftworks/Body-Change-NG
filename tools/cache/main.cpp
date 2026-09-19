#define NOMINMAX
#include "CacheCompaction.h"
#include <Windows.h>
#include <shobjidl.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    std::filesystem::path SelectFolder()
    {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return {};
        struct Uninitialize { ~Uninitialize() { CoUninitialize(); } } uninitialize;
        IFileDialog* dialog{};
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
        struct Release { IFileDialog* d; ~Release() { d->Release(); } } release{dialog};
        DWORD options{};
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"BCNG 1.3.1 - Select MO2 Overwrite or textures\\BodyChangeNG\\Cache");
        if (FAILED(dialog->Show(nullptr))) return {};
        IShellItem* item{};
        if (FAILED(dialog->GetResult(&item))) return {};
        PWSTR text{};
        const auto result = item->GetDisplayName(SIGDN_FILESYSPATH, &text);
        item->Release();
        if (FAILED(result)) return {};
        std::filesystem::path path(text);
        CoTaskMemFree(text);
        return path;
    }
    std::wstring Describe(const bcn::cache_compaction::Result& r)
    {
        std::wostringstream out;
        out << L"Files checked: " << r.files << L"\nDuplicate paths: " << r.candidates
            << L"\nUnpublished staging files: " << r.stagingFiles << L"\nStaging files removed: " << r.stagingRemoved
            << L"\nEstimated reclaimable space: " << r.reclaimableBytes / (1024.0 * 1024.0) << L" MiB"
            << L"\nPaths compacted: " << r.linked << L"\nEstimated space reclaimed: "
            << r.reclaimedBytes / (1024.0 * 1024.0) << L" MiB\nSkipped: " << r.skipped
            << L"\nErrors: " << r.failures;
        if (!r.complete) out << L"\nScan stopped or root was rejected. Results are incomplete.";
        out << L"\n\nAll DDS paths are preserved. Source packs and saves are not modified."
            L"\nFolder size can look unchanged because hard links share disk storage."
            L"\nUnique textures and files with unknown ownership are retained.";
        return out.str();
    }
}
int wmain(int argc, wchar_t** argv) try
{
    const bool interactive = argc == 1;
    if (!interactive && (argc != 3 || (std::wstring_view(argv[1]) != L"--scan" && std::wstring_view(argv[1]) != L"--compact"))) {
        std::wcerr << L"BodyChangeNGCache.exe --scan|--compact \"full path to MO2 Overwrite or textures\\BodyChangeNG\\Cache\"\n";
        return 2;
    }
    if (bcn::cache_compaction::GameRunning()) {
        if (interactive) MessageBoxW(nullptr, L"Close Skyrim before scanning or compacting its cache.", L"Body Change NG Cache", MB_OK | MB_ICONWARNING);
        std::wcerr << L"Skyrim is running or its process list is unavailable. No files changed.\n";
        return 2;
    }
    const auto single = CreateMutexW(nullptr, TRUE, L"Local\\BodyChangeNGCacheMaintenance");
    if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (single) CloseHandle(single);
        std::wcerr << L"Another cache-maintenance tool is running, or its lock is unavailable.\n";
        return 2;
    }
    struct ReleaseMutex { HANDLE h; ~ReleaseMutex() { ::ReleaseMutex(h); CloseHandle(h); } } releaseMutex{single};
    auto root = interactive ? SelectFolder() : std::filesystem::path(argv[2]);
    if (root.empty()) return 0;
    std::error_code error;
    if (std::filesystem::is_directory(root / L"textures" / L"BodyChangeNG" / L"Cache", error))
        root /= L"textures\\BodyChangeNG\\Cache";
    root = std::filesystem::absolute(root).lexically_normal();
    auto checked = std::chrono::steady_clock::now();
    bool stopped{};
    auto allowed = [&] {
        if (stopped) return false;
        const auto now = std::chrono::steady_clock::now();
        if (now - checked < std::chrono::milliseconds(250)) return true;
        checked = now;
        stopped = bcn::cache_compaction::GameRunning();
        return !stopped;
    };
    std::wcout << L"Scanning BCNG cache (read-only). Keep Skyrim and texture editors closed...\n" << std::flush;
    const auto scan = bcn::cache_compaction::Run(root, false, allowed);
    if (!scan.validRoot) {
        if (interactive) MessageBoxW(nullptr, L"Select a physical textures\\BodyChangeNG\\Cache folder (or its MO2 Overwrite). Junctions/symlinks are not accepted.", L"Body Change NG Cache", MB_OK | MB_ICONWARNING);
        std::wcerr << L"Invalid or unsafe cache root. No files changed.\n";
        return 2;
    }
    std::wcout << root.wstring() << L"\n" << Describe(scan) << L"\n";
    if (!scan.complete || scan.failures) {
        if (interactive) MessageBoxW(nullptr, Describe(scan).c_str(), L"Body Change NG Cache - scan incomplete", MB_OK | MB_ICONWARNING);
        return 1;
    }
    const bool apply = interactive ? (scan.candidates || scan.stagingFiles) && MessageBoxW(nullptr,
        (Describe(scan) + L"\n\nCompact these duplicates now? / 중복 저장 공간을 정리할까요?").c_str(),
        L"Body Change NG Cache", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES :
        std::wstring_view(argv[1]) == L"--compact";
    if (interactive && !scan.candidates && !scan.stagingFiles) MessageBoxW(nullptr, Describe(scan).c_str(), L"Body Change NG Cache", MB_OK | MB_ICONINFORMATION);
    if (!apply) return 0;
    if (bcn::cache_compaction::GameRunning()) return 2;
    const auto result = bcn::cache_compaction::Run(root, true, allowed);
    std::wcout << Describe(result) << L"\n";
    if (interactive) MessageBoxW(nullptr, Describe(result).c_str(), L"Body Change NG Cache", MB_OK | (result.failures ? MB_ICONWARNING : MB_ICONINFORMATION));
    return result.validRoot && result.complete && !result.failures ? 0 : 1;
}
catch (const std::exception& e) { std::cerr << "Cache maintenance stopped: " << e.what() << '\n'; return 1; }
