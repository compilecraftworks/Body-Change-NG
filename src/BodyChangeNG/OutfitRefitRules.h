#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bcn::outfit_refit_rules
{
    struct LocalFormReference final
    {
        std::string plugin;
        std::string formID;

        bool operator==(const LocalFormReference&) const = default;
    };

    struct ImportedRules final
    {
        std::unordered_map<std::string, std::string> femalePresetByOutfit;
        std::unordered_map<std::string, std::string> malePresetByOutfit;
        std::unordered_set<std::string> blacklistedOutfitNames;
        std::unordered_set<std::string> blacklistedPlugins;
        std::vector<LocalFormReference> blacklistedFormIDs;
        std::unordered_set<std::string> forcedOutfitNames;
        std::vector<LocalFormReference> forcedFormIDs;
    };

    // Reads only OBody NG's ORefit-related keys. Game FormIDs are deliberately
    // left unresolved so this parser remains deterministic and independently
    // testable; OutfitRefit resolves plugin-local IDs against the active load
    // order on the game thread.
    [[nodiscard]] inline bool ParseOBodyRules(const nlohmann::json& root, ImportedRules& output)
    {
        output = {};
        if (!root.is_object()) return false;

        constexpr std::size_t kMaximumEntries = 65536U;
        constexpr std::size_t kMaximumTextLength = 1024U;
        std::size_t accepted{};

        const auto acceptText = [&](const nlohmann::json& value, std::string& text) {
            if (!value.is_string() || accepted >= kMaximumEntries) return false;
            text = value.get<std::string>();
            if (text.empty() || text.size() > kMaximumTextLength) return false;
            ++accepted;
            return true;
        };
        const auto readStringSet = [&](const char* key, auto& destination) {
            const auto node = root.find(key);
            if (node == root.end() || !node->is_array()) return;
            for (const auto& value : *node) {
                std::string text;
                if (acceptText(value, text)) destination.insert(std::move(text));
            }
        };
        const auto readMappings = [&](const char* key, auto& destination) {
            const auto node = root.find(key);
            if (node == root.end() || !node->is_object()) return;
            for (auto entry = node->begin(); entry != node->end() && accepted < kMaximumEntries; ++entry) {
                if (entry.key().empty() || entry.key().size() > kMaximumTextLength) continue;
                std::string preset;
                if (acceptText(entry.value(), preset)) destination.insert_or_assign(entry.key(), std::move(preset));
            }
        };
        const auto readFormReferences = [&](const char* key, auto& destination) {
            const auto node = root.find(key);
            if (node == root.end() || !node->is_object()) return;
            for (auto plugin = node->begin(); plugin != node->end() && accepted < kMaximumEntries; ++plugin) {
                if (plugin.key().empty() || plugin.key().size() > kMaximumTextLength || !plugin.value().is_array()) continue;
                for (const auto& source : plugin.value()) {
                    std::string formID;
                    if (acceptText(source, formID)) destination.push_back({ plugin.key(), std::move(formID) });
                }
            }
        };

        readStringSet("blacklistedOutfitsFromORefit", output.blacklistedOutfitNames);
        readStringSet("blacklistedOutfitsFromORefitPlugin", output.blacklistedPlugins);
        readFormReferences("blacklistedOutfitsFromORefitFormID", output.blacklistedFormIDs);
        readStringSet("outfitsForceRefit", output.forcedOutfitNames);
        readFormReferences("outfitsForceRefitFormID", output.forcedFormIDs);
        readMappings("refitOutfitPresetsFemale", output.femalePresetByOutfit);
        readMappings("refitOutfitPresetsMale", output.malePresetByOutfit);
        return true;
    }
}
