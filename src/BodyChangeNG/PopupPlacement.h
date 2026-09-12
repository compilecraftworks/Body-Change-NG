#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace bcn::popup_placement
{
    enum class Kind : std::size_t
    {
        outfit, settings, tintColor, overlayColor,
        distributionBody, distributionSkin, distributionFutanari, distributionOverlay,
        count
    };

    inline constexpr std::array keys{
        "outfit", "settings", "tintColor", "overlayColor",
        "distributionBody", "distributionSkin", "distributionFutanari", "distributionOverlay"
    };
    static_assert(keys.size() == static_cast<std::size_t>(Kind::count));

    struct Position
    {
        bool set{};
        float x{};
        float y{};
    };
    using Positions = std::array<Position, keys.size()>;

    [[nodiscard]] inline bool Valid(const float x, const float y)
    {
        return std::isfinite(x) && std::isfinite(y) &&
            std::abs(x) <= 32768.0F && std::abs(y) <= 32768.0F;
    }

    [[nodiscard]] inline bool Changed(const Position saved, const float x, const float y)
    {
        return Valid(x, y) && (!saved.set ||
            std::abs(saved.x - x) > 0.5F || std::abs(saved.y - y) > 0.5F);
    }

    [[nodiscard]] inline float ClampAxis(const float position, const float windowSize,
        const float workStart, const float workSize)
    {
        // Oversized windows retain an accessible title bar instead of producing
        // an inverted clamp interval. Ignore minimized/initializing viewports.
        if (workSize <= 0.0F) return position;
        return std::clamp(position, workStart,
            workStart + (std::max)(0.0F, workSize - windowSize));
    }
}
