#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bcn::slider_name
{
    // RaceMenu's SKEEFixedString compares morph names case-insensitively.
    // Fold ASCII for identity only: keep the first authored spelling and leave
    // non-ASCII bytes, preset IDs and morph ownership keys unchanged.
    [[nodiscard]] constexpr unsigned char Fold(const unsigned char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
    }

    struct Equal final
    {
        using is_transparent = void;
        [[nodiscard]] constexpr bool operator()(const std::string_view left,
            const std::string_view right) const noexcept
        {
            if (left.size() != right.size()) return false;
            for (std::size_t i = 0; i < left.size(); ++i) {
                if (Fold(static_cast<unsigned char>(left[i])) != Fold(static_cast<unsigned char>(right[i]))) {
                    return false;
                }
            }
            return true;
        }
    };

    struct Hash final
    {
        using is_transparent = void;
        [[nodiscard]] constexpr std::size_t operator()(const std::string_view value) const noexcept
        {
            std::size_t hash = 5381U;
            for (const unsigned char byte : value) hash = hash * 33U + Fold(byte);
            return hash;
        }
    };

    template <class Value>
    using Map = std::unordered_map<std::string, Value, Hash, Equal>;
}
