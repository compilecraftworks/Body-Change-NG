#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bcn
{
    struct BodySlider final
    {
        std::string name;
        float lowWeight{};
        float highWeight{};
    };

    struct BodyPreset final
    {
        std::string name;
        std::string source;
        std::string family;
        std::string bodySet;
        std::vector<BodySlider> sliders;
        bool isRefit{};
        bool male{};
        std::uint64_t cachedContentHash{};

        [[nodiscard]] std::string PersistentId() const;
        [[nodiscard]] std::uint64_t ContentHash() const;
    };

    class PresetCatalog final
    {
    public:
        static PresetCatalog& Get();

        void Refresh();
        [[nodiscard]] std::vector<BodyPreset> Snapshot() const;
        [[nodiscard]] std::vector<BodyPreset> RefitSnapshot() const;
        [[nodiscard]] std::optional<BodyPreset> Find(std::string_view id, bool refit = false) const;
        [[nodiscard]] std::optional<BodyPreset> FindRefit(const std::vector<std::string>& names, bool male) const;
        [[nodiscard]] std::uint64_t ContentHash(std::string_view id) const;
        // Avoid copying every preset's slider vector while evaluating each
        // NPC; only return compatible IDs from the requested rule pool.
        [[nodiscard]] std::vector<std::string> CompatibleIds(
            const std::vector<std::string>& a_ids, bool a_male,
            body_family::Mask a_actorFamily) const;
        // Returns a refresh-cached union of every slider used by presets that
        // are compatible with this actor.  Apply code uses the full union so
        // a slider omitted by the selected XML can intentionally resolve to
        // zero without rescanning every preset for every NPC.
        [[nodiscard]] std::shared_ptr<const std::vector<std::string>> CompatibleSliderUniverse(
            bool a_male, body_family::Mask a_actorFamily) const;
        [[nodiscard]] static std::vector<std::string> CollectCompatibleSliderNames(
            const std::vector<BodyPreset>& a_presets, bool a_male,
            body_family::Mask a_actorFamily);
        [[nodiscard]] static std::filesystem::path BodySlidePresetDirectory();
        [[nodiscard]] static std::vector<BodyPreset> ScanDirectory(const std::filesystem::path& a_directory);

    private:
        mutable std::mutex lock_;
        std::vector<BodyPreset> presets_;
        std::vector<BodyPreset> refitPresets_;
        std::unordered_map<std::string, std::uint64_t> contentHashes_;
        mutable std::unordered_map<std::uint64_t,
            std::shared_ptr<const std::vector<std::string>>> sliderUniverseCache_;
    };
}
