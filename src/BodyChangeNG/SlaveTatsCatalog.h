#pragma once

#include "BodyChangeNG/RaceMenuOverlay.h"

#include <filesystem>
#include <vector>

namespace bcn::overlay
{
    // Reads the public SlaveTats pack format. Each JSON row becomes one
    // ordinary BCNG-owned RaceMenu paint; SlaveTats' own actor state and slot
    // ownership are never modified.
    [[nodiscard]] std::vector<Entry> ScanSlaveTatsDirectory(
        const std::filesystem::path& a_root);
}
