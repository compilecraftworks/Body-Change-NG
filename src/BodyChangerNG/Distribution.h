#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace RE
{
    class Actor;
    class TESForm;
}

namespace bcn
{
    enum class DistributionScope : std::uint8_t
    {
        allNPCs,
        npcBaseForm,
        npcName,
        factionEditorID,
        pluginFile,
        raceEditorID,
        modInstalledFollower,
        elderNPC
    };

    struct DistributionRule final
    {
        std::string id;
        std::string name;
        bool enabled{ true };
        bool female{ true };
        DistributionScope scope{ DistributionScope::allNPCs };
        // Runtime-only resolved Base FormID. Persistent rules use the plugin
        // name and local BaseID below so load-order changes cannot retarget a
        // rule to another NPC.
        std::uint32_t npcBaseFormID{};
        std::string npcPlugin;
        std::uint32_t npcLocalFormID{};
        // Name, faction EditorID, plugin file name, or race EditorID depending
        // on scope.  Form rules use the resolved runtime FormID above.
        std::string target;
        // This is an editor-side preset-pool filter. The actual preset IDs remain
        // authoritative, because an NPC's installed mesh family cannot be proven
        // safely from a generic actor handle.
        std::string bodyFamily;
        std::vector<std::string> presetIds;
        // Shared RaceMenu texture-profile pool.  A rule may contain body
        // presets, skin profiles, or both; each pool is sampled independently
        // but uses the same rule scope and priority.
        std::vector<std::string> skinProfileIds;
        // Legacy full-rule exclusion used while reading/importing OBody and
        // schema-1 data. Normalization expands it into both channel flags.
        bool excluded{};
        bool bodyExcluded{};
        bool skinExcluded{};
        // Source tagging lets repeated OBody imports replace only their own
        // generated rows while preserving Body Changer NG user rules.
        bool importedFromOBody{};
    };

    // Accepts either an NPC base form or an actor reference and normalizes it
    // to a persistent plugin + local NPC BaseID rule target.
    [[nodiscard]] bool SetDistributionRuleNPC(DistributionRule& a_rule, RE::TESForm* a_form);

    class Distribution final
    {
    public:
        static Distribution& Get();

        // Returns true only when Body Changer NG's own rule file was present
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
        std::vector<DistributionRule> rules_;
    };
}
