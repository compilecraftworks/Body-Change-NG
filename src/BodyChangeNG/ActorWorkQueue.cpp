#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AppearanceEventPolicy.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/SkinApplication.h"
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
    bool Schedule(RE::FormID id, PendingActor request, unsigned delay)
    {
        const auto queued = bcn::frame_tasks::Queue(id, [id, request]() mutable {
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
            if (bcn::frame_tasks::HasPreview(id)) {
                Schedule(id, request, 1);
                return; // One coalesced request resumes on confirm/cancel.
            }
            if (!actor->Is3DLoaded()) {
                // Bounded retry across actual input updates, never a same-FIFO
                // spin. Detach/new session cancels immediately. A later attach
                // can restart after expiry; saved desired choices are untouched.
                if (++request.retries <= 100) { Schedule(id, request, 6); return; }
            } else {
                (void)bcn::Distribution::Get().ApplyActor(actor.get());
                bcn::OutfitRefit::Get().ProcessActor(actor.get());
                // RaceMenu itself recreates standard overlay geometry. BCNG
                // only restores its four persisted exact-slot selections at
                // this actor-load boundary; no per-frame scan is used.
                bcn::overlay::QueueReapplySaved(actor.get());
                if (bcn::appearance::NeedsReconcile(
                        bcn::appearance::Feature::maleGenitalAddon,
                        bcn::appearance::Event::actor3DAttached)) {
                    bcn::skin_application::QueueReapplyCurrentMaleGenitals(actor.get(), true);
                }
                if (bcn::appearance::NeedsReconcile(
                        bcn::appearance::Feature::futanariAddon,
                        bcn::appearance::Event::actor3DAttached)) {
                    bcn::skin_application::QueueReapplyCurrentFutanari(actor.get(), true);
                }
            }
            std::scoped_lock lock(g_lock);
            const auto found = g_pending.find(id);
            if (found != g_pending.end() && found->second.revision == request.revision) g_pending.erase(found);
        }, delay, bcn::appearance::WorkChannel::actorReconcile,
            request.reason != bcn::ActorWorkReason::bulkLoad);
        if (!queued) {
            std::scoped_lock lock(g_lock);
            const auto found = g_pending.find(id);
            if (found != g_pending.end() && found->second.revision == request.revision) g_pending.erase(found);
        }
        return queued;
    }
}
namespace bcn
{
    ActorWorkQueue& ActorWorkQueue::Get() { static ActorWorkQueue queue; return queue; }
    bool ActorWorkQueue::Request(RE::Actor* actor, ActorWorkReason reason)
    {
        if (!frame_tasks::Active() || !actor || !actor->GetFormID() ||
            actor == RE::PlayerCharacter::GetSingleton()) return false;
        const auto id = actor->GetFormID();
        PendingActor request;
        {
            std::scoped_lock lock(g_lock);
            auto found = g_pending.find(id);
            if (found == g_pending.end() && g_pending.size() >= 4096) return false;
            if (found != g_pending.end()) {
                if (reason == ActorWorkReason::bulkLoad) reason = found->second.reason;
            }
            request = {actor->GetHandle(), reason, ++g_revision, ActorRegistry::Get().SessionGeneration(), 0};
            g_pending[id] = request;
        }
        return Schedule(id, request, 1);
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
}
