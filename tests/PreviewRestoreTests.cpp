// Exact product function bodies; only engine/VM boundaries are fakes.
// These tests neither start Skyrim nor read/write any game save.
#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/OverlayReplacementState.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
namespace RE {
using FormID = std::uint32_t;
struct TESNPC {
    void* skin{};
    void* farSkin{};
    FormID GetFormID() const { return 2; }
};
struct Actor {
    bool loaded = true;
    TESNPC base;
    FormID GetFormID() const { return 1; }
    TESNPC* GetActorBase() { return &base; }
    bool Is3DLoaded() const { return loaded; }
};
Actor instance;
struct ActorHandle { std::shared_ptr<Actor> get() const { return { &instance, [](auto*) {} }; } };
struct TESForm { template<class T> static T* LookupByID(FormID) { return &instance; } };
}
namespace REL { struct Module { static Module& get() { static Module v; return v; } int version() { return 1; } }; }
namespace bcn::runtime {
enum class GameBranch { supported, unsupported };
bool supported = true;
GameBranch ResolveGameBranch(int) { return supported ? GameBranch::supported : GameBranch::unsupported; }
}
namespace bcn {
struct Settings {
    bool removal{};
    static Settings& Get() { static Settings v; return v; }
    bool RemovalMode() const { return removal; }
};
struct ActorRegistry {
    std::mutex lock_;
    std::unordered_map<std::uint32_t, ActorState> states_;
    unsigned marked{}, invalidated{};
    static ActorRegistry& Get() { static ActorRegistry v; return v; }
    std::optional<ActorState> Snapshot(RE::Actor* actor) {
        const auto found = states_.find(actor->GetFormID());
        return found == states_.end() ? std::nullopt : std::optional{found->second};
    }
    std::vector<OverlayItemState> SelectedOverlays(RE::Actor* actor, overlay::Area area) {
        const auto state = Snapshot(actor);
        return state ? state->overlay.areas[static_cast<unsigned>(area)].items : std::vector<OverlayItemState>{};
    }
    void MarkSkinApplied(RE::Actor*, const std::string&, bool) { ++marked; }
    void InvalidateSkin(RE::Actor*) { ++invalidated; }
    void ForgetTransient(std::uint32_t actorFormID);
};
#include "registry_forget.inc"
}
namespace bcn::frame_tasks {
async_work::FrameTaskQueue queue;
bool Active() { return queue.Active(); }
bool Queue(unsigned actor, std::function<void()> work) { return queue.Submit(actor, 0, std::move(work)); }
void Drain() {
    queue.Advance();
    while (auto job = queue.Take()) job->run();
    queue.Advance(); queue.Advance();
}
}
namespace overlay_test {
using bcn::overlay::Area;
using bcn::overlay::PreviewState;
constexpr auto kAreas = bcn::overlay::kAreas;
std::uint64_t PreviewKey(unsigned id, Area area) { return (std::uint64_t(id) << 8U) | unsigned(area); }
std::mutex g_previewLock;
std::unordered_map<std::uint64_t, PreviewState> g_previews;
std::unordered_map<unsigned, bcn::OverlayItemState> nodes;
std::optional<PreviewState> PreviewFor(unsigned id, Area area) {
    const auto found = g_previews.find(PreviewKey(id, area));
    return found == g_previews.end() ? std::nullopt : std::optional{found->second};
}
int InterfacesNow() { return 1; }
bool ColorOwnedNode(int, RE::Actor*, Area, const bcn::OverlayItemState& item, unsigned color) {
    const auto found = nodes.find(item.ownedSlot);
    if (found == nodes.end() || found->second.texturePath != item.texturePath) return false;
    found->second.color = color;
    return true;
}
void RemovePreviewItemValue(RE::Actor*, Area, const PreviewState&, const bcn::OverlayItemState& item) {
    const auto found = nodes.find(item.ownedSlot);
    if (found != nodes.end() && found->second.texturePath == item.texturePath) nodes.erase(found);
}
void QueueReapplySaved(RE::Actor*) {}
using bcn::ActorRegistry;
namespace frame_tasks = bcn::frame_tasks;
#include "overlay_cleanup.inc"
#include "overlay_forget.inc"
void Begin(bool persistent = false) {
    nodes.clear(); g_previews.clear();
    auto& registry = ActorRegistry::Get();
    registry.states_.clear();
    auto& state = registry.states_[1];
    state.actorFormID = 1;
    bcn::CompleteOverlayTransaction(state.overlay.areas[bcn::overlay::Index(Area::body)], {"paint", "preview.dds", 0},
        bcn::overlay::ApplyMode::preview, 0);
    if (persistent) state.body.selection = {"saved", true, false};
    PreviewState preview;
    preview.live = bcn::OverlayItemState{"paint", "preview.dds", 0, 0xFF123456};
    nodes[0] = *preview.live;
    g_previews[PreviewKey(1, Area::body)] = preview;
}
void Detach() {
    frame_tasks::queue.CancelActor(1);
    ForgetActorState(1);
    ActorRegistry::Get().ForgetTransient(1);
}
void Run() {
    for (const auto persistent : {false, true}) {
        Begin(persistent); Detach(); frame_tasks::Drain();
        Check(nodes.empty(), "detached preview survives transient registry pruning");
        Check(g_previews.empty() && !frame_tasks::queue.HasWork(), "detached preview retained jobs or state");
    }
    Begin(); Detach();
    const auto newer = bcn::OverlayItemState{"new", "preview.dds", 0, 0xFF654321};
    g_previews[PreviewKey(1, Area::body)].live = newer;
    nodes[0] = newer;
    frame_tasks::Drain();
    Check(nodes.size() == 1 && nodes.at(0).color == newer.color, "old detach erased newer preview in same slot");
    for (const auto path : {"preview.dds", "different.dds"}) {
        Begin(); Detach();
        const bcn::OverlayItemState committed{"committed", path, 0, 0xFFABCDEF};
        ActorRegistry::Get().states_[1].overlay.areas[bcn::overlay::Index(Area::body)].items = {committed};
        nodes[0] = committed;
        nodes[0].color = 0xFFFFFFFF;
        frame_tasks::Drain();
        Check(nodes.size() == 1, "old detach erased newer commit");
        Check(nodes.at(0).color == (std::string_view(path) == "preview.dds" ? committed.color : 0xFFFFFFFF),
            "cleanup failed to restore committed color or touched a different texture");
    }
    Begin(); Detach(); nodes[0].texturePath = "foreign.dds"; frame_tasks::Drain();
    Check(nodes.size() == 1 && nodes.at(0).texturePath == "foreign.dds", "cleanup removed another mod's texture");
    Begin(); g_previews[PreviewKey(1, Area::body)].batch.push_back({"second", "second.dds", 1});
    nodes[1] = {"second", "second.dds", 1};
    Detach(); frame_tasks::Drain();
    Check(nodes.empty(), "batch preview cleanup missed a checked paint");
    for (int cycle = 0; cycle < 10000; ++cycle) { Begin(); Detach(); frame_tasks::Drain(); }
    Check(nodes.empty() && g_previews.empty() && ActorRegistry::Get().states_.empty() &&
        !frame_tasks::queue.HasWork(), "repeated detach leaves temporary registrations/state/jobs");
}
}
namespace native_test {
struct AppliedSnapshot { void *skinGraph{}, *farGraph{}, *skinPointer{}, *farPointer{}; };
struct Graph { void* armor{}; };
struct BaseInstance {
    RE::TESNPC* base = &RE::instance.base;
    std::uint64_t generation = 7;
    std::string desiredProfileId;
    Graph skin, farSkin;
    bool skinAttached = true, farSkinAttached = false;
    std::string appliedProfileId = "custom";
    std::uint64_t appliedContentHash = 99;
    bool appliedDefault = false, tracked = true;
    std::shared_ptr<AppliedSnapshot> goodState;
};
std::mutex g_lock;
std::unordered_map<RE::FormID, BaseInstance> g_instances;
bool restoreSucceeds{}, faceSucceeds = true, privateCustom = true, allowUnloaded{}, owns = true;
unsigned restores{}, refreshes{}, faces{};
bool RequestStillCurrent(unsigned id, std::uint64_t generation, const std::string& profile) {
    const auto found = g_instances.find(id);
    return found != g_instances.end() && found->second.generation == generation && found->second.desiredProfileId == profile;
}
bool RestoreOwnedPointers(BaseInstance& value) {
    ++restores;
    value.skinAttached = value.farSkinAttached = false;
    if (restoreSucceeds) privateCustom = false;
    return restoreSucceeds;
}
bool OwnsCurrentPointers(const BaseInstance&) { return owns; }
bool RestoreApplied(BaseInstance&, const AppliedSnapshot&) { return false; }
std::shared_ptr<AppliedSnapshot> CaptureApplied(const BaseInstance&) { return std::make_shared<AppliedSnapshot>(); }
void RefreshLoadedActors(RE::Actor*, const std::function<void(RE::Actor*)>&) { ++refreshes; }
#include "native_complete.inc"
}
namespace bcn::face_skin {
void Clear(RE::Actor*, std::function<void(bool)> callback, bool, skin_transaction::Mode, bool unloaded) {
    ++native_test::faces;
    native_test::allowUnloaded = unloaded;
    callback(native_test::faceSucceeds);
}
}
namespace native_test {
#include "native_clear.inc"
void Reset() {
    g_instances.clear(); g_instances[2] = {};
    restores = refreshes = faces = 0;
    restoreSucceeds = faceSucceeds = owns = privateCustom = true;
    bcn::ActorRegistry::Get().marked = bcn::ActorRegistry::Get().invalidated = 0;
}
void Run() {
    using bcn::skin_transaction::Mode;
    for (const auto bodyOK : {false, true}) for (const auto faceOK : {false, true}) {
        Reset(); restoreSucceeds = bodyOK; faceSucceeds = faceOK;
        ClearNow({}, 2, 7, [](auto*) {}, Mode::commit);
        Check(bcn::ActorRegistry::Get().marked == unsigned(bodyOK && faceOK), "partial Default was recorded as complete");
        Check(bcn::ActorRegistry::Get().invalidated == unsigned(!bodyOK || !faceOK), "failed Default cannot retry");
        Check(g_instances.at(2).appliedDefault == bodyOK, "body restoration failure marked Default");
        if (!bodyOK) Check(!g_instances.at(2).goodState && g_instances.at(2).appliedProfileId == "custom",
            "failed restoration discarded evidence or became last-known-good");
    }
    Reset(); restoreSucceeds = false;
    ClearNow({}, 2, 7, {}, Mode::restore);
    restoreSucceeds = true;
    ClearNow({}, 2, 7, {}, Mode::restore);
    Check(!privateCustom && bcn::ActorRegistry::Get().marked == 1 && allowUnloaded, "Default retry/undo failed");
    Reset();
    ClearNow({}, 2, 6, {}, Mode::restore);
    Check(restores == 0 && faces == 0, "stale cancel overwrote a newer generation");
    ClearNow({}, 2, 7, {}, Mode::commit);
    ClearNow({}, 2, 7, {}, Mode::commit);
    Check(faces == 2 && refreshes == 1 && !allowUnloaded, "shared-base Default lost per-reference face cleanup or rebuilt twice");
    Reset();
    ClearNow({}, 2, 7, {}, Mode::preview);
    Check(bcn::ActorRegistry::Get().marked == 0, "Default preview became committed selection");
    Reset(); restoreSucceeds = false;
    CompleteMutation({}, 2, 7, {}, Mode::commit, CaptureApplied(g_instances.at(2)), {}, false);
    Check(!g_instances.at(2).appliedDefault && refreshes == 0, "failed fallback restoration was reported as recovery");
}
}
namespace bcn {
struct TestProfile { std::vector<int> maleGenitals; };
struct SkinProfiles {
    static SkinProfiles& Get() { static SkinProfiles profiles; return profiles; }
    std::optional<TestProfile> Find(const std::string& id) { return id == "missing" ? std::nullopt : std::optional{TestProfile{}}; }
};
}
namespace bcn::skin_application { enum class ApplyResult { queued, noTaskInterface, unsupportedRuntime, actor3DUnavailable, missingProfile }; }
namespace bcn::native_skin {
unsigned calls{};
skin_application::ApplyResult QueueClear(RE::Actor*, std::function<void(RE::Actor*)>, skin_transaction::Mode, bool = false) {
    ++calls; return skin_application::ApplyResult::queued;
}
skin_application::ApplyResult QueueApply(RE::Actor*, const std::string& id, std::function<void(RE::Actor*)>, skin_transaction::Mode) {
    return id == "missing" ? skin_application::ApplyResult::missingProfile : skin_application::ApplyResult::queued;
}
}
namespace bcn::skin_session {
unsigned tracked{};
std::string lastId;
void TrackSkinSelection(unsigned, std::string id) { ++tracked; lastId = std::move(id); }
}
namespace bcn::skin_application {
void RefreshNativeSkin3D(RE::Actor*) {}
unsigned BeginSkinChange(unsigned) { return 1; }
void QueueMaleGenitalClear(RE::Actor*, unsigned) {}
void QueueMaleGenitalApply(RE::Actor*, const TestProfile&, unsigned) {}
#include "skin_apply.inc"
#include "skin_clear.inc"
void TestRouting() {
    using skin_transaction::Mode;
    RE::instance.loaded = false;
    Settings::Get().removal = false;
    native_skin::calls = 0;
    Check(QueueClear(&RE::instance, Mode::commit, false) == ApplyResult::queued && native_skin::calls == 0,
        "new unloaded commit mutates native forms instead of only storing intent");
    Check(QueueClear(&RE::instance, Mode::preview, false) == ApplyResult::actor3DUnavailable && native_skin::calls == 0,
        "new unloaded preview was accepted");
    Check(QueueClear(&RE::instance, Mode::restore, false) == ApplyResult::queued && native_skin::calls == 1,
        "unloaded cancellation never reached native restoration");
    Settings::Get().removal = true;
    QueueClear(&RE::instance, Mode::commit, false);
    Check(native_skin::calls == 2, "removal-mode unloaded restoration regressed");
    Settings::Get().removal = false; RE::instance.loaded = true;
    for (const auto mode : {Mode::commit, Mode::preview, Mode::restore}) QueueClear(&RE::instance, mode, false);
    Check(native_skin::calls == 5, "normal loaded selection was blocked");
    RE::instance.loaded = false; skin_session::tracked = 0;
    QueueApply(&RE::instance, "original", Mode::commit);
    QueueApply(&RE::instance, "original", Mode::preview);
    Check(skin_session::tracked == 0, "unloaded new choice changed the runtime skin tracking");
    QueueApply(&RE::instance, "original", Mode::restore);
    Check(skin_session::tracked == 1 && skin_session::lastId == "original", "custom-skin undo left the preview tracked");
    QueueApply(&RE::instance, "missing", Mode::restore);
    Check(native_skin::calls == 6 && skin_session::tracked == 2 && skin_session::lastId.empty(),
        "removed original pack failed to fall back to Default on cancellation");
    RE::instance.loaded = true;
}
}
int main() try {
    overlay_test::Run(); native_test::Run(); bcn::skin_application::TestRouting();
    std::cout << "Preview restoration production-function regressions passed (10,000 detach cycles).\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
