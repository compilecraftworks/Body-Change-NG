#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace bcn::ui
{
    // UI-session values only: no actors, textures, task callbacks, or save ownership.
    // Separate instances keep tint and overlay namespaces independent.
    template <class Color>
    class AppearanceColorDrafts final
    {
    public:
        struct Entry { std::uint8_t channel; std::string id; Color color; };

        void Set(std::uint32_t actor, std::uint8_t channel, std::string_view id, Color color)
        {
            if (actor && !id.empty()) values_.insert_or_assign(
                Key{ actor, channel, std::string(id) }, color);
        }
        [[nodiscard]] std::optional<Color> Find(
            std::uint32_t actor, std::uint8_t channel, std::string_view id) const
        {
            const auto found = values_.find(Key{ actor, channel, std::string(id) });
            return found == values_.end() ? std::nullopt : std::optional{ found->second };
        }
        [[nodiscard]] std::vector<Entry> Entries(std::uint32_t actor) const
        {
            std::vector<Entry> result;
            for (const auto& [key, color] : values_) {
                if (std::get<0>(key) == actor) result.push_back(
                    { std::get<1>(key), std::get<2>(key), color });
            }
            return result;
        }
        template<class SelectedIds, class Fallback>
        [[nodiscard]] std::map<std::string, Color, std::less<>> CopySelection(
            std::uint32_t actor, std::uint8_t channel, const SelectedIds& ids, Fallback&& fallback) const
        {
            std::map<std::string, Color, std::less<>> result;
            for (const auto& id : ids) {
                const auto draft = Find(actor, channel, id);
                result[id] = draft ? *draft : fallback(id);
            }
            return result;
        }
        void EraseActor(std::uint32_t actor)
        {
            std::erase_if(values_, [actor](const auto& item) { return std::get<0>(item.first) == actor; });
        }
        void Clear() { values_.clear(); }

    private:
        using Key = std::tuple<std::uint32_t, std::uint8_t, std::string>;
        std::map<Key, Color> values_;
    };
}
