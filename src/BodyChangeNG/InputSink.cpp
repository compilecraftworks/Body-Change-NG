#include "BodyChangeNG/InputSink.h"

#include "BodyChangeNG/Hotkey.h"
#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/UI.h"

#include <SKSE/Logger.h>

namespace
{
    constexpr std::uint32_t kEscape = 0x01;
    constexpr std::array kCtrl{ 0x1DU, 0x9DU };
    constexpr std::array kShift{ 0x2AU, 0x36U };
    constexpr std::array kAlt{ 0x38U, 0xB8U };
    constexpr auto kToggleDebounceMilliseconds = std::int64_t{ 250 };

    [[nodiscard]] bool Contains(const std::unordered_set<std::uint32_t>& keys,
        const std::array<std::uint32_t, 2>& candidates)
    {
        return std::ranges::any_of(candidates, [&keys](const auto key) { return keys.contains(key); });
    }

#ifdef _WIN32
    [[nodiscard]] bool KeyDown(const int virtualKey) noexcept
    {
        return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }

    [[nodiscard]] unsigned ScanCodeToVirtualKey(const std::uint32_t directInputCode) noexcept
    {
        auto scanCode = directInputCode;
        if ((scanCode & 0x80U) != 0) scanCode = 0xE000U | (scanCode & 0x7FU);
        return MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
    }

    [[nodiscard]] bool GameWindowHasFocus() noexcept
    {
        const auto foreground = GetForegroundWindow();
        if (!foreground) return false;
        DWORD process{};
        GetWindowThreadProcessId(foreground, &process);
        return process == GetCurrentProcessId();
    }

    [[nodiscard]] bool ModifiersMatch(const bcn::input::HotkeyChord& hotkey) noexcept
    {
        const auto ctrl = KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
        const auto shift = KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
        const auto alt = KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
        return ctrl == hotkey.ctrl && shift == hotkey.shift && alt == hotkey.alt;
    }
#endif
}

namespace bcn
{
    InputSink& InputSink::Get()
    {
        static InputSink input;
        return input;
    }

    void InputSink::Register()
    {
        bool expected = false;
        if (!registered_.compare_exchange_strong(expected, true)) return;
        if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
            manager->AddEventSink<RE::InputEvent*>(this);
            // Native menus or other plugins can stop propagation. See the
            // event first when possible, then retain the focused-window
            // fallback below for handlers installed after us.
            {
                RE::BSSpinLockGuard guard(manager->lock);
                const auto found = std::find(manager->sinks.begin(), manager->sinks.end(), this);
                if (found != manager->sinks.end() && found != manager->sinks.begin()) {
                    std::rotate(manager->sinks.begin(), found, std::next(found));
                }
            }
            StartFocusedWindowHotkeyFallback();
            SKSE::log::info("Body Change NG priority keyboard input sink registered");
        } else {
            registered_ = false;
            SKSE::log::warn("Body Change NG will retry input registration after input is initialized");
        }
    }

    void InputSink::BeginHotkeyCapture()
    {
        capture_ = true;
    }

    void InputSink::CancelHotkeyCapture()
    {
        capture_ = false;
    }

    void InputSink::ResetTransientState()
    {
        capture_ = false;
        std::scoped_lock lock(lock_);
        heldKeys_.clear();
    }

    bool InputSink::IsCapturingHotkey() const noexcept
    {
        return capture_.load();
    }

    RE::BSEventNotifyControl InputSink::ProcessEvent(RE::InputEvent* const* const a_events,
        RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!a_events) return RE::BSEventNotifyControl::kContinue;
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto* event = *a_events; event; event = event->next) {
            if (event->GetDevice() != RE::INPUT_DEVICE::kKeyboard ||
                event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }
            if (const auto* button = event->AsButtonEvent()) ProcessKeyboardButton(*button);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    void InputSink::ProcessKeyboardButton(const RE::ButtonEvent& event)
    {
        const auto scanCode = event.GetIDCode();
        const bool release = event.IsUp();
        const bool pressed = event.IsPressed();
        bool firstPress = false;
        {
            std::scoped_lock lock(lock_);
            if (release) {
                heldKeys_.erase(scanCode);
                return;
            }
            if (!pressed) return;
            firstPress = heldKeys_.insert(scanCode).second;
        }
        if (!firstPress || input::IsModifierKey(scanCode)) return;

        if (capture_.load()) {
            if (scanCode == kEscape) {
                capture_ = false;
                ui::Notify(ui::Localize("단축키 변경을 취소했습니다.", "Shortcut capture was cancelled.", "已取消快捷键设置。"));
                return;
            }
            auto settings = Settings::Get().Snapshot();
            settings.openHotkey = {
                .key = scanCode,
                .ctrl = CtrlDown(),
                .shift = ShiftDown(),
                .alt = AltDown()
            };
            Settings::Get().Update(settings);
            if (!Settings::Get().Save()) {
                ui::Notify(ui::Localize("창 열기 단축키를 저장하지 못했습니다.", "Could not save the opening shortcut.", "无法保存窗口打开快捷键。"));
            }
            capture_ = false;
            ui::Notify(std::string(ui::Localize("창 열기 단축키: ", "Open-window shortcut: ", "窗口打开快捷键：")) +
                settings.openHotkey.DisplayName());
            return;
        }

        if (scanCode == kEscape && native_ui::IsOpen()) {
            native_ui::SubmitEscape();
            return;
        }

        const auto hotkey = Settings::Get().Snapshot().openHotkey;
        if (hotkey.Matches(scanCode, CtrlDown(), ShiftDown(), AltDown())) {
            RequestToggle("SKSE input event");
        }
    }

    void InputSink::RequestToggle(const std::string_view source)
    {
        if (capture_.load()) return;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        auto previous = lastToggleMilliseconds_.load();
        do {
            if (now - previous < kToggleDebounceMilliseconds) return;
        } while (!lastToggleMilliseconds_.compare_exchange_weak(previous, now));

        if (native_ui::Toggle()) {
            SKSE::log::info("Body Change NG opening shortcut accepted via {}", source);
        } else {
            SKSE::log::warn("Body Change NG opening shortcut via {} arrived before the native UI was ready", source);
        }
    }

    void InputSink::StartFocusedWindowHotkeyFallback()
    {
#ifdef _WIN32
        if (focusedWindowFallbackStarted_.exchange(true)) return;
        try {
            std::thread([this] {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
                bool wasDown{};
                std::uint32_t previousScanCode{};
                for (;;) {
                    const auto hotkey = Settings::Get().Snapshot().openHotkey;
                    const auto virtualKey = ScanCodeToVirtualKey(hotkey.key);
                    if (hotkey.key != previousScanCode) {
                        previousScanCode = hotkey.key;
                        wasDown = virtualKey != 0U && KeyDown(static_cast<int>(virtualKey));
                    }
                    const auto down = virtualKey != 0U && KeyDown(static_cast<int>(virtualKey));
                    if (!capture_.load() && GameWindowHasFocus() && down && !wasDown && ModifiersMatch(hotkey)) {
                        RequestToggle("focused-window key-state fallback");
                    }
                    wasDown = down;
                    std::this_thread::sleep_for(std::chrono::milliseconds(12));
                }
            }).detach();
            SKSE::log::info("Body Change NG focused-window shortcut fallback started");
        } catch (const std::exception& exception) {
            focusedWindowFallbackStarted_ = false;
            SKSE::log::error("Body Change NG could not start its focused-window shortcut fallback: {}", exception.what());
        }
#endif
    }

    bool InputSink::CtrlDown() const noexcept
    {
        std::scoped_lock lock(lock_);
        return Contains(heldKeys_, kCtrl);
    }

    bool InputSink::ShiftDown() const noexcept
    {
        std::scoped_lock lock(lock_);
        return Contains(heldKeys_, kShift);
    }

    bool InputSink::AltDown() const noexcept
    {
        std::scoped_lock lock(lock_);
        return Contains(heldKeys_, kAlt);
    }
}
