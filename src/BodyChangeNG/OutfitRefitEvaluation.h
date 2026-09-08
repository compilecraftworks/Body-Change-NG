#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bcn::outfit_refit_evaluation
{
    struct ArmorIdentity final
    {
        std::string_view name;
        std::string_view plugin;
        std::uint32_t formID{};
    };

    template <class Rules>
    [[nodiscard]] bool IsBlacklisted(const ArmorIdentity& armor, const Rules& rules)
    {
        return rules.blacklistedOutfitNames.contains(std::string(armor.name)) ||
               rules.blacklistedPlugins.contains(std::string(armor.plugin)) ||
               rules.blacklistedFormIDs.contains(armor.formID);
    }

    template <class Rules>
    [[nodiscard]] bool IsForced(const ArmorIdentity& armor, const Rules& rules)
    {
        return rules.forcedOutfitNames.contains(std::string(armor.name)) ||
               rules.forcedFormIDs.contains(armor.formID);
    }

    struct CandidateDecision final
    {
        bool eligible{};
        bool forced{};
    };

    template <class Rules>
    [[nodiscard]] CandidateDecision Evaluate(const ArmorIdentity& armor, const Rules& rules)
    {
        // This mirrors OBody NG: a blacklisted torso item is treated as if it
        // were not clothing, while a force-refit entry in any worn slot can
        // still make the actor clothed for ORefit purposes.
        return {
            .eligible = !IsBlacklisted(armor, rules),
            .forced = IsForced(armor, rules)
        };
    }

    [[nodiscard]] constexpr bool ShouldApply(const bool hasEligibleTorso, const bool hasForcedWornItem)
    {
        return hasEligibleTorso || hasForcedWornItem;
    }
}
