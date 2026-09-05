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
        incompatibleRace,
        incompatibleBodyFamily,
        faceGeometryUnavailable,
        noTaskInterface
    };

    enum class LiveCheckScope : std::uint8_t
    {
        fullProfile,
        equipmentParts
    };

    void ResetSessionState();

    // Applies a shared texture profile to either the player or an NPC. It does
    // not change the actor's Skin Armor, NIF path, inventory, or equipment slots.
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, std::string a_profileId);
    // Removes only Body Change NG's texture-path overrides for body, hands,
    // feet, and the FaceGen face-head node. Other NiOverride shader values
    // (for example Wet Function's gloss/alpha values) remain.
    [[nodiscard]] ApplyResult QueueClear(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* a_actor);
    // Reconciles only a currently selected skin's face after another provider
    // (notably RSV) finishes its deferred NiNode update. Calls are coalesced
    // per actor and never enumerate the skin catalog or filesystem.
    void NotifyNiNodeUpdated(RE::Actor* a_actor);
    // Performs one bounded live-geometry verification after a save load. It
    // never mutates overrides and does not enumerate NPCs or the skin catalog.
    [[nodiscard]] std::optional<bool> LiveSkinStateMatches(
        RE::Actor* a_actor, std::string_view a_profileId, bool a_expectDefault,
        LiveCheckScope a_scope = LiveCheckScope::fullProfile);
    // Diagnostic snapshot of the live material texture paths. Safe on the game thread.
    void AuditNow(RE::Actor* a_actor, std::string_view a_reason);
    void ForgetActorState(std::uint32_t a_actorFormID);
}
