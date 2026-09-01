#include "BodyChangeNG/Hotkey.h"

#include <iostream>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using bcn::input::HotkeyChord;

    const HotkeyChord defaultKey{};
    if (!Require(defaultKey.IsValid(), "default F7 hotkey is invalid")) return 1;
    if (!Require(defaultKey.Matches(0x41, false, false, false), "default F7 did not match")) return 1;
    if (!Require(!defaultKey.Matches(0x41, true, false, false), "plain F7 incorrectly matched Ctrl+F7")) return 1;
    if (!Require(defaultKey.DisplayName() == "F7", "default hotkey display name changed")) return 1;

    const HotkeyChord ctrlF7{ .key = 0x41, .ctrl = true };
    if (!Require(ctrlF7.IsValid(), "Ctrl+F7 hotkey is invalid")) return 1;
    if (!Require(ctrlF7.Matches(0x41, true, false, false), "Ctrl+F7 did not match")) return 1;
    if (!Require(!ctrlF7.Matches(0x41, true, true, false), "Ctrl+F7 incorrectly matched Ctrl+Shift+F7")) return 1;
    if (!Require(ctrlF7.DisplayName() == "Ctrl+F7", "Ctrl+F7 display name changed")) return 1;

    if (!Require(!HotkeyChord{ .key = 0x1D }.IsValid(), "modifier-only hotkey became valid")) return 1;
    if (!Require(bcn::input::KeyDisplayName(0x20) == "D", "keyboard scan-code display changed")) return 1;
    return 0;
}
