#pragma once

#include "BodyChangeNG/FaceSkinPolicy.h"

namespace bcn::face_skin
{
    // Only the current UI preview is observed. This is not a persistent
    // override and must never become part of a save's restoration baseline.
    struct PreviewWatch
    {
        Paths expected;
        unsigned repairs{};
        static constexpr unsigned kMaxRepairs = 8;
        enum class Action { unchanged, repair, stop };

        [[nodiscard]] bool Empty() const noexcept
        {
            for (const auto& path : expected) if (!path.empty()) return false;
            return true;
        }

        [[nodiscard]] Action Observe(const Paths& properties, const Paths& textures,
            const std::array<bool, kChannels.size()>& ready) noexcept
        {
            bool changed{};
            for (std::size_t i{}; i < expected.size(); ++i) {
                if (!expected[i].empty() && !CanKeepVisibleTexture(
                        expected[i], properties[i], textures[i], ready[i], false, {})) changed = true;
            }
            if (!changed) return Action::unchanged;
            // A late RaceMenu replay can arrive channel by channel. Allow it
            // to drain, but never fight an endlessly writing external mod.
            if (repairs == kMaxRepairs) return Action::stop;
            ++repairs;
            return Action::repair;
        }
    };
}
