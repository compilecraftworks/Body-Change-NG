#include "BodyChangeNG/RuntimeAssetCache.h"

#include "BodyChangeNG/PathText.h"

#include <SKSE/Logger.h>

#include <format>
#include <mutex>
#include <ranges>
#include <unordered_map>

namespace
{
    std::mutex g_registeredSourcesLock;
    std::unordered_map<std::string, std::filesystem::path> g_registeredSources;

    [[nodiscard]] std::string NormalizeGamePath(std::string_view path)
    {
        std::string result{ path };
        std::ranges::replace(result, '/', '\\');
        std::ranges::transform(result, result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    [[nodiscard]] std::uint64_t Hash(const std::string_view value) noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (const auto character : value) {
            hash = (hash ^ static_cast<std::uint8_t>(character)) * 1099511628211ULL;
        }
        return hash;
    }

    [[nodiscard]] bool SameFile(const std::filesystem::path& left,
        const std::filesystem::path& right)
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(left, error) || error) return false;
        const auto leftSize = std::filesystem::file_size(left, error);
        if (error) return false;
        const auto rightSize = std::filesystem::file_size(right, error);
        if (error || leftSize != rightSize) return false;
        const auto leftTime = std::filesystem::last_write_time(left, error);
        if (error) return false;
        const auto rightTime = std::filesystem::last_write_time(right, error);
        return !error && leftTime == rightTime;
    }
}

namespace bcn::runtime_assets
{
    void ClearGameRelativeSources(const std::string_view prefix)
    {
        const auto normalized = NormalizeGamePath(prefix);
        std::scoped_lock lock(g_registeredSourcesLock);
        std::erase_if(g_registeredSources, [&normalized](const auto& entry) {
            return entry.first.starts_with(normalized);
        });
    }

    void RegisterGameRelativeSource(const std::string_view path,
        const std::filesystem::path& source)
    {
        if (path.empty() || source.empty()) return;
        std::error_code error;
        if (!std::filesystem::is_regular_file(source, error) || error) return;
        std::scoped_lock lock(g_registeredSourcesLock);
        g_registeredSources.insert_or_assign(NormalizeGamePath(path), source);
    }

    std::string TexturePath(const std::filesystem::path& source,
        const std::string_view nameSpace)
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(source, error) || error) {
            SKSE::log::error("Body Change NG texture source is missing: {}", path_text::Utf8(source));
            return {};
        }

        const auto identity = path_text::GenericUtf8(source);
        auto extension = path_text::Utf8(source.extension());
        if (extension.empty()) extension = ".dds";
        const auto filename = std::format("{:016X}{}", Hash(identity), extension);
        const auto relative = std::filesystem::path{ "textures" } / "BodyChangeNG" / "Cache" /
            path_text::FromUtf8(nameSpace) / filename;
        const auto destination = std::filesystem::current_path() / "Data" / relative;

        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            SKSE::log::error("Body Change NG could not create texture cache directory {}: {}",
                path_text::Utf8(destination.parent_path()), error.message());
            return {};
        }
        if (!SameFile(destination, source)) {
            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::create_hard_link(source, destination, error);
            if (error) {
                error.clear();
                std::filesystem::copy_file(source, destination,
                    std::filesystem::copy_options::overwrite_existing, error);
                if (!error) {
                    const auto sourceTime = std::filesystem::last_write_time(source, error);
                    if (!error) std::filesystem::last_write_time(destination, sourceTime, error);
                }
            }
            if (error) {
                SKSE::log::error("Body Change NG could not materialize texture cache {} from {}: {}",
                    path_text::Utf8(destination), path_text::Utf8(source), error.message());
                return {};
            }
        }

        auto result = path_text::GenericUtf8(relative);
        std::ranges::replace(result, '/', '\\');
        return result;
    }

    std::string TexturePathFromGameRelative(const std::string_view path,
        const std::string_view nameSpace)
    {
        if (path.empty()) return {};
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            if (const auto found = g_registeredSources.find(NormalizeGamePath(path));
                found != g_registeredSources.end()) {
                std::error_code error;
                if (std::filesystem::is_regular_file(found->second, error) && !error) {
                    return TexturePath(found->second, nameSpace);
                }
            }
        }
        auto relative = path_text::FromUtf8(path);
        return TexturePath(std::filesystem::current_path() / "Data" / relative, nameSpace);
    }
}
