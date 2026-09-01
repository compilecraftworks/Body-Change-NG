#pragma once

#include <RE/B/BSTEvent.h>
#include <RE/I/InputEvent.h>

#include <atomic>
#include <mutex>
#include <unordered_set>

namespace bcn
{
    class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputSink& Get();

        void Register();
        void BeginHotkeyCapture();
        void CancelHotkeyCapture();
        void ResetTransientState();
        [[nodiscard]] bool IsCapturingHotkey() const noexcept;

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) override;

    private:
        void ProcessKeyboardButton(const RE::ButtonEvent& a_event);
        void RequestToggle(std::string_view a_source);
        void StartFocusedWindowHotkeyFallback();
        [[nodiscard]] bool CtrlDown() const noexcept;
        [[nodiscard]] bool ShiftDown() const noexcept;
        [[nodiscard]] bool AltDown() const noexcept;

        mutable std::mutex lock_;
        std::unordered_set<std::uint32_t> heldKeys_;
        std::atomic_bool registered_{ false };
        std::atomic_bool capture_{ false };
        std::atomic_bool focusedWindowFallbackStarted_{ false };
        std::atomic<std::int64_t> lastToggleMilliseconds_{ 0 };
    };
}
