#include "BodyChangeNG/Hotkey.h"

#include <array>
#include <string_view>
#include <utility>

namespace
{
    struct KeyName
    {
        std::uint32_t code;
        std::string_view name;
    };

    constexpr std::array kKeyNames{
        KeyName{ 0x01, "Esc" }, KeyName{ 0x0E, "Backspace" }, KeyName{ 0x0F, "Tab" },
        KeyName{ 0x1C, "Enter" }, KeyName{ 0x39, "Space" }, KeyName{ 0x3B, "F1" },
        KeyName{ 0x3C, "F2" }, KeyName{ 0x3D, "F3" }, KeyName{ 0x3E, "F4" },
        KeyName{ 0x3F, "F5" }, KeyName{ 0x40, "F6" }, KeyName{ 0x41, "F7" },
        KeyName{ 0x42, "F8" }, KeyName{ 0x43, "F9" }, KeyName{ 0x44, "F10" },
        KeyName{ 0x57, "F11" }, KeyName{ 0x58, "F12" }, KeyName{ 0xC7, "Home" },
        KeyName{ 0xC8, "Up" }, KeyName{ 0xC9, "Page Up" }, KeyName{ 0xCB, "Left" },
        KeyName{ 0xCD, "Right" }, KeyName{ 0xCF, "End" }, KeyName{ 0xD0, "Down" },
        KeyName{ 0xD1, "Page Down" }, KeyName{ 0xD2, "Insert" }, KeyName{ 0xD3, "Delete" }
    };
}

namespace bcn::input
{
    bool HotkeyChord::IsValid() const noexcept
    {
        return key != 0 && !IsModifierKey(key);
    }

    bool HotkeyChord::Matches(const std::uint32_t a_key, const bool a_ctrl,
        const bool a_shift, const bool a_alt) const noexcept
    {
        return key == a_key && ctrl == a_ctrl && shift == a_shift && alt == a_alt;
    }

    std::string HotkeyChord::DisplayName() const
    {
        std::string result;
        if (ctrl) result += "Ctrl+";
        if (shift) result += "Shift+";
        if (alt) result += "Alt+";
        result += KeyDisplayName(key);
        return result;
    }

    bool IsModifierKey(const std::uint32_t a_scanCode) noexcept
    {
        switch (a_scanCode) {
        case 0x1D:  // Left Ctrl
        case 0x9D:  // Right Ctrl
        case 0x2A:  // Left Shift
        case 0x36:  // Right Shift
        case 0x38:  // Left Alt
        case 0xB8:  // Right Alt
            return true;
        default:
            return false;
        }
    }

    std::string KeyDisplayName(const std::uint32_t a_scanCode)
    {
        for (const auto& entry : kKeyNames) {
            if (entry.code == a_scanCode) return std::string(entry.name);
        }
        if (a_scanCode >= 0x02 && a_scanCode <= 0x0A) {
            return std::string(1, static_cast<char>('1' + (a_scanCode - 0x02)));
        }
        if (a_scanCode == 0x0B) return "0";
        // DIK letter keys are consecutive only in the physical keyboard order.
        constexpr std::array letterKeys{
            std::pair{ 0x10U, 'Q' }, std::pair{ 0x11U, 'W' }, std::pair{ 0x12U, 'E' },
            std::pair{ 0x13U, 'R' }, std::pair{ 0x14U, 'T' }, std::pair{ 0x15U, 'Y' },
            std::pair{ 0x16U, 'U' }, std::pair{ 0x17U, 'I' }, std::pair{ 0x18U, 'O' },
            std::pair{ 0x19U, 'P' }, std::pair{ 0x1EU, 'A' }, std::pair{ 0x1FU, 'S' },
            std::pair{ 0x20U, 'D' }, std::pair{ 0x21U, 'F' }, std::pair{ 0x22U, 'G' },
            std::pair{ 0x23U, 'H' }, std::pair{ 0x24U, 'J' }, std::pair{ 0x25U, 'K' },
            std::pair{ 0x26U, 'L' }, std::pair{ 0x2CU, 'Z' }, std::pair{ 0x2DU, 'X' },
            std::pair{ 0x2EU, 'C' }, std::pair{ 0x2FU, 'V' }, std::pair{ 0x30U, 'B' },
            std::pair{ 0x31U, 'N' }, std::pair{ 0x32U, 'M' }
        };
        for (const auto& [code, letter] : letterKeys) {
            if (code == a_scanCode) return std::string(1, letter);
        }
        return "Scan code " + std::to_string(a_scanCode);
    }
}
