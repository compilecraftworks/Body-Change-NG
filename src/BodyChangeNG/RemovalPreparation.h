#pragma once
#include <cstddef>
#include <cstdint>

namespace bcn::removal
{
    enum class Phase { idle, queued, running, ready, incomplete, failed };
    struct Status {
        Phase phase{};
        std::size_t pendingActors{};
        bool nativeSkinPending{}, morphsPending{}, tintPending{}, facePending{};
        std::uint64_t epoch{};
    };
    [[nodiscard]] constexpr bool CleanupComplete(const Status& status)
    {
        return status.phase == Phase::running && !status.pendingActors &&
            !status.nativeSkinPending && !status.morphsPending && !status.tintPending && !status.facePending;
    }
    [[nodiscard]] bool Begin();
    [[nodiscard]] Status Snapshot();
    // Resuming does not restore erased selections. Rules are never deleted.
    [[nodiscard]] bool Resume();
}
