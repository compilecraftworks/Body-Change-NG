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

namespace bcn::skin_application
{
    using ApplyResult = bcn::SkinApplyResult;

    enum class FutanariSelectionMode : std::uint8_t
    {
        preview,
        manual,
        automatic,
        restore
    };

    void ResetSessionState();

    // Applies a shared texture profile to either the player or an NPC through
    // a private clone of the current native TXST -> ARMA -> Skin Armor graph.
    // NIF paths, inventory, and equipment slots are never changed.
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, std::string a_profileId);
    // Detaches only Body Change NG's still-owned native clones. Another
    // provider's later native Skin Armor/TXST graph is never removed, so RSV
    // becomes visible again after a clear.
    [[nodiscard]] ApplyResult QueueClear(RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* a_actor);
    // True only for actors whose skin was explicitly managed this session,
    // including an explicit Default Skin selection.
    [[nodiscard]] bool HasTrackedSelection(const RE::Actor* a_actor);
    // Male SOS/TNG geometry is an optional external addon even though its
    // texture choice belongs to the selected general male BodySkin. Only this
    // addon adapter observes provider rebuilds; native body/hand/foot TXSTs
    // remain equipment-independent.
    [[nodiscard]] bool HasCurrentMaleGenitalSkin(const RE::Actor* a_actor);
    void QueueReapplyCurrentMaleGenitals(
        RE::Actor* a_actor, bool a_onlyIfAddonChanged = false);
    // The futanari path is independent from the full BodySkin profile. It
    // targets only the provider's exact loaded slot-52 ArmorAddon and retains
    // the chosen profile while unequipped. SOS and TNG use the same native
    // TXST visitor adapter; only the male/futanari role and profile selection differ.
    [[nodiscard]] std::optional<bcn::FutanariSkinType> CurrentFutanariType(
        RE::Actor* a_actor, bool a_refresh = false);
    [[nodiscard]] ApplyResult QueueApplyFutanari(RE::Actor* a_actor, std::string a_profileId,
        FutanariSelectionMode a_mode = FutanariSelectionMode::manual);
    [[nodiscard]] ApplyResult QueueClearFutanari(RE::Actor* a_actor,
        FutanariSelectionMode a_mode = FutanariSelectionMode::manual);
    [[nodiscard]] std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* a_actor);
    void QueueReapplyCurrentFutanari(
        RE::Actor* a_actor, bool a_onlyIfAddonChanged = false);
    void InvalidateFutanariDetection(std::uint32_t a_actorFormID);
    // Performs one bounded live-geometry verification after a save load. It
    // does not enumerate NPCs or the skin catalog.
    [[nodiscard]] std::optional<bool> LiveSkinStateMatches(
        RE::Actor* a_actor, std::string_view a_profileId, bool a_expectDefault);
    void ForgetActorState(std::uint32_t a_actorFormID);
}
