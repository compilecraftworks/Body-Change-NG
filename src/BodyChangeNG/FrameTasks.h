#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/FrameTaskQueue.h"

namespace bcn::frame_tasks
{
    using Lease = async_work::FrameTaskQueue::Lease;
    bool Queue(std::uint32_t actor, std::function<void()> work,
        std::uint32_t delay = 1,
        appearance::WorkChannel channel = appearance::WorkChannel::none,
        bool urgent = false, bool interactive = false);
    // Continuations retain the ORIGINAL actor lease and must not acquire it
    // again. Used only by already-dispatched asynchronous skin callbacks.
    bool Continue(Lease lease, std::function<void()> work, std::uint32_t delay = 1);
    Lease CurrentLease();
    bool ValidLease(const Lease& lease);
    void SetAvailable(bool available);
    void OnInputTick();
    void Reset(bool active);
    bool Active();
    bool InGameTask();
    // One UI actor can own uncommitted previews; automatic reconciliation waits.
    void SetPreviewActor(std::uint32_t actor);
    bool HasPreview(std::uint32_t actor);
    std::uint64_t Epoch();
    bool IsCurrent(std::uint64_t epoch);
    void CancelActor(std::uint32_t actor);
    bool HasActorWork(std::uint32_t actor);
    bool HasActorChannelWork(std::uint32_t actor, appearance::WorkChannel channel);
    async_work::FrameTaskQueue::WorkStatus Status(std::uint32_t actor);
}
