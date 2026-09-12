#include "BodyChangeNG/Hotkey.h"
#include "BodyChangeNG/MenuActionInput.h"

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

    using bcn::input::MenuAction;
    using bcn::input::MenuActionBindings;
    bcn::input::MenuActionInput keyboard;
    const MenuActionBindings remapped{ .activate = 0x21, .cancel = 0x0F }; // F / Tab
    if (!Require(keyboard.Process(0x12, true, false, true, false, remapped).action ==
        MenuAction::none, "old E key still activates after rebinding")) return 1;
    if (!Require(keyboard.Process(0x21, true, false, true, false, remapped).action ==
        MenuAction::activate, "remapped keyboard Activate did not confirm")) return 1;
    if (!Require(keyboard.Process(0x21, true, false, true, false, remapped).action ==
        MenuAction::none, "holding Activate repeatedly confirmed")) return 1;
    if (!Require(keyboard.Process(0x21, false, true, false, false, {}).block &&
        !keyboard.HasOwnedButton(), "closed menu leaked its owned release")) return 1;
    if (!Require(keyboard.Process(0x0F, true, false, true, true, remapped).action ==
        MenuAction::cancel, "mapped Cancel failed during text entry")) return 1;
    keyboard.Reset();
    if (!Require(!keyboard.HasOwnedButton(), "reset retained action ownership")) return 1;
    if (!Require(keyboard.Process(0x21, true, false, true, false, remapped, false).action ==
        MenuAction::none, "focus-return hold was mistaken for a fresh press")) return 1;
    keyboard.Reset();
    if (!Require(keyboard.Process(0x21, true, false, true, true, remapped).action ==
        MenuAction::none, "typing the Activate key confirmed an item")) return 1;
    keyboard.Reset();
    (void)keyboard.Process(0x21, true, false, false, false, remapped);
    if (!Require(keyboard.Process(0x21, true, false, true, false, remapped).action ==
        MenuAction::none && !keyboard.Process(0x21, false, true, true, false, remapped).block,
        "button held before opening lost its original release owner")) return 1;

    bcn::input::MenuActionInput pad;
    const MenuActionBindings padBindings{ .activate = 0x4000, .cancel = 0x8000 }; // X / Y
    if (!Require(pad.Process(0x1000, true, false, true, false, padBindings).action ==
        MenuAction::none, "old gamepad A still activates after rebinding")) return 1;
    if (!Require(pad.Process(0x4000, true, false, true, false, padBindings).action ==
        MenuAction::activate, "remapped gamepad Activate did not confirm")) return 1;
    if (!Require(pad.Process(0x4000, false, true, true, false,
        { .activate = 0x9, .cancel = 0x8000 }).block, "rebind leaked old button release")) return 1;
    if (!Require(pad.Process(0x8000, true, false, true, false, padBindings).action ==
        MenuAction::cancel, "remapped gamepad Cancel did not close")) return 1;
    pad.Reset();
    for (int index = 0; index < 10000; ++index) {
        if (!Require(pad.Process(0x9, true, false, true, false,
            { .activate = 0x9 }).action == MenuAction::activate,
            "game-defined trigger ID did not activate")) return 1;
        (void)pad.Process(0x9, false, true, true, false, { .activate = 0x9 });
    }
    if (!Require(!pad.HasOwnedButton(), "repeated presses retained owned buttons")) return 1;
    if (!Require(MenuActionBindings{}.Resolve(0xFF) == MenuAction::none,
        "unbound action resolved as valid")) return 1;
    if (!Require(MenuActionBindings{ .activate = 1, .cancel = 1 }.Resolve(1) ==
        MenuAction::cancel, "Cancel did not win an ambiguous binding")) return 1;
    return 0;
}
