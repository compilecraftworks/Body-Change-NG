#pragma once

namespace bcn::body_morph_weight
{
    // TESForm::GetWeight on a TESNPC returns engine percent (0..100),
    // NOT RaceMenu's displayed 0..1 slider position. Shared by every body
    // family, preview/commit and refit. Match OBody NG without a <=1 heuristic:
    // an actual engine weight of 1 is 1%, not 100%.
    [[nodiscard]] constexpr float FromEnginePercent(float enginePercent) noexcept
    {
        return enginePercent / 100.0F;
    }
}
