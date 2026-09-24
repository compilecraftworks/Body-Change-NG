#include "BodyChangeNG/DistributionAuthoring.h"
#include "BodyChangeNG/DistributionAuthoringRuntime.h"
#include "BodyChangeNG/DistributionOverlayColors.h"
#include <iostream>
#include <chrono>

namespace bcn::distribution_authoring {
    // Only the immutable catalog boundary is faked; converters below are
    // extracted verbatim from the product implementation by XMake.
    std::array<std::shared_ptr<const AssetIndex>,7> testCatalogs;
    std::shared_ptr<const std::vector<TargetEntry>> testTargets;
    std::shared_ptr<const AssetIndex> Catalog(Kind kind) { return testCatalogs[static_cast<unsigned>(kind)]; }
    auto TargetSnapshot() { return testTargets; }
    bool Equal(std::string_view a, std::string_view b) { return EqualName(a,b); }
#include "DistributionAuthoringRuntime.inl"
}

using namespace bcn::distribution_authoring;
namespace {
    void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    template<class Action> void Reject(Action&& action) {
        bool rejected{};
        try { action(); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "invalid input was accepted");
    }
    bcn::DistributionRule Rule(const Json& value) {
        bcn::DistributionRule rule;
        rule.id = value.value("id", std::string{});
        rule.name = value.value("name", std::string{});
        rule.nameKey = value.value("nameKey", std::string{});
        rule.enabled = value.value("enabled", true);
        rule.female = value.at("female");
        rule.scope = static_cast<bcn::DistributionScope>(value.at("scope").get<unsigned>());
        rule.target = value.value("target", std::string{});
        rule.targetLabel = value.value("targetLabel", std::string{});
        rule.npcPlugin = value.value("npcPlugin", std::string{});
        rule.npcLocalFormID = value.value("npcLocalFormID", 0U);
        rule.targetPlugin = value.value("targetPlugin", std::string{});
        rule.targetLocalFormID = value.value("targetLocalFormID", 0U);
        rule.presetIds = value.value("presetIds", std::vector<std::string>{});
        rule.skinProfileIds = value.value("skinProfileIds", std::vector<std::string>{});
        rule.futanariSkinIds = value.value("futanariSkinIds", std::vector<std::string>{});
        rule.overlayIds = value.value("overlayIds", decltype(rule.overlayIds){});
        rule.overlayColors = bcn::ReadDistributionOverlayColors(value.value("overlayColors", Json::array()), rule.overlayIds);
        rule.includeCustomFollowers = value.value("includeCustomFollowers", false);
        rule.includeElderNPCs = value.value("includeElderNPCs", false);
        return rule;
    }
}
int main() try {
    const Json basic{{"id","fixed-rule"},{"sex","female"},{"scope","all"},{"presets",{"Preset A","프리셋 B"}}};
    for (unsigned scope{}; scope < kScopes.size(); ++scope) for (bool female : {false,true})
        for (bool enabled : {false,true}) {
            auto value = basic;
            value["scope"] = kScopes[scope]; value["sex"] = female ? "female" : "male";
            value["enabled"] = enabled;
            value["name"] = "한글 \\\" // /* name */";
            if (scope != 0 && scope != 6 && scope != 7) value["target"] = "Named Target";
            value["skins"] = {"Skin A","Skin B"};
            value["futaSkins"] = Json::array({"Futa A",Json{{"name","Futa B"},{"type","TRX"}}});
            for (auto area : {"face","body","hands","feet"})
                value["overlays"][area] = Json::array({Json{{"name","Paint A"},{"color","#FF000080"}},"Paint B"});
            const auto decoded = DecodeRule(value,8);
            const auto normalized = Encode({Rule(decoded)});
            const auto second = Encode({Rule(Decode(normalized).at("rules").at(0))});
            Require(normalized == second, "named rule round-trip changed flags/pools/target/color");
            Require(decoded.at("enabled") == enabled && decoded.at("female") == female, "flag decode failed");
        }
    for (int schema = 3; schema <= 7; ++schema) {
        Json old{{"schemaVersion",schema},{"rules",Json::array({{
            {"id","old"},{"enabled",false},{"female",true},{"scope",5},
            {"targetPlugin","Skyrim.esm"},{"targetLocalFormID",79686U},
            {"presetIds",{"old-file.xml\u001fOld preset","missing-old-id"}}
        }})}};
        auto decoded = Decode(old).at("rules").at(0);
        Require(decoded.at("enabled") == true, "legacy enabled behavior changed");
        const auto migrated = Encode({Rule(decoded)});
        auto reload = Decode(migrated).at("rules").at(0);
        Require(reload.at("presetIds") == decoded.at("presetIds") &&
            reload.at("targetLocalFormID") == 79686U && reload.at("targetPlugin") == "Skyrim.esm",
            "legacy stable identity or missing candidate lost during migration");
    }
    for (auto scope : {"npc","race","faction","keyword","class","combatStyle"}) {
        auto row = basic; row["scope"] = scope;
        row["target"] = Json{{"plugin","Example.esl"},{"formId","0xABC"},{"label","not an identity"}};
        const auto decoded = DecodeRule(row,8);
        Require(!decoded.contains("target"), "annotation became an unsafe identity fallback");
        const auto encoded = Encode({Rule(decoded)});
        Require(encoded["rules"][0]["target"]["formId"] == "0xabc", "hex ID failed");
        Require(encoded["rules"][0]["target"]["label"] == "not an identity", "annotation lost");
    }
    auto missing = basic; missing["scope"] = "race";
    missing["target"] = Json{{"name","MissingRace"},{"plugin","Missing.esp"}};
    const auto savedMissing = Encode({Rule(DecodeRule(missing,8))});
    Require(savedMissing["rules"][0]["target"] == missing["target"], "unresolved target lost");
    for (auto field : {"scope","sex","typo"}) {
        auto invalid = basic; invalid[field] = "invalid";
        Reject([&] { (void)DecodeRule(invalid,8); });
    }
    for (const auto& target : {Json{{"formId","0xFE001234"},{"plugin","A.esl"}},
            Json{{"formId",0x100000001ULL},{"plugin","A.esp"}}, Json{{"formId",0U},{"plugin","A.esp"}},
            Json{{"name","A"},{"plugni","A.esp"}}, Json(nullptr)}) {
        auto invalid=basic; invalid["scope"]="race"; invalid["target"]=target;
        Reject([&] { (void)DecodeRule(invalid,8); });
    }
    Reject([&] { auto row=basic; row["presets"]=nullptr; (void)DecodeRule(row,8); });
    Reject([&] { auto row=basic; row["target"]="NordRace"; (void)DecodeRule(row,8); });
    Reject([&] { (void)Asset(Json{{"id","@BCNG-asset:broken"}}); });
    Reject([&] { (void)Asset(Json{{"name",std::string(1024,'a')},{"file",std::string(1024,'a')},
        {"type",std::string(1024,'a')},{"texture",std::string(1024,'a')}}); });
    for (const auto color : {"red","#GG00FF","#123456789","#1234","112233"}) Reject([&] { (void)Color(color); });
    Require(Color("#FF000080") == 0x80FF0000U && Color("#123456") == 0xFF123456U, "RGBA conversion failed");
    for (unsigned alpha{}; alpha < 256; ++alpha) for (unsigned low{}; low < 256; ++low) {
        auto color = (alpha<<24) | (low<<16) | (0xFF-low);
        Require(Color(ColorText(color)) == color, "color round-trip failed");
    }
    AssetIndex assets;
    assets.Assign({{"one","Preset A","A.xml","female",{}}, {"two","Preset A","B.xml","female",{}},
        {"three","프리셋 B","C.xml","male",{}}, {"four","Paint","","","A.dds"}});
    const auto all = [](const auto& values) { return values; };
    const auto onlyOne = [](auto values) { std::erase_if(values,[](const auto& id){return id!="one";}); return values; };
    Require(ResolvePool({Asset("Preset A")},assets,all).ids.empty(), "ambiguous name picked an arbitrary candidate");
    Require(ResolvePool({Asset("Preset A")},assets,onlyOne).ids == std::vector<std::string>{"one"},
        "actor compatibility was not used to disambiguate");
    Require(ResolvePool({Asset(Json{{"name","Preset A"},{"file","B.xml"}})},assets,all).ids == std::vector<std::string>{"two"},
        "file qualifier ignored");
    Require(ResolvePool({Asset("preset a"),Asset("missing")},assets,all).ids.empty(), "asset matching silently changed spelling");
    Require(ResolvePool({"one","one","two"},assets,all).ids == std::vector<std::string>({"one","one","two"}),
        "legacy order/weighting changed");
    unsigned compatibilityCalls{};
    (void)ResolvePool({"one","two",Asset("Preset A")},assets,[&](const auto& values) { ++compatibilityCalls; return values; });
    Require(compatibilityCalls == 1, "actor compatibility rescanned once per reference");
    for (auto id : {"one","two","three","four","missing"}) {
        const auto reference = assets.Describe(id);
        Require(assets.Candidates(reference) == std::vector<std::string>{id}, "exact ID migration chose a different entry");
        Require(Asset(AssetValue(reference)) == reference, "asset reference codec not reversible");
    }
    auto overlayRef = Asset(Json{{"name","Paint"},{"texture","A.dds"}});
    Require(ResolvePool({overlayRef},assets,all).references.at("four") == overlayRef, "color source reference lost");
    assets.Assign({{"dup1","same","same.xml",{},{}},{"dup2","same","same.xml",{},{}}});
    Require(assets.Describe("dup1") == "dup1", "unresolvable migration must retain exact old ID");
    assets.Assign({{"empty","",{},{},{}},{"oversize",std::string(1025,'a'),{},{},{}},
        {"bad-utf8",std::string(1,static_cast<char>(0xFF)),{},{},{}}});
    for (auto id : {"empty","oversize","bad-utf8"})
        Require(assets.Describe(id) == id, "unrepresentable label lost legacy identity");

    using S = bcn::DistributionScope;
    std::vector<TargetEntry> targets{{S::raceEditorID,1,1,"Skyrim.esm","노드","NordRace"},
        {S::raceEditorID,2,2,"Custom.esp","노드","CustomRace"},
        {S::factionEditorID,3,3,"Skyrim.esm","NordRace","BanditFaction"}};
    Require(FindTarget(targets,S::raceEditorID,"nordrace")->id == 1, "EditorID/case lookup failed");
    Require(!FindTarget(targets,S::raceEditorID,"노드"), "ambiguous display target selected");
    Require(FindTarget(targets,S::raceEditorID,Json{{"name","노드"},{"plugin","CUSTOM.ESP"}})->id == 2,
        "plugin qualification failed");
    Require(!FindTarget(targets,S::npcClass,"NordRace") && !FindTarget(targets,S::raceEditorID,"") &&
        !FindTarget(targets,S::raceEditorID,"missing"), "wrong/empty scope target matched");
    targets.push_back({S::raceEditorID,4,4,"Else.esp","Other","NordRace"});
    Require(!FindTarget(targets,S::raceEditorID,"NordRace"), "duplicate EditorID selected arbitrarily");
    auto index = std::make_shared<AssetIndex>();
    index->Assign({{"preset-id","Preset","Presets.xml",{},{}},{"skin-id","Skin","Skin/Textures","female:3BA",{}},
        {"futa-id","Futa","Futa/Textures","TRX",{}},{"paint-id","Paint",{},{},"A.dds"}});
    testCatalogs.fill(index);
    testTargets = std::make_shared<const std::vector<TargetEntry>>(targets);
    bcn::DistributionRule legacy;
    legacy.id="stable"; legacy.scope=S::raceEditorID; legacy.targetPlugin="Skyrim.esm"; legacy.targetLocalFormID=1;
    legacy.presetIds={"preset-id","missing-id"}; legacy.skinProfileIds={"skin-id"}; legacy.futanariSkinIds={"futa-id"};
    for (auto area : bcn::overlay::kAreas) {
        legacy.overlayIds[bcn::overlay::Index(area)]={"paint-id"};
        legacy.overlayColors[bcn::overlay::Index(area)]["paint-id"]=0x80FF0000U;
    }
    const auto converted=ReadableRules({legacy}).front();
    Require(legacy.presetIds[0]=="preset-id" && converted.presetIds[1]=="missing-id", "migration mutated input/lost missing ID");
    const auto migratedDisk=Encode({converted});
    auto reloaded=Rule(Decode(migratedDisk)["rules"][0]);
    Require(ResolveNamedTarget(reloaded) && reloaded.targetFormID==1 && reloaded.targetPlugin=="Skyrim.esm",
        "actual migration did not preserve exact qualified target");
    Require(ResolvePool(reloaded.presetIds,*index,all).ids==legacy.presetIds &&
        ResolvePool(reloaded.skinProfileIds,*index,all).ids==legacy.skinProfileIds &&
        ResolvePool(reloaded.futanariSkinIds,*index,all).ids==legacy.futanariSkinIds, "actual migration changed pools/order");
    for (auto area : bcn::overlay::kAreas) {
        const auto pool=ResolvePool(reloaded.overlayIds[bcn::overlay::Index(area)],*index,all);
        Require(pool.ids==std::vector<std::string>{"paint-id"} &&
            bcn::DistributionOverlayColor(reloaded,area,pool.references.at("paint-id"))==0x80FF0000U,
            "actual migration lost named-overlay color");
    }
    Require(Encode(ReadableRules({reloaded}))==migratedDisk,"repeated name migration changed data");
    reloaded.target=std::string(kTargetPrefix)+Json("MissingRace").dump();
    Require(!ResolveNamedTarget(reloaded) && reloaded.targetFormID==0 && reloaded.targetPlugin.empty(),
        "unresolved name retained stale runtime identity");
    // Large immutable catalog exercise; no sliders or engine pointers are involved.
    std::vector<AssetEntry> entries;
    for (unsigned i{};i<10000;++i) entries.push_back({std::to_string(i),"Preset "+std::to_string(i),{},{},{}});
    assets.Assign(std::move(entries));
    const auto begin=std::chrono::steady_clock::now();
    const auto reference=Asset("Preset 9999");
    for (unsigned i{};i<100000;++i) Require(ResolvePool({reference},assets,all).ids[0]=="9999","catalog lookup failed");
    std::cout << "Authoring passed: all scopes/sexes, legacy 3..7, ambiguity, qualifiers, missing names, 65536 colors, 100000 lookups in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count() << " ms\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
