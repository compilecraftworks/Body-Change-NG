#pragma once

#include <cstddef>
#include <cstdint>

namespace RE
{
    class Actor;
}

namespace bcn
{
    enum class ActorWorkReason : std::uint8_t
    {
        bulkLoad,
        initialized,
        cellAttached,
        rulesChanged
    };

    struct ActorWorkMetrics final
    {
        std::uint64_t requests{};
        std::uint64_t coalesced{};
        std::uint64_t waitingFor3D{};
        std::uint64_t processed{};
        std::uint64_t changed{};
        std::uint64_t unchanged{};
        std::uint64_t totalProcessingMicros{};
        std::size_t pending{};
        std::size_t maximumPending{};
    };

    [[nodiscard]] constexpr bool UsesQueuedPerformancePath(const bool performanceMode) noexcept
    {
        return performanceMode;
    }

    class ActorWorkQueue final
    {
    public:
        static ActorWorkQueue& Get();

        // Performance mode coalesces every automatic actor event and bounds
        // work to one actor per game-task turn. Newly visible actors are
        // prioritized ahead of save-load bulk work, so they still update in
        // real time without making a dense cell process all morphs at once.
        [[nodiscard]] bool Request(RE::Actor* a_actor, ActorWorkReason a_reason);
        void NotifyDetached(std::uint32_t a_actorFormID);
        void ResetSession();
        [[nodiscard]] ActorWorkMetrics Metrics() const;
    };
}
