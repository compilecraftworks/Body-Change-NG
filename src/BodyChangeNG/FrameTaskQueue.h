#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace bcn::async_work
{
    struct FrameLeaseState final
    {
        std::atomic_bool cancelled{};
        // A later direct selection may promote already-running prerequisites.
        std::atomic_bool interactive{};
    };

    // Engine-independent policy. The owner serializes access; callbacks run
    // OUTSIDE its lock. Only Advance (an external engine tick) advances time.
    class FrameTaskQueue final
    {
    public:
        using Lease = std::shared_ptr<FrameLeaseState>;
        using Clock = std::chrono::steady_clock;
        struct WorkStatus
        {
            bool queued{}, busy{};
            std::uint64_t elapsedMs{};
            bool Delayed() const { return (queued || busy) && elapsedMs >= 1500; }
        };
        static bool ValidLease(const Lease& lease)
        {
            return !lease || !lease->cancelled.load();
        }
        static bool InteractiveLease(const Lease& lease) { return lease && lease->interactive.load(); }
        using Task = std::function<void()>;
        struct Job {
            std::uint32_t actor{}, channel{};
            std::uint64_t due{}, born{};
            bool urgent{}, interactive{};
            Task run;
            Lease lease;
            Clock::time_point requestedAt;
        };
        bool Submit(std::uint32_t actor, std::uint32_t channel, Task run,
            std::uint32_t delay = 1, bool urgent = false, bool interactive = false, Lease continuation = {})
        {
            if (!active_ || !run || (continuation && (actor != 0 || channel != 0))) return false;
            if (actor && interactive) {
                if (const auto busy = busy_.find(actor); busy != busy_.end()) {
                    if (const auto lease = busy->second.lease.lock()) lease->interactive.store(true);
                }
            }
            if (channel != 0) {
                for (auto it = jobs_.begin(); it != jobs_.end(); ++it) if (it->actor == actor && it->channel == channel) {
                    auto job = std::move(*it);
                    jobs_.erase(it);
                    job.run = std::move(run);
                    job.urgent |= urgent;
                    if (actor && interactive && !job.interactive) ++actorPending_.at(actor).interactive;
                    job.interactive |= interactive;
                    // Replacement retains age (no starvation), but waits for
                    // the newest event's geometry to reach its due tick.
                    job.due = tick_ + std::max(1U, delay);
                    // Latest request goes AFTER earlier other-channel work:
                    // preview -> commit -> preview must end at preview.
                    jobs_.push_back(std::move(job));
                    return true;
                }
            }
            const auto now = Clock::now();
            jobs_.push_back({actor, channel, tick_ + std::max(1U, delay), tick_, urgent,
                interactive, std::move(run), std::move(continuation), now});
            if (actor) {
                auto& pending = actorPending_[actor];
                if (pending.count++ == 0) pending.since = now;
                if (interactive) ++pending.interactive;
            }
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
        std::optional<Job> Take(bool reserveInput = false)
        {
            if (!active_) return {};
            auto best = jobs_.end();
            // One reserved opportunity per pump, WITHIN its existing budget.
            // At most three consecutive reservations: a fourth pump uses the
            // normal aging policy, even if each native call consumes a batch.
            const auto reserve = reserveInput && inputReservations_ < 3;
            const auto score = [&](const Job& job) {
                const auto helpsInput = job.interactive || InteractiveLease(job.lease) ||
                    (job.actor && actorPending_.at(job.actor).interactive != 0);
                if (reserve && helpsInput) return 5;
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
            if (reserveInput) inputReservations_ = reserve && score(*best) == 5 ? inputReservations_ + 1 : 0;
            auto job = std::move(*best);
            jobs_.erase(best);
            job.interactive |= InteractiveLease(job.lease);
            if (job.actor) {
                auto& pending = actorPending_.at(job.actor);
                // Preserve FIFO: promote an earlier prerequisite, never let a
                // new preview/commit jump over the actor's preceding work.
                const auto helpsInput = pending.interactive != 0;
                if (job.interactive) --pending.interactive;
                job.interactive |= helpsInput;
                if (--pending.count == 0) actorPending_.erase(job.actor);
                job.lease = std::make_shared<FrameLeaseState>();
                job.lease->interactive.store(job.interactive);
                busy_[job.actor] = {job.lease, 0, job.requestedAt};
            }
            return job;
        }
        void Reset(bool active)
        {
            for (const auto& [actor, busy] : busy_) {
                if (const auto lease = busy.lease.lock()) lease->cancelled.store(true);
            }
            active_ = active;
            ++epoch_;
            jobs_.clear();
            actorPending_.clear();
            busy_.clear();
            inputReservations_ = 0;
        }
        void CancelActor(std::uint32_t actor)
        {
            std::erase_if(jobs_, [actor](const Job& job) { return job.actor == actor; });
            actorPending_.erase(actor);
            if (const auto found = busy_.find(actor); found != busy_.end()) {
                if (const auto lease = found->second.lease.lock()) lease->cancelled.store(true);
            }
            // Keep an already executing lease: cancellation is NOT completion.
        }
        bool Active() const { return active_; }
        std::uint64_t Epoch() const { return epoch_; }
        std::size_t Pending() const { return jobs_.size(); }
        std::uint64_t Tick() const { return tick_; }
        bool HasActorWork(std::uint32_t actor) const { return actorPending_.contains(actor) || busy_.contains(actor); }
        WorkStatus Status(std::uint32_t actor, Clock::time_point now = Clock::now()) const
        {
            WorkStatus result;
            auto since = now;
            if (const auto pending = actorPending_.find(actor); pending != actorPending_.end()) {
                result.queued = true;
                since = std::min(since, pending->second.since);
            }
            if (const auto busy = busy_.find(actor); busy != busy_.end()) {
                result.busy = true;
                since = std::min(since, busy->second.since);
            }
            result.elapsedMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - since).count());
            return result;
        }
    private:
        struct Busy { std::weak_ptr<FrameLeaseState> lease; std::uint64_t released{}; Clock::time_point since; };
        std::deque<Job> jobs_;
        std::unordered_map<std::uint32_t, Busy> busy_;
        struct PendingActorState { std::size_t count{}, interactive{}; std::uint64_t seen{}; Clock::time_point since; };
        std::unordered_map<std::uint32_t, PendingActorState> actorPending_;
        std::uint64_t scan_{};
        std::uint64_t tick_{}, epoch_{1};
        unsigned inputReservations_{};
        bool active_{true};
    };
}
