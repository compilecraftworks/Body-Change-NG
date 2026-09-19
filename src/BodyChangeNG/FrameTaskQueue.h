#pragma once
#include "BodyChangeNG/AppearanceWork.h"

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
        // A detached undo survives CancelActor, but its continuations must
        // retain both the RaceMenu gate and the session that accepted it.
        std::uint32_t restorationActor{};
        std::uint64_t restorationEpoch{};
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
            return SubmitImpl(actor, channel, std::move(run), delay, urgent, interactive,
                std::move(continuation), false);
        }
    private:
        bool SubmitImpl(std::uint32_t actor, std::uint32_t channel, Task run,
            std::uint32_t delay, bool urgent, bool interactive, Lease continuation, bool restoration)
        {
            const auto owningRestoration = restoration && continuation && actor && channel &&
                continuation->restorationActor == actor;
            if (!active_ || !run || (continuation && (actor != 0 || channel != 0) && !owningRestoration) ||
                (continuation && continuation->restorationEpoch && continuation->restorationEpoch != epoch_)) return false;
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
                    // A newer choice in this channel supersedes a parked undo,
                    // just as it superseded the old actor-owned cancel job.
                    if (job.lease) job.lease->cancelled.store(true);
                    job.lease = std::move(continuation);
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
    public:
        bool SubmitRestoration(std::uint32_t actor, Task run, std::uint32_t delay = 1,
            std::uint32_t channel = 0)
        {
            if (!active_ || !actor || !run) return false;
            auto lease = std::make_shared<FrameLeaseState>();
            lease->restorationActor = actor;
            lease->restorationEpoch = epoch_;
            lease->interactive.store(true);
            // Channelled undo shares ordinary actor FIFO/coalescing and holds
            // its busy lease through deferred writes. Unchannelled native-skin
            // undo keeps its existing independent, per-base generation policy.
            return SubmitImpl(channel ? actor : 0, channel, std::move(run), delay, true, true, std::move(lease), true);
        }
        void Advance()
        {
            ++tick_;
            for (auto it = busy_.begin(); it != busy_.end();) {
                // A timed-out native callback can remain owned by the VM
                // indefinitely.  Cancellation is therefore a terminal lease
                // state just like destruction: late callbacks observe the
                // atomic flag and may not mutate the actor, while the queue
                // retains the usual one quiet-tick boundary before another
                // job for that actor can begin.
                const auto lease = it->second.lease.lock();
                if (lease && !lease->cancelled.load()) { ++it; continue; }
                if (!it->second.released) { it->second.released = tick_; ++it; }
                else if (tick_ > it->second.released) it = busy_.erase(it);
                else ++it;
            }
        }
        std::optional<Job> Take(bool reserveInput = false, std::uint32_t blockedRestorationActor = 0)
        {
            if (!active_) return {};
            // Continuations have actor/channel zero and carry their original
            // lease. CancelActor cannot find them by actor ID; discard them
            // here instead of executing an old clear/registration later.
            std::erase_if(jobs_, [](const Job& job) {
                return job.actor == 0U && !ValidLease(job.lease);
            });
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
                return (job.urgent ? 2 : 0) +
                    (job.actor && bcn::appearance::IsInteractiveChannel(job.channel) ? 1 : 0);
            };
            ++scan_;
            for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
                // Leave the same job in place, without copying captures,
                // spinning callbacks or losing its age/order while editing.
                if (blockedRestorationActor && it->lease &&
                    it->lease->restorationActor == blockedRestorationActor) continue;
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
                if (!job.lease) job.lease = std::make_shared<FrameLeaseState>();
                job.lease->interactive.store(job.interactive);
                busy_[job.actor] = {job.lease, 0, job.requestedAt, job.channel};
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
            std::erase_if(jobs_, [actor](const Job& job) {
                return job.actor == actor && (!job.lease || job.lease->restorationActor != actor);
            });
            // Retained undo is still actor-owned for serialization/status.
            // Recount only this actor; never leave stale interactive counts.
            if (const auto found = actorPending_.find(actor); found != actorPending_.end()) {
                auto& pending = found->second;
                pending.count = pending.interactive = 0;
                for (const auto& job : jobs_) if (job.actor == actor) {
                    if (pending.count++ == 0) pending.since = job.requestedAt;
                    else pending.since = std::min(pending.since, job.requestedAt);
                    if (job.interactive) ++pending.interactive;
                }
                if (!pending.count) actorPending_.erase(found);
            }
            if (const auto found = busy_.find(actor); found != busy_.end()) {
                if (const auto lease = found->second.lease.lock(); lease && lease->restorationActor != actor)
                    lease->cancelled.store(true);
            }
            // Keep an already executing lease: cancellation is NOT completion.
        }
        bool Active() const { return active_; }
        std::uint64_t Epoch() const { return epoch_; }
        std::size_t Pending() const { return jobs_.size(); }
        bool HasWork() const { return !jobs_.empty() || !busy_.empty(); }
        std::uint64_t Tick() const { return tick_; }
        bool HasActorWork(std::uint32_t actor) const { return actorPending_.contains(actor) || busy_.contains(actor); }
        bool HasActorChannelWork(std::uint32_t actor, std::uint32_t channel) const
        {
            if (const auto found = busy_.find(actor);
                found != busy_.end() && found->second.channel == channel) {
                if (const auto lease = found->second.lease.lock();
                    lease && !lease->cancelled.load()) return true;
            }
            return std::ranges::any_of(jobs_, [actor, channel](const Job& job) {
                return job.actor == actor && job.channel == channel;
            });
        }
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
        struct Busy {
            std::weak_ptr<FrameLeaseState> lease;
            std::uint64_t released{};
            Clock::time_point since;
            std::uint32_t channel{};
        };
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
