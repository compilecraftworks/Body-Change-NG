#pragma once

#include "BodyChangeNG/Hotkey.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace bcn
{
    enum class UiLanguage : std::uint8_t
    {
        automatic,
        korean,
        english,
        chineseSimplified
    };

    enum class CharacterPosition : std::uint8_t
    {
        left,
        right,
        disabled
    };

    // Captured once, immediately before Body Change NG changes a player tint
    // layer. This is intentionally a texture path and RGBA value only: player
    // tint masks are a RaceMenu/FaceGen feature and are never distributed to
    // NPCs.
    struct PlayerTintBackup
    {
        std::uint8_t type{};
        std::string texturePath;
        std::array<std::uint8_t, 3> color{};
        float alpha{};
    };

    struct SettingsData
    {
        input::HotkeyChord openHotkey{};
        UiLanguage language{ UiLanguage::automatic };
        // Keep the character in the open left half of the screen on a new
        // installation. The user can choose Disabled or Right at any time.
        CharacterPosition characterPosition{ CharacterPosition::left };
        // One scale controls both glyphs and the surrounding controls so the
        // complete interface remains proportional at every resolution.
        float textScale{ 1.0F };
        // No saved position means the first installation: the UI opens with
        // its left edge at screen center and records that placement later.
        bool mainWindowPositionSet{};
        float mainWindowPositionX{};
        float mainWindowPositionY{};
        bool pauseGameWhenOpen{ false };
        bool performanceMode{ true };
        bool orefitEnabled{ true };
        bool orefitNippleMorphing{ true };
        bool nippleRandomization{};
        bool genitalRandomization{};
        std::vector<std::string> favoriteBodyPresets;
        std::vector<std::string> favoriteSkinProfiles;
        std::vector<std::string> favoriteTintPacks;
        std::vector<PlayerTintBackup> playerTintBackups;
    };

    struct BodyMorphOptions
    {
        bool outfitCorrection{};
        bool outfitNippleCorrection{};
        bool nippleRandomization{};
        bool genitalRandomization{};
    };

    class Settings final
    {
    public:
        static Settings& Get();

        void Load();
        [[nodiscard]] bool Save() const;
        [[nodiscard]] SettingsData Snapshot() const;
        [[nodiscard]] bool PerformanceMode() const;
        [[nodiscard]] bool OutfitCorrectionEnabled() const;
        [[nodiscard]] std::uint32_t RandomizationOptions() const;
        [[nodiscard]] BodyMorphOptions MorphOptions() const;
        void Update(const SettingsData& a_data);

    private:
        [[nodiscard]] static std::filesystem::path Path();

        mutable std::mutex lock_;
        SettingsData data_{};
    };
}
