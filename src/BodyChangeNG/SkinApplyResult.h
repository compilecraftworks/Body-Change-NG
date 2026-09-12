#pragma once

#include <cstdint>

namespace bcn
{
    enum class SkinApplyResult : std::uint8_t
    {
        queued,
        invalidActor,
        missingProfile,
        incompatibleSex,
        incompatibleRace,
        incompatibleBodyFamily,
        ambiguousProfileLayout,
        ambiguousActorLayout,
        incompatibleFutanariType,
        noTaskInterface,
        unsupportedRuntime,
        actorBaseUnavailable,
        sharedActorBaseConflict,
        ownershipConflict
    };
}
