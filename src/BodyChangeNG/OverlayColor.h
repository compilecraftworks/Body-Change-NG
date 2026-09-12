#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace bcn::overlay
{
    [[nodiscard]] inline std::array<float, 4> UnpackColor(const std::uint32_t color)
    {
        return { ((color >> 16U) & 255U) / 255.0F, ((color >> 8U) & 255U) / 255.0F,
            (color & 255U) / 255.0F, (color >> 24U) / 255.0F };
    }

    [[nodiscard]] inline std::uint32_t PackColor(const std::array<float, 4>& color)
    {
        const auto byte = [](const float value) {
            return static_cast<std::uint32_t>(std::clamp(std::isfinite(value) ? value : 1.0F,
                0.0F, 1.0F) * 255.0F + 0.5F);
        };
        return (byte(color[3]) << 24U) | (byte(color[0]) << 16U) |
            (byte(color[1]) << 8U) | byte(color[2]);
    }
}
