#include "BodyChangeNG/OutfitRefitEvaluation.h"
#include "BodyChangeNG/OutfitRefitRules.h"

#include <fstream>
#include <iostream>

namespace
{
    [[nodiscard]] bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main(const int argc, char** argv)
{
    using bcn::outfit_refit_evaluation::ArmorIdentity;
    using bcn::outfit_refit_evaluation::Evaluate;
    using bcn::outfit_refit_evaluation::IsBlacklisted;
    using bcn::outfit_refit_evaluation::IsForced;
    using bcn::outfit_refit_evaluation::ShouldApply;
    using bcn::outfit_refit_rules::ImportedRules;
    using bcn::outfit_refit_rules::LocalFormReference;
    using bcn::outfit_refit_rules::ParseOBodyRules;

    const nlohmann::json source{
        { "blacklistedOutfitsFromORefit", { "Open Dress", "Nude Armor" } },
        { "blacklistedOutfitsFromORefitPlugin", { "Excluded.esp" } },
        { "blacklistedOutfitsFromORefitFormID", {
            { "Armors.esp", { "0x001234", "00ABCD" } }
        } },
        { "outfitsForceRefit", { "Force Ring" } },
        { "outfitsForceRefitFormID", {
            { "Accessories.esl", { "0x812" } }
        } },
        { "refitOutfitPresetsFemale", {
            { "Iron Armor", "Iron Female-Refit" }
        } },
        { "refitOutfitPresetsMale", {
            { "Iron Armor", "Iron Male-Refit" }
        } }
    };

    ImportedRules rules;
    if (!Require(ParseOBodyRules(source, rules), "synthetic ORefit JSON was rejected") ||
        !Require(rules.blacklistedOutfitNames.contains("Open Dress") &&
            rules.blacklistedOutfitNames.contains("Nude Armor"),
            "ORefit outfit-name exclusions were dropped") ||
        !Require(rules.blacklistedPlugins.contains("Excluded.esp"),
            "ORefit plugin exclusion was dropped") ||
        !Require(rules.blacklistedFormIDs == std::vector<LocalFormReference>{
            { "Armors.esp", "0x001234" }, { "Armors.esp", "00ABCD" }
        }, "ORefit FormID exclusions were dropped") ||
        !Require(rules.forcedOutfitNames.contains("Force Ring"),
            "ORefit forced outfit name was dropped") ||
        !Require(rules.forcedFormIDs == std::vector<LocalFormReference>{
            { "Accessories.esl", "0x812" }
        }, "ORefit forced FormID was dropped") ||
        !Require(rules.femalePresetByOutfit.at("Iron Armor") == "Iron Female-Refit" &&
            rules.malePresetByOutfit.at("Iron Armor") == "Iron Male-Refit",
            "ORefit sex-specific mapping was dropped")) return 1;

    struct ResolvedRules final
    {
        std::unordered_map<std::string, std::string> femalePresetByOutfit;
        std::unordered_map<std::string, std::string> malePresetByOutfit;
        std::unordered_set<std::string> blacklistedOutfitNames;
        std::unordered_set<std::string> blacklistedPlugins;
        std::unordered_set<std::uint32_t> blacklistedFormIDs;
        std::unordered_set<std::string> forcedOutfitNames;
        std::unordered_set<std::uint32_t> forcedFormIDs;
    };
    ResolvedRules resolved{
        .blacklistedOutfitNames = rules.blacklistedOutfitNames,
        .blacklistedPlugins = rules.blacklistedPlugins,
        .blacklistedFormIDs = { 0x1234U },
        .forcedOutfitNames = rules.forcedOutfitNames,
        .forcedFormIDs = { 0xFE000812U }
    };
    if (!Require(IsBlacklisted(ArmorIdentity{ "Open Dress", "Allowed.esp", 1U }, resolved),
            "runtime policy ignored an outfit-name exclusion") ||
        !Require(IsBlacklisted(ArmorIdentity{ "Allowed Outfit", "Excluded.esp", 2U }, resolved),
            "runtime policy ignored a plugin exclusion") ||
        !Require(IsBlacklisted(ArmorIdentity{ "Allowed Outfit", "Allowed.esp", 0x1234U }, resolved),
            "runtime policy ignored a resolved FormID exclusion") ||
        !Require(!IsBlacklisted(ArmorIdentity{ "Allowed Outfit", "Allowed.esp", 3U }, resolved),
            "runtime policy excluded an unrelated outfit") ||
        !Require(IsForced(ArmorIdentity{ "Force Ring", "Allowed.esp", 4U }, resolved) &&
            IsForced(ArmorIdentity{ "Other Ring", "Allowed.esp", 0xFE000812U }, resolved),
            "runtime policy ignored a force-refit entry") ||
        !Require(!Evaluate(ArmorIdentity{ "Open Dress", "Allowed.esp", 1U }, resolved).eligible,
            "a blacklisted outfit remained eligible for correction") ||
        !Require(ShouldApply(false, true) && ShouldApply(true, false) && !ShouldApply(false, false),
            "torso/force-refit precedence diverged from OBody NG")) return 1;

    ImportedRules invalid;
    invalid.forcedOutfitNames.insert("stale");
    if (!Require(!ParseOBodyRules(nlohmann::json::array(), invalid) &&
            invalid.forcedOutfitNames.empty(),
            "invalid ORefit root retained stale rules")) return 1;

    const nlohmann::json malformed{
        { "outfitsForceRefit", "not-an-array" },
        { "refitOutfitPresetsFemale", { { "Broken", 42 } } },
        { "refitOutfitPresetsMale", { { "Valid", "Male-Refit" } } }
    };
    if (!Require(ParseOBodyRules(malformed, rules), "partially malformed ORefit root was rejected") ||
        !Require(rules.forcedOutfitNames.empty() && rules.femalePresetByOutfit.empty() &&
            rules.malePresetByOutfit.at("Valid") == "Male-Refit",
            "malformed ORefit fields contaminated valid fields")) return 1;

    for (int index = 1; index < argc; ++index) {
        std::ifstream stream(argv[index]);
        if (!stream) {
            std::cerr << "Could not open " << argv[index] << '\n';
            return 10;
        }
        const auto realSource = nlohmann::json::parse(stream);
        if (!ParseOBodyRules(realSource, rules)) {
            std::cerr << "OBody root is not an object: " << argv[index] << '\n';
            return 11;
        }
        const auto sourceCount = [&](const char* key) {
            const auto node = realSource.find(key);
            return node != realSource.end() && (node->is_array() || node->is_object()) ? node->size() : 0U;
        };
        const auto sourceBlacklistedNames = sourceCount("blacklistedOutfitsFromORefit");
        const auto sourceBlacklistedPlugins = sourceCount("blacklistedOutfitsFromORefitPlugin");
        const auto sourceForcedNames = sourceCount("outfitsForceRefit");
        if ((sourceBlacklistedNames > 0U && rules.blacklistedOutfitNames.empty()) ||
            (sourceBlacklistedPlugins > 0U && rules.blacklistedPlugins.empty()) ||
            (sourceForcedNames > 0U && rules.forcedOutfitNames.empty())) {
            std::cerr << "ORefit parser dropped populated supported fields from " << argv[index] << '\n';
            return 12;
        }
        ResolvedRules realResolved{
            .femalePresetByOutfit = rules.femalePresetByOutfit,
            .malePresetByOutfit = rules.malePresetByOutfit,
            .blacklistedOutfitNames = rules.blacklistedOutfitNames,
            .blacklistedPlugins = rules.blacklistedPlugins,
            .forcedOutfitNames = rules.forcedOutfitNames
        };
        for (const auto& name : rules.blacklistedOutfitNames) {
            if (!IsBlacklisted(ArmorIdentity{ name, "Allowed.esp", 1U }, realResolved)) {
                std::cerr << "ORefit name exclusion was not enforced for " << name << " from " << argv[index] << '\n';
                return 13;
            }
        }
        for (const auto& plugin : rules.blacklistedPlugins) {
            if (!IsBlacklisted(ArmorIdentity{ "Allowed Outfit", plugin, 1U }, realResolved)) {
                std::cerr << "ORefit plugin exclusion was not enforced for " << plugin << " from " << argv[index] << '\n';
                return 14;
            }
        }
        for (const auto& name : rules.forcedOutfitNames) {
            if (!IsForced(ArmorIdentity{ name, "Allowed.esp", 1U }, realResolved)) {
                std::cerr << "ORefit force entry was not enforced for " << name << " from " << argv[index] << '\n';
                return 15;
            }
        }
        std::cout << argv[index]
                  << " source-blacklisted-names=" << sourceBlacklistedNames
                  << " source-blacklisted-plugins=" << sourceBlacklistedPlugins
                  << " source-forced-names=" << sourceForcedNames
                  << " blacklisted-names=" << rules.blacklistedOutfitNames.size()
                  << " blacklisted-plugins=" << rules.blacklistedPlugins.size()
                  << " blacklisted-formids=" << rules.blacklistedFormIDs.size()
                  << " forced-names=" << rules.forcedOutfitNames.size()
                  << " forced-formids=" << rules.forcedFormIDs.size()
                  << " female-mappings=" << rules.femalePresetByOutfit.size()
                  << " male-mappings=" << rules.malePresetByOutfit.size() << '\n';
    }
}
