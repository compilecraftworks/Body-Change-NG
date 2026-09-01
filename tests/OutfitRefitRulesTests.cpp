#include "BodyChangerNG/OutfitRefitRules.h"

#include <cassert>

int main()
{
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
    assert(ParseOBodyRules(source, rules));
    assert(rules.blacklistedOutfitNames.contains("Open Dress"));
    assert(rules.blacklistedOutfitNames.contains("Nude Armor"));
    assert(rules.blacklistedPlugins.contains("Excluded.esp"));
    assert((rules.blacklistedFormIDs == std::vector<LocalFormReference>{
        { "Armors.esp", "0x001234" }, { "Armors.esp", "00ABCD" }
    }));
    assert(rules.forcedOutfitNames.contains("Force Ring"));
    assert((rules.forcedFormIDs == std::vector<LocalFormReference>{
        { "Accessories.esl", "0x812" }
    }));
    assert(rules.femalePresetByOutfit.at("Iron Armor") == "Iron Female-Refit");
    assert(rules.malePresetByOutfit.at("Iron Armor") == "Iron Male-Refit");

    ImportedRules invalid;
    invalid.forcedOutfitNames.insert("stale");
    assert(!ParseOBodyRules(nlohmann::json::array(), invalid));
    assert(invalid.forcedOutfitNames.empty());

    const nlohmann::json malformed{
        { "outfitsForceRefit", "not-an-array" },
        { "refitOutfitPresetsFemale", { { "Broken", 42 } } },
        { "refitOutfitPresetsMale", { { "Valid", "Male-Refit" } } }
    };
    assert(ParseOBodyRules(malformed, rules));
    assert(rules.forcedOutfitNames.empty());
    assert(rules.femalePresetByOutfit.empty());
    assert(rules.malePresetByOutfit.at("Valid") == "Male-Refit");
}
