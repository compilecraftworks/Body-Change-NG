#pragma once

#include "BodyChangeNG/Distribution.h"
#include <limits>
#include <unordered_map>

namespace bcn::distribution_editor
{
    enum class Pool { body, skin, futanari, overlay };
    inline constexpr auto noRule = (std::numeric_limits<std::size_t>::max)();

    [[nodiscard]] inline std::size_t Count(const DistributionRule& rule, Pool pool)
    {
        switch (pool) {
        case Pool::body: return rule.presetIds.size();
        case Pool::skin: return rule.skinProfileIds.size();
        case Pool::futanari: return rule.futanariSkinIds.size();
        default: {
            std::size_t count{};
            for (const auto& ids : rule.overlayIds) count += ids.size();
            return count;
        }
        }
    }

    // Empty, unsaved rows have no persisted feature discriminator. Remember
    // their originating tab locally, without inventing a JSON schema field.
    struct Tabs
    {
        std::unordered_map<std::string, Pool> emptyPools;
        void Remember(const DistributionRule& rule, Pool pool) { emptyPools[rule.id] = pool; }
        [[nodiscard]] std::vector<std::size_t> Visible(
            const std::vector<DistributionRule>& rules, Pool pool) const
        {
            std::vector<std::size_t> result;
            for (std::size_t i{}; i < rules.size(); ++i) {
                const auto& rule = rules[i];
                const bool empty = rule.presetIds.empty() && rule.skinProfileIds.empty() &&
                    rule.futanariSkinIds.empty() && std::ranges::all_of(rule.overlayIds,
                        [](const auto& ids) { return ids.empty(); });
                const auto found = emptyPools.find(rule.id);
                if (Count(rule, pool) || (empty &&
                        (found == emptyPools.end() ? pool == Pool::body : found->second == pool)))
                    result.push_back(i);
            }
            return result;
        }
        [[nodiscard]] std::size_t Select(const std::vector<DistributionRule>& rules,
            Pool pool, std::size_t previous) const
        {
            const auto visible = Visible(rules, pool);
            if (std::ranges::find(visible, previous) != visible.end()) return previous;
            return visible.empty() ? noRule : visible.front();
        }
    };

    // Separate from main-window preview selection. Only this feature's item
    // pool is committed; conditions, rule order, other pools and colors survive.
    struct Items
    {
        DistributionRule value;
        Pool pool{};

        [[nodiscard]] std::vector<std::string>& Ids(overlay::Area area = overlay::Area::body)
        {
            switch (pool) {
            case Pool::body: return value.presetIds;
            case Pool::skin: return value.skinProfileIds;
            case Pool::futanari: return value.futanariSkinIds;
            default: return value.overlayIds[overlay::Index(area)];
            }
        }
        [[nodiscard]] bool Selected(std::string_view id, overlay::Area area = overlay::Area::body)
        {
            const auto& ids = Ids(area);
            return asset_identity::Find(ids, id) != ids.end();
        }
        void Set(const std::string& id, overlay::Area area, bool selected)
        {
            auto& ids = Ids(area);
            if (selected) {
                if (asset_identity::Find(ids, id) == ids.end()) ids.push_back(id);
            } else {
                std::erase_if(ids, [&](const auto& item) { return asset_identity::Equal{}(item, id); });
            }
        }
        void Clear()
        {
            if (pool == Pool::overlay) for (auto& ids : value.overlayIds) ids.clear();
            else Ids().clear();
        }
        [[nodiscard]] bool Commit(DistributionRule& rule) const
        {
            if (rule.id != value.id || rule.female != value.female) return false;
            switch (pool) {
            case Pool::body: rule.presetIds = value.presetIds; break;
            case Pool::skin: rule.skinProfileIds = value.skinProfileIds; break;
            case Pool::futanari: rule.futanariSkinIds = value.futanariSkinIds; break;
            case Pool::overlay:
                rule.overlayIds = value.overlayIds;
                // Keep existing tint for retained IDs, including unavailable
                // assets. New IDs retain the established default-white policy.
                PruneDistributionOverlayColors(rule);
                break;
            }
            return true;
        }
    };
}
