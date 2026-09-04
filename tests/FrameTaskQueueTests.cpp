#include "BodyChangeNG/FrameTaskQueue.h"
#include <iostream>
#include <stdexcept>
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
        std::cout << "FrameTaskQueueTests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
