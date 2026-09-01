#include "BodyChangerNG/CatalogRoots.h"

#include "BodyChangerNG/PathText.h"

#if defined(BODY_CHANGER_NG_RUNTIME)
#include <SKSE/Logger.h>
#endif

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>

namespace
{
    [[nodiscard]] std::optional<std::filesystem::path> FinalPath(const std::filesystem::path& path)
    {
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) return std::nullopt;
        const auto directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
        const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
        std::wstring buffer(32768U, L'\0');
        const auto length = GetFinalPathNameByHandleW(handle, buffer.data(),
            static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        CloseHandle(handle);
        if (length == 0U || length >= buffer.size()) return std::nullopt;
        buffer.resize(length);
        constexpr std::wstring_view uncPrefix{ LR"(\\?\UNC\)" };
        constexpr std::wstring_view extendedPrefix{ LR"(\\?\)" };
        if (buffer.starts_with(uncPrefix)) buffer = LR"(\\)" + buffer.substr(uncPrefix.size());
        else if (buffer.starts_with(extendedPrefix)) buffer.erase(0U, extendedPrefix.size());
        return std::filesystem::path{ buffer };
    }

    [[nodiscard]] std::string PathKey(const std::filesystem::path& path)
    {
        auto key = bcn::path_text::GenericUtf8(path.lexically_normal());
        std::ranges::transform(key, key.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return key;
    }

    void AddUnique(std::vector<std::filesystem::path>& roots, const std::filesystem::path& candidate)
    {
        if (candidate.empty()) return;
        std::error_code error;
        if (!std::filesystem::is_directory(candidate, error) || error) return;
        const auto key = PathKey(candidate);
        if (!std::ranges::any_of(roots, [&](const auto& root) { return PathKey(root) == key; })) {
            roots.push_back(candidate.lexically_normal());
        }
    }

    void AddProviderForEntry(std::vector<std::filesystem::path>& roots,
        const std::filesystem::path& logicalRoot, const std::filesystem::path& entry)
    {
        const auto resolved = FinalPath(entry);
        if (!resolved) return;
        const auto relative = entry.lexically_relative(logicalRoot);
        if (relative.empty() || relative == std::filesystem::path{ "." } || relative.is_absolute()) {
            AddUnique(roots, *resolved);
            return;
        }
        auto providerRoot = *resolved;
        for ([[maybe_unused]] const auto& component : relative) providerRoot = providerRoot.parent_path();
        AddUnique(roots, providerRoot);
    }
}

namespace bcn::catalog_roots
{
    std::vector<std::filesystem::path> Discover(const std::filesystem::path& logicalRoot)
    {
        std::vector<std::filesystem::path> roots;
        AddProviderForEntry(roots, logicalRoot, logicalRoot);

        std::error_code error;
        std::size_t inspected{};
        for (std::filesystem::recursive_directory_iterator it(logicalRoot,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end && inspected < 4096U; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            AddProviderForEntry(roots, logicalRoot, it->path());
            ++inspected;
        }
        // The logical root is deliberately last: when the same asset exists in
        // several providers, USVFS' current winning file must own the row.
        AddUnique(roots, logicalRoot);
#if defined(BODY_CHANGER_NG_RUNTIME)
        SKSE::log::info("Body Changer NG refresh roots for {}: {}", path_text::Utf8(logicalRoot), roots.size());
#endif
        return roots;
    }
}
