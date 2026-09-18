#include "BodyChangeNG/SessionSnapshot.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include <array>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

// Only the UI storage and persistence provider are stand-ins. The four reset
// helpers included below are extracted from the CURRENT production UI.cpp.
namespace bcn {
    struct DistributionRule { int id{}; };
    struct Distribution {
        static Distribution& Get() { static Distribution value; return value; }
        std::vector<DistributionRule> SavedRulesSnapshot() const { return saved; }
        std::vector<DistributionRule> saved{{42}};
    };
}
enum class DistributionPool { body, skin, futanari, overlay };
struct DistributionTargetSnapshot { std::vector<std::string> names; };
bcn::async_work::SessionSnapshot<DistributionTargetSnapshot> g_distributionTargets;
std::shared_ptr<const DistributionTargetSnapshot> g_distributionTargetOptions;
bool g_distributionEditorLoaded{}, g_distributionSelectionMode{}, g_distributionFemale{true};
bool g_showDistribution{};
DistributionPool g_distributionPool{};
std::size_t g_selectedDistributionRule{};
std::uint64_t g_distributionCatalogRevision{};
std::vector<bcn::DistributionRule> g_distributionRules;
std::unordered_set<std::string> g_distributionSelectedIds;
std::array<std::unordered_set<std::string>, 4> g_distributionSelectedOverlayIds;
std::array<bool, 4> g_overlayDistributionPreviewDirty{};
unsigned navigationResets{};
void ResetCatalogNavigation() { ++navigationResets; }
#include "DistributionLifecycleFunctions.inl"

namespace checkpoint {
    bcn::async_work::FrameTaskQueue queue;
    bool g_inPump{};
    std::uint64_t g_workEpoch{};
    bcn::async_work::FrameTaskQueue::Lease g_lease;
    bool ValidLease(const bcn::async_work::FrameTaskQueue::Lease& value)
    { return bcn::async_work::FrameTaskQueue::ValidLease(value); }
    bool IsCurrent(std::uint64_t epoch) { return queue.Active() && queue.Epoch() == epoch; }
    #include "MutationCheckpoint.inl"
}

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main() try {
    using Snapshot = bcn::async_work::SessionSnapshot<DistributionTargetSnapshot>;
    for (int pool{}; pool != 4; ++pool) for (bool female : {false, true}) {
        for (bool loaded : {false, true}) {
            g_distributionSelectionMode = true;
            g_distributionFemale = female;
            g_distributionPool = static_cast<DistributionPool>(pool);
            g_showDistribution = true;
            g_distributionEditorLoaded = loaded;
            g_distributionRules = {{999}};
            g_distributionSelectedIds = {"a", "b"};
            for (auto& ids : g_distributionSelectedOverlayIds) ids = {"overlay"};
            const auto revision = g_distributionCatalogRevision;
            DiscardDistributionDraft();
            Check(g_distributionRules.size() == (loaded ? 1U : 0U), "draft discard lost saved rules");
            if (loaded) Check(g_distributionRules[0].id == 42, "unsaved rule survived discard");
            ResetDistributionSelectionSession();
            ResetDistributionEditor();
            Check(!g_distributionSelectionMode && g_distributionFemale && !g_showDistribution &&
                g_distributionPool == DistributionPool::body, "selection mode/sex/pool leaked across close/load");
            Check(g_distributionCatalogRevision == revision + 1 && !g_distributionEditorLoaded &&
                g_distributionRules.empty() && g_selectedDistributionRule == 0, "editor reset failed");
            Check(g_distributionSelectedIds.empty(), "selected body/skin/futa candidates leaked");
            for (std::size_t area{}; area < 4; ++area) {
                Check(g_distributionSelectedOverlayIds[area].empty() &&
                    !g_overlayDistributionPreviewDirty[area], "overlay preview flags leaked");
            }
            Check(bcn::Distribution::Get().saved[0].id == 42, "reset modified persistent rules");
        }
    }
    Check(navigationResets == 16, "navigation not reset exactly once per boundary");

    // Cancellation after Pump's first admission check is observed by the
    // production mutation checkpoint. Already executing engine calls are not
    // simulated or claimed interruptible here.
    using namespace checkpoint;
    Check(CurrentWorkAllowed(), "direct non-queued caller was newly blocked");
    for (bool continuation : {false, true}) for (bool reset : {false, true}) {
        queue.Reset(true);
        g_workEpoch = queue.Epoch();
        queue.Submit(7, 1, []{});
        queue.Advance();
        auto job = queue.Take();
        Check(job.has_value(), "no initial actor job");
        g_lease = job->lease;
        if (continuation) {
            queue.Submit(0, 0, []{}, 1, true, false, g_lease);
            queue.Advance();
            auto next = queue.Take();
            Check(next && next->actor == 0 && next->lease == g_lease, "continuation lost actor lease");
        }
        g_inPump = true;
        Check(CurrentWorkAllowed(), "current actor work blocked");
        if (reset) queue.Reset(true);
        else queue.CancelActor(7);
        Check(!CurrentWorkAllowed(), "post-admission cancellation allowed mutation");
        g_inPump = false;
        g_lease.reset();
    }
    queue.Reset(true);
    g_workEpoch = queue.Epoch();
    g_inPump = true;
    Check(CurrentWorkAllowed(), "lease-free global task blocked");
    queue.Reset(true);
    Check(!CurrentWorkAllowed(), "old lease-free task bypassed epoch guard");
    g_inPump = false;

    Snapshot mailbox;
    auto old = mailbox.Begin();
    Check(old && !mailbox.Begin(), "duplicate per-frame collection admitted");
    mailbox.Reset();
    const auto fresh = mailbox.Begin();
    Check(fresh && !mailbox.Publish(*old, {{"old"}}), "obsolete publication accepted");
    mailbox.Fail(*old);
    Check(mailbox.Current(*fresh), "old failure cancelled fresh request");
    Check(mailbox.Publish(*fresh, {{"new"}}), "fresh result rejected");
    auto reader = mailbox.Read();
    std::weak_ptr<const DistributionTargetSnapshot> weak = reader;
    Check(reader && reader->names[0] == "new" && !mailbox.Begin(), "completed data rescanned each frame");
    mailbox.Reset();
    Check(!mailbox.Read() && !weak.expired() && reader->names[0] == "new", "reader invalidated by reset");
    reader.reset();
    Check(weak.expired(), "old metadata leaked after final reader released");
    const auto failed = mailbox.Begin();
    mailbox.Fail(*failed);
    Check(mailbox.Status() == Snapshot::State::failed && !mailbox.Begin(), "failure caused a per-frame retry loop");
    mailbox.Reset();
    Check(mailbox.Begin().has_value(), "reopen cannot retry failed request");

    // Deterministic late callbacks and simultaneous reset/publication. Both
    // orderings must release obsolete ownership and allow a fresh request.
    for (unsigned iteration{}; iteration < 400; ++iteration) {
        mailbox.Reset();
        const auto ticket = *mailbox.Begin();
        std::latch start{1};
        std::thread publisher([&] {
            start.wait();
            mailbox.Publish(ticket, {{"obsolete"}});
        });
        if (iteration % 2 == 0) mailbox.Reset();
        start.count_down();
        mailbox.Reset();
        publisher.join();
        Check(!mailbox.Read(), "reset raced with publication and retained an obsolete result");
        const auto next = mailbox.Begin();
        Check(next && mailbox.Publish(*next, {{"current"}}), "late callback blocked fresh collection");
    }
    std::cout << "DistributionLifecycleTests passed: 16 reset combinations, 5 cancellation checkpoints, snapshot ownership and 400 publication/reset races\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
