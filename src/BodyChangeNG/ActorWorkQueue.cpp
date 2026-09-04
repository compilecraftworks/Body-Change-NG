#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/OutfitRefit.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace
{
    struct PendingActor {
        RE::ActorHandle handle;
        bcn::ActorWorkReason reason{};
        std::uint64_t revision{}, session{};
        unsigned retries{};
    };
    std::mutex g_lock;
    std::unordered_map<RE::FormID, PendingActor> g_pending;
    std::uint64_t g_revision{};
    std::atomic_uint64_t g_requests{}, g_coalesced{}, g_waiting{}, g_processed{}, g_changed{}, g_unchanged{}, g_processingMicros{};
    std::atomic_size_t g_maxPending{};
    void Schedule(RE::FormID id, PendingActor request, unsigned delay)
    {
        bcn::frame_tasks::Queue(id, [id, request]() mutable {
            {
                std::scoped_lock lock(g_lock);
                const auto found = g_pending.find(id);
                if (found == g_pending.end() || found->second.revision != request.revision) return;
            }
            const auto actor = request.handle.get();
            if (bcn::ActorRegistry::Get().SessionGeneration() != request.session || !actor ||
                actor->GetFormID() != id) {
                std::scoped_lock lock(g_lock);
                const auto found = g_pending.find(id);
                if (found != g_pending.end() && found->second.revision == request.revision) g_pending.erase(found);
                return;
            }
            if (!actor->Is3DLoaded()) {
                ++g_waiting;
                // Bounded retry across actual input updates, never a same-FIFO
                // spin. Detach/new session cancels immediately. A later attach
                // can restart after expiry; saved desired choices are untouched.
                if (++request.retries <= 100) { Schedule(id, request, 6); return; }
            } else {
                const auto started = std::chrono::steady_clock::now();
                const auto changed = bcn::Distribution::Get().ApplyActor(actor.get());
                bcn::OutfitRefit::Get().ProcessActor(actor.get());
                g_processingMicros += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started).count());
                ++g_processed;
                ++(changed ? g_changed : g_unchanged);
            }
            std::scoped_lock lock(g_lock);
            const auto found = g_pending.find(id);
            if (found != g_pending.end() && found->second.revision == request.revision) g_pending.erase(found);
        }, delay, 100, request.reason != bcn::ActorWorkReason::bulkLoad);
    }
}
namespace bcn
{
    ActorWorkQueue& ActorWorkQueue::Get() { static ActorWorkQueue queue; return queue; }
    bool ActorWorkQueue::Request(RE::Actor* actor, ActorWorkReason reason)
    {
        if (!frame_tasks::Active() || !actor || !actor->GetFormID() ||
            actor == RE::PlayerCharacter::GetSingleton()) return false;
        ++g_requests;
        const auto id = actor->GetFormID();
        PendingActor request;
        {
            std::scoped_lock lock(g_lock);
            auto found = g_pending.find(id);
            if (found == g_pending.end() && g_pending.size() >= 4096) return false;
            if (found != g_pending.end()) {
                ++g_coalesced;
                if (reason == ActorWorkReason::bulkLoad) reason = found->second.reason;
            }
            request = {actor->GetHandle(), reason, ++g_revision, ActorRegistry::Get().SessionGeneration(), 0};
            g_pending[id] = request;
            g_maxPending.store(std::max(g_maxPending.load(), g_pending.size()));
        }
        Schedule(id, request, 1);
        return true;
    }
    void ActorWorkQueue::NotifyDetached(std::uint32_t id)
    {
        { std::scoped_lock lock(g_lock); g_pending.erase(id); }
        frame_tasks::CancelActor(id);
    }
    void ActorWorkQueue::ResetSession()
    {
        std::scoped_lock lock(g_lock);
        g_pending.clear();
        ++g_revision;
    }
    ActorWorkMetrics ActorWorkQueue::Metrics() const
    {
        std::scoped_lock lock(g_lock);
        return {g_requests.load(), g_coalesced.load(), g_waiting.load(), g_processed.load(),
            g_changed.load(), g_unchanged.load(), g_processingMicros.load(), g_pending.size(), g_maxPending.load()};
    }
}
