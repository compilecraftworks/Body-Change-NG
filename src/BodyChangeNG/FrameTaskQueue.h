#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace bcn::async_work
{
    // Engine-independent policy. The owner serializes access; callbacks run
    // OUTSIDE its lock. Only Advance (an external engine tick) advances time.
    class FrameTaskQueue final
    {
    public:
        using Lease = std::shared_ptr<void>;
        static bool ValidLease(const Lease& lease)
        {
            return !lease || !std::static_pointer_cast<std::atomic_bool>(lease)->load();
        }
        using Task = std::function<void()>;
        struct Job { std::uint32_t actor{}, channel{}; std::uint64_t due{}, born{}; bool urgent{}; Task run; Lease lease; };
        bool Submit(std::uint32_t actor, std::uint32_t channel, Task run,
            std::uint32_t delay = 1, bool urgent = false)
        {
            if (!active_ || !run) return false;
            if (channel != 0) {
                for (auto it = jobs_.begin(); it != jobs_.end(); ++it) if (it->actor == actor && it->channel == channel) {
                    auto job = std::move(*it);
                    jobs_.erase(it);
                    job.run = std::move(run);
                    job.urgent |= urgent;
                    // Replacement retains age (no starvation), but waits for
                    // the newest event's geometry to reach its due tick.
                    job.due = tick_ + std::max(1U, delay);
                    // Latest request goes AFTER earlier other-channel work:
                    // preview -> commit -> preview must end at preview.
                    jobs_.push_back(std::move(job));
                    return true;
                }
            }
            jobs_.push_back({actor, channel, tick_ + std::max(1U, delay), tick_, urgent, std::move(run), {}});
            if (actor) ++actorPending_[actor].count;
            return true;
        }
        void Advance()
        {
            ++tick_;
            for (auto it = busy_.begin(); it != busy_.end();) {
                if (!it->second.lease.expired()) { ++it; continue; }
                if (!it->second.released) { it->second.released = tick_; ++it; }
                else if (tick_ > it->second.released) it = busy_.erase(it);
                else ++it;
            }
        }
        std::optional<Job> Take()
        {
            if (!active_) return {};
            auto best = jobs_.end();
            const auto score = [&](const Job& job) {
                if (tick_ - job.born >= 60) return 4;
                return (job.urgent ? 2 : 0) + (job.actor && job.channel >= 200 ? 1 : 0);
            };
            ++scan_;
            for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
                if (it->actor) {
                    auto& pending = actorPending_.at(it->actor);
                    if (pending.seen == scan_) continue;
                    pending.seen = scan_;
                }
                if (it->due > tick_ || (it->actor && busy_.contains(it->actor))) continue;
                if (best == jobs_.end() || score(*it) > score(*best)) best = it;
            }
            if (best == jobs_.end()) return {};
            auto job = std::move(*best);
            jobs_.erase(best);
            if (job.actor && --actorPending_.at(job.actor).count == 0) actorPending_.erase(job.actor);
            if (job.actor) {
                job.lease = std::make_shared<std::atomic_bool>(false);
                busy_[job.actor] = {job.lease, 0};
            }
            return job;
        }
        void Reset(bool active)
        {
            for (const auto& [actor, busy] : busy_) {
                if (const auto lease = busy.lease.lock()) std::static_pointer_cast<std::atomic_bool>(lease)->store(true);
            }
            active_ = active;
            ++epoch_;
            jobs_.clear();
            actorPending_.clear();
            busy_.clear();
        }
        void CancelActor(std::uint32_t actor)
        {
            std::erase_if(jobs_, [actor](const Job& job) { return job.actor == actor; });
            actorPending_.erase(actor);
            if (const auto found = busy_.find(actor); found != busy_.end()) {
                if (const auto lease = found->second.lease.lock()) std::static_pointer_cast<std::atomic_bool>(lease)->store(true);
            }
            // Keep an already executing lease: cancellation is NOT completion.
        }
        bool Active() const { return active_; }
        std::uint64_t Epoch() const { return epoch_; }
        std::size_t Pending() const { return jobs_.size(); }
        std::uint64_t Tick() const { return tick_; }
        bool HasActorWork(std::uint32_t actor) const { return actorPending_.contains(actor) || busy_.contains(actor); }
    private:
        struct Busy { std::weak_ptr<void> lease; std::uint64_t released{}; };
        std::deque<Job> jobs_;
        std::unordered_map<std::uint32_t, Busy> busy_;
        struct PendingActorState { std::size_t count{}; std::uint64_t seen{}; };
        std::unordered_map<std::uint32_t, PendingActorState> actorPending_;
        std::uint64_t scan_{};
        std::uint64_t tick_{}, epoch_{1};
        bool active_{true};
    };
}
