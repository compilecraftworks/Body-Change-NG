#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include "BodyChangeNG/Settings.h"
#include <RE/R/RaceSexMenu.h>
#include <chrono>
#include <mutex>

namespace
{
    bcn::async_work::FrameTaskQueue g_queue;
    std::mutex g_lock;
    bool g_scheduled{};
    bool g_available{};
    thread_local bcn::frame_tasks::Lease g_lease;
    thread_local bool g_inPump{}, g_urgent{}, g_interactive{};

    void Pump(std::uint64_t epoch)
    {
        const auto performance = bcn::Settings::Get().Snapshot().performanceMode;
        const auto started = std::chrono::steady_clock::now();
        const auto budget = std::chrono::microseconds(performance ? 1000 : 2000);
        // The budget includes actual native calls, not only distribution
        // planning. An indivisible RaceMenu call cannot be preempted.
        unsigned actorJobs{};
        for (unsigned count{}; count < 64U && actorJobs < bcn::AutomaticActorBudget(performance); ++count) {
            std::optional<bcn::async_work::FrameTaskQueue::Job> job;
            {
                std::scoped_lock lock(g_lock);
                if (epoch != g_queue.Epoch()) return;
                job = g_queue.Take(count == 0);
            }
            if (!job) break;
            if (job->actor) ++actorJobs;
            // RaceMenu owns the player's rebuilding geometry. Its close
            // handler restores desired selections; never mutate it mid-edit.
            if (const auto* player = RE::PlayerCharacter::GetSingleton();
                player && job->actor == player->GetFormID()) {
                auto* ui = RE::UI::GetSingleton();
                if (ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) continue;
            }
            auto previous = std::exchange(g_lease, job->lease);
            const auto previousPump = std::exchange(g_inPump, true);
            const auto previousUrgent = std::exchange(g_urgent, job->urgent);
            const auto previousInteractive = std::exchange(g_interactive, job->interactive);
            try { job->run(); }
            catch (const std::exception& error) { SKSE::log::error("BCNG queued work failed: {}", error.what()); }
            catch (...) { SKSE::log::error("BCNG queued work failed with an unknown C++ exception"); }
            g_lease = std::move(previous);
            g_inPump = previousPump;
            g_urgent = previousUrgent;
            g_interactive = previousInteractive;
            if (std::chrono::steady_clock::now() - started >= budget) break;
        }
        std::scoped_lock lock(g_lock);
        if (epoch == g_queue.Epoch()) g_scheduled = false;
        // Never enqueue Pump from Pump: SKSE drains a live FIFO. Only the
        // next external input update may schedule another processing batch.
    }
}

namespace bcn::frame_tasks
{
    bool Queue(std::uint32_t actor, std::function<void()> work, std::uint32_t delay,
        std::uint32_t channel, bool urgent, bool interactive)
    {
        std::scoped_lock lock(g_lock);
        if (!g_available) return false;
        return g_queue.Submit(actor, channel, std::move(work), delay,
            urgent || g_urgent || (!g_inPump && channel >= 200),
            interactive || g_interactive || (!g_inPump && channel >= 200));
    }
    bool Continue(Lease lease, std::function<void()> work, std::uint32_t delay)
    {
        std::scoped_lock lock(g_lock);
        if (!g_available || !async_work::FrameTaskQueue::ValidLease(lease)) return false;
        const auto interactive = async_work::FrameTaskQueue::InteractiveLease(lease);
        // A continuation owns the ORIGINAL lease, never reacquires its actor.
        // Keeping it on the job also lets a later user request promote already
        // queued callbacks of an automatic skin update. Pump restores TLS even
        // when the continuation throws.
        return g_queue.Submit(0, 0, std::move(work), delay, true, interactive, std::move(lease));
    }
    Lease CurrentLease() { return g_lease; }
    bool ValidLease(const Lease& lease) { return async_work::FrameTaskQueue::ValidLease(lease); }
    void SetAvailable(bool available) { std::scoped_lock lock(g_lock); g_available = available; }
    void OnInputTick()
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        std::uint64_t epoch;
        {
            std::scoped_lock lock(g_lock);
            g_queue.Advance();
            if (!g_queue.Active() || g_scheduled || g_queue.Pending() == 0) return;
            epoch = g_queue.Epoch();
            g_scheduled = true;
        }
        // Input handling itself does not touch actor geometry or pause state.
        tasks->AddTask([epoch] { Pump(epoch); });
    }
    void Reset(bool active)
    {
        std::scoped_lock lock(g_lock);
        g_queue.Reset(active);
        g_scheduled = false;
    }
    bool Active() { std::scoped_lock lock(g_lock); return g_available && g_queue.Active(); }
    std::uint64_t Epoch() { std::scoped_lock lock(g_lock); return g_queue.Epoch(); }
    bool IsCurrent(std::uint64_t epoch)
    {
        std::scoped_lock lock(g_lock);
        return g_queue.Active() && epoch == g_queue.Epoch();
    }
    void CancelActor(std::uint32_t actor) { std::scoped_lock lock(g_lock); g_queue.CancelActor(actor); }
    bool HasActorWork(std::uint32_t actor) { std::scoped_lock lock(g_lock); return g_queue.HasActorWork(actor); }
    async_work::FrameTaskQueue::WorkStatus Status(std::uint32_t actor)
    {
        std::scoped_lock lock(g_lock);
        return g_queue.Status(actor);
    }
}
