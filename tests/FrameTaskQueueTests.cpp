#include "BodyChangeNG/FrameTaskQueue.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using bcn::async_work::FrameTaskQueue;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    try {
        FrameTaskQueue queue;
        int result{};
        queue.Submit(1, 1, [&] { result = 1; });
        queue.Submit(1, 1, [&] { result = 2; });
        Check(queue.Pending() == 1 && !queue.Take(), "coalescing or initial boundary");
        queue.Advance();
        auto job = queue.Take();
        Check(job.has_value(), "ready job missing"); job->run();
        Check(result == 2, "old coalesced value applied");
        queue.Submit(1, 2, [] {});
        queue.Submit(2, 1, [] {});
        queue.Advance();
        auto other = queue.Take();
        Check(other && other->actor == 2 && !queue.Take(), "in-flight actor overlapped or blocked other actor");
        auto callbackLease = job->lease; job.reset();
        Check(FrameTaskQueue::ValidLease(callbackLease), "live lease rejected");
        queue.Advance(); Check(!queue.Take(), "async callback lease lost");
        callbackLease.reset(); queue.Advance();
        Check(!queue.Take(), "release observation treated as settled frame");
        queue.Advance(); Check(queue.Take().has_value(), "completed actor never resumed");
        queue.Reset(true);
        queue.Submit(0, 0, [&] { queue.Submit(0, 0, [&] { ++result; }); });
        queue.Advance(); job = queue.Take(); job->run();
        Check(!queue.Take(), "nested submission executed in same tick");
        queue.Advance(); Check(queue.Take().has_value(), "nested submission lost");
        const auto epoch = queue.Epoch(); queue.Reset(false);
        Check(queue.Epoch() != epoch && !queue.Submit(1, 1, [] {}) && !queue.Take(), "load barrier");
        queue.Reset(true);
        queue.Submit(3, 1, [] {}); queue.CancelActor(3); queue.Advance();
        Check(!queue.Take(), "detached actor retained");
        queue.Submit(4, 1, [] {}); queue.Submit(5, 1, [] {}, 1, true); queue.Advance();
        Check(queue.Take()->actor == 5, "visible actor priority");
        for (int i=0;i<60;++i) queue.Advance();
        queue.Submit(6, 1, [] {}, 1, true); queue.Advance();
        Check(queue.Take()->actor == 4, "bulk starvation");
        queue.Submit(7, 1, [] {}); queue.Advance(); job = queue.Take();
        // Actor 6 is still pending from the priority test; cancel whichever
        // actor was selected and verify that cancellation keeps its lease.
        queue.CancelActor(job->actor);
        Check(!FrameTaskQueue::ValidLease(job->lease), "active callback not invalidated");
        queue.Reset(true);
        for (unsigned i=1;i<=500;++i) for (int j=0;j<10;++j) queue.Submit(i, 1, [] {});
        Check(queue.Pending() == 500, "duplicate event growth");
        queue.Reset(true);
        queue.Submit(8, 200, [] {}, 1, true);
        queue.Submit(8, 201, [] {});
        queue.Submit(8, 200, [] {}, 1, true);
        queue.Advance();
        Check(queue.Take()->channel == 201, "new preview overtook earlier commit");
        queue.Reset(true);
        queue.Submit(1, 100, [] {}, 1, true);
        queue.Submit(2, 201, [] {}, 1, true);
        queue.Advance();
        Check(queue.Take()->actor == 2, "visible native application delayed behind unrelated planning");
        queue.Reset(true);
        queue.Submit(10, 100, [] {});
        for (unsigned n=0; n<60; ++n) queue.Advance();
        // Simulate an expensive native call using an entire pump. Direct input
        // wins three reserved opportunities; aged bulk still gets a turn.
        for (unsigned n=0; n<3; ++n) {
            queue.Submit(20+n, 200, [] {}, 1, true, true);
            queue.Advance();
            auto input = queue.Take(true);
            Check(input && input->actor == 20+n && FrameTaskQueue::InteractiveLease(input->lease),
                "aged bulk stole reserved input opportunity");
        }
        queue.Submit(23, 200, [] {}, 1, true, true); queue.Advance();
        Check(queue.Take(true)->actor == 10, "continuous input starved aged bulk");
        Check(queue.Take()->actor == 23, "input lost after fairness opportunity");

        queue.Reset(true);
        queue.Submit(30, 201, [] {});
        queue.Submit(31, 100, [] {});
        queue.Submit(30, 200, [] {}, 1, true, true);
        queue.Advance(); job = queue.Take(true);
        Check(job && job->actor == 30 && job->channel == 201 && job->interactive,
            "reserved input overtook its own commit or failed to promote prerequisite");
        Check(queue.Take(true)->actor == 31 && !queue.Take(true), "input bypassed live actor lease");
        job.reset(); queue.Advance(); queue.Advance();
        Check(queue.Take(true)->channel == 200, "latest preview lost after earlier commit");

        queue.Reset(true);
        queue.Submit(40, 100, [] {}, 6);
        queue.Submit(40, 204, [] {}, 1, true, true);
        queue.Submit(41, 204, [] {}, 1, true, true);
        queue.Advance();
        Check(queue.Take(true)->actor == 41 && !queue.Take(true), "direct input bypassed same-actor due boundary");

        queue.Reset(true);
        queue.Submit(42, 204, [] {}); queue.Advance(); job = queue.Take();
        auto automaticLease = job->lease;
        Check(!FrameTaskQueue::InteractiveLease(automaticLease), "automatic skin incorrectly started interactive");
        Check(!queue.Submit(42, 204, [] {}, 1, true, false, automaticLease),
            "continuation allowed to reacquire its own busy actor");
        queue.Submit(0, 0, [] {}, 1, true, false, automaticLease);
        job.reset(); queue.Submit(43, 100, [] {});
        for (unsigned n=0; n<60; ++n) queue.Advance();
        queue.Submit(42, 201, [] {}, 1, true, true); queue.Advance();
        Check(FrameTaskQueue::InteractiveLease(automaticLease), "direct selection failed to promote running skin");
        job = queue.Take(true);
        Check(job && job->actor == 0 && job->lease == automaticLease && job->interactive,
            "queued skin continuation did not inherit later input priority");
        Check(queue.Take()->actor == 43 && !queue.Take(), "new selection overlapped promoted continuation");
        job.reset(); automaticLease.reset(); queue.Advance(); queue.Advance();
        Check(queue.Take(true)->channel == 201, "new selection lost after promoted skin finished");

        queue.Reset(true);
        queue.Submit(50, 204, [] {});
        for (int i=0; i<100; ++i) queue.Submit(50, 204, [&] { result = 77; }, 1, true, true);
        Check(queue.Pending() == 1 && queue.Status(50).queued && !queue.Status(50).busy,
            "latest-choice coalescing or queued status");
        Check(queue.Status(50, FrameTaskQueue::Clock::now() + std::chrono::seconds(2)).Delayed(),
            "long pending wait not exposed");
        queue.Advance(); job = queue.Take(true); job->run();
        Check(result == 77 && job->interactive && !queue.Status(50).queued && queue.Status(50).busy,
            "coalesced result or active status");
        // The VM retains its callback object, but a completed callback must
        // not keep the actor busy. Its real continuation DOES keep it busy.
        bcn::async_work::CompletionPayload<FrameTaskQueue::Lease> retainedCallback(job->lease);
        auto continuation = retainedCallback.Take(); job.reset();
        queue.Submit(50, 201, [] {}, 1, true, true); queue.Advance();
        Check(!queue.Take(true), "callback payload transfer released unfinished continuation");
        Check(!retainedCallback.Take(), "retained callback invoked twice");
        continuation.reset(); queue.Advance();
        Check(!queue.Take(true), "completion transfer removed quiet boundary");
        queue.Advance();
        Check(queue.Take(true).has_value(), "completed-but-retained callback blocked next choice");
        queue.Advance(); queue.Advance();
        Check(!queue.Status(50).busy && !queue.Status(50).queued, "completed work left stale UI status");

        // Multiple outstanding native callbacks: a newer selection may
        // supersede follow-ups, but cannot overlap any unfinished native call.
        queue.Reset(true);
        queue.Submit(55, 204, [] {}, 1, true, true); queue.Advance(); job = queue.Take(true);
        using NativeCallback = bcn::async_work::CompletionPayload<FrameTaskQueue::Lease>;
        std::vector<std::unique_ptr<NativeCallback>> nativeCallbacks;
        for (unsigned n=0; n<3; ++n) nativeCallbacks.push_back(std::make_unique<NativeCallback>(job->lease));
        job.reset();
        for (int n=0; n<100; ++n) queue.Submit(55, 204, [&, n] { result = n; }, 1, true, true);
        for (unsigned n=0; n<2; ++n) {
            auto finishedNative = nativeCallbacks[n]->Take(); finishedNative.reset();
            queue.Advance(); queue.Advance();
            Check(!queue.Take(true), "new skin overlapped remaining native callbacks");
        }
        auto lastNative = nativeCallbacks.back()->Take(); lastNative.reset();
        queue.Advance(); queue.Advance(); job = queue.Take(true);
        Check(job.has_value(), "retained completed native callbacks blocked latest skin"); job->run();
        Check(result == 99 && queue.Pending() == 0, "intermediate skin replayed or final skin lost");

        queue.Reset(true);
        queue.Submit(56, 204, [] {}, 1, true, true); queue.Advance(); job = queue.Take(true);
        NativeCallback cancelled(job->lease); job.reset();
        queue.CancelActor(56);
        auto cancelledResult = cancelled.Take();
        Check(cancelledResult && !FrameTaskQueue::ValidLease(*cancelledResult), "cancelled callback lost invalidation at transfer");
        cancelledResult.reset(); queue.Advance(); queue.Advance();
        Check(!queue.HasActorWork(56), "cancelled completed callback retained actor");

        queue.Reset(true);
        queue.Submit(60, 204, [] {}, 1, true, true); queue.Advance(); job = queue.Take(true);
        auto detachedLease = job->lease; job.reset(); queue.CancelActor(60);
        Check(!FrameTaskQueue::ValidLease(detachedLease) && queue.Status(60).busy,
            "detach incorrectly reported unfinished callback complete");
        queue.Reset(false);
        Check(!queue.Status(60).busy && !queue.Status(60).queued && !FrameTaskQueue::ValidLease(detachedLease),
            "session reset leaked status or revived callback");
        FrameTaskQueue::WorkStatus threshold{true, false, 1499};
        Check(!threshold.Delayed(), "delay indicator threshold too early");
        threshold.elapsedMs = 1500;
        Check(threshold.Delayed(), "delay indicator threshold missing");
        std::cout << "FrameTaskQueueTests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
