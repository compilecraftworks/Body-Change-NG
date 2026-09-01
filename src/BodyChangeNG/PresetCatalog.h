#pragma once

#include <filesystem>
#include <mutex>
#include <string>
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

        [[nodiscard]] std::string PersistentId() const;
    };

    class PresetCatalog final
    {
    public:
        static PresetCatalog& Get();

        void Refresh();
        [[nodiscard]] std::vector<BodyPreset> Snapshot() const;
        [[nodiscard]] std::vector<BodyPreset> RefitSnapshot() const;
        [[nodiscard]] static std::filesystem::path BodySlidePresetDirectory();
        [[nodiscard]] static std::vector<BodyPreset> ScanDirectory(const std::filesystem::path& a_directory);

    private:
        mutable std::mutex lock_;
        std::vector<BodyPreset> presets_;
        std::vector<BodyPreset> refitPresets_;
    };
}
