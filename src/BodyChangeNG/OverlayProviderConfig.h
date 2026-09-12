#pragma once
#include "BodyChangeNG/OverlayTypes.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace bcn::overlay
{
    using ProviderCounts = std::array<std::uint32_t, Index(Area::count)>;
    inline constexpr std::array<const char*, Index(Area::count)> kProviderCountPaths{
        "_global.skse.plugins.NiOverride.face.iNumOverlays",
        "_global.skse.plugins.NiOverride.body.iNumOverlays",
        "_global.skse.plugins.NiOverride.hand.iNumOverlays",
        "_global.skse.plugins.NiOverride.feet.iNumOverlays"
    };
    // Values published by RegisterNiOverrideScaleform are the already-clamped
    // normal-slot counts, after both INI files and the face-enable flag.
    template <class ReadCount>
    [[nodiscard]] std::optional<ProviderCounts> ReadProviderCounts(
        const std::optional<bool> enabled, ReadCount&& readCount)
    {
        if (!enabled) return {};
        ProviderCounts counts{};
        if (!*enabled) return counts;
        for (const auto area : kAreas) {
            const auto value = readCount(kProviderCountPaths[Index(area)]);
            if (!value || !std::isfinite(*value) || *value < 0.0 ||
                *value > 127.0 || std::floor(*value) != *value) return {};
            counts[Index(area)] = static_cast<std::uint32_t>(*value);
        }
        return counts;
    }
}
