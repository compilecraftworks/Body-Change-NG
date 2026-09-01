#pragma once

#include <RE/N/NiFrustum.h>

#include <cmath>
#include <numbers>

namespace bcn::camera_projection
{
    [[nodiscard]] inline bool SetHorizontalFov(RE::NiFrustum& frustum,
        const float degrees) noexcept
    {
        if (frustum.bOrtho || !std::isfinite(degrees) || degrees <= 0.0F || degrees >= 179.0F) {
            return false;
        }

        const auto currentHalfWidth = std::abs(frustum.fRight - frustum.fLeft) * 0.5F;
        if (!std::isfinite(currentHalfWidth) || currentHalfWidth <= 0.0001F) return false;

        const auto desiredHalfWidth = std::tan(degrees * std::numbers::pi_v<float> / 360.0F);
        const auto scale = desiredHalfWidth / currentHalfWidth;
        if (!std::isfinite(scale)) return false;

        // Scale every lateral plane together. This preserves aspect ratio and
        // any off-centre projection while changing only the rendered FOV.
        frustum.fLeft *= scale;
        frustum.fRight *= scale;
        frustum.fTop *= scale;
        frustum.fBottom *= scale;
        return true;
    }
}
