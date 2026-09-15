#pragma once

#include "BodyChangeNG/SliderName.h"

namespace bcn::ube_morph
{
    // Verified against UBE 2.0 Release Body / Preview OSP and their Zeroed
    // output TRI/NIF. These are the only non-zero body build defaults. The
    // remaining body, hand and foot sliders have a zero build baseline.
    // An omitted XML endpoint means the OSP default: leave its delta at zero.
    // Do not infer polarity from names such as "n|p", or clamp authored values.
    [[nodiscard]] constexpr float BuildDefault(std::string_view name, bool large) noexcept
    {
        if (slider_name::Equal{}(name, "NipplesShowUp")) return 1.0F;
        if (!large && slider_name::Equal{}(name, "SkinnyMorph")) return 1.0F;
        return 0.0F;
    }

    [[nodiscard]] constexpr float AuthoredDelta(std::string_view name, float value, bool large) noexcept
    {
        return value - BuildDefault(name, large);
    }
}
