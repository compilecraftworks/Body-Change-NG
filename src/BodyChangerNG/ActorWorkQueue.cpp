#include "BodyChangerNG/ActorWorkQueue.h"

#include "BodyChangerNG/ActorRegistry.h"
#include "BodyChangerNG/Distribution.h"
#include "BodyChangerNG/OutfitRefit.h"
#include "BodyChangerNG/Settings.h"

#include <SKSE/Logger.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace
{
    struct PendingActor final
    {
        RE::ActorHandle handle;
        bcn::ActorWorkReason reason{ bcn::ActorWorkReason::bulkLoad };
        std::uint64_t session{};
        bool queued{};
        bool waitingFor3D{};
    };

    std::mutex g_lock;
    std::unordered_map<RE::FormID, PendingActor> g_pending;
    std::deque<RE::FormID> g_order;
    bool g_drainScheduled{};
    std::atomic_uint64_t g_session{ 1U };
    std::atomic_uint64_t g_requests{};
    std::atomic_uint64_t g_coalesced{};
    std::atomic_uint64_t g_waiting{};
    std::atomic_uint64_t g_processed{};
    std::atomic_uint64_t g_changed{};
    std::atomic_uint64_t g_unchanged{};
    std::atomic_uint64_t g_processingMicros{};
    std::atomic_size_t g_maxPending{};

    void ScheduleDrain();

    void DrainOne()
    {
        PendingActor request;
        RE::FormID formID{};
        {
            std::scoped_lock lock(g_lock);
            g_drainScheduled = false;
            while (!g_order.empty()) {
                formID = g_order.front();
                g_order.pop_front();
                const auto found = g_pending.find(formID);
                if (found == g_pending.end() || !found->second.queued) continue;
                request = found->second;
                found->second.queued = false;
                break;
            }
        }
        if (formID != 0U && request.session == g_session.load(std::memory_order_acquire)) {
            const auto actor = request.handle.get();
            if (actor && actor->Is3DLoaded()) {
                const auto started = std::chrono::steady_clock::now();
                const auto changed = bcn::Distribution::Get().ApplyActor(actor.get());
                bcn::OutfitRefit::Get().ProcessActor(actor.get());
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started).count();
                g_processingMicros.fetch_add(elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U,
                    std::memory_order_relaxed);
                g_processed.fetch_add(1U, std::memory_order_relaxed);
                (changed ? g_changed : g_unchanged).fetch_add(1U, std::memory_order_relaxed);
                std::scoped_lock lock(g_lock);
                g_pending.erase(formID);
            } else {
                std::scoped_lock lock(g_lock);
                const auto found = g_pending.find(formID);
                if (found != g_pending.end()) {
                    found->second.waitingFor3D = true;
                    g_waiting.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        }
        ScheduleDrain();
    }

    void ScheduleDrain()
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        {
            std::scoped_lock lock(g_lock);
            if (g_drainScheduled || g_order.empty()) return;
            g_drainScheduled = true;
        }
        tasks->AddTask(DrainOne);
    }

    [[nodiscard]] bool IsNearPlayer(RE::Actor* actor)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !player || actor == player) return true;
        return actor->GetDistance(player) <= 3000.0F;
    }
}

namespace bcn
{
    ActorWorkQueue& ActorWorkQueue::Get()
    {
        static ActorWorkQueue queue;
        return queue;
    }

    bool ActorWorkQueue::Request(RE::Actor* actor, const ActorWorkReason reason)
    {
        if (!actor || actor->GetFormID() == 0U) return false;
        g_requests.fetch_add(1U, std::memory_order_relaxed);
        const auto performanceMode = Settings::Get().Snapshot().performanceMode;
        const auto fastPath = !performanceMode || reason != ActorWorkReason::bulkLoad || IsNearPlayer(actor);
        if (fastPath && actor->Is3DLoaded()) {
            {
                std::scoped_lock lock(g_lock);
                g_pending.erase(actor->GetFormID());
            }
            const auto started = std::chrono::steady_clock::now();
            const auto changed = Distribution::Get().ApplyActor(actor);
            OutfitRefit::Get().ProcessActor(actor);
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count();
            g_processingMicros.fetch_add(elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U,
                std::memory_order_relaxed);
            g_processed.fetch_add(1U, std::memory_order_relaxed);
            (changed ? g_changed : g_unchanged).fetch_add(1U, std::memory_order_relaxed);
            return changed;
        }

        const auto formID = actor->GetFormID();
        {
            std::scoped_lock lock(g_lock);
            auto [found, inserted] = g_pending.try_emplace(formID);
            auto& pending = found->second;
            if (!inserted) g_coalesced.fetch_add(1U, std::memory_order_relaxed);
            pending.handle = actor->GetHandle();
            pending.reason = reason;
            pending.session = g_session.load(std::memory_order_relaxed);
            pending.waitingFor3D = !actor->Is3DLoaded();
            if (!pending.waitingFor3D && !pending.queued) {
                pending.queued = true;
                g_order.push_back(formID);
            }
            auto maximum = g_maxPending.load(std::memory_order_relaxed);
            while (g_pending.size() > maximum &&
                !g_maxPending.compare_exchange_weak(maximum, g_pending.size(), std::memory_order_relaxed)) {}
        }
        ScheduleDrain();
        return true;
    }

    void ActorWorkQueue::NotifyDetached(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0U) return;
        std::scoped_lock lock(g_lock);
        // A later attach event submits a fresh request and handle. Retaining
        // an unloaded actor here only grows the wait map for NPCs that never
        // return to the current game session.
        g_pending.erase(actorFormID);
    }

    void ActorWorkQueue::ResetSession()
    {
        const auto generation = g_session.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        {
            std::scoped_lock lock(g_lock);
            g_pending.clear();
            g_order.clear();
            g_drainScheduled = false;
        }
        SKSE::log::info("Body Changer NG reset its actor work queue for session {}", generation);
    }

    ActorWorkMetrics ActorWorkQueue::Metrics() const
    {
        std::size_t pending{};
        {
            std::scoped_lock lock(g_lock);
            pending = g_pending.size();
        }
        return ActorWorkMetrics{
            .requests = g_requests.load(std::memory_order_relaxed),
            .coalesced = g_coalesced.load(std::memory_order_relaxed),
            .waitingFor3D = g_waiting.load(std::memory_order_relaxed),
            .processed = g_processed.load(std::memory_order_relaxed),
            .changed = g_changed.load(std::memory_order_relaxed),
            .unchanged = g_unchanged.load(std::memory_order_relaxed),
            .totalProcessingMicros = g_processingMicros.load(std::memory_order_relaxed),
            .pending = pending,
            .maximumPending = g_maxPending.load(std::memory_order_relaxed)
        };
    }
}
