#pragma once

#include <filesystem>

namespace bcn::path_migration
{
    struct ResolvedFile final
    {
        std::filesystem::path path;
        bool legacy{};
    };

    [[nodiscard]] inline ResolvedFile ResolveFile(
        const std::filesystem::path& preferred, const std::filesystem::path& legacy) noexcept
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(preferred, error) && !error) return { preferred, false };
        error.clear();
        if (std::filesystem::is_regular_file(legacy, error) && !error) return { legacy, true };
        return { preferred, false };
    }
}
