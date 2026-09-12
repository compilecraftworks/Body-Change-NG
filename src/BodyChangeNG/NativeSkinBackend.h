#pragma once

#include "BodyChangeNG/SkinApplyResult.h"

#include <functional>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace RE
{
    class Actor;
}

namespace bcn::native_skin
{
    // Body/Hands/Feet are implemented through cloned native TXST -> ARMA ->
    // Skin Armor forms. The actor's equipment is never inspected or tracked.
    // afterMutation runs once for each loaded reference sharing the mutated
    // ActorBase, and only after its pointers actually changed. The facade is
    // the sole owner of the public Actor.QueueNiNodeUpdate route; this backend
    // must never issue a second 3D reset or repaint outfit-owned geometry.
    [[nodiscard]] SkinApplyResult QueueApply(RE::Actor* actor, std::string profileId,
        std::function<void(RE::Actor*)> afterMutation = {});
    [[nodiscard]] SkinApplyResult QueueClear(RE::Actor* actor,
        std::function<void(RE::Actor*)> afterMutation = {});
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* actor);
    [[nodiscard]] bool HasTrackedSelection(const RE::Actor* actor);
    // A DDS replacement does not change the body's UV family. Only return
    // the verified source family while this actor still uses our graph.
    [[nodiscard]] std::optional<std::uint32_t> SourceBodyFamily(const RE::Actor* actor);
    [[nodiscard]] std::optional<bool> LiveStateMatches(
        const RE::Actor* actor, std::string_view profileId, bool expectDefault);

    // Restores only pointers still owned by this backend. An external change
    // made after BCNG attached its clones is never overwritten.
    void ResetSessionState();
}
