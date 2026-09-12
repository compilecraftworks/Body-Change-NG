#pragma once

#include <cstdint>
#include <unordered_set>

namespace bcn::input
{
    enum class MenuAction : std::uint8_t { none, activate, cancel };

    struct MenuActionBindings
    {
        static constexpr std::uint32_t invalid = 0xFFU;
        std::uint32_t activate{ invalid };
        std::uint32_t cancel{ invalid };

        [[nodiscard]] constexpr MenuAction Resolve(const std::uint32_t key) const noexcept
        {
            if (key == invalid) return MenuAction::none;
            if (key == cancel) return MenuAction::cancel;
            return key == activate ? MenuAction::activate : MenuAction::none;
        }
    };

    // Tracks physical edges, not held-duration repeats. A press consumed by
    // this menu owns its release even after closing or rebinding. A button
    // already held before opening still releases back to its original owner.
    class MenuActionInput final
    {
    public:
        struct Result { MenuAction action{}; bool block{}; };

        [[nodiscard]] Result Process(const std::uint32_t key, const bool pressed,
            const bool released, const bool open, const bool typing,
            const MenuActionBindings bindings, const bool initialPress = true)
        {
            const bool wasDown = down_.contains(key);
            if (released) down_.erase(key);
            else if (pressed) down_.insert(key);

            if (owned_.contains(key)) {
                if (released) owned_.erase(key);
                return { MenuAction::none, true };
            }
            const auto action = bindings.Resolve(key);
            if (!open || action == MenuAction::none ||
                (typing && action == MenuAction::activate)) return {};
            if (released) return {};
            if (pressed && !wasDown && initialPress) {
                owned_.insert(key);
                return { action, true };
            }
            return { MenuAction::none, true };
        }

        [[nodiscard]] bool HasOwnedButton() const noexcept { return !owned_.empty(); }
        void Reset() { down_.clear(); owned_.clear(); }

    private:
        std::unordered_set<std::uint32_t> down_;
        std::unordered_set<std::uint32_t> owned_;
    };
}
