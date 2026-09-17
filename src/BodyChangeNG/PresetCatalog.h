#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <filesystem>
#include <mutex>
#include <memory>
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
        // UBE distributes a valid Zeroed preset with no SetSlider entries.
        // It selects the built baseline, not an invalid/unknown preset.
        [[nodiscard]] bool UsesBuildDefaults() const;
    };

    // Immutable UI projection: deliberately cannot contain slider data.
    struct PresetListEntry final
    {
        std::string id, name, source, family;
        bool male{};
    };
    using PresetList = std::vector<PresetListEntry>;
    [[nodiscard]] inline PresetList BuildPresetList(const std::vector<BodyPreset>& presets)
    {
        PresetList result;
        result.reserve(presets.size());
        for (const auto& p : presets)
            result.push_back({p.PersistentId(), p.name, p.source, p.family, p.male});
        return result;
    }

    class PresetCatalog final
    {
    public:
        static PresetCatalog& Get();

        void Refresh();
        [[nodiscard]] std::vector<BodyPreset> Snapshot() const;
        [[nodiscard]] std::shared_ptr<const PresetList> ListSnapshot() const;
        [[nodiscard]] std::vector<BodyPreset> RefitSnapshot() const;
        [[nodiscard]] std::optional<BodyPreset> Find(std::string_view id, bool refit = false) const;
        [[nodiscard]] std::optional<BodyPreset> FindRefit(const std::vector<std::string>& names, bool male,
            body_family::Mask actorFamily) const;
        [[nodiscard]] static std::optional<BodyPreset> SelectRefit(
            const std::vector<BodyPreset>& presets, const std::vector<std::string>& names,
            bool male, body_family::Mask actorFamily);
        [[nodiscard]] std::uint64_t ContentHash(std::string_view id) const;
        // Avoid copying every preset's slider vector while evaluating each
        // NPC; only return compatible IDs from the requested rule pool.
        [[nodiscard]] std::vector<std::string> CompatibleIds(
            const std::vector<std::string>& a_ids, bool a_male,
            body_family::Mask a_actorFamily) const;
        [[nodiscard]] static std::filesystem::path BodySlidePresetDirectory();
        [[nodiscard]] static std::vector<BodyPreset> ScanDirectory(const std::filesystem::path& a_directory);

    private:
        mutable std::mutex lock_;
        std::vector<BodyPreset> presets_;
        std::shared_ptr<const PresetList> list_ = std::make_shared<const PresetList>();
        std::vector<BodyPreset> refitPresets_;
        std::unordered_map<std::string, std::uint64_t> contentHashes_;
    };
}
