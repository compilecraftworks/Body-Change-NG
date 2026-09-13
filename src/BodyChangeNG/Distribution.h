#pragma once

#include "BodyChangeNG/OverlayTypes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RE
{
    class Actor;
    class TESForm;
}

namespace bcn
{
    // Share the distribution rule's existing custom-follower classification
    // with automatic UI preview selection, without filtering manual targets.
    [[nodiscard]] bool IsCustomFollowerActor(RE::Actor* actor);

    enum class DistributionFeature : std::uint8_t
    {
        body,
        skin,
        futanari,
        overlayFace,
        overlayBody,
        overlayHands,
        overlayFeet
    };

    // BodyMorph is reference-scoped, while native TXST skin is ActorBase-
    // scoped. Preserve the 1.1.4 rule conditions but choose a single stable
    // skin for every live reference sharing one NPC base.
    [[nodiscard]] constexpr std::uint32_t DistributionReferenceSeed(
        const DistributionFeature feature, const std::uint32_t actorFormId) noexcept
    {
        return feature == DistributionFeature::skin ? 0U : actorFormId;
    }

    [[nodiscard]] constexpr bool MayRetainPreviousDistributionSelection(
        const DistributionFeature feature) noexcept
    {
        // 1.1.x stored skin candidates per reference. Retaining one of those
        // values could leave two references of the same ActorBase with
        // conflicting native selections forever. BodyMorph remains reference-
        // scoped and keeps its established per-save selection.
        return feature != DistributionFeature::skin;
    }

    [[nodiscard]] constexpr bool IsDistributionActorStateEligible(const bool isPlayer,
        const bool disabled, const bool dead, const bool loaded3D, const bool actorTypeNPC) noexcept
    {
        // A loaded corpse is still a valid RaceMenu morph/texture target. The
        // dead parameter is intentionally accepted to make that policy
        // explicit and regression-testable.
        static_cast<void>(dead);
        return !isPlayer && !disabled && loaded3D && actorTypeNPC;
    }

    enum class DistributionScope : std::uint8_t
    {
        allNPCs,
        npcBaseForm,
        npcName,
        factionEditorID,
        pluginFile,
        raceEditorID,
        modInstalledFollower,
        elderNPC,
        keyword,
        npcClass,
        combatStyle
    };

    struct DistributionRule final
    {
        std::string id;
        std::string name;
        // Built-in/sample names and untouched generated names are localized
        // at display time. A user edit clears this key and makes `name` an
        // ordinary custom value that is never translated or overwritten.
        std::string nameKey;
        bool enabled{ true };
        bool female{ true };
        DistributionScope scope{ DistributionScope::allNPCs };
        // Runtime-only resolved Base FormID. Persistent rules use the plugin
        // name and local BaseID below so load-order changes cannot retarget a
        // rule to another NPC.
        std::uint32_t npcBaseFormID{};
        std::string npcPlugin;
        std::uint32_t npcLocalFormID{};
        // Faction, race, keyword, class and legacy combat-style targets use the same
        // load-order-independent identity. targetFormID is resolved once when
        // rules are loaded/edited, so per-actor matching never scans forms.
        std::uint32_t targetFormID{};
        std::string targetPlugin;
        std::uint32_t targetLocalFormID{};
        // Name, faction EditorID, plugin file name, or race EditorID depending
        // on scope. For form-backed scopes this remains a display/legacy
        // EditorID fallback while the stable fields above are authoritative.
        std::string target;
        // Legacy schema field. Since 1.1.2 the editor and runtime both use the
        // female/male NPC body type selected in Mod Settings, so normalization
        // clears this obsolete per-rule filter while retaining JSON compatibility.
        std::string bodyFamily;
        std::vector<std::string> presetIds;
        // Native TXST texture-profile pool. Schema-7 normalization keeps this
        // in a skin-only rule so changing one feature cannot alter another.
        std::vector<std::string> skinProfileIds;
        // Female-only SOS/TNG futanari addon texture profiles. Runtime
        // registration is checked independently, so merely being female is
        // never enough to receive this channel.
        std::vector<std::string> futanariSkinIds;
        // RaceMenu paints are reference-scoped. Each anatomical area owns an
        // independent pool so one rule cannot accidentally route a hand
        // texture into a body node. Overlay areas may share one overlay-only
        // condition row, but never a row with Body, Skin, or Futanari.
        std::array<std::vector<std::string>, overlay::Index(overlay::Area::count)> overlayIds;
        // Optional schema-7 extension, keyed by the exact candidate ID in its area.
        // Absent values retain legacy opaque white; alpha is the high byte (AARRGGBB).
        std::array<std::map<std::string, std::uint32_t, std::less<>>,
            overlay::Index(overlay::Area::count)> overlayColors;
        // Positive all-NPC rules may narrow their target set without creating
        // a separate blacklist/exclusion rule.
        // All-NPC distribution is conservative by default. The editor exposes
        // these as checked "Exclude" options and stores the inverse here to
        // preserve the existing on-disk schema and evaluator semantics.
        bool includeCustomFollowers{ false };
        bool includeElderNPCs{ false };
    };

    [[nodiscard]] constexpr bool MatchesDistributionScopeFilters(const DistributionRule& rule,
        const bool customFollower, const bool elder) noexcept
    {
        // These checkboxes belong only to All NPCs. A hidden previous value
        // must not veto an explicitly selected NPC/name/faction/etc.
        return rule.scope != DistributionScope::allNPCs ||
            ((!customFollower || rule.includeCustomFollowers) &&
                (!elder || rule.includeElderNPCs));
    }

    [[nodiscard]] inline std::uint32_t DistributionOverlayColor(const DistributionRule& rule,
        const overlay::Area area, const std::string_view id)
    {
        const auto& colors = rule.overlayColors[overlay::Index(area)];
        const auto found = colors.find(id);
        return found == colors.end() ? 0xFFFFFFFFU : found->second;
    }

    inline void PruneDistributionOverlayColors(DistributionRule& rule)
    {
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            std::erase_if(rule.overlayColors[index], [&](const auto& item) {
                return std::ranges::find(rule.overlayIds[index], item.first) == rule.overlayIds[index].end();
            });
        }
    }

    // Every catalog shown by the rule editor is sex-specific. A sex change
    // must not leave now-hidden IDs in any persisted distribution channel.
    [[nodiscard]] inline bool SetDistributionRuleSex(DistributionRule& rule, const bool female)
    {
        if (rule.female == female) return false;
        rule.female = female;
        rule.bodyFamily.clear();
        rule.presetIds.clear();
        rule.skinProfileIds.clear();
        rule.futanariSkinIds.clear();
        for (auto& pool : rule.overlayIds) pool.clear();
        for (auto& colors : rule.overlayColors) colors.clear();
        return true;
    }

    // Editor operations intentionally do not distinguish built-in samples
    // from user-created rows. Samples are examples, not protected defaults.
    [[nodiscard]] inline bool EraseDistributionRule(
        std::vector<DistributionRule>& rules, std::size_t& selected)
    {
        if (selected >= rules.size()) return false;
        rules.erase(rules.begin() + static_cast<std::ptrdiff_t>(selected));
        if (selected >= rules.size() && !rules.empty()) --selected;
        if (rules.empty()) selected = 0U;
        return true;
    }

    [[nodiscard]] inline bool MoveDistributionRule(
        std::vector<DistributionRule>& rules, std::size_t& selected, const int direction)
    {
        if (selected >= rules.size() || (direction != -1 && direction != 1)) return false;
        if ((direction < 0 && selected == 0U) ||
            (direction > 0 && selected + 1U >= rules.size())) return false;
        const auto destination = static_cast<std::size_t>(
            static_cast<std::ptrdiff_t>(selected) + direction);
        std::swap(rules[selected], rules[destination]);
        selected = destination;
        return true;
    }

    [[nodiscard]] inline std::string GenerateUniqueUserRuleId(
        const std::vector<DistributionRule>& rules, std::uint32_t& nextId)
    {
        for (;;) {
            auto candidate = "user-rule-" + std::to_string(nextId++);
            if (std::ranges::none_of(rules, [&](const DistributionRule& rule) {
                    return rule.id == candidate;
                })) {
                return candidate;
            }
        }
    }

    // Accepts either an NPC base form or an actor reference and normalizes it
    // to a persistent plugin + local NPC BaseID rule target.
    [[nodiscard]] bool SetDistributionRuleNPC(DistributionRule& a_rule, RE::TESForm* a_form);

    // Normalizes a faction/race/keyword/class/legacy-combat-style form into the
    // persistent plugin + local FormID representation used by distribution.
    [[nodiscard]] bool SetDistributionRuleTargetForm(DistributionRule& a_rule, RE::TESForm* a_form);

    class Distribution final
    {
    public:
        static Distribution& Get();

        // Returns true only when Body Change NG's own rule file was present
        // and accepted.
        [[nodiscard]] bool Load();
        // Writes editor rules for the next game without replacing the active
        // rules used by actors in the current session.
        [[nodiscard]] bool SaveRulesForNextGame(std::vector<DistributionRule> a_rules) const;
        [[nodiscard]] std::vector<DistributionRule> SavedRulesSnapshot() const;
        void SetRules(std::vector<DistributionRule> a_rules);
        void SetManualAssignment(RE::Actor* a_actor, std::string a_presetId);
        void SetManualSkinAssignment(RE::Actor* a_actor, std::string a_profileId);
        void SetManualDefaultBody(RE::Actor* a_actor);
        void SetManualDefaultSkin(RE::Actor* a_actor);
        [[nodiscard]] bool HasManualAssignment(const RE::Actor* a_actor) const;
        // Returns true if a body morph or texture-profile application was
        // accepted by the SKSE task queue for this actor.
        [[nodiscard]] bool ApplyActor(RE::Actor* a_actor) const;
        // Provider registration can finish after the initial distribution pass.
        // Re-evaluate only this channel; never reroll body/skin/overlays here.
        void RefreshFutanariSelection(RE::Actor* a_actor) const;
        [[nodiscard]] std::size_t ApplyLoadedNPCs();
    private:
        [[nodiscard]] static std::filesystem::path Path();
        mutable std::mutex lock_;
        [[nodiscard]] std::shared_ptr<const std::vector<DistributionRule>> EvaluationRules() const;
        mutable std::shared_ptr<const std::vector<DistributionRule>> evaluationRules_;
        std::vector<DistributionRule> rules_;
        // Explicitly saved next-launch rules can differ from this session's rules.
        mutable std::optional<std::vector<DistributionRule>> savedRules_;
    };
}
