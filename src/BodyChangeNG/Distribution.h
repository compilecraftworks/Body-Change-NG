#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RE
{
    class Actor;
    class TESForm;
}

namespace bcn
{
    enum class DistributionFeature : std::uint8_t
    {
        body,
        skin
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
        return feature == DistributionFeature::body;
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
        // Shared native TXST texture-profile pool. A rule may contain body
        // presets, skin profiles, or both; each pool is sampled independently
        // but uses the same rule scope and priority.
        std::vector<std::string> skinProfileIds;
        // Legacy full-rule exclusion used while reading/importing OBody and
        // schema-1 data. Normalization expands it into both channel flags.
        bool excluded{};
        bool bodyExcluded{};
        bool skinExcluded{};
        // Source tagging lets repeated OBody imports replace only their own
        // generated rows while preserving Body Change NG user rules.
        bool importedFromOBody{};
    };

    // Changing a rule's sex invalidates every catalog choice made under the
    // previous sex. Keep this transition in one place so hidden preset/skin
    // IDs can never leak through the editor into runtime distribution.
    [[nodiscard]] inline bool SetDistributionRuleSex(DistributionRule& rule, const bool female)
    {
        if (rule.female == female) return false;
        rule.female = female;
        rule.bodyFamily.clear();
        rule.presetIds.clear();
        rule.skinProfileIds.clear();
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

    [[nodiscard]] inline std::optional<std::size_t> EarlierCatchAllRule(
        const std::vector<DistributionRule>& rules, const std::size_t index)
    {
        if (index >= rules.size()) return std::nullopt;
        for (std::size_t earlier{}; earlier < index; ++earlier) {
            if (rules[earlier].enabled &&
                rules[earlier].female == rules[index].female &&
                rules[earlier].scope == DistributionScope::allNPCs) {
                return earlier;
            }
        }
        return std::nullopt;
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
        // and accepted. OBody's distribution file is imported only through an
        // explicit editor action and is never a startup fallback.
        [[nodiscard]] bool Load();
        [[nodiscard]] bool Save() const;
        // Writes editor rules for the next game without replacing the active
        // rules used by actors in the current session.
        [[nodiscard]] bool SaveRulesForNextGame(std::vector<DistributionRule> a_rules) const;
        [[nodiscard]] std::vector<DistributionRule> Snapshot() const;
        void SetRules(std::vector<DistributionRule> a_rules);
        void SetManualAssignment(RE::Actor* a_actor, std::string a_presetId);
        void SetManualSkinAssignment(RE::Actor* a_actor, std::string a_profileId);
        void SetManualDefaultBody(RE::Actor* a_actor);
        void SetManualDefaultSkin(RE::Actor* a_actor);
        [[nodiscard]] bool RemoveManualAssignment(RE::Actor* a_actor);
        [[nodiscard]] bool RemoveManualBodyAssignment(RE::Actor* a_actor);
        void ClearManualAssignments();
        void ClearManualBodyAssignments();
        [[nodiscard]] bool HasManualAssignment(const RE::Actor* a_actor) const;
        // Returns true if a body morph or texture-profile application was
        // accepted by the SKSE task queue for this actor.
        [[nodiscard]] bool ApplyActor(RE::Actor* a_actor) const;
        [[nodiscard]] std::size_t ApplyLoadedNPCs();
        [[nodiscard]] std::size_t ResetLoadedNPCs();
        [[nodiscard]] bool ImportOBodyDefaults();

    private:
        [[nodiscard]] static std::filesystem::path Path();
        mutable std::mutex lock_;
        [[nodiscard]] std::shared_ptr<const std::vector<DistributionRule>> EvaluationRules() const;
        mutable std::shared_ptr<const std::vector<DistributionRule>> evaluationRules_;
        std::vector<DistributionRule> rules_;
    };
}
