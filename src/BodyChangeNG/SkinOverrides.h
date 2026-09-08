#pragma once

#include "BodyChangeNG/SkinApplyResult.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace RE
{
    class Actor;
}

namespace bcn
{
    enum class FutanariSkinType : std::uint8_t;
}

namespace bcn::skin_override
{
    [[nodiscard]] constexpr bool CanFinalizeSkinApply(
        const bool complete, const bool rebuildQueued) noexcept
    {
        // QueueNiNodeUpdate may replace the live Biped clone. Do not cache a
        // successful result until the final clone has been repainted.
        return complete && !rebuildQueued;
    }

    using ApplyResult = bcn::SkinApplyResult;

    enum class LiveCheckScope : std::uint8_t
    {
        fullProfile,
        equipmentParts
    };

    void ResetSessionState();

    // Applies a shared texture profile to either the player or an NPC through
    // a private clone of the current native TXST -> ARMA -> Skin Armor graph.
    // NIF paths, inventory, and equipment slots are never changed.
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, std::string a_profileId);
    // Detaches only Body Change NG's still-owned native clones. Another
    // provider's later form pointer or persistent NiOverride values are never
    // removed; RSV therefore becomes visible again after a clear.
    [[nodiscard]] ApplyResult QueueClear(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* a_actor);
    // True only for actors whose skin was explicitly managed this session,
    // including an explicit Default Skin selection.
    [[nodiscard]] bool HasTrackedSelection(const RE::Actor* a_actor);
    // Male SOS/TNG geometry is an optional external addon even though its
    // texture choice belongs to the selected general male BodySkin. Only this
    // addon adapter observes equipment replacement; native body/hand/foot
    // TXSTs remain equipment-independent.
    [[nodiscard]] bool HasCurrentMaleGenitalSkin(const RE::Actor* a_actor);
    void QueueReapplyCurrentMaleGenitals(
        RE::Actor* a_actor, bool a_onlyIfAddonChanged = false);
    // The futanari path is independent from the full BodySkin profile. It
    // targets only a currently loaded TRX/ERF genital ArmorAddon and retains
    // the chosen profile while Gender Bender/TNG temporarily removes it.
    [[nodiscard]] std::optional<bcn::FutanariSkinType> CurrentFutanariType(
        RE::Actor* a_actor, bool a_refresh = false);
    [[nodiscard]] ApplyResult QueueApplyFutanari(RE::Actor* a_actor, std::string a_profileId);
    [[nodiscard]] ApplyResult QueueClearFutanari(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* a_actor);
    void QueueReapplyCurrentFutanari(
        RE::Actor* a_actor, bool a_onlyIfAddonChanged = false);
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
