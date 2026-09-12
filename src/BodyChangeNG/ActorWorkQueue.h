#pragma once

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

    [[nodiscard]] constexpr bool UsesQueuedAutomaticPath(const bool) noexcept
    {
        return true;
    }

    [[nodiscard]] constexpr unsigned AutomaticActorBudget(const bool performanceMode) noexcept
    {
        return performanceMode ? 2U : 4U;
    }

    [[nodiscard]] constexpr std::uint8_t InitialDistributionDelayTicks() noexcept
    {
        // Allow serialization listeners and other RaceMenu users to finish
        // their load callbacks before BCNG begins touching loaded actors.
        return 2U;
    }

    class ActorWorkQueue final
    {
    public:
        static ActorWorkQueue& Get();

        // Every mode uses the externally clocked frame queue. Performance
        // mode lowers its actor/time budget, never adds same-FIFO hops.
        [[nodiscard]] bool Request(RE::Actor* a_actor, ActorWorkReason a_reason);
        void NotifyDetached(std::uint32_t a_actorFormID);
        void ResetSession();
    };
}
