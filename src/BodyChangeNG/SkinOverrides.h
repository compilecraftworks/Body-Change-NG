#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bcn
{
    enum class FutanariSkinType : std::uint8_t;
}

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
        incompatibleFutanariType,
        futanariGeometryUnavailable,
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
    // The futanari path is independent from the full BodySkin profile. It
    // targets only a currently loaded TRX/ERF genital ArmorAddon and retains
    // the chosen profile while Gender Bender/TNG temporarily removes it.
    [[nodiscard]] std::optional<bcn::FutanariSkinType> CurrentFutanariType(
        RE::Actor* a_actor, bool a_refresh = false);
    [[nodiscard]] ApplyResult QueueApplyFutanari(RE::Actor* a_actor, std::string a_profileId);
    [[nodiscard]] ApplyResult QueueClearFutanari(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* a_actor);
    void QueueReapplyCurrentFutanari(RE::Actor* a_actor);
    void InvalidateFutanariDetection(std::uint32_t a_actorFormID);
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
