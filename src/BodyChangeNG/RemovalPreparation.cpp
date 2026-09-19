#include "BodyChangeNG/RemovalPreparation.h"
#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorSettingsReset.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/FaceSkinOverrides.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/NativeAddonSkinBackend.h"
#include "BodyChangeNG/NativeSkinBackend.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/Settings.h"

#include <chrono>
#include <mutex>
#include <unordered_set>

namespace
{
    std::mutex g_mutex;
    bcn::removal::Status g_status;
    std::uint64_t g_revision{};
    using Clock = std::chrono::steady_clock;

    void Publish(bcn::removal::Status status, std::uint64_t revision)
    {
        std::scoped_lock lock(g_mutex);
        if (revision == g_revision) g_status = status;
    }

    void Verify(std::uint64_t epoch, std::uint64_t revision, Clock::time_point deadline) try
    {
        using namespace bcn;
        if (!frame_tasks::IsCurrent(epoch) || !Settings::Get().RemovalMode()) return;
        { std::scoped_lock lock(g_mutex); if (revision != g_revision) return; }
        removal::Status status{ .phase = removal::Phase::running, .epoch = epoch };
        // Includes asynchronous actor leases, not just queued callbacks. This
        // monitor uses actor 0 and has no lease of its own.
        if (frame_tasks::HasWork() || face_skin::HasActiveWork()) {
            if (Clock::now() < deadline && frame_tasks::Queue(0,
                    [=] { Verify(epoch, revision, deadline); }, 15U)) return;
            status.phase = removal::Phase::incomplete;
            Publish(status, revision);
            return;
        }
        std::unordered_set<RE::FormID> pending;
        for (const auto& state : ActorRegistry::Get().SnapshotAll()) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(state.actorFormID);
            if (!actor || std::ranges::any_of(state.overlay.areas, [](const auto& area) {
                    return !area.items.empty();
                }) || native_addon::HasSelection(state.actorFormID, native_addon::Channel::maleGenitals) ||
                native_addon::HasSelection(state.actorFormID, native_addon::Channel::futanari)) {
                pending.insert(state.actorFormID);
            }
        }
        for (const auto& baseline : face_skin::SnapshotBaselines()) {
            if (baseline.touched) { pending.insert(baseline.actor); status.facePending = true; }
        }
        status.pendingActors = pending.size();
        const auto morphs = racemenu::RemainingOwnedMorphActors();
        status.morphsPending = !morphs || *morphs != 0U;
        status.nativeSkinPending = !native_skin::PrivateTexturesRestored();
        status.tintPending = !player_tint::OriginalStateRestored();
        if (!removal::CleanupComplete(status)) {
            status.phase = removal::Phase::incomplete;
        } else {
            // Only discard restoration evidence AFTER each backend confirms
            // cleanup. A failed/unloaded face must retain its co-save baseline.
            ActorRegistry::Get().Revert();
            player_tint::ResetPersistedState();
            status.phase = removal::Phase::ready;
        }
        Publish(status, revision);
    }
    catch (...) {
        Publish({ .phase = bcn::removal::Phase::failed, .epoch = epoch }, revision);
    }
}

namespace bcn::removal
{
    Status Snapshot()
    {
        const auto epoch = frame_tasks::Epoch();
        std::scoped_lock lock(g_mutex);
        return g_status.epoch == epoch ? g_status : Status{};
    }

    bool Begin()
    {
        if (!frame_tasks::Active() || !SKSE::GetTaskInterface() || !racemenu::IsReady() ||
            runtime::ResolveGameBranch(REL::Module::get().version()) == runtime::GameBranch::unsupported) return false;
        const auto previous = Snapshot();
        if (previous.phase == Phase::queued || previous.phase == Phase::running) return false;
        const auto epoch = frame_tasks::Epoch();
        auto settings = Settings::Get().Snapshot();
        const auto wasRemoval = settings.removalMode;
        settings.removalMode = true;
        Settings::Get().Update(settings);
        if (!Settings::Get().Save()) {
            settings.removalMode = wasRemoval;
            Settings::Get().Update(settings);
            return false;
        }
        std::uint64_t revision;
        { std::scoped_lock lock(g_mutex); revision = ++g_revision; g_status = { .phase = Phase::queued, .epoch = epoch }; }
        const auto queued = frame_tasks::Queue(0, [epoch, revision] {
            if (!frame_tasks::IsCurrent(epoch)) return;
            try {
                ActorWorkQueue::Get().ResetSession();
                frame_tasks::SetPreviewActor(0);
                const auto result = actor_settings_reset::QueueAll(true);
                Publish({ .phase = result.accepted ? Phase::running : Phase::failed, .epoch = epoch }, revision);
                if (result.accepted && !frame_tasks::Queue(0,
                        [=] { Verify(epoch, revision, Clock::now() + std::chrono::minutes(2)); }, 15U)) {
                    Publish({ .phase = Phase::failed, .epoch = epoch }, revision);
                }
            } catch (...) {
                // Keep suspension and restoration records for a retry. Never
                // leave a failed startup looking like an ongoing cleanup.
                Publish({ .phase = Phase::failed, .epoch = epoch }, revision);
            }
        });
        if (!queued) Publish({ .phase = Phase::failed, .epoch = epoch }, revision);
        return queued;
    }

    bool Resume()
    {
        const auto state = Snapshot();
        if (state.phase == Phase::queued || state.phase == Phase::running ||
            frame_tasks::HasWork() || face_skin::HasActiveWork()) return false;
        auto settings = Settings::Get().Snapshot();
        settings.removalMode = false;
        Settings::Get().Update(settings);
        if (!Settings::Get().Save()) {
            settings.removalMode = true;
            Settings::Get().Update(settings);
            return false;
        }
        std::scoped_lock lock(g_mutex);
        ++g_revision;
        g_status = {};
        return true;
    }
}
