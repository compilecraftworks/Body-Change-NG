#include "BodyChangeNG/TextInputFilter.h"

#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/RuntimeLayout.h"

#include <RE/B/BSWin32MouseDevice.h>
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
    std::mutex g_filterLock;
    std::unordered_set<std::uint32_t> g_downKeyboardButtons;
    std::unordered_set<std::uint32_t> g_preSuppressionButtons;
    std::unordered_set<std::uint32_t> g_swallowedUntilReleaseButtons;
    std::unordered_set<std::uint32_t> g_releaseMustPassButtons;
    std::unordered_set<std::uint32_t> g_downMouseButtons;
    std::unordered_set<std::uint32_t> g_preSuppressionMouseButtons;
    std::unordered_set<std::uint32_t> g_swallowedMouseUntilReleaseButtons;
    bool g_suppressionWasActive{};
    bool g_mouseSuppressionWasActive{};

    using PollInputDevices = void(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent**);
    REL::Relocation<PollInputDevices> g_originalPollInputDevices;

    void ResetLocked() noexcept
    {
        g_downKeyboardButtons.clear();
        g_preSuppressionButtons.clear();
        g_swallowedUntilReleaseButtons.clear();
        g_releaseMustPassButtons.clear();
        g_downMouseButtons.clear();
        g_preSuppressionMouseButtons.clear();
        g_swallowedMouseUntilReleaseButtons.clear();
        g_suppressionWasActive = false;
        g_mouseSuppressionWasActive = false;
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

            if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton &&
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
    }

    void HookedPollInputDevices(RE::BSTEventSource<RE::InputEvent*>* dispatcher,
        RE::InputEvent** events)
    {
        if (events) {
            // While an edit field owns input, the filtered list reaches no
            // shortcut sink. This blocks WASD/arrows, confirm/cancel and the
            // menu hotkey throughout Korean or English text composition.
            FilterKeyboardEvents(events);
        }
        g_originalPollInputDevices(dispatcher, events);
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
