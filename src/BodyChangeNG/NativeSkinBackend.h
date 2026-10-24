#pragma once

#include "BodyChangeNG/SkinApplyResult.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"

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
    // Skin Armor forms. Resource verification reads only current private skin
    // parts; ordinary equipment is not tracked or repainted.
    // afterMutation refreshes loaded references after graph mutation, or just
    // the failing reference during bounded resource recovery. The facade is
    // the sole owner of the native Actor.QueueNiNodeUpdate equivalent; this backend
    // must never issue a second 3D reset or repaint outfit-owned geometry.
    [[nodiscard]] SkinApplyResult QueueApply(RE::Actor* actor, std::string profileId,
        std::function<void(RE::Actor*)> afterMutation = {},
        skin_transaction::Mode mode = skin_transaction::Mode::commit,
        skin_transaction::Selection selection = skin_transaction::Selection::automatic);
    [[nodiscard]] SkinApplyResult QueueClear(RE::Actor* actor,
        std::function<void(RE::Actor*)> afterMutation = {},
        skin_transaction::Mode mode = skin_transaction::Mode::commit,
        bool resetSharedBase = false);
    [[nodiscard]] std::optional<std::string> CurrentProfileId(const RE::Actor* actor);
    [[nodiscard]] bool HasTrackedSelection(const RE::Actor* actor);
    // A DDS replacement does not change the body's UV family. Only return
    // the source classification (including unknown/zero) while this actor
    // still uses our graph. Do not reclassify our private cached DDS paths.
    [[nodiscard]] std::optional<std::uint32_t> SourceBodyFamily(const RE::Actor* actor);
    [[nodiscard]] std::optional<bool> LiveStateMatches(
        const RE::Actor* actor, std::string_view profileId, bool expectDefault);

    // Verifies/retries resources in the existing owned graph after rebuild.
    void QueueVerifyBodyTextures(RE::Actor* actor, std::function<void(RE::Actor*)> refresh);
    void ForgetBodyTextureRecovery(std::uint32_t actor);

    // Game task thread only. Detaches only owned forms, keeps reusable graphs
    // per player race and invalidates old callbacks. Never changes selection.
    // nullopt: no transition; bool: whether the native body needs a rebuild.
    [[nodiscard]] std::optional<bool> PreparePlayerRace(RE::Actor* actor, bool suspended);

#ifdef BCNG_SKIN_DIAGNOSTICS
    // Game task thread only. Reads current geometry by value, retains no
    // render pointers and never changes forms/materials/application markers.
    void LogCurrentSkinState(RE::Actor* actor, std::string_view stage, bool includeReady = false);
#else
    inline void LogCurrentSkinState(RE::Actor*, std::string_view, bool = false) {}
#endif

    // Restores only pointers still owned by this backend. An external change
    // made after BCNG attached its clones is never overwritten.
    void ResetSessionState();
    [[nodiscard]] bool PrivateTexturesRestored();
}
