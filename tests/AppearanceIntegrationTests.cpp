// Product-function regression tests; only engine boundaries are fakes.
#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <utility>

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
namespace RE {
class Actor;
struct Handle { Actor* value; std::shared_ptr<Actor> get() const { return {value, [](Actor*) {}}; } };
class Actor {
public:
    unsigned GetFormID() const { return 14; }
    bool Is3DLoaded() const { return true; }
    Handle GetHandle() { return {this}; }
};
Actor actor;
struct PlayerCharacter { static Actor* GetSingleton() { return &actor; } };
struct RaceSexMenu { static constexpr auto MENU_NAME = "RaceSex Menu"; };
struct UI {
    bool open = true;
    static UI* GetSingleton() { static UI ui; return &ui; }
    bool IsMenuOpen(const char*) { return open; }
};
}
namespace SKSE::log { template<class... T> void error(T&&...) {} }
namespace bcn {
struct Settings { static Settings& Get() { static Settings s; return s; } bool PerformanceMode() { return false; } };
unsigned AutomaticActorBudget(bool) { return 16; }
struct ActorRegistry {
    ActorState state;
    std::vector<OverlayItemState> completed;
    std::function<bool()> needsBody;
    static ActorRegistry& Get() { static ActorRegistry r; return r; }
    std::optional<ActorState> Snapshot(RE::Actor*) { return state; }
    bool NeedsBodyApply(RE::Actor*, const std::string&, bool) { return needsBody(); }
    bool CompleteOverlayApply(RE::Actor*, overlay::Area, OverlayItemState item, overlay::ApplyMode, std::uint64_t) {
        completed.push_back(std::move(item)); return true;
    }
};
namespace body_family { unsigned ResolveActor(RE::Actor*) { return 1; } }
namespace frame_tasks {
using Lease = async_work::FrameTaskQueue::Lease;
std::vector<std::function<void()>> pending;
bool Queue(unsigned, std::function<void()> fn, unsigned, appearance::WorkChannel) {
    pending.push_back(std::move(fn)); return true;
}
bool HasActorChannelWork(unsigned, appearance::WorkChannel) { return false; }
bool HasPreview(unsigned) { return false; }
bool CurrentWorkAllowed() { return true; } // No cancellation at the tested checkpoint.
void Drain() { auto copy = std::exchange(pending, {}); for (auto& fn : copy) fn(); }
}
}
namespace overlay_probe {
using namespace bcn;
using namespace bcn::overlay;
struct Interfaces { bool overlay = true, override = true; };
struct Request { std::string node, path, previous; std::function<void()> done; };
std::vector<Request> requests;
unsigned capacity = 8;
std::unordered_map<std::string, std::string> occupied;
Interfaces InterfacesNow() { return {}; }
bool HasFaceOverlaySource(RE::Actor*) { return true; }
void ClearPendingResetNow(RE::Actor*, Area) {}
bool HasOverlays(Interfaces, RE::Actor*) { return true; }
void AddOverlays(Interfaces, RE::Actor*) {}
bool Female(RE::Actor*) { return true; }
unsigned OverlayCount(Interfaces, Area) { return capacity; }
std::optional<std::string> NodeName(Interfaces, Area, unsigned slot) { return std::to_string(slot); }
bool NodeMatchesPath(Interfaces, RE::Actor*, bool, const std::string&, const std::string&) { return false; }
bool LiveNodeMatches(RE::Actor*, const std::string&, const std::string&) { return false; }
bool NodeOwnedBySelection(Interfaces, RE::Actor*, bool, const std::string& node, const std::string& path) {
    const auto found = occupied.find(node); return found != occupied.end() && found->second == path;
}
bool NodeIsFree(Interfaces, RE::Actor*, bool, const std::string& node) { return !occupied.contains(node); }
void LogOwnershipConflict(Interfaces, RE::Actor*, const std::string&, const std::string&, const char*) {}
void RevertExactNode(Interfaces, RE::Actor*, Area, const std::string&, bool) {}
bool FinalizeNodeProperties(RE::Actor*, std::string node, std::string path, std::string previous,
    unsigned, std::function<void()> done) {
    // Product FinalizeNodeProperties also defers all writes until next tick.
    // Observe allocations before those engine/VM writes, without emulating them.
    requests.push_back({std::move(node), std::move(path), std::move(previous), std::move(done)});
    return true;
}
bool HasActivePreview(RE::Actor*) { return false; }
appearance::WorkChannel Channel(Area) { return appearance::WorkChannel::none; }
bool EntryMatchesActor(Layout, Sex, unsigned, bool) { return true; }
}
namespace bcn::overlay {
ApplyResult QueueReset(RE::Actor*, Area) { return ApplyResult::queued; }
void ResolveInstalledEntryMetadata(Entry&) {}
std::vector<int> TextureLayers(const std::string&) { return {0}; }
std::vector<int> ObsoleteTextureIndices(const std::string&, const std::string&) { return {}; }
}
namespace overlay_probe {
#include "overlay_apply.inc"
#include "overlay_reapply.inc"
void Run() {
    auto& registry = ActorRegistry::Get();
    for (const bool assigned : {false, true}) {
        registry.state = {};
        auto& area = registry.state.overlay.areas[Index(Area::body)];
        area.useDefault = false;
        area.items = {{"one", "one.dds", std::uint8_t(assigned ? 0 : kNoOwnedSlot)},
                      {"two", "two.dds", std::uint8_t(assigned ? 1 : kNoOwnedSlot)},
                      {"three", "three.dds", std::uint8_t(assigned ? 2 : kNoOwnedSlot)}};
        requests.clear(); QueueReapplySaved(&RE::actor); frame_tasks::Drain();
        Check(requests.size() == 3, "expected three product ApplyNow calls");
        std::unordered_set<std::string> slots;
        for (const auto& request : requests) slots.insert(request.node);
        Check(slots.size() == 3, "restored overlays share a slot");
        std::cout << "Overlay " << (assigned ? "assigned-slot control" : "unassigned restore")
                  << ": 3 requests, " << slots.size() << " distinct slots\n";
    }
    // Contrast the real batch-preview caller's reservation mechanism.
    requests.clear(); std::vector<OverlayItemState> reserved;
    for (unsigned index = 0; index < 3; ++index) {
        Entry entry{.id = std::to_string(index), .texturePath = std::to_string(index) + ".dds"};
        std::uint8_t slot = kNoOwnedSlot;
        Check(ApplyNow(&RE::actor, Area::body, entry, ApplyMode::preview, &slot, nullptr,
            false, {}, {}, reserved) == ApplyResult::queued, "reserved allocation failed");
        reserved.push_back({entry.id, entry.texturePath, slot});
    }
    Check(reserved[0].ownedSlot == 0 && reserved[1].ownedSlot == 1 && reserved[2].ownedSlot == 2,
        "reserved batch control failed");
    std::cout << "Overlay reserved-batch control: slots 0, 1, 2\n";
    auto& area = registry.state.overlay.areas[Index(Area::body)];
    area.items = {{"new", "new.dds", kNoOwnedSlot, 0xFF123456},
                  {"old", "old.dds", 0, 0xAABBCCDD}, {"outside", "outside.dds", 12}};
    requests.clear(); registry.completed.clear(); capacity = 3;
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 3 && requests[0].path == "old.dds" && requests[0].node == "0" &&
        requests[1].node == "1" && requests[2].node == "2", "new paint stole a later saved slot");
    for (auto& request : requests) request.done();
    Check(registry.completed[0].color == 0xAABBCCDD && registry.completed[1].color == 0xFF123456,
        "restored colors changed");
    occupied["0"] = "foreign.dds"; requests.clear(); registry.completed.clear();
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 2 && requests[0].node == "1" && requests[1].node == "2" &&
        occupied.at("0") == "foreign.dds", "foreign slot overwritten or allocation stopped too early");
    occupied["1"] = "foreign2.dds"; requests.clear();
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 1 && requests[0].node == "2", "capacity limit reused a reserved slot");
    occupied.clear(); capacity = 8; requests.clear();
    area.items = {{"one", "same.dds", 0}, {"two", "same.dds", 0, 0xFF010203}};
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 2 && requests[0].node == "0" && requests[1].node == "1",
        "old duplicate slot ownership was restored to one shared node");
    requests.clear();
    area.items.push_back({"three", "three.dds", 1});
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 3 && requests[0].node == "0" && requests[1].node == "1" &&
        requests[1].path == "three.dds" && requests[2].node == "2" && requests[2].path == "same.dds",
        "duplicate ownership stole a later unique saved slot");
    requests.clear(); occupied["0"] = "same.dds";
    area.items = {{"", "invalid.dds", 0}, {"one", "same.dds", 0}};
    QueueReapplySaved(&RE::actor); frame_tasks::Drain();
    Check(requests.size() == 1 && requests[0].node == "0", "invalid item displaced valid existing ownership");
    occupied.clear(); requests.clear();
    for (const auto region : {Area::face, Area::body, Area::hands, Area::feet}) {
        registry.state = {};
        auto& selected = registry.state.overlay.areas[Index(region)];
        selected.useDefault = false;
        selected.items = {{"one", "one.dds", kNoOwnedSlot}, {"two", "two.dds", kNoOwnedSlot}};
        for (const unsigned slots : {0U, 1U, 2U}) {
            capacity = slots; requests.clear();
            QueueReapplySaved(&RE::actor); frame_tasks::Drain();
            Check(requests.size() == slots, "region capacity allocation mismatch");
            for (unsigned n{}; n < slots; ++n)
                Check(requests[n].node == std::to_string(n), "region reused a reserved slot");
        }
    }
    capacity = 8;
    requests.clear(); registry.completed.clear();
}
}
namespace pump_probe {
bcn::async_work::FrameTaskQueue g_queue;
std::mutex g_lock;
bool g_scheduled{};
thread_local bcn::frame_tasks::Lease g_lease;
thread_local bool g_inPump{}, g_urgent{}, g_interactive{};
thread_local std::uint64_t g_workEpoch{};
#include "frame_pump.inc"
void Run() {
    unsigned playerWrites{}, undoWrites{};
    g_queue.Submit(14, 1, [&] { ++playerWrites; });
    g_queue.SubmitRestoration(14, [&] { ++undoWrites; });
    g_queue.Advance(); Pump(g_queue.Epoch());
    Check(playerWrites == 0 && undoWrites == 0 && g_queue.Pending() == 1, "RaceMenu-open undo ran or was discarded");
    RE::UI::GetSingleton()->open = false; Pump(g_queue.Epoch());
    Check(undoWrites == 1 && !g_queue.Pending(), "undo did not resume after RaceMenu closed");
    RE::UI::GetSingleton()->open = true;
    g_queue.Reset(true);
    g_queue.Submit(14, 1, [&] { ++playerWrites; });
    g_queue.SubmitRestoration(14, [&] { ++undoWrites; });
    g_queue.CancelActor(14); g_queue.Advance(); Pump(g_queue.Epoch());
    Check(playerWrites == 0 && undoWrites == 1 && g_queue.Pending() == 1, "CancelActor lost or ran deferred undo");
    RE::UI::GetSingleton()->open = false; Pump(g_queue.Epoch());
    Check(undoWrites == 2, "cancelled actor's undo did not survive");
    std::cout << "RaceMenu-open Pump: restoration retained until close; detach survival preserved\n";
}
}
namespace body_probe {
using bcn::ActorRegistry;
namespace frame_tasks = bcn::frame_tasks;
namespace appearance = bcn::appearance;
constexpr auto kCommittedKey = "committed", kLegacyCommittedKey = "legacyCommitted";
constexpr auto kPreviewKey = "preview", kLegacyPreviewKey = "legacyPreview";
constexpr auto kOutfitKey = "outfit", kLegacyOutfitKey = "legacyOutfit";
namespace keys { constexpr auto obody = "OBody", oclothe = "OClothe"; }
struct Morphs {
    std::unordered_set<std::string> keys;
    bool HasBodyMorphKey(RE::Actor*, const char* key) { return keys.contains(key); }
} morphs;
Morphs* Interface() { return &morphs; }
std::optional<std::string> CurrentPresetId(const RE::Actor*) { return {}; }
struct Preset { bool UsesBuildDefaults() const { return false; } };
struct PresetCatalog {
    static PresetCatalog& Get() { static PresetCatalog c; return c; }
    std::optional<Preset> Find(const std::string&) { return {}; }
};
#include "body_live.inc"
bool IsReady() { return true; }
bool HasActivePreview(RE::Actor*) { return false; }
unsigned clearQueued{};
void QueueClearBodyChangeMorphs(RE::Actor*) { ++clearQueued; }
enum class ApplyMode { commit };
enum class ApplyResult { queued };
enum class UpdatePolicy { deferred };
ApplyResult QueueApply(RE::Actor*, const std::string&, ApplyMode, unsigned, UpdatePolicy) { return ApplyResult::queued; }
unsigned refits{};
struct OutfitRefit { static OutfitRefit& Get() { static OutfitRefit r; return r; } void ProcessActor(RE::Actor*) { ++refits; } };
#include "body_verify.inc"
void Run() {
    Check(LiveBodyChangeStateMatches(&RE::actor, true) == true, "empty Default control failed");
    morphs.keys.insert(kOutfitKey);
    morphs.keys.insert(kLegacyOutfitKey);
    Check(LiveBodyChangeStateMatches(&RE::actor, true) == true, "legitimate outfit-only Default rejected");
    auto& registry = ActorRegistry::Get();
    registry.state = {};
    registry.state.body.selection.useDefault = true;
    registry.needsBody = [] { return !LiveBodyChangeStateMatches(&RE::actor, true).value(); };
    QueueVerifySavedBody(&RE::actor); frame_tasks::Drain();
    Check(clearQueued == 0 && refits == 0, "healthy Default refit was cleared or recomputed");
    for (const auto key : {kCommittedKey, kLegacyCommittedKey, kPreviewKey, kLegacyPreviewKey, keys::obody, keys::oclothe}) {
        morphs.keys.insert(key);
        Check(LiveBodyChangeStateMatches(&RE::actor, true) == false, "stale base/preview key accepted as Default");
        QueueVerifySavedBody(&RE::actor); frame_tasks::Drain();
        morphs.keys.erase(key);
    }
    Check(clearQueued == 6 && refits == 6, "actual base recovery skipped clear or normal outfit reevaluation");
    std::cout << "Default body: valid outfit retained; stale base recovery reevaluates clothing\n";
}
}
int main() try {
    overlay_probe::Run(); pump_probe::Run(); body_probe::Run();
    std::cout << "Appearance integration product-function regressions passed (fake engine boundaries).\n";
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
