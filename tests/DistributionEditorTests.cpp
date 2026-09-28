#include "BodyChangeNG/DistributionEditorState.h"
#include <iostream>
#include <stdexcept>

using namespace bcn;
using namespace bcn::distribution_editor;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }

int main() try
{
    DistributionRule original;
    original.id = "stable";
    original.name = "User name ## do not alter";
    original.enabled = false;
    original.scope = DistributionScope::factionEditorID;
    original.targetPlugin = "Follower.esl";
    original.targetLocalFormID = 0x801;
    original.presetIds = {"bodyA", "MissingBody"};
    original.skinProfileIds = {"skinA", "MissingSkin"};
    original.futanariSkinIds = {"futaA"};
    for (auto area : overlay::kAreas) {
        const auto index = overlay::Index(area);
        original.overlayIds[index] = {"Retained", "Remove", "Missing"};
        original.overlayColors[index] = {{"Retained", 0xAABBCCDD}, {"Remove", 0x11223344}, {"Missing", 0x99887766}};
    }
    // Both sexes, every feature and area: edits never mutate the source until
    // accepted and never clear the other channels or retarget a rule.
    for (bool female : {true, false}) for (auto pool : {Pool::body, Pool::skin, Pool::futanari, Pool::overlay}) {
        original.female = female;
        auto rule = original;
        Items draft{rule, pool};
        for (auto area : overlay::kAreas) {
            draft.Set("New\\Asset", area, true);
            draft.Set("NEW/ASSET", area, true);
            Check(draft.Selected("new/asset", area), "case/separator-insensitive selection lost");
            draft.Set("Remove", area, false);
        }
        Check(rule.overlayIds == original.overlayIds && rule.presetIds == original.presetIds,
            "checkbox editing wrote through to the rule");
        auto other = rule; other.id = "wrong-rule";
        Check(!draft.Commit(other), "stale modal changed another rule");
        other = rule; other.female = !female;
        Check(!draft.Commit(other), "stale modal changed a retargeted sex");
        Check(draft.Commit(rule), "item edit failed");
        Check(rule.id == original.id && rule.name == original.name && !rule.enabled &&
            rule.scope == original.scope && rule.targetPlugin == original.targetPlugin &&
            rule.targetLocalFormID == original.targetLocalFormID, "rule identity/conditions were altered");
        if (pool != Pool::body) Check(rule.presetIds == original.presetIds, "body leaked");
        if (pool != Pool::skin) Check(rule.skinProfileIds == original.skinProfileIds, "skin leaked");
        if (pool != Pool::futanari) Check(rule.futanariSkinIds == original.futanariSkinIds, "futa leaked");
        if (pool != Pool::overlay) Check(rule.overlayColors == original.overlayColors &&
            rule.overlayIds == original.overlayIds, "overlay leaked");
        else for (auto area : overlay::kAreas) {
            Check(DistributionOverlayColor(rule, area, "retained") == 0xAABBCCDD &&
                DistributionOverlayColor(rule, area, "MISSING") == 0x99887766 &&
                DistributionOverlayColor(rule, area, "Remove") == 0xFFFFFFFF &&
                DistributionOverlayColor(rule, area, "New/Asset") == 0xFFFFFFFF,
                "overlay tint lost or unrelated/stale color retained");
        }
        draft.Clear();
        Check(Count(draft.value, pool) == 0, "clear did not clear this feature");
        Check(Count(rule, pool) != 0, "cancelled clear wrote through");
        Check(draft.Commit(rule) && Count(rule, pool) == 0, "cannot remove last item");
    }

    std::vector<DistributionRule> rules(4);
    rules[0].id = "body"; rules[0].presetIds = {"a"};
    rules[1].id = "skin"; rules[1].skinProfileIds = {"b"};
    rules[2].id = "futa"; rules[2].futanariSkinIds = {"c"};
    rules[3].id = "overlay"; rules[3].overlayIds[1] = {"d"};
    Tabs tabs;
    std::size_t index{};
    for (auto pool : {Pool::body, Pool::skin, Pool::futanari, Pool::overlay}) {
        Check(tabs.Select(rules, pool, 0) == index, "tab selected an unrelated rule");
        Check(tabs.Visible(rules, pool) == std::vector<std::size_t>{index}, "tab contains unrelated rules");
        auto empty = rules[index];
        Items draft{empty, pool}; draft.Clear(); Check(draft.Commit(empty), "failed to clear draft");
        rules[index] = empty; tabs.Remember(empty, pool);
        Check(tabs.Select(rules, pool, noRule) == index, "empty rule disappeared before editing");
        ++index;
    }
    std::swap(rules[0], rules[3]);
    Check(tabs.Select(rules, Pool::body, 0) == 3 && tabs.Select(rules, Pool::overlay, 3) == 0,
        "reordering lost empty draft tab ownership");
    rules.erase(rules.begin());
    Check(tabs.Select(rules, Pool::overlay, 0) == noRule, "empty tab edited the first unrelated rule");
    Check(tabs.Select({}, Pool::body, 0) == noRule, "empty file auto-created a rule");
    std::cout << "DistributionEditorTests passed: feature isolation, cancel, stable IDs, missing assets, colors, tabs and empty drafts\n";
}
catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
