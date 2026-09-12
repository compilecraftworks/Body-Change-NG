#include "BodyChangeNG/TextInputFilter.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/InputSink.h"
#include "BodyChangeNG/MenuActionInput.h"

#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/RuntimeLayout.h"

#include <RE/B/BSWin32MouseDevice.h>
#include <RE/C/ControlMap.h>
#include <SKSE/Logger.h>
#include <SKSE/Trampoline.h>

#include <array>
#include <cstring>
#include <mutex>
#include <unordered_set>

namespace
{
    constexpr auto kRightMouseButton =
        static_cast<std::uint32_t>(RE::BSWin32MouseDevice::Key::kRightButton);
    // DirectInput scan codes consumed by Body Change NG catalog navigation.
    // They must not reach gameplay or another mod's hotkey sink while this
    // menu owns them.
    constexpr std::array kMenuNavigationButtons{
        0x11U, 0x1CU, 0x1EU, 0x1FU, 0x20U, 0x9CU,
        0xC8U, 0xCBU, 0xCDU, 0xD0U
    };
    std::mutex g_filterLock;
    bcn::input::MenuActionInput g_keyboardActions;
    bcn::input::MenuActionInput g_gamepadActions;
    std::unordered_set<std::uint32_t> g_downKeyboardButtons;
    std::unordered_set<std::uint32_t> g_preSuppressionButtons;
    std::unordered_set<std::uint32_t> g_swallowedUntilReleaseButtons;
    std::unordered_set<std::uint32_t> g_releaseMustPassButtons;
    std::unordered_set<std::uint32_t> g_preMenuNavigationButtons;
    std::unordered_set<std::uint32_t> g_swallowedMenuNavigationUntilReleaseButtons;
    std::unordered_set<std::uint32_t> g_downMouseButtons;
    std::unordered_set<std::uint32_t> g_preSuppressionMouseButtons;
    std::unordered_set<std::uint32_t> g_swallowedMouseUntilReleaseButtons;
    bool g_suppressionWasActive{};
    bool g_menuNavigationSuppressionWasActive{};
    bool g_mouseSuppressionWasActive{};

    using PollInputDevices = void(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent**);
    REL::Relocation<PollInputDevices> g_originalPollInputDevices;

    void ResetLocked() noexcept
    {
        g_keyboardActions.Reset();
        g_gamepadActions.Reset();
        bcn::native_ui::SetMenuActionHeld(false);
        g_downKeyboardButtons.clear();
        g_preSuppressionButtons.clear();
        g_swallowedUntilReleaseButtons.clear();
        g_releaseMustPassButtons.clear();
        g_preMenuNavigationButtons.clear();
        g_swallowedMenuNavigationUntilReleaseButtons.clear();
        g_downMouseButtons.clear();
        g_preSuppressionMouseButtons.clear();
        g_swallowedMouseUntilReleaseButtons.clear();
        g_suppressionWasActive = false;
        g_menuNavigationSuppressionWasActive = false;
        g_mouseSuppressionWasActive = false;
    }

    [[nodiscard]] bool IsMenuNavigationButton(const std::uint32_t scanCode) noexcept
    {
        return std::ranges::find(kMenuNavigationButtons, scanCode) !=
            kMenuNavigationButtons.end();
    }

    void FilterKeyboardEvents(RE::InputEvent** events)
    {
        if (!events) return;

        std::scoped_lock lock(g_filterLock);
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
            // Skyrim and console/IME extensions own keyboard input while the
            // console is open. Never consume Enter or a stale key release.
            ResetLocked();
            return;
        }

        const bool suppressionActive = bcn::native_ui::WantsTextInput();
        const bool menuNavigationSuppressionActive = bcn::native_ui::IsOpen();
        const bool actionInputActive = menuNavigationSuppressionActive &&
            !bcn::InputSink::Get().IsCapturingHotkey();
        const auto bindingsFor = [actionInputActive](const RE::INPUT_DEVICE device) {
            bcn::input::MenuActionBindings bindings;
            if (actionInputActive) {
                if (const auto* controls = RE::ControlMap::GetSingleton()) {
                    // Gameplay Activate and menu Cancel are distinct mappings.
                    // Contexts 0/1 and the flat keyboard/gamepad device indices
                    // are stable across the supported SE/AE layout boundaries.
                    bindings.activate = controls->GetMappedKey("Activate", device,
                        RE::ControlMap::InputContextID::kGameplay);
                    bindings.cancel = controls->GetMappedKey("Cancel", device,
                        RE::ControlMap::InputContextID::kMenuMode);
                }
            }
            return bindings;
        };
        const auto keyboardBindings = bindingsFor(RE::INPUT_DEVICE::kKeyboard);
        const auto gamepadBindings = bindingsFor(RE::INPUT_DEVICE::kGamepad);
        // Match SFS's native-menu input ownership, but make the ownership
        // explicit at PollInputDevices as well.  Skyrim can otherwise still
        // deliver DirectInput mouse events to PlayerControls while the Win32
        // message has already been consumed by ImGui; RMB character rotation
        // then also blocks/attacks/casts when the menu does not pause time.
        const bool mouseSuppressionActive = bcn::native_ui::IsOpen();
        if (suppressionActive && !g_suppressionWasActive) {
            g_preSuppressionButtons = g_downKeyboardButtons;
            for (const auto scanCode : g_swallowedUntilReleaseButtons) {
                g_preSuppressionButtons.erase(scanCode);
            }
        } else if (!suppressionActive && g_suppressionWasActive) {
            for (const auto scanCode : g_preSuppressionButtons) {
                if (g_downKeyboardButtons.contains(scanCode)) {
                    g_releaseMustPassButtons.insert(scanCode);
                }
            }
            g_preSuppressionButtons.clear();
        }
        g_suppressionWasActive = suppressionActive;

        if (menuNavigationSuppressionActive && !g_menuNavigationSuppressionWasActive) {
            g_preMenuNavigationButtons = g_downKeyboardButtons;
            for (const auto scanCode : g_swallowedMenuNavigationUntilReleaseButtons) {
                g_preMenuNavigationButtons.erase(scanCode);
            }
        } else if (!menuNavigationSuppressionActive &&
            g_menuNavigationSuppressionWasActive) {
            g_preMenuNavigationButtons.clear();
        }
        g_menuNavigationSuppressionWasActive = menuNavigationSuppressionActive;

        if (mouseSuppressionActive && !g_mouseSuppressionWasActive) {
            g_preSuppressionMouseButtons = g_downMouseButtons;
            for (const auto buttonID : g_swallowedMouseUntilReleaseButtons) {
                g_preSuppressionMouseButtons.erase(buttonID);
            }
        } else if (!mouseSuppressionActive && g_mouseSuppressionWasActive) {
            g_preSuppressionMouseButtons.clear();
        }
        g_mouseSuppressionWasActive = mouseSuppressionActive;

        auto** link = events;
        while (*link) {
            auto* event = *link;
            bool blockEvent{};

            if (const auto* button = event->AsButtonEvent()) {
                const auto device = event->GetDevice();
                if (device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kGamepad) {
                    auto& tracker = device == RE::INPUT_DEVICE::kKeyboard ?
                        g_keyboardActions : g_gamepadActions;
                    const auto result = tracker.Process(button->GetIDCode(), button->IsPressed(),
                        button->IsUp(), actionInputActive, suppressionActive,
                        device == RE::INPUT_DEVICE::kKeyboard ? keyboardBindings : gamepadBindings,
                        button->IsDown());
                    blockEvent = result.block;
                    if (result.action == bcn::input::MenuAction::activate) {
                        bcn::native_ui::SubmitActivate();
                    } else if (result.action == bcn::input::MenuAction::cancel) {
                        bcn::native_ui::SubmitCancel();
                    }
                }
            }

            if (!blockEvent && event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton &&
                event->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                if (const auto* button = event->AsButtonEvent()) {
                    const auto scanCode = button->GetIDCode();
                    const bool release = button->IsUp();

                    // PollInputDevices is upstream of the menu's Scaleform
                    // key event. Preserve editing-key edges before removing
                    // them from gameplay and shortcut sinks. Character/IME
                    // input continues through GFxCharEvent separately.
                    if (suppressionActive && (button->IsDown() || release)) {
                        bcn::native_ui::SubmitTextInputKey(scanCode, !release);
                    }

                    if (suppressionActive) {
                        if (g_swallowedUntilReleaseButtons.contains(scanCode)) {
                            blockEvent = true;
                            if (release) g_swallowedUntilReleaseButtons.erase(scanCode);
                        } else if (g_preSuppressionButtons.contains(scanCode)) {
                            // A key held before typing began must keep its
                            // release visible to Skyrim and other mods.
                            blockEvent = !release;
                            if (release) g_preSuppressionButtons.erase(scanCode);
                        } else {
                            blockEvent = true;
                            if (!release && button->IsPressed()) {
                                g_swallowedUntilReleaseButtons.insert(scanCode);
                            }
                        }
                    } else if (g_swallowedUntilReleaseButtons.contains(scanCode)) {
                        // Do not leak the release of a key whose press was
                        // swallowed while the text field was focused.
                        blockEvent = true;
                        if (release) g_swallowedUntilReleaseButtons.erase(scanCode);
                    } else if (menuNavigationSuppressionActive &&
                        IsMenuNavigationButton(scanCode)) {
                        if (button->IsDown() || release) {
                            bcn::native_ui::SubmitMenuNavigationKey(scanCode, !release);
                        }
                        if (g_swallowedMenuNavigationUntilReleaseButtons.contains(scanCode)) {
                            blockEvent = true;
                            if (release) {
                                g_swallowedMenuNavigationUntilReleaseButtons.erase(scanCode);
                            }
                        } else if (g_preMenuNavigationButtons.contains(scanCode)) {
                            // A key held before opening still owns its release;
                            // otherwise Skyrim could retain a stuck control.
                            blockEvent = !release;
                            if (release) g_preMenuNavigationButtons.erase(scanCode);
                        } else {
                            blockEvent = true;
                            if (!release && button->IsPressed()) {
                                g_swallowedMenuNavigationUntilReleaseButtons.insert(scanCode);
                            }
                        }
                    } else if (g_swallowedMenuNavigationUntilReleaseButtons.contains(scanCode)) {
                        // The matching release remains ours if the menu closes
                        // between the press and release edges.
                        blockEvent = true;
                        if (release) {
                            g_swallowedMenuNavigationUntilReleaseButtons.erase(scanCode);
                        }
                    }

                    if (release) {
                        g_downKeyboardButtons.erase(scanCode);
                        g_releaseMustPassButtons.erase(scanCode);
                    } else if (button->IsPressed()) {
                        g_downKeyboardButtons.insert(scanCode);
                    }
                }
            } else if (event->GetDevice() == RE::INPUT_DEVICE::kMouse) {
                // MenuCursor position is advanced by Skyrim's mouse-move
                // events. Never remove movement, wheel, left-click or other
                // UI input here. Own only RMB while the menu is open so the
                // same press used for actor rotation cannot reach attack,
                // block or spell controls.
                if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                    if (const auto* button = event->AsButtonEvent()) {
                        const auto buttonID = button->GetIDCode();
                        if (buttonID != kRightMouseButton) {
                            link = &event->next;
                            continue;
                        }
                        const bool release = button->IsUp();
                        if (mouseSuppressionActive ||
                            g_swallowedMouseUntilReleaseButtons.contains(buttonID)) {
                            if (release) bcn::native_ui::SubmitRightMouseButton(false);
                            else if (button->IsPressed()) bcn::native_ui::SubmitRightMouseButton(true);
                        }

                        if (mouseSuppressionActive) {
                            if (g_swallowedMouseUntilReleaseButtons.contains(buttonID)) {
                                blockEvent = true;
                                if (release) g_swallowedMouseUntilReleaseButtons.erase(buttonID);
                            } else if (g_preSuppressionMouseButtons.contains(buttonID)) {
                                // A button held before opening belongs to the
                                // game. Let its release through so no control
                                // remains stuck, but suppress further holds.
                                blockEvent = !release;
                                if (release) g_preSuppressionMouseButtons.erase(buttonID);
                            } else {
                                blockEvent = true;
                                if (!release && button->IsPressed()) {
                                    g_swallowedMouseUntilReleaseButtons.insert(buttonID);
                                }
                            }
                        } else if (g_swallowedMouseUntilReleaseButtons.contains(buttonID)) {
                            // Keep ownership through the matching release even
                            // if the menu closes during a drag.
                            blockEvent = true;
                            if (release) g_swallowedMouseUntilReleaseButtons.erase(buttonID);
                        }

                        if (release) {
                            g_downMouseButtons.erase(buttonID);
                        } else if (button->IsPressed()) {
                            g_downMouseButtons.insert(buttonID);
                        }
                    }
                }
            }

            if (blockEvent) {
                *link = event->next;
            } else {
                link = &event->next;
            }
        }
        bcn::native_ui::SetMenuActionHeld(
            g_keyboardActions.HasOwnedButton() || g_gamepadActions.HasOwnedButton());
    }

    void HookedPollInputDevices(RE::BSTEventSource<RE::InputEvent*>* dispatcher,
        RE::InputEvent** events)
    {
        if (events) {
            // Text editing owns every key. Outside text editing, catalog
            // navigation owns only WASD/arrows and its confirm keys. Removed
            // events cannot trigger gameplay or another mod's hotkey sink.
            FilterKeyboardEvents(events);
        }
        g_originalPollInputDevices(dispatcher, events);
        bcn::frame_tasks::OnInputTick();
    }
}

namespace bcn::text_input
{
    bool Install()
    {
        const auto version = REL::Module::get().version();
        const auto layout = runtime::ResolveInputPollHook(version);
        if (!layout) {
            SKSE::log::warn("Body Change NG text-input event filter disabled on unsupported runtime {}",
                version.string("."));
            return false;
        }

        const auto callSite = REL::ID(layout->relocationID).address() + layout->callOffset;
        constexpr std::array<std::uint8_t, 1> directCall{ 0xE8 };
        if (callSite == 0 || std::memcmp(reinterpret_cast<const void*>(callSite),
            directCall.data(), directCall.size()) != 0) {
            SKSE::log::critical("Body Change NG input-hook signature validation failed for {}; filter not installed",
                layout->name);
            return false;
        }

        g_originalPollInputDevices = SKSE::GetTrampoline().write_call<5>(
            callSite, HookedPollInputDevices);
        SKSE::log::info("Body Change NG text-focus input filter installed for {} ({})",
            layout->name, version.string("."));
        return true;
    }

    void Reset() noexcept
    {
        std::scoped_lock lock(g_filterLock);
        ResetLocked();
    }
}
