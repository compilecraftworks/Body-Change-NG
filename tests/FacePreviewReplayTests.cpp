// Production observer/scheduler bodies, with only engine/queue boundaries
// replaced. These are ordering/lifetime tests, not rendered Skyrim tests.
#include "BodyChangeNG/FacePreviewWatch.h"
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using namespace bcn::face_skin;
using bcn::skin_transaction::Mode;

namespace RE {
    struct Base { unsigned id{7}; unsigned GetFormID() const { return id; } };
    struct Actor {
        Base base;
        bool loaded{true};
        Base* GetActorBase() { return &base; }
        bool Is3DLoaded() const { return loaded; }
    };
    struct ActorHandle { std::shared_ptr<Actor> value; auto get() const { return value; } };
}
namespace SKSE::log { template<class... T> void warn(T&&...) {} }
namespace bcn::face_preview {
    template<class... T> bool Publish(T&&...) { return true; }
    bool Restore(unsigned) { return true; }
}
namespace bcn::appearance { enum class WorkChannel { none }; }
namespace bcn::frame_tasks {
    bool active{true}, pending{}, accept{true};
    unsigned owner{1};
    std::vector<std::function<void()>> jobs;
    bool Active() { return active; }
    bool HasPreview(unsigned actor) { return actor == owner; }
    bool HasActorWork(unsigned) { return pending; }
    bool Queue(unsigned actor, std::function<void()> job, unsigned delay,
        appearance::WorkChannel, bool, bool) {
        if (!active || !accept) return false;
        if (actor != 0 || delay != 2) throw std::runtime_error("observer acquired an actor mutation lease or lost pacing");
        jobs.push_back(std::move(job));
        return true;
    }
    void Tick() {
        auto due = std::move(jobs); jobs.clear();
        for (auto& job : due) job();
        if (jobs.size() > 1) throw std::runtime_error("observation queue grew");
    }
}
namespace {
    struct Request {
        RE::ActorHandle handle;
        std::uint64_t generation{1};
        bool complete{true}, running{};
        RebuildGate rebuild;
        Mode mode{Mode::preview};
    };
    std::unordered_map<unsigned, Request> g_requests;
    std::mutex g_mutex;
    std::uint64_t g_epoch{1};
    #include "preview_state.inc"
    struct Texture { std::string name; bool rendererTexture{true}; };
    std::array<Texture, kChannels.size()> live;
    Paths property, saved;
    Paths desired;
    Baseline testBaseline{.actor = 1, .base = 7, .node = "Face", .female = true};
    bool directAccess{true}, readOK{true};
    std::string node{"Face"};
    RebuildObservation engine{RebuildObservation::ready};
    unsigned writes{}, pumps{}, reads{};
    bool pumpSuccess{true};
    struct NodeAccess {
        static NodeAccess Connect() { return {}; }
        explicit operator bool() const { return directAccess; }
        bool Read(RE::Actor*, bool, const std::string&, unsigned channel, bool persistent, std::string& value) const {
            if (persistent) throw std::runtime_error("observer accessed persistent registry");
            ++reads;
            for (unsigned i{}; i < kChannels.size(); ++i) if (channel == kChannels[i]) value = property[i];
            return readOK;
        }
    };
    Texture* VisibleTexture(RE::Actor*, const std::string&, unsigned channel) {
        for (unsigned i{}; i < kChannels.size(); ++i) if (kChannels[i] == channel) return &live[i];
        return nullptr;
    }
    RebuildObservation ReadRebuildState(RE::Actor*, bool, bool) { return engine; }
    std::string ResolveNodeName(RE::Actor*) { return node; }
    void Pump(unsigned);
    #include "preview_observer.inc"
    void Pump(unsigned id) {
        ++pumps;
        auto& request = g_requests.at(id);
        if (!pumpSuccess) return; // production failure handler owns rollback, not this observer
        for (unsigned i{}; i < kChannels.size(); ++i) {
            if (desired[i].empty() || CanKeepVisibleTexture(desired[i], property[i], live[i].name,
                    live[i].rendererTexture, false, {})) continue;
            ++writes;
            property[i] = live[i].name = desired[i];
            live[i].rendererTexture = true;
        }
        request.complete = true;
        ArmPreviewObservation(id, g_epoch, request.generation, testBaseline, desired);
    }
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void Reset() {
        bcn::frame_tasks::jobs.clear();
        bcn::frame_tasks::owner = 1;
        bcn::frame_tasks::active = bcn::frame_tasks::accept = true;
        bcn::frame_tasks::pending = false;
        g_preview = {}; g_requests.clear(); g_epoch = 1;
        g_requests[1].handle.value = std::make_shared<RE::Actor>();
        directAccess = readOK = pumpSuccess = true; node = "Face";
        engine = RebuildObservation::ready; writes = pumps = reads = 0;
        for (unsigned i{}; i < kChannels.size(); ++i) {
            desired[i] = "textures/preview/face" + std::to_string(i) + ".dds";
            saved[i] = "textures/committed/face" + std::to_string(i) + ".dds";
            property[i] = live[i].name = desired[i]; live[i].rendererTexture = true;
        }
        ArmPreviewObservation(1, 1, 1, testBaseline, desired);
    }
    void Replay(unsigned i) { property[i] = live[i].name = saved[i]; }
}
int main() try {
    using namespace bcn::frame_tasks;
    for (bool native : {true, false}) {
        Reset(); directAccess = native;
        const auto originalSaved = saved;
        // Arbitrarily delayed Papyrus replay: do not assume a 1-second window.
        for (unsigned i{}; i < 10000; ++i) Tick();
        Check(pumps == 0 && writes == 0 && jobs.size() == 1, "unchanged preview reloaded or lost its observation");
        for (unsigned i{}; i < kChannels.size(); ++i) {
            Replay(i); Tick();
            Check(Owns(live[i].name, desired[i]), "late single-channel RaceMenu replay remained on face");
        }
        Check(pumps == 5 && writes == 5 && saved == originalSaved, "repair touched matching channels or committed keys");
        for (unsigned i{}; i < 1000; ++i) Tick();
        Check(pumps == 5, "successful repair repeated itself");
    }
    Reset(); property[0] = saved[0]; Tick();
    Check(pumps == 1, "stale TXST with a still-correct texture was missed");
    Reset(); live[1].rendererTexture = false; Tick();
    Check(pumps == 1 && live[1].rendererTexture, "renderer loss not repaired");
    Reset(); desired = {}; g_preview = {}; jobs.clear();
    desired[0] = live[0].name = property[0] = "textures/original/face.dds";
    ArmPreviewObservation(1, 1, 1, testBaseline, desired);
    Replay(0); Tick();
    Check(pumps == 1 && writes == 1 && live[0].name == desired[0], "Default preview did not retain original face");

    for (unsigned boundary{}; boundary < 12; ++boundary) {
        Reset(); Replay(0);
        if (boundary == 0) owner = 0; // close/cancel
        if (boundary == 1) owner = 2; // actor switch
        if (boundary == 2) ++g_epoch; // load/new game
        if (boundary == 3) g_requests.clear(); // detach/Forget
        if (boundary == 4) ++g_requests[1].generation; // newer selection
        if (boundary == 5) g_requests[1].mode = Mode::commit;
        if (boundary == 6) g_requests[1].mode = Mode::restore;
        if (boundary == 7) g_requests[1].handle.value->loaded = false;
        if (boundary == 8) ++g_requests[1].handle.value->base.id;
        if (boundary == 9) node = "DifferentHead";
        if (boundary == 10) active = false;
        if (boundary == 11) readOK = false;
        Tick();
        Check(pumps == 0 && jobs.empty(), "stale/unsafe observer mutated face or stayed queued");
    }
    for (bool busy : {true, false}) {
        Reset(); Replay(0);
        if (busy) pending = true; else engine = RebuildObservation::waiting;
        for (unsigned i{}; i < 100; ++i) Tick();
        Check(pumps == 0 && reads == 0 && jobs.size() == 1, "observer overlapped an appearance mutation");
        pending = false; engine = RebuildObservation::ready; Tick();
        Check(pumps == 1, "preview did not resume when safe");
    }
    Reset();
    // Old queued callback cannot consume the new selection's queued marker.
    auto stale = std::move(jobs.front()); jobs.clear();
    ++g_requests[1].generation; desired[0] = property[0] = live[0].name = "textures/new/face.dds";
    ArmPreviewObservation(1, 1, 2, testBaseline, desired);
    stale(); Check(g_preview.queued && jobs.size() == 1 && pumps == 0, "old callback crossed generations");
    Replay(0); Tick(); Check(pumps == 1 && live[0].name == desired[0], "newest choice was not repaired");
    Reset();
    for (unsigned i{}; i < PreviewWatch::kMaxRepairs + 20; ++i) { Replay(0); Tick(); }
    Check(pumps == PreviewWatch::kMaxRepairs && jobs.empty() && g_preview.actor == 0,
        "continuous external writer caused unbounded repainting");
    Reset(); pumpSuccess = false; Replay(0); Tick();
    Check(pumps == 1 && jobs.empty(), "failed repair retried indefinitely");
    Reset(); jobs.clear(); g_preview.queued = false; accept = false;
    SchedulePreviewObservation(1, 1, 1);
    Check(g_preview.actor == 0 && jobs.empty(), "rejected queue retained an armed observation");
    Reset(); jobs.clear(); g_preview = {}; desired = {};
    ArmPreviewObservation(1, 1, 1, testBaseline, desired);
    Check(jobs.empty(), "body-only preview scheduled face observation");
    for (unsigned i{}; i < 10000; ++i) {
        Reset(); Replay(0); Tick(); owner = 0; Tick();
        Check(jobs.empty() && g_preview.actor == 0 && g_preview.watch.Empty(),
            "repeated close retained preview paths or queued jobs");
    }
    std::cout << "Face preview replay: product scheduler, delayed replay, cancellation, save isolation and bounded repair passed\n";
    std::cout << "Single observation state: " << sizeof(PreviewObservation)
              << " bytes plus at most six strings; 10000 repair/close cycles drained\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
