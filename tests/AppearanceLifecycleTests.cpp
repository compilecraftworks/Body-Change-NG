// Regression tests for preview undo/RaceMenu/player restore boundaries.
// Product functions are extracted unchanged; engine/provider boundaries are fakes.
#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <utility>

void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
namespace RE {
using FormID = unsigned;
class Actor;
struct ActorHandle {
    Actor* value{};
    explicit operator bool() const { return value != nullptr; }
    std::shared_ptr<Actor> get() const { return {value, [](Actor*) {}}; }
    bool operator==(const ActorHandle&) const = default;
};
class Actor {
public:
    unsigned id = 14;
    bool loaded = true;
    unsigned GetFormID() const { return id; }
    bool Is3DLoaded() const { return loaded; }
    ActorHandle GetHandle() { return {this}; }
};
Actor actor;
Actor npc{15};
struct PlayerCharacter { static Actor* GetSingleton() { return &actor; } };
struct RaceSexMenu { static constexpr auto MENU_NAME = "RaceSex Menu"; };
struct UI {
    bool open{};
    static UI* GetSingleton() { static UI ui; return &ui; }
    bool IsMenuOpen(const char*) { return open; }
};
struct TESForm { template<class T> static T* LookupByID(unsigned id) { return id == 14 ? &actor : id == 15 ? &npc : nullptr; } };
}
namespace SKSE {
const int* GetTaskInterface() { static int tasks; return &tasks; }
namespace log {
template<class... T> void error(T&&...) {}
template<class... T> void warn(T&&...) {}
template<class... T> void info(T&&...) {}
}
}
namespace runtime_probe {
bcn::async_work::FrameTaskQueue g_queue;
std::mutex g_lock;
bool g_scheduled{};
thread_local bcn::async_work::FrameTaskQueue::Lease g_lease;
thread_local bool g_inPump{}, g_urgent{}, g_interactive{};
thread_local std::uint64_t g_workEpoch{};
}
namespace bcn {
struct Settings {
    bool removal{}, correction = true;
    static Settings& Get() { static Settings s; return s; }
    bool PerformanceMode() { return false; }
    bool RemovalMode() { return removal; }
    bool OutfitCorrectionEnabled() { return correction; }
};
unsigned AutomaticActorBudget(bool) { return 16; }
struct ActorRegistry {
    ActorState state;
    unsigned session = 1;
    std::optional<std::uint64_t> outfitSignature;
    static ActorRegistry& Get() { static ActorRegistry r; return r; }
    std::optional<ActorState> Snapshot(RE::Actor*) { return state; }
    unsigned SessionGeneration() { return session; }
    void InvalidateOutfit(RE::Actor*) { outfitSignature.reset(); }
    void MarkBodyApplied(RE::Actor*, const std::string&, bool) {}
    void MarkOutfitApplied(RE::Actor*, std::uint64_t signature) { outfitSignature = signature; }
    bool NeedsOutfitApply(RE::Actor*, std::uint64_t signature) { return outfitSignature != signature; }
};
unsigned refits{};
struct OutfitRefit {
    enum class Action { clear, procedural, named, defer };
    struct Preset { std::string PersistentId() const { return "refit"; } };
    struct Plan { Action action = Action::procedural; std::uint64_t signature = 42; std::optional<Preset> preset; };
    Plan evaluatedPlan;
    static OutfitRefit& Get() { static OutfitRefit r; return r; }
    Plan Evaluate(RE::Actor*) const {
        ++refits; auto result = evaluatedPlan;
        if (!Settings::Get().OutfitCorrectionEnabled()) result.action = Action::clear;
        return result;
    }
    void ProcessActor(RE::Actor*) const;
};
namespace body_family { unsigned ResolveActor(RE::Actor*) { return 1; } }
namespace frame_tasks {
bool Active() { return runtime_probe::g_queue.Active(); }
bool InGameTask() { return runtime_probe::g_inPump; }
bool Queue(unsigned actor, std::function<void()> work, unsigned delay = 1,
    appearance::WorkChannel channel = appearance::WorkChannel::none, bool urgent = false) {
    return runtime_probe::g_queue.Submit(actor, appearance::ChannelValue(channel), std::move(work), delay, urgent);
}
bool HasActorWork(unsigned actor) { return runtime_probe::g_queue.HasActorWork(actor); }
bool QueueRestoration(unsigned actor, std::function<void()> work, unsigned delay = 1,
    appearance::WorkChannel channel = appearance::WorkChannel::none) {
    return runtime_probe::g_queue.SubmitRestoration(actor, std::move(work), delay, appearance::ChannelValue(channel));
}
bool HasActorChannelWork(unsigned actor, appearance::WorkChannel channel) {
    return runtime_probe::g_queue.HasActorChannelWork(actor, appearance::ChannelValue(channel));
}
bool CurrentWorkAllowed() {
    return !runtime_probe::g_inPump ||
        (async_work::FrameTaskQueue::ValidLease(runtime_probe::g_lease) &&
         runtime_probe::g_workEpoch == runtime_probe::g_queue.Epoch());
}
}
namespace rendered_outfit {
bool available{};
bool Available() { return available; }
void Request(RE::Actor*) {}
}
namespace skin_application {
int QueueClear(RE::Actor*) { return 0; }
std::optional<std::string> CurrentProfileId(RE::Actor*) { return {}; }
int QueueApply(RE::Actor*, const std::string&, skin_transaction::Mode, skin_transaction::Selection) { return 0; }
void InvalidateFutanariDetection(unsigned) {}
void QueueReapplyCurrentFutanari(RE::Actor*) {}
}
namespace player_tint { int QueueReapplyCurrent() { return 0; } }
namespace racemenu {
#include "body_queue_actor.inc"
constexpr auto kPreviewKey = "preview", kLegacyPreviewKey = "old-preview";
bool activePreview{};
bool previewKey{};
unsigned previewClears{}, bodyRefreshes{}, defaultClears{}, bodyCommits{};
unsigned outfitWrites{};
bool outfitPresent{};
std::optional<std::string> currentPreset;
struct Morphs { bool HasBodyMorphKey(RE::Actor*, const char*) { return previewKey; } } morphs;
Morphs* Interface() { return &morphs; }
bool HasActivePreview(RE::Actor*) { return activePreview; }
namespace keys {
void ClearPreview(Morphs&, RE::Actor*) { previewKey = false; ++previewClears; }
void ClearOwned(Morphs&, RE::Actor*) { ++defaultClears; outfitPresent = false; previewKey = false; }
void ClearReplacedBody(Morphs& morph, RE::Actor* actor) { ClearOwned(morph, actor); }
}
void ApplyVisibleMorphs(Morphs&, RE::Actor*, bool) { ++bodyRefreshes; }
RE::ActorHandle CancelPreviewTracking(RE::ActorHandle = {}) {
    if (!activePreview) return {};
    activePreview = false; return RE::actor.GetHandle();
}
std::optional<std::string> CurrentPresetId(RE::Actor*) { return currentPreset; }
enum class ApplyMode { preview, commit, outfit };
enum class ApplyResult { queued };
std::mutex g_selectionLock;
std::unordered_map<unsigned, std::string> g_currentPresetIds;
std::unordered_map<unsigned, unsigned> generations;
bool IsReady() { return true; }
void InvalidateActorApplies(unsigned id) { ++generations[id]; }
unsigned BeginApply(unsigned id, ApplyMode) { return ++generations[id]; }
bool IsCurrentApply(unsigned id, ApplyMode, unsigned generation) { return generations[id] == generation; }
ApplyResult QueueApply(RE::Actor* actor, const std::string&, ApplyMode) {
    ActorRegistry::Get().InvalidateOutfit(actor);
    frame_tasks::Queue(actor->GetFormID(), [] { ++bodyCommits; outfitPresent = false; }, 1,
        appearance::WorkChannel::bodyCommit);
    return ApplyResult::queued;
}
bool HasOutfitCorrection(RE::Actor*) { return outfitPresent; }
void CancelPendingOutfit(RE::Actor*) {}
void WriteOutfit(RE::Actor* actor, std::uint64_t signature, bool present) {
    frame_tasks::Queue(actor->GetFormID(), [actor, signature, present] {
        ++outfitWrites; outfitPresent = present; ActorRegistry::Get().MarkOutfitApplied(actor, signature);
    }, 1, appearance::WorkChannel::outfitRefit);
}
void QueueClearOutfit(RE::Actor* actor, std::uint64_t signature) { WriteOutfit(actor, signature, false); }
void QueueApplyProceduralOutfit(RE::Actor* actor, std::uint64_t signature) { WriteOutfit(actor, signature, true); }
ApplyResult QueueApplyOutfit(RE::Actor* actor, const std::string&, std::uint64_t signature) {
    WriteOutfit(actor, signature, true); return ApplyResult::queued;
}
void QueueClearBodyChangeMorphs(RE::Actor*, bool ownedOnly = false);
#include "body_clear_preview.inc"
#include "body_cancel_preview.inc"
#include "body_clear_inactive.inc"
#include "body_clear_default.inc"
#include "body_reapply.inc"
}
#include "outfit_process.inc"
namespace overlay {
std::mutex g_previewLock;
std::unordered_map<std::uint64_t, bool> g_previews;
unsigned restored{}, applied{};
std::uint64_t PreviewKey(unsigned id, Area area) { return (static_cast<std::uint64_t>(id) << 8) | Index(area); }
std::optional<bool> PreviewFor(unsigned id, Area area) {
    return g_previews.contains(PreviewKey(id, area)) ? std::optional{true} : std::nullopt;
}
bool HasActivePreview(const RE::Actor* actor) {
    return actor && std::ranges::any_of(kAreas, [&](Area area) { return PreviewFor(actor->GetFormID(), area).has_value(); });
}
void RestorePreviewNow(RE::Actor* actor, Area area) { g_previews.erase(PreviewKey(actor->GetFormID(), area)); ++restored; }
appearance::WorkChannel Channel(Area area) {
    return static_cast<appearance::WorkChannel>(appearance::ChannelValue(appearance::WorkChannel::overlayFaceApply) + Index(area));
}
struct Interfaces {};
Interfaces InterfacesNow() { return {}; }
unsigned OverlayCount(Interfaces, Area) { return 8; }
void ResolveInstalledEntryMetadata(Entry&) {}
bool EntryMatchesActor(Layout, Sex, unsigned, bool) { return true; }
bool Female(RE::Actor*) { return true; }
ApplyResult QueueReset(RE::Actor*, Area) { return ApplyResult::queued; }
ApplyResult ApplyNow(RE::Actor*, Area, const Entry&, ApplyMode, std::uint8_t* slot,
    const std::optional<OverlayItemState>*, bool, std::function<void()>, std::optional<unsigned>,
    std::span<const OverlayItemState> reserved) {
    ++applied; *slot = static_cast<std::uint8_t>(reserved.size()); return ApplyResult::queued;
}
#include "overlay_cancel.inc"
#include "overlay_reapply.inc"
}
namespace event_probe {
std::atomic_uint64_t g_raceMenuRestoreGeneration{};
#include "player_restore.inc"
}
}
namespace runtime_probe {
#include "frame_pump.inc"
void Drain(unsigned ticks = 80) { for (unsigned n{}; n < ticks; ++n) { g_queue.Advance(); Pump(g_queue.Epoch()); } }
void Reset() {
    g_queue.Reset(true); RE::UI::GetSingleton()->open = false;
    RE::actor.loaded = RE::npc.loaded = true;
    bcn::Settings::Get().removal = false; bcn::Settings::Get().correction = true;
    bcn::rendered_outfit::available = false;
    bcn::OutfitRefit::Get().evaluatedPlan = {};
    bcn::ActorRegistry::Get().state = {};
    bcn::ActorRegistry::Get().outfitSignature.reset();
    ++bcn::ActorRegistry::Get().session;
    bcn::overlay::g_previews.clear(); bcn::overlay::restored = bcn::overlay::applied = 0;
    bcn::racemenu::activePreview = bcn::racemenu::previewKey = false;
    bcn::racemenu::previewClears = bcn::racemenu::bodyRefreshes = 0;
    bcn::racemenu::defaultClears = bcn::racemenu::bodyCommits = bcn::refits = 0;
    bcn::racemenu::outfitWrites = 0; bcn::racemenu::outfitPresent = false;
    bcn::racemenu::generations.clear(); bcn::racemenu::g_currentPresetIds.clear();
    bcn::racemenu::currentPreset.reset();
}
}
void CheckQueueModel() {
    using bcn::async_work::FrameTaskQueue;
    constexpr unsigned actors = 32, channels = 8;
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        FrameTaskQueue queue;
        std::array<unsigned, actors * channels> expected{}, observed{};
        std::array<bool, actors * channels> restoration{};
        std::uint32_t random = 0x9E3779B9U ^ seed;
        const auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
        for (unsigned step = 1; step <= 100000; ++step) {
            const auto actor = next() % actors;
            const auto channel = next() % channels;
            const auto index = actor * channels + channel;
            switch (next() % 20) {
            case 0:
                queue.Reset(true); expected = observed; restoration.fill(false); break;
            case 1:
                queue.CancelActor(actor + 1);
                for (unsigned c{}; c < channels; ++c) if (!restoration[actor * channels + c])
                    expected[actor * channels + c] = observed[actor * channels + c];
                break;
            case 2: case 3: case 4: case 5:
                queue.Advance();
                if (auto job = queue.Take(next() % 2 != 0)) job->run();
                break;
            default:
                expected[index] = step;
                restoration[index] = next() % 3 == 0;
                if (restoration[index]) {
                    Check(queue.SubmitRestoration(actor + 1, [&, index, step] { observed[index] = step; },
                        next() % 8 + 1, channel + 200), "random undo submit rejected");
                } else {
                    Check(queue.Submit(actor + 1, channel + 200, [&, index, step] { observed[index] = step; },
                        next() % 8 + 1, next() % 2 != 0, next() % 2 != 0), "random queue submit rejected");
                }
            }
            Check(queue.Pending() <= actors * channels, "coalesced queue grew beyond semantic keys");
        }
        for (unsigned tick{}; tick < 10000 && queue.HasWork(); ++tick) {
            queue.Advance();
            while (auto job = queue.Take()) job->run();
        }
        Check(!queue.HasWork() && observed == expected, "random queue differed from latest-choice/cancel model");
    }
    std::cout << "Queue model: 1,000,000 deterministic operations, bounded growth and final outcomes PASS\n";
}
int main() try {
    using namespace bcn; using namespace runtime_probe;
    // Normal closed-menu cancellation control, then the editor boundary.
    for (const bool open : {false, true}) {
        Reset(); RE::UI::GetSingleton()->open = open;
        racemenu::activePreview = racemenu::previewKey = true;
        racemenu::QueueCancelPreview();
        if (open) g_queue.CancelActor(14); // Actual menu-open handler action.
        Drain();
        Check(racemenu::previewClears == (open ? 0U : 1U) && racemenu::bodyRefreshes == (open ? 0U : 1U),
            "body undo ran during RaceMenu or failed in closed-menu control");
        RE::UI::GetSingleton()->open = false; Drain();
        Check(racemenu::previewClears == 1 && racemenu::bodyRefreshes == 1 && !g_queue.HasWork(),
            "body undo did not resume exactly once after RaceMenu");
    }
    for (const auto area : overlay::kAreas) {
        Reset(); auto& saved = ActorRegistry::Get().state.overlay.areas[overlay::Index(area)];
        saved.useDefault = false;
        saved.items = {{"saved", "saved.dds", 0}};
        overlay::g_previews[overlay::PreviewKey(14, area)] = true;
        overlay::QueueCancelPreviews(&RE::actor); Drain();
        Check(overlay::restored == 1 && !overlay::HasActivePreview(&RE::actor), "normal overlay cancel failed");
        overlay::g_previews[overlay::PreviewKey(14, area)] = true;
        overlay::QueueCancelPreviews(&RE::actor);
        RE::UI::GetSingleton()->open = true; g_queue.CancelActor(14); Drain();
        Check(overlay::restored == 1 && overlay::HasActivePreview(&RE::actor) && g_queue.Pending() == 1,
            "overlay undo was lost or ran during RaceMenu");
        RE::UI::GetSingleton()->open = false;
        event_probe::ReapplyPlayerSelectionsAfterRaceMenu(RE::actor.GetHandle(), 1, 120, 2, 0); Drain();
        Check(overlay::restored == 2 && !overlay::HasActivePreview(&RE::actor) && overlay::applied > 0 && !g_queue.HasWork(),
            "overlay undo or saved restore did not resume after RaceMenu");
    }
    std::cout << "Body/overlay undo: waits during RaceMenu and resumes exactly once; all 4 areas restored\n";
    for (const bool useDefault : {false, true}) {
        Reset(); auto& body = ActorRegistry::Get().state.body.selection;
        body.manual = true; body.useDefault = useDefault;
        racemenu::outfitPresent = true;
        if (!useDefault) racemenu::currentPreset = "body-preset";
        event_probe::ReapplyPlayerSelectionsAfterRaceMenu(RE::actor.GetHandle(), 1, 120, 2, 0); Drain();
        Check(useDefault ? racemenu::defaultClears == 3 && refits == 3 && racemenu::outfitPresent :
            racemenu::bodyCommits == 3 && refits == 3 && racemenu::outfitPresent,
            "player outfit not reapplied after body clear/commit");
        std::cout << "Player restore: default=" << useDefault << " refit-evaluations=" << refits << '\n';
    }
    // Exclude the initially suspected distribution-vs-restore overwrite.
    Reset(); auto& saved = ActorRegistry::Get().state.overlay.areas[overlay::Index(overlay::Area::body)];
    saved.items = {{"old", "old.dds", 0}};
    unsigned selected{};
    frame_tasks::Queue(14, [&] { ++selected; }, 1, overlay::Channel(overlay::Area::body));
    overlay::QueueReapplySaved(&RE::actor); Drain();
    Check(selected == 1 && overlay::applied == 0, "queued-choice priority regressed");
    std::cout << "Control: queued new overlay choice is not replaced by saved restore\n";

    // New choices supersede parked cancellation via the SAME area channel.
    // Do not erase the newer preview, commit, color or reset.
    for (const auto area : overlay::kAreas) for (unsigned choice = 0; choice < 4; ++choice) {
        Reset(); overlay::g_previews[overlay::PreviewKey(14, area)] = true;
        overlay::QueueCancelPreviews(&RE::actor);
        RE::UI::GetSingleton()->open = true; g_queue.CancelActor(14); Drain();
        RE::UI::GetSingleton()->open = false;
        unsigned newest{};
        frame_tasks::Queue(14, [&] { newest = choice + 1; }, 1, overlay::Channel(area));
        Drain(); Check(newest == choice + 1 && overlay::restored == 0, "old overlay undo overwrote newer choice");
    }
    // A pending preview request before close is replaced, not run after undo.
    Reset(); overlay::g_previews[overlay::PreviewKey(14, overlay::Area::body)] = true;
    unsigned latePreview{};
    frame_tasks::Queue(14, [&] { ++latePreview; }, 1, overlay::Channel(overlay::Area::body));
    overlay::QueueCancelPreviews(&RE::actor); Drain();
    Check(!latePreview && overlay::restored == 1, "cancel allowed a pending preview to reappear");

    Reset(); racemenu::activePreview = racemenu::previewKey = true;
    racemenu::QueueCancelPreview(); racemenu::activePreview = true;
    Drain(); Check(!racemenu::previewClears && racemenu::previewKey, "old body undo cleared new active preview");
    // Detach/repeated cleanup is bounded and clears stored keys without 3D.
    Reset(); RE::actor.loaded = false; racemenu::previewKey = true;
    for (unsigned n{}; n < 10000; ++n) racemenu::QueueClearInactivePreview(&RE::actor);
    Check(g_queue.Pending() == 1, "repeated inactive cleanup grew without coalescing");
    g_queue.CancelActor(14); Drain();
    Check(racemenu::previewClears == 1 && !racemenu::bodyRefreshes && !g_queue.HasWork(), "unloaded body undo lost or refreshed missing geometry");
    // Session reset must discard parked work; new-session previews survive.
    Reset(); racemenu::activePreview = racemenu::previewKey = true;
    racemenu::QueueCancelPreview();
    overlay::g_previews[overlay::PreviewKey(14, overlay::Area::face)] = true;
    overlay::QueueCancelPreviews(&RE::actor);
    g_queue.Reset(true); ++ActorRegistry::Get().session; Drain();
    Check(!racemenu::previewClears && !overlay::restored, "undo crossed save/session boundary");
    // NPC restoration must keep working while the player is edited.
    Reset(); RE::UI::GetSingleton()->open = true;
    overlay::g_previews[overlay::PreviewKey(15, overlay::Area::hands)] = true;
    overlay::QueueCancelPreviews(&RE::npc); Drain();
    Check(overlay::restored == 1, "player RaceMenu unnecessarily blocked NPC undo");

    for (const bool sfs : {false, true}) for (const bool correction : {false, true})
        for (const auto action : {OutfitRefit::Action::clear, OutfitRefit::Action::procedural,
                OutfitRefit::Action::named, OutfitRefit::Action::defer}) {
            Reset(); rendered_outfit::available = sfs; Settings::Get().correction = correction;
            OutfitRefit::Get().evaluatedPlan = {action, 42, OutfitRefit::Preset{}};
            auto& body = ActorRegistry::Get().state.body.selection;
            body.manual = body.useDefault = true;
            ActorRegistry::Get().outfitSignature = 42; racemenu::outfitPresent = true;
            event_probe::ReapplyPlayerSelectionsAfterRaceMenu(RE::actor.GetHandle(), 1, 120, 0, 0); Drain();
            const auto shouldFit = correction && (action == OutfitRefit::Action::procedural || action == OutfitRefit::Action::named);
            Check(racemenu::defaultClears == 1 && racemenu::outfitPresent == shouldFit,
                "Default restore bypassed disabled/nude/deferred/named/procedural outfit policy");
        }
    Reset(); ActorRegistry::Get().state.body.selection = {"", true, true};
    Settings::Get().removal = true; racemenu::outfitPresent = true;
    event_probe::ReapplyPlayerSelectionsAfterRaceMenu(RE::actor.GetHandle(), 1, 120, 2, 0); Drain();
    Check(!racemenu::defaultClears && !refits, "player restore ran in removal mode");
    Reset(); racemenu::outfitPresent = true;
    racemenu::QueueClearBodyChangeMorphs(&RE::actor, true); Drain();
    Check(racemenu::defaultClears == 1 && !racemenu::outfitPresent && !refits,
        "explicit reset/removal incorrectly restored outfit corrections");
    std::cout << "Latest choices, inactive cleanup, NPCs, reset/removal and SFS policy controls PASS\n";
    CheckQueueModel();
    std::cout << "AppearanceLifecycleTests passed (offline engine boundaries)\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
