#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace bcn
{
    struct OBodyOutfitImportReport final
    {
        bool loaded{};
        std::size_t excludedNames{};
        std::size_t excludedPlugins{};
        std::size_t excludedFormIDs{};
        std::size_t forcedNames{};
        std::size_t forcedFormIDs{};
        std::size_t femaleMappings{};
        std::size_t maleMappings{};
    };

    class OutfitRefit final
    {
    public:
        static OutfitRefit& Get();

        // Registers OBody's complete outfit-correction rule set from
        // OBody_presetDistributionConfig.json without changing that source file:
        // exclusions, force-refit entries and sex-specific outfit mappings.
        // NPC body-distribution entries remain outside this importer.
        [[nodiscard]] OBodyOutfitImportReport LoadOBodyRules();
        void ClearLegacyRules();
        void ProcessActor(RE::Actor* a_actor) const;
        // Re-evaluate every currently loaded actor immediately after the user
        // registers a new OBody/ORefit list. This makes new exclusions clear
        // an already-applied clothing layer without waiting for another equip
        // or cell-attach event.
        [[nodiscard]] std::size_t ProcessLoadedActors() const;

    public:
        struct Rules final
        {
            std::unordered_map<std::string, std::string> femalePresetByOutfit;
            std::unordered_map<std::string, std::string> malePresetByOutfit;
            std::unordered_set<std::string> blacklistedOutfitNames;
            std::unordered_set<std::string> blacklistedPlugins;
            std::unordered_set<std::uint32_t> blacklistedFormIDs;
            std::unordered_set<std::string> forcedOutfitNames;
            std::unordered_set<std::uint32_t> forcedFormIDs;
        };

    private:
        [[nodiscard]] std::shared_ptr<const Rules> Snapshot() const;

        mutable std::mutex lock_;
        Rules rules_;
        mutable std::shared_ptr<const Rules> evaluationRules_;
    };
}
