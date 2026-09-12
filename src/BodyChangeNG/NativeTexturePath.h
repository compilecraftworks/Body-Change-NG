#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace bcn::native_skin
{
    struct TexturePaths final
    {
        std::string native;    // BGSTextureSet: relative to Data/Textures.
        std::string resource;  // BSResource: relative to Data.
    };

    // Only converts BCNG cache output, never rewrites a provider's baseline.
    // The native TXST loader prepends "Data\\Textures\\" unconditionally.
    [[nodiscard]] inline std::optional<TexturePaths> PathsFromCache(std::string_view cached)
    {
        std::string path(cached);
        std::ranges::replace(path, '/', '\\');
        constexpr std::string_view prefix = "textures\\";
        if (path.size() <= prefix.size()) return std::nullopt;
        for (std::size_t i{}; i < prefix.size(); ++i) {
            const char c = path[i] >= 'A' && path[i] <= 'Z' ? path[i] - 'A' + 'a' : path[i];
            if (c != prefix[i]) return std::nullopt;
        }
        auto relative = path.substr(prefix.size());
        // The verified loader has a 260-byte destination including its prefix
        // and terminator. Fail before attaching a form on an invalid path.
        if (relative.size() + std::string_view("Data\\Textures\\").size() >= 260U ||
            relative.find_first_of(":|\0", 0, 3) != std::string::npos) return std::nullopt;
        for (std::size_t begin{}; begin <= relative.size();) {
            const auto end = relative.find('\\', begin);
            const auto part = std::string_view(relative).substr(begin,
                end == std::string::npos ? relative.size() - begin : end - begin);
            if (part.empty() || part == "." || part == "..") return std::nullopt;
            if (end == std::string::npos) break;
            begin = end + 1U;
        }
        if (!relative.starts_with("BodyChangeNG\\Cache\\")) return std::nullopt;
        return TexturePaths{ relative, "textures\\" + relative };
    }
}
