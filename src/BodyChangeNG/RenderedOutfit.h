#pragma once
#include "BodyChangeNG/RenderedOutfitPolicy.h"
namespace RE { class Actor; }

namespace bcn::rendered_outfit
{
    void Initialize();
    bool Available();
    void Reset();
    void Forget(std::uint32_t actor);
    void Request(RE::Actor* actor);
    // Only call from frame_tasks game-task execution. Read enrolls this actor
    // for SFS changes and records the immutable version used to plan a refit.
    View Read(RE::Actor* actor);
    bool ValidateApply(RE::Actor* actor);
}
