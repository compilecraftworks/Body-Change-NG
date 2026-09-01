#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bcn::skin_override
{
    enum class ApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidActor,
        actor3DUnavailable,
        missingProfile,
        incompatibleSex,
        faceGeometryUnavailable,
        noTaskInterface
    };

    // Applies a shared texture profile to either the player or an NPC. It does
    // not change the actor's Skin Armor, NIF path, inventory, or equipment slots.
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, std::string a_profileId);
    // Removes only Body Changer NG's texture-path overrides for body, hands,
    // feet, and the FaceGen face-head node. Other NiOverride shader values
    // (for example Wet Function's gloss/alpha values) remain.
    [[nodiscard]] ApplyResult QueueClear(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* a_actor);
    // Diagnostic snapshot of the live material texture paths. Safe on the game thread.
    void AuditNow(RE::Actor* a_actor, std::string_view a_reason);
    void ForgetActorState(std::uint32_t a_actorFormID);
}
