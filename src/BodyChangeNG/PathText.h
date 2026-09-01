#pragma once

#include <filesystem>
#include <string>

namespace bcn::path_text
{
    // std::filesystem::path stores UTF-16 on Windows.  Never pass the active
    // ANSI code-page representation returned by path::string() to ImGui or
    // persist it as an identifier: that would make Korean and Simplified
    // Chinese names depend on Windows' optional UTF-8 system-locale switch.
    [[nodiscard]] inline std::string Utf8(const std::filesystem::path& path)
    {
        const auto value = path.u8string();
        return { reinterpret_cast<const char*>(value.data()), value.size() };
    }

    [[nodiscard]] inline std::string GenericUtf8(const std::filesystem::path& path)
    {
        const auto value = path.generic_u8string();
        return { reinterpret_cast<const char*>(value.data()), value.size() };
    }

    [[nodiscard]] inline std::filesystem::path FromUtf8(const std::string_view value)
    {
        const std::u8string converted{
            reinterpret_cast<const char8_t*>(value.data()), value.size()
        };
        return std::filesystem::path{ converted };
    }
}
