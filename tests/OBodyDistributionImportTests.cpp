#include "BodyChangeNG/OBodyDistributionImport.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    struct Preset final
    {
        std::string name;
        std::string id;
        bool male{};

        [[nodiscard]] std::string PersistentId() const { return id; }
    };

    [[nodiscard]] bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    [[nodiscard]] bool IsStringArray(const nlohmann::json& value)
    {
        return value.is_array() && std::ranges::all_of(value, [](const auto& entry) {
            return entry.is_string();
        });
    }

    [[nodiscard]] bool IsPresetMap(const nlohmann::json& value)
    {
        if (!value.is_object()) return false;
        return std::ranges::all_of(value.items(), [](const auto& entry) {
            return IsStringArray(entry.value());
        });
    }

    [[nodiscard]] bool IsPluginFormMap(const nlohmann::json& value, const bool assignments)
    {
        if (!value.is_object()) return false;
        return std::ranges::all_of(value.items(), [assignments](const auto& plugin) {
            if (!assignments) return IsStringArray(plugin.value());
            if (!plugin.value().is_object()) return false;
            return std::ranges::all_of(plugin.value().items(), [](const auto& form) {
                return IsStringArray(form.value());
            });
        });
    }
}

int main(const int argc, char** argv)
{
    const std::vector<Preset> catalog{
        { "Shared", "cbbe/shared", false },
        { "Shared", "ube/shared", false },
        { "Shared", "himbo/shared", true },
        { "Duplicate ID", "cbbe/shared", false },
        { "Empty ID", "", false }
    };

    std::size_t requested{};
    std::unordered_set<std::string> missing;
    const auto female = bcn::obody_distribution::MatchingPresetIds(
        catalog, std::vector<std::string>{ "Shared", "Duplicate ID", "Missing", "Empty ID" },
        true, requested, missing);
    if (!Require(female == std::vector<std::string>{ "cbbe/shared", "ube/shared" },
            "same-name female BodyFamily candidates were not all retained") ||
        !Require(requested == 4U, "requested OBody preset-name count is wrong") ||
        !Require(missing.contains("Missing") && missing.contains("Empty ID"),
            "missing OBody preset names were not reported") ||
        !Require(!missing.contains("Shared"), "a matched OBody preset name was reported missing")) return 1;

    requested = 0U;
    missing.clear();
    const auto male = bcn::obody_distribution::MatchingPresetIds(
        catalog, std::vector<std::string>{ "Shared" }, false, requested, missing);
    if (!Require(male == std::vector<std::string>{ "himbo/shared" },
            "OBody preset-name import crossed the requested sex") ||
        !Require(requested == 1U && missing.empty(), "matched male OBody preset was not accounted for")) return 1;

    requested = 0U;
    missing.clear();
    static_cast<void>(bcn::obody_distribution::MatchingPresetIds(
        catalog, std::vector<std::string>{ "Shared", "Actually Missing" }, false, requested, missing));
    bcn::obody_distribution::RetainGloballyMissingPresetNames(catalog, missing);
    if (!Require(missing == std::unordered_set<std::string>{ "Actually Missing" },
            "a valid opposite-sex OBody preset was reported as globally missing")) return 1;

    constexpr std::array stringArrays{
        "blacklistedPresetsFromRandomDistribution", "blacklistedNpcs",
        "blacklistedNpcsPluginFemale", "blacklistedNpcsPluginMale",
        "blacklistedRacesFemale", "blacklistedRacesMale"
    };
    constexpr std::array presetMaps{
        "npc", "factionFemale", "factionMale", "npcPluginFemale",
        "npcPluginMale", "raceFemale", "raceMale"
    };
    for (int index = 1; index < argc; ++index) {
        std::ifstream stream(argv[index]);
        if (!stream) {
            std::cerr << "Could not open " << argv[index] << '\n';
            return 10;
        }
        const auto root = nlohmann::json::parse(stream);
        if (!Require(root.is_object(), "OBody distribution root is not an object")) return 11;
        std::size_t populatedFields{};
        std::size_t sourceEntries{};
        for (const auto* key : stringArrays) {
            const auto node = root.find(key);
            if (node == root.end()) continue;
            if (!Require(IsStringArray(*node), "OBody distribution string-list field has an unsupported shape")) return 12;
            populatedFields += !node->empty();
            sourceEntries += node->size();
        }
        for (const auto* key : presetMaps) {
            const auto node = root.find(key);
            if (node == root.end()) continue;
            if (!Require(IsPresetMap(*node), "OBody distribution assignment field has an unsupported shape")) return 13;
            populatedFields += !node->empty();
            sourceEntries += node->size();
        }
        if (const auto node = root.find("blacklistedNpcsFormID"); node != root.end()) {
            if (!Require(IsPluginFormMap(*node, false), "OBody excluded FormID field has an unsupported shape")) return 14;
            populatedFields += !node->empty();
            sourceEntries += node->size();
        }
        if (const auto node = root.find("npcFormID"); node != root.end()) {
            if (!Require(IsPluginFormMap(*node, true), "OBody assigned FormID field has an unsupported shape")) return 15;
            populatedFields += !node->empty();
            sourceEntries += node->size();
        }
        std::cout << argv[index] << " distribution-populated-fields=" << populatedFields
                  << " top-level-source-entries=" << sourceEntries << '\n';
    }
}
