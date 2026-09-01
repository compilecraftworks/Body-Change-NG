#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

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
        std::uint32_t npcBaseFormID{};
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
    };

    // A direct selection is deliberately stored separately from rule priority.
    // When the player locks an NPC manually, its currently selected preset must
    // survive later actor-init distribution passes without altering a broad
    // race/faction/plugin rule.
    struct ManualAssignment final
    {
        std::uint32_t actorFormID{};
        std::string presetId;
        std::string skinProfileId;
        bool useDefaultBody{};
        bool useDefaultSkin{};
    };

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
        [[nodiscard]] std::size_t ApplyLoadedNPCs() const;
        [[nodiscard]] std::size_t ResetLoadedNPCs();
        [[nodiscard]] bool ImportOBodyDefaults();

    private:
        [[nodiscard]] static std::filesystem::path Path();
        mutable std::mutex lock_;
        std::vector<DistributionRule> rules_;
        std::vector<ManualAssignment> manualAssignments_;
    };
}
