#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace bcn::asset_identity
{
    // Skyrim's resource/fixed-string identifiers are ASCII case-insensitive.
    // Compare identity without changing authored spelling, persistent IDs, or
    // UTF-8 bytes. Do not trim, strip roots, or resolve '..': different resource
    // namespaces and invalid paths must not become interchangeable.
    [[nodiscard]] constexpr unsigned char FoldCase(unsigned char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
    }

    [[nodiscard]] constexpr unsigned char Fold(unsigned char value) noexcept
    {
        if (value == '/') return '\\';
        return FoldCase(value);
    }

    template<bool PathSeparators>
    struct BasicEqual final
    {
        using is_transparent = void;
        [[nodiscard]] constexpr bool operator()(std::string_view left,
            std::string_view right) const noexcept
        {
            if (left.size() != right.size()) return false;
            for (std::size_t i{}; i < left.size(); ++i) {
                const auto fold = PathSeparators ? Fold : FoldCase;
                if (fold(static_cast<unsigned char>(left[i])) !=
                    fold(static_cast<unsigned char>(right[i]))) return false;
            }
            return true;
        }
    };

    template<bool PathSeparators>
    struct BasicHash final
    {
        using is_transparent = void;
        [[nodiscard]] constexpr std::size_t operator()(std::string_view value) const noexcept
        {
            std::size_t hash = 5381U;
            const auto fold = PathSeparators ? Fold : FoldCase;
            for (unsigned char byte : value) hash = hash * 33U + fold(byte);
            return hash;
        }
    };

    using Equal = BasicEqual<true>;
    using Hash = BasicHash<true>;
    // Geometry names are not paths: slash and backslash remain distinct.
    using NameEqual = BasicEqual<false>;
    using NameHash = BasicHash<false>;

    struct Less final
    {
        using is_transparent = void;
        [[nodiscard]] constexpr bool operator()(std::string_view left, std::string_view right) const noexcept
        {
            const auto size = (std::min)(left.size(), right.size());
            for (std::size_t i{}; i < size; ++i) {
                const auto a = Fold(static_cast<unsigned char>(left[i]));
                const auto b = Fold(static_cast<unsigned char>(right[i]));
                if (a != b) return a < b;
            }
            return left.size() < right.size();
        }
    };

    [[nodiscard]] constexpr bool StartsWith(std::string_view value, std::string_view prefix) noexcept
    { return value.size() >= prefix.size() && Equal{}(value.substr(0, prefix.size()), prefix); }

    [[nodiscard]] constexpr bool EndsWith(std::string_view value, std::string_view suffix) noexcept
    { return value.size() >= suffix.size() && Equal{}(value.substr(value.size() - suffix.size()), suffix); }

    template<class Range, class Projection = std::identity>
    [[nodiscard]] auto Find(Range&& range, std::string_view value, Projection projection = {})
    {
        return std::ranges::find_if(range,
            [&](const auto& item) { return Equal{}(std::invoke(projection, item), value); });
    }

    template<class Value>
    using Map = std::unordered_map<std::string, Value, Hash, Equal>;
    template<class Key = std::string>
    using Set = std::unordered_set<Key, Hash, Equal>;

    template<class Range>
    [[nodiscard]] auto FindName(Range&& range, std::string_view value)
    {
        return std::ranges::find_if(range,
            [&](const auto& item) { return NameEqual{}(item, value); });
    }
}
