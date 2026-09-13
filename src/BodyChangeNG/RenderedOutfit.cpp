#include "BodyChangeNG/RenderedOutfit.h"
#ifndef BCNG_RENDERED_OUTFIT_TEST
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#endif
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace bcn::rendered_outfit
{
    namespace
    {
        std::atomic<abi::Query> g_query{};
        bool g_listening{};
        struct Tracked final
        {
            std::optional<Stamp> planned, notified;
            unsigned warmupRetries{8U};
        };
        std::mutex g_lock;
        std::unordered_map<std::uint32_t, Tracked> g_tracked;
        // SKSE game-task only. No cached engine pointers or GPU resources.
        Reader g_reader;

        void OnChange(SKSE::MessagingInterface::Message* message)
        {
            if (!message || message->type != abi::kChangedMessage) return;
            const auto event = DecodeChange(message->data, message->dataLen);
            if (!event) return;
            if (event->actorFormID == 0U && (event->reasons & abi::EpochChanged)) {
                std::vector<std::uint32_t> ids;
                {
                    std::scoped_lock lock(g_lock);
                    for (auto& [id, tracked] : g_tracked) {
                        ids.push_back(id);
                        tracked = {};
                    }
                }
                // Only previously enrolled actors; never a world scan.
                for (const auto id : ids) Request(RE::TESForm::LookupByID<RE::Actor>(id));
                return;
            }
            if (!event->actorFormID) return;
            {
                std::scoped_lock lock(g_lock);
                const auto found = g_tracked.find(event->actorFormID);
                if (found == g_tracked.end()) return;
                if (event->status == abi::Status::InvalidActor) {
                    g_tracked.erase(found);
                    return;
                }
                const auto stamp = VersionOf(*event);
                if (found->second.notified == stamp) return;
                found->second.notified = stamp;
            }
            // Includes same-outfit SceneChanged. Re-evaluate but do NOT erase
            // the applied morph signature: that would rebuild on our own
            // morph notification forever. SFS/RaceMenu morph new attachments.
            Request(RE::TESForm::LookupByID<RE::Actor>(event->actorFormID));
        }
    }

    void Initialize()
    {
        if (Available()) return;
        const auto module = ::GetModuleHandleW(L"SFSCore.dll");
        if (!module) return;
        const auto version = reinterpret_cast<abi::GetVersion>(::GetProcAddress(module, abi::kVersionExport));
        const auto query = ResolveQuery([module](const char* name) {
            return reinterpret_cast<abi::Query>(::GetProcAddress(module, name));
        });
        if (!version || !query || version() != abi::kVersion) return;
        if (auto* messaging = SKSE::GetMessagingInterface(); messaging && !g_listening) {
            g_listening = messaging->RegisterListener(abi::kSender, OnChange);
        }
        // A query without change notifications could remain NotReady forever.
        if (g_listening) g_query.store(query, std::memory_order_release);
    }
    bool Available() { return g_query.load(std::memory_order_acquire) != nullptr; }
    void Reset()
    {
        std::scoped_lock lock(g_lock);
        decltype(g_tracked){}.swap(g_tracked);
    }
    void Forget(std::uint32_t actor)
    {
        std::scoped_lock lock(g_lock);
        g_tracked.erase(actor);
    }
    void Request(RE::Actor* actor)
    {
        if (!Available() || !actor || !frame_tasks::Active()) return;
        if (!Settings::Get().OutfitCorrectionEnabled() && !racemenu::HasOutfitCorrection(actor)) return;
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actor->GetFormID(), [handle] {
            if (const auto actor = handle.get()) OutfitRefit::Get().ProcessActor(actor.get());
        }, 1U, appearance::WorkChannel::renderedOutfitReconcile);
    }
    View Read(RE::Actor* actor)
    {
        if (!actor) return {Route::invalidActor};
        if (!Available()) return {Route::worn};
        if (!frame_tasks::InGameTask()) return {};
        const auto id = actor->GetFormID();
        const auto result = g_reader.Read(g_query.load(std::memory_order_acquire), id);
        bool retry{};
        {
            std::scoped_lock lock(g_lock);
            if (result.route == Route::invalidActor) g_tracked.erase(id);
            else {
                auto& tracked = g_tracked[id];
                tracked.planned = (result.route == Route::worn || result.route == Route::rendered) ?
                    std::optional{VersionOf(result.snapshot)} : std::nullopt;
                // The provider may not have established its game-task thread
                // on the very first load. That pre-subscription case emits no
                // actor notification. Only this startup case gets bounded retries.
                retry = result.route == Route::defer && result.snapshot.epoch == 0U &&
                    result.snapshot.status == abi::Status::NotReady && tracked.warmupRetries > 0U;
                if (retry) --tracked.warmupRetries;
            }
        }
        if (retry) Request(actor);
        return result;
    }
    bool ValidateApply(RE::Actor* actor)
    {
        if (!Available() || !Settings::Get().OutfitCorrectionEnabled()) return true;
        if (!actor || !frame_tasks::InGameTask()) return false;
        if (racemenu::HasActivePreview(actor)) return false;
        std::optional<Stamp> planned;
        {
            std::scoped_lock lock(g_lock);
            if (const auto found = g_tracked.find(actor->GetFormID()); found != g_tracked.end())
                planned = found->second.planned;
        }
        const auto latest = g_reader.Read(g_query.load(std::memory_order_acquire), actor->GetFormID());
        if (CanApply(planned, latest)) return true;
        // An outfit changed while this morph job waited. Never apply the old
        // mapping to new/hidden armor. Replan actor-locally, not inside SFS.
        Request(actor);
        return false;
    }
}
