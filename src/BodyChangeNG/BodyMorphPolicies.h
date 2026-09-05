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

    // Outfit breast/nipple correction is derived from OBody NG's CBBE/3BA
    // slider set. UBE uses a materially different anatomy and non-zero body
    // defaults, so guessing equivalent targets can visibly damage its shape.
    [[nodiscard]] constexpr bool SupportsOutfitCorrection(const FemaleFamily family) noexcept
    {
        return family == FemaleFamily::cbbe3ba;
    }

    // Anatomy randomization is an NPC distribution feature and currently has
    // the same verified CBBE/3BA-only support boundary as OBody NG.
    [[nodiscard]] constexpr bool SupportsNpcAnatomyRandomization(
        const FemaleFamily family, const bool player) noexcept
    {
        return !player && family == FemaleFamily::cbbe3ba;
    }
}
