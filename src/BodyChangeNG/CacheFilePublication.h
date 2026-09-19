#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <string>
#include <atomic>

namespace bcn::cache_files
{
    // Build beside the destination, then publish atomically. A failed copy
    // never leaves a partial DDS at the public path, and never deletes the
    // previous good alias. Temp names are unique and owned by this call only.
    inline bool Publish(const std::filesystem::path& source, const std::filesystem::path& destination,
        std::error_code& error)
    {
        static std::atomic_uint64_t sequence{};
        std::filesystem::path temporary;
        bool materialized{};
        bool copied{};
        struct Cleanup {
            std::filesystem::path& path; bool& owned; bool& copy;
            ~Cleanup() {
                if (!owned) return;
                // Only a private copy's attributes may be changed. Never
                // change attributes through a link to the original source.
                if (copy) {
                    const auto attributes = GetFileAttributesW(path.c_str());
                    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY))
                        SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
                }
                DeleteFileW(path.c_str());
            }
        } cleanup{temporary, materialized, copied};
        const auto attributes = GetFileAttributesW(source.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) { error = std::error_code(GetLastError(), std::system_category()); return false; }
        for (unsigned attempt{}; attempt < 32; ++attempt) {
            temporary = destination.wstring() + L".bcng-prepare-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed));
            if (!(attributes & FILE_ATTRIBUTE_READONLY) && CreateHardLinkW(temporary.c_str(), source.c_str(), nullptr)) {
                materialized = true;
                break;
            }
            if (CopyFileW(source.c_str(), temporary.c_str(), TRUE)) {
                copied = materialized = true;
                if (attributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(temporary.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
                break;
            }
            const auto code = GetLastError();
            // An existing name is not ours; try another without touching it.
            if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) continue;
            copied = materialized = GetFileAttributesW(temporary.c_str()) != INVALID_FILE_ATTRIBUTES;
            error = std::error_code(code, std::system_category());
            return false;
        }
        if (!materialized) { error = std::make_error_code(std::errc::file_exists); return false; }
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = std::error_code(GetLastError(), std::system_category()); return false;
        }
        materialized = false; // Rename transferred ownership to the public alias.
        error.clear();
        return true;
    }
}
