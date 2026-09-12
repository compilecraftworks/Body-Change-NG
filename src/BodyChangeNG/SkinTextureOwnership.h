#pragma once

#include <string_view>

namespace bcn::skin_texture::ownership
{
    [[nodiscard]] constexpr char Normalize(const char value) noexcept
    {
        if (value == '/') return '\\';
        return value >= 'A' && value <= 'Z' ?
            static_cast<char>(value - 'A' + 'a') : value;
    }

    [[nodiscard]] constexpr bool ContainsNormalized(
        const std::string_view value, const std::string_view needle) noexcept
    {
        if (needle.empty()) return true;
        if (needle.size() > value.size()) return false;
        for (std::size_t offset{}; offset + needle.size() <= value.size(); ++offset) {
            bool equal = true;
            for (std::size_t position{}; position < needle.size(); ++position) {
                if (Normalize(value[offset + position]) != Normalize(needle[position])) {
                    equal = false;
                    break;
                }
            }
            if (equal) return true;
        }
        return false;
    }

    // RuntimeAssetCache places BCNG-owned assets in private namespaces. These
    // checks identify cloned native TXST and cloned live-material baselines;
    // they are unrelated to RaceMenu node/armor overrides.
    [[nodiscard]] constexpr bool IsOwnedBodySkinTexturePath(
        const std::string_view value) noexcept
    {
        return ContainsNormalized(value, "bodychangeng\\cache\\skin\\") ||
            ContainsNormalized(value, "bodychangeng\\cache\\skin-face\\") ||
            ContainsNormalized(value, "bodychangerng\\cache\\skin\\") ||
            ContainsNormalized(value, "bodychangerng\\cache\\skin-face\\");
    }

    [[nodiscard]] constexpr bool IsOwnedTexturePath(
        const std::string_view value) noexcept
    {
        return IsOwnedBodySkinTexturePath(value) ||
            ContainsNormalized(value, "bodychangeng\\cache\\futanari\\") ||
            ContainsNormalized(value, "bodychangerng\\cache\\futanari\\");
    }

    [[nodiscard]] constexpr bool IsRacialSkinVarianceTexturePath(
        const std::string_view value) noexcept
    {
        return ContainsNormalized(value, "actors\\character\\rsv\\");
    }
}
