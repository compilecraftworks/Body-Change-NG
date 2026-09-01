#pragma once

#include "BodyChangerNG/Hotkey.h"

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

    // Captured once, immediately before Body Changer NG changes a player tint
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
        // Match SFS's safe, user-facing default: frame the character to the
        // right when third-person presentation is available. The user can
        // choose Disabled or Left in Mod Settings at any time.
        CharacterPosition characterPosition{ CharacterPosition::right };
        // One scale controls both glyphs and the surrounding controls so the
        // complete interface remains proportional at every resolution.
        float textScale{ 1.0F };
        // No saved position means the first installation: the UI opens in the
        // center and records that placement for later sessions.
        bool mainWindowPositionSet{};
        float mainWindowPositionX{};
        float mainWindowPositionY{};
        bool pauseGameWhenOpen{ true };
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

    class Settings final
    {
    public:
        static Settings& Get();

        void Load();
        [[nodiscard]] bool Save() const;
        [[nodiscard]] SettingsData Snapshot() const;
        void Update(const SettingsData& a_data);

    private:
        [[nodiscard]] static std::filesystem::path Path();

        mutable std::mutex lock_;
        SettingsData data_{};
    };
}
