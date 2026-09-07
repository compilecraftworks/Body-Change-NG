#pragma once

#include "BodyChangeNG/SkinApplyResult.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace RE
{
    class Actor;
}

namespace bcn::native_skin
{
    // General BodySkin is implemented through cloned native TXST -> ARMA ->
    // Skin Armor forms. The actor's equipment is never inspected or tracked.
    // beforeQueue is invoked synchronously only after validation and ownership
    // admission, immediately before native graph work is submitted. It lets
    // the 1.1.x compatibility facade enqueue its owned-key cleanup first.
    [[nodiscard]] SkinApplyResult QueueApply(RE::Actor* actor, std::string profileId,
        std::function<void()> beforeQueue = {});
    [[nodiscard]] SkinApplyResult QueueClear(RE::Actor* actor,
        std::function<void()> beforeQueue = {});
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* actor);
    [[nodiscard]] bool HasTrackedSelection(const RE::Actor* actor);
    [[nodiscard]] std::optional<bool> LiveStateMatches(
        const RE::Actor* actor, std::string_view profileId, bool expectDefault);

    // Restores only pointers still owned by this backend. An external change
    // made after BCNG attached its clones is never overwritten.
    void ResetSessionState();
}
