#pragma once

#include "BodyChangeNG/ActorState.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace bcn::overlay
{
    // One area may contain several committed RaceMenu paint slots. Preview is
    // deliberately singular: moving through the list replaces only the live
    // transient item and never mutates the committed vector underneath it.
    struct PreviewState final
    {
        std::vector<OverlayItemState> original;
        std::optional<OverlayItemState> live;
        bool liveDefault{};
    };
}
