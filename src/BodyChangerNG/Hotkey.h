#pragma once

#include <cstdint>
#include <string>

namespace bcn::input
{
    struct HotkeyChord
    {
        std::uint32_t key{ 0x41 };  // DirectInput F7
        bool ctrl{};
        bool shift{};
        bool alt{};

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] bool Matches(std::uint32_t a_key, bool a_ctrl, bool a_shift,
            bool a_alt) const noexcept;
        [[nodiscard]] std::string DisplayName() const;

        friend bool operator==(const HotkeyChord&, const HotkeyChord&) = default;
    };

    [[nodiscard]] bool IsModifierKey(std::uint32_t a_scanCode) noexcept;
    [[nodiscard]] std::string KeyDisplayName(std::uint32_t a_scanCode);
}
