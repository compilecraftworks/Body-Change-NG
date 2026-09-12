#pragma once

#include "BodyChangeNG/FaceSkinPolicy.h"
#include <functional>
#include <vector>

namespace RE { class Actor; }
namespace bcn { struct SkinTextureLayer; }

namespace bcn::face_skin
{
    // DDS paths are already prepared by the skin transaction. Uses only the
    // public NiOverride Papyrus API, never the overlay installation API.
    void Apply(RE::Actor* actor, const std::vector<SkinTextureLayer>& layers,
        std::string profileId, std::function<void(bool)> completion = {}, bool deferForRebuild = false);
    void Clear(RE::Actor* actor, std::function<void(bool)> completion = {}, bool deferForRebuild = false);
    // Drain an already-dispatched face call before requesting a BCNG rebuild.
    // The callback returns dispatch acceptance, NOT engine completion.
    void QueueRebuild(RE::Actor* actor, std::function<bool()> dispatch);
    // Called at the actual NiNodeUpdate event; invalidates old callbacks now,
    // then schedules the latest face selection without another 3D rebuild.
    void OnNiNodeUpdate(RE::Actor* actor);
    void Reset(bool preserveBaselines = false);
    void Forget(std::uint32_t actor);
    [[nodiscard]] bool Matches(const RE::Actor* actor, std::string_view profileId);
    [[nodiscard]] bool Pending(const RE::Actor* actor, std::string_view profileId);
    [[nodiscard]] std::vector<Baseline> SnapshotBaselines();
    void RestoreBaselines(std::vector<Baseline> values);
}
