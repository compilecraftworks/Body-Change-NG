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

    class ActorWorkQueue final
    {
    public:
        static ActorWorkQueue& Get();

        // Newly visible actors use the fast path. Bulk save-load work is
        // bounded to one actor per game-task turn when performance mode is on.
        [[nodiscard]] bool Request(RE::Actor* a_actor, ActorWorkReason a_reason);
        void NotifyDetached(std::uint32_t a_actorFormID);
        void ResetSession();
        [[nodiscard]] ActorWorkMetrics Metrics() const;
    };
}
