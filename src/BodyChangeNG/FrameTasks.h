#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace bcn::frame_tasks
{
    using Lease = std::shared_ptr<void>;
    bool Queue(std::uint32_t actor, std::function<void()> work,
        std::uint32_t delay = 1, std::uint32_t channel = 0, bool urgent = false);
    // Continuations retain the ORIGINAL actor lease and must not acquire it
    // again. Used only by already-dispatched asynchronous skin callbacks.
    bool Continue(Lease lease, std::function<void()> work, std::uint32_t delay = 1);
    Lease CurrentLease();
    bool ValidLease(const Lease& lease);
    void SetAvailable(bool available);
    void OnInputTick();
    void Reset(bool active);
    bool Active();
    std::uint64_t Epoch();
    bool IsCurrent(std::uint64_t epoch);
    void CancelActor(std::uint32_t actor);
    bool HasActorWork(std::uint32_t actor);
}
