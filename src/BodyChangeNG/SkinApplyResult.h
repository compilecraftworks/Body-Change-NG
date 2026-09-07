#pragma once

#include <cstdint>

namespace bcn
{
    enum class SkinApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidActor,
        actor3DUnavailable,
        missingProfile,
        incompatibleSex,
        incompatibleRace,
        incompatibleBodyFamily,
        ambiguousProfileLayout,
        ambiguousActorLayout,
        incompatibleFutanariType,
        futanariGeometryUnavailable,
        faceGeometryUnavailable,
        noTaskInterface,
        unsupportedRuntime,
        actorBaseUnavailable,
        nativeCloneFailed,
        sharedActorBaseConflict,
        ownershipConflict
    };
}
