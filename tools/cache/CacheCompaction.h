#pragma once

// Offline-only maintenance. Keep every DDS resource name: unknown/old saves
// may still use it. Reclaim duplicate storage, never guess live references.
#include <filesystem>
#include <cstdint>
#include <functional>

namespace bcn::cache_compaction
{
    struct Result {
        std::uint64_t files{}, candidates{}, linked{}, skipped{}, failures{};
        std::uint64_t stagingFiles{}, stagingRemoved{};
        std::uint64_t reclaimableBytes{}, reclaimedBytes{};
        bool validRoot{}, complete{};
    };
    // Must be the physical .../textures/BodyChangeNG/Cache directory. Reparse
    // points, unknown shapes, external hard links and differing timestamps
    // are excluded. apply=false never changes a file.
    Result Run(const std::filesystem::path& root, bool apply,
        const std::function<bool()>& allowed = [] { return true; });
    bool GameRunning();
}
