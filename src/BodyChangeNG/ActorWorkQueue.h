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

    [[nodiscard]] constexpr bool UsesQueuedAutomaticPath(const bool) noexcept
    {
        return true;
    }

    [[nodiscard]] constexpr std::uint8_t AutomaticDrainDelayHops(const bool performanceMode) noexcept
    {
        return performanceMode ? 1U : 0U;
    }

    class ActorWorkQueue final
    {
    public:
        static ActorWorkQueue& Get();

        // Every mode coalesces automatic actor events and bounds work to one
        // actor per game-task turn. Performance mode adds one scheduling hop
        // between actors. Newly visible actors are always prioritized ahead
        // of save-load bulk work so neither mode bursts a dense cell at once.
        [[nodiscard]] bool Request(RE::Actor* a_actor, ActorWorkReason a_reason);
        void NotifyDetached(std::uint32_t a_actorFormID);
        void ResetSession();
        [[nodiscard]] ActorWorkMetrics Metrics() const;
    };
}
