#pragma once

#include <string_view>

namespace bcn::skin_override::ownership
{
    [[nodiscard]] constexpr char Normalize(const char value) noexcept
    {
        if (value == '/') return '\\';
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
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

    // RuntimeAssetCache gives every BCNG-owned texture a private namespace.
    // RaceMenu does not expose an owner field for override keys, so the value
    // namespace is the durable ownership marker that survives save/load.
    [[nodiscard]] constexpr bool IsOwnedTexturePath(const std::string_view value) noexcept
    {
        return ContainsNormalized(value, "bodychangeng\\cache\\skin\\") ||
            ContainsNormalized(value, "bodychangeng\\cache\\skin-face\\") ||
            ContainsNormalized(value, "bodychangerng\\cache\\skin\\") ||
            ContainsNormalized(value, "bodychangerng\\cache\\skin-face\\");
    }

    [[nodiscard]] constexpr bool MayReplace(const bool exists, const std::string_view currentValue) noexcept
    {
        return !exists || IsOwnedTexturePath(currentValue);
    }

    [[nodiscard]] constexpr bool MayRemove(const bool exists, const std::string_view currentValue) noexcept
    {
        return exists && IsOwnedTexturePath(currentValue);
    }
}
