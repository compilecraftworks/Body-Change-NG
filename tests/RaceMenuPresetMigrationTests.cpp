#include "BodyChangeNG/RaceMenuPresetMigrationRules.h"

#include <nlohmann/json.hpp>

#include <iostream>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    const bcn::racemenu_preset_migration::HeadPartTarget kHighPolyTarget{
        .plugin = "High Poly Head.esm",
        .formIdentifier = "High Poly Head.esm|000A06",
        .runtimeFormID = 0xFE001A06U
    };
}

int main()
{
    using namespace bcn::racemenu_preset_migration;

    auto preset = nlohmann::json::parse(R"({
        "headParts": [
            {"type": 7, "formIdentifier": "BodyChange.esp|0BB89F", "formId": 4261976095},
            {"type": 1, "formIdentifier": "Skyrim.esm|05150F", "formId": 333071}
        ],
        "modNames": ["BodyChange.esp", "Skyrim.esm"]
    })");
    auto result = TransformLegacyBodyChangeHeadParts(preset, kHighPolyTarget);
    if (!Require(result.replaced == 1U && result.removedDuplicates == 0U && !result.skippedUBE,
            "legacy head part was not replaced")) return 1;
    if (!Require(preset["headParts"][0]["type"] == 7,
            "RaceMenu head-part slot index was changed")) return 1;
    if (!Require(preset["headParts"][0]["formIdentifier"] == "High Poly Head.esm|000A06" &&
                 preset["headParts"][0]["formId"] == 0xFE001A06U,
            "replacement identifiers were not written")) return 1;
    if (!Require(preset["modNames"] == nlohmann::json::array({ "Skyrim.esm", "High Poly Head.esm" }),
            "modNames were not migrated")) return 1;

    result = TransformLegacyBodyChangeHeadParts(preset, kHighPolyTarget);
    if (!Require(!result.Changed(), "migration was not idempotent")) return 1;

    auto duplicate = nlohmann::json::parse(R"({
        "headParts": [
            {"type": 4, "formIdentifier": "bodychange.ESP|00086E", "formId": 1},
            {"type": 9, "formIdentifier": "HIGH POLY HEAD.ESM|000A06", "formId": 2}
        ],
        "modNames": ["BODYCHANGE.ESP", "HIGH POLY HEAD.ESM"]
    })");
    result = TransformLegacyBodyChangeHeadParts(duplicate, kHighPolyTarget);
    if (!Require(result.replaced == 0U && result.removedDuplicates == 1U &&
                 duplicate["headParts"].size() == 1U,
            "existing target was not de-duplicated")) return 1;

    auto ube = nlohmann::json::parse(R"({
        "headParts": [{"type": 0, "formIdentifier": "BodyChange.esp|0008A0", "formId": 1}],
        "modNames": ["BodyChange.esp", "UBE_AllRace.esp"]
    })");
    result = TransformLegacyBodyChangeHeadParts(ube, kHighPolyTarget);
    if (!Require(result.skippedUBE && !result.Changed() &&
                 ube["headParts"][0]["formIdentifier"] == "BodyChange.esp|0008A0",
            "UBE custom-race preset was not preserved")) return 1;

    auto ubeHeadPartAddon = nlohmann::json::parse(R"({
        "headParts": [
            {"type": 0, "formIdentifier": "BodyChange.esp|0008A0", "formId": 1},
            {"type": 5, "formIdentifier": "Kyoe BanginBrows UBE.esp|001234", "formId": 2}
        ],
        "modNames": ["BodyChange.esp", "Kyoe BanginBrows UBE.esp"]
    })");
    result = TransformLegacyBodyChangeHeadParts(ubeHeadPartAddon, kHighPolyTarget);
    if (!Require(result.skippedUBE && !result.Changed(),
            "UBE head-part dependency was not recognized")) return 1;

    auto cbbeWithUbeAnusSlider = nlohmann::json::parse(R"({
        "headParts": [{"type": 0, "formIdentifier": "BodyChange.esp|0008A0", "formId": 1}],
        "bodyMorphs": [{"name": "3BBB Body Amazing UBE Anus"}],
        "modNames": ["BodyChange.esp"]
    })");
    result = TransformLegacyBodyChangeHeadParts(cbbeWithUbeAnusSlider, kHighPolyTarget);
    if (!Require(result.replaced == 1U && !result.skippedUBE,
            "a CBBE 3BA UBE Anus slider was mistaken for a UBE race dependency")) return 1;

    auto unrelated = nlohmann::json::parse(R"({
        "headParts": [{"type": 0, "formIdentifier": "Skyrim.esm|051623", "formId": 333347}],
        "modNames": ["Skyrim.esm"]
    })");
    result = TransformLegacyBodyChangeHeadParts(unrelated, kHighPolyTarget);
    if (!Require(!result.Changed() && !result.skippedUBE,
            "unrelated preset was changed")) return 1;

    auto retainedReference = nlohmann::json::parse(R"({
        "headParts": [{"type": 0, "formIdentifier": "BodyChange.esp|0008A1", "formId": 1}],
        "mods": [{"formIdentifier": "BodyChange.esp|000123"}],
        "modNames": ["BodyChange.esp"]
    })");
    result = TransformLegacyBodyChangeHeadParts(retainedReference, kHighPolyTarget);
    if (!Require(result.Changed() && retainedReference["modNames"][0] == "BodyChange.esp",
            "modNames dropped a plugin that is still referenced")) return 1;

    return 0;
}
