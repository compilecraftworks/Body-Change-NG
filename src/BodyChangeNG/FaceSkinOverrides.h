#pragma once

#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"
#include <functional>
#include <vector>

namespace RE { class Actor; }
namespace bcn { struct SkinTextureLayer; }

namespace bcn::face_skin
{
    // DDS paths are already prepared by the skin transaction. Uses only the
    // versioned NiOverride interface (Papyrus fallback), not overlay slots.
    void Apply(RE::Actor* actor, const std::vector<SkinTextureLayer>& layers,
        std::string profileId, std::function<void(bool)> completion = {}, bool deferForRebuild = false,
        skin_transaction::Mode mode = skin_transaction::Mode::commit);
    void Clear(RE::Actor* actor, std::function<void(bool)> completion = {}, bool deferForRebuild = false,
        skin_transaction::Mode mode = skin_transaction::Mode::commit);
    // Drain an already-dispatched face call before requesting a BCNG rebuild.
    // The callback runs the native rebuild on the game task thread and returns
    // after that call. Engine flags/head readiness decide actual completion;
    // neither dispatch acceptance nor elapsed time can release the face early.
    void QueueRebuild(RE::Actor* actor, std::function<bool()> dispatch);
    // Called at the actual NiNodeUpdate event; invalidates old callbacks now,
    // then schedules the latest face selection without another 3D rebuild.
    void OnNiNodeUpdate(RE::Actor* actor);
    void Reset(bool preserveBaselines = false);
    void Forget(std::uint32_t actor);
    [[nodiscard]] bool Matches(const RE::Actor* actor, std::string_view profileId,
        skin_transaction::Mode mode = skin_transaction::Mode::commit);
    [[nodiscard]] bool Pending(const RE::Actor* actor, std::string_view profileId,
        skin_transaction::Mode mode = skin_transaction::Mode::commit);
    [[nodiscard]] std::vector<Baseline> SnapshotBaselines();
    void RestoreBaselines(std::vector<Baseline> values);
}
