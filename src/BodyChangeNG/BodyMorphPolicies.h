#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <cstdint>

namespace bcn::body_morph_policy
{
    enum class FemaleFamily : std::uint8_t
    {
        none,
        cbbe3ba,
        bhunpUnp,
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
            body_family::Bit(body_family::Family::unp) |
            body_family::Bit(body_family::Family::ube);
        auto candidates = actorFamily & supported;
        if (candidates == 0U) candidates = presetFamily & supported;
        if (candidates == body_family::Bit(body_family::Family::cbbe)) {
            return FemaleFamily::cbbe3ba;
        }
        if (candidates == body_family::Bit(body_family::Family::unp)) {
            return FemaleFamily::bhunpUnp;
        }
        if (candidates == body_family::Bit(body_family::Family::ube)) {
            return FemaleFamily::ube;
        }
        return FemaleFamily::none;
    }

    // CBBE/3BA and BHUNP/UNP use different verified slider dialects; the
    // caller selects the matching dialect. UBE uses materially different
    // anatomy and non-zero body defaults, so guessing targets can visibly
    // damage its shape.
    [[nodiscard]] constexpr bool SupportsOutfitCorrection(const FemaleFamily family) noexcept
    {
        return family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp;
    }

    // Anatomy randomization is an NPC-only feature. Both supported families
    // have an explicit slider dialect; UBE and ambiguous actors fail closed.
    [[nodiscard]] constexpr bool SupportsNpcAnatomyRandomization(
        const FemaleFamily family, const bool player) noexcept
    {
        return !player &&
            (family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp);
    }
}
