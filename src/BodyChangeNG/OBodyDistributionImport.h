#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace bcn::obody_distribution
{
    // OBody rules identify presets by display name, while BCNG identities also
    // include the source and BodyFamily. Preserve every same-name candidate for
    // the requested sex so runtime BodyFamily filtering can choose the correct
    // installed variant instead of being pinned to catalog order.
    template <class PresetRange>
    [[nodiscard]] std::vector<std::string> MatchingPresetIds(
        const PresetRange& catalog,
        const std::vector<std::string>& names,
        const bool female,
        std::size_t& requestedNames,
        std::unordered_set<std::string>& missingNames)
    {
        std::vector<std::string> ids;
        for (const auto& name : names) {
            ++requestedNames;
            bool foundUsable{};
            for (const auto& preset : catalog) {
                if (preset.male != !female || preset.name != name) continue;
                auto id = preset.PersistentId();
                if (id.empty()) continue;
                foundUsable = true;
                if (std::ranges::find(ids, id) == ids.end()) ids.push_back(std::move(id));
            }
            if (!foundUsable) missingNames.insert(name);
        }
        return ids;
    }

    // Name-based OBody entries are imported once for each sex because NPC-name
    // keys carry no sex metadata. A valid female-only preset will therefore be
    // absent from the synthetic male row (and vice versa), but it is not truly
    // missing. Keep only names that do not resolve anywhere in the catalog for
    // the user-facing warning and log.
    template <class PresetRange>
    void RetainGloballyMissingPresetNames(
        const PresetRange& catalog, std::unordered_set<std::string>& names)
    {
        std::erase_if(names, [&](const auto& name) {
            return std::ranges::any_of(catalog, [&](const auto& preset) {
                return preset.name == name && !preset.PersistentId().empty();
            });
        });
    }
}
