#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <cstdint>

namespace bcn::body_morph_policy
{
    enum class FemaleFamily : std::uint8_t
    {
        none,
        cbbe3ba,
        ube
    };

    // Runtime actor evidence wins. Preset metadata is a safe fallback only
    // when it identifies exactly one supported family. An ambiguous
    // CBBE+UBE result must never receive both families' anatomical sliders.
    [[nodiscard]] constexpr FemaleFamily ResolveFemaleFamily(
        const body_family::Mask actorFamily,
        const body_family::Mask presetFamily = 0U) noexcept
    {
        const auto supported = body_family::Bit(body_family::Family::cbbe) |
            body_family::Bit(body_family::Family::ube);
        auto candidates = actorFamily & supported;
        if (candidates == 0U) candidates = presetFamily & supported;
        if (candidates == body_family::Bit(body_family::Family::cbbe)) {
            return FemaleFamily::cbbe3ba;
        }
        if (candidates == body_family::Bit(body_family::Family::ube)) {
            return FemaleFamily::ube;
        }
        return FemaleFamily::none;
    }
}
