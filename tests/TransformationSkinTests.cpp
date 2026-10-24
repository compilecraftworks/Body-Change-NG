// Production transition, graph restoration, policy, and async coordinator
// bodies with engine/queue boundaries replaced. No duplicated state machine.
#include "BodyChangeNG/TransformationSkin.h"
#include "BodyChangeNG/SkinTransactionPolicy.h"
#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
namespace RE {
using FormID = unsigned;
enum class SEX { kMale, kFemale };
struct TESObjectARMO { unsigned id{}; };
class TESRace {
public:
    unsigned id{}; TESObjectARMO* skin{}; TESRace* armorParentRace{};
    unsigned GetFormID() const { return id; }
};
struct TESNPC {
    TESObjectARMO* skin{}, *farSkin{};
    unsigned GetFormID() const { return 7; }
    SEX GetSex() const { return SEX::kFemale; }
};
class Actor;
struct ActorHandle { Actor* p{}; std::shared_ptr<Actor> get() const { return p ? std::shared_ptr<Actor>(p, [](Actor*){}) : nullptr; } };
class Actor {
public:
    unsigned id=0x14; bool loaded=true; TESNPC base; TESRace* race{};
    unsigned GetFormID() const { return id; }
    TESRace* GetRace() const { return race; }
    TESNPC* GetActorBase() { return &base; }
    const TESNPC* GetActorBase() const { return &base; }
    TESObjectARMO* GetSkin() { return base.skin ? base.skin : race ? race->skin : nullptr; }
    bool Is3DLoaded() const { return loaded; }
    ActorHandle GetHandle() { return {this}; }
};
struct TESDataHandler {
    TESRace* vampire{}; TESRace* wolf{};
    static TESDataHandler* GetSingleton() { static TESDataHandler d; return &d; }
    template<class T> T* LookupForm(unsigned id, std::string_view plugin) {
        if (id == 0x283A && plugin == "Dawnguard.esm") return vampire;
        if (id == 0xCDD84 && plugin == "Skyrim.esm") return wolf;
        return nullptr;
    }
};
}
namespace SKSE::log { template<class... T> void warn(T&&...) {} }
namespace bcn {
struct Settings {
    bool keepSkinDuringVampireLord{}, keepSkinDuringWerewolf{}, removal{};
    static Settings& Get() { static Settings s; return s; }
    Settings Snapshot() const { return *this; }
    bool RemovalMode() const { return removal; }
};
struct ActorRegistry {
    std::optional<std::string> chosen = "B";
    unsigned invalidated{};
    static ActorRegistry& Get() { static ActorRegistry r; return r; }
    std::optional<std::string> SelectedSkinId(const RE::Actor*) const { return chosen; }
    void InvalidateSkin(RE::Actor*) { ++invalidated; }
};
namespace body_family { using Mask = unsigned; }
enum class SkinUvLayout { cbbe };
}
namespace bcn::transformation_skin {
#include "transformation_policy.inc"
// The extraction includes this namespace's closing brace.

namespace probe {
struct ArmorGraph { RE::TESObjectARMO* armor{}; };
struct AppliedSnapshot {};
#include "transformation_instance.inc"
std::mutex g_lock;
std::unordered_map<unsigned, BaseInstance> g_instances, g_playerRaceGraphs;
std::atomic_uint64_t g_nextGeneration{1};
std::unordered_map<unsigned, int> g_bodyRecoveries;
unsigned g_playerRace{};
bool g_playerSuspended{};
bool g_playerForeignOwner{};
RE::TESNPC* g_fallbackBase{};
RE::TESObjectARMO* g_fallbackSkin{}, *g_fallbackFarSkin{};
#include "transformation_fallback.inc"
std::vector<std::unique_ptr<RE::TESObjectARMO>> allocations;
struct GraphConstruction { void Release() {} };
bool IsTngSkinArmor(RE::TESObjectARMO*) { return false; }
bool IsTngChildOfGraph(RE::TESObjectARMO*, const ArmorGraph&) { return false; }
bool RestorePrivateTextures(ArmorGraph&) { return true; }
bool OwnsCurrentPointers(const BaseInstance& s) {
    return s.base && (!s.skinAttached || s.base->skin == s.skin.armor) &&
        (!s.farSkinAttached || s.base->farSkin == s.farSkin.armor);
}
bool SourceStillMatches(const BaseInstance& s) {
    return s.base && s.base->skin == s.originalSkin && s.base->farSkin == s.originalFarSkin;
}
bool UnownedSourceChanged(const BaseInstance& s) {
    return s.base && ((!s.skinAttached && s.base->skin != s.originalSkin) ||
        (!s.farSkinAttached && s.base->farSkin != s.originalFarSkin));
}
std::optional<ArmorGraph> CloneArmorGraph(RE::TESObjectARMO*, RE::SEX, bcn::SkinUvLayout,
    RE::TESRace*, GraphConstruction&, bool = false) {
    allocations.push_back(std::make_unique<RE::TESObjectARMO>());
    return ArmorGraph{ allocations.back().get() };
}
#include "transformation_detach.inc"
#include "transformation_build.inc"
}
namespace bcn::native_skin {
using namespace probe;
#include "transformation_prepare.inc"
bool HasTrackedSelection(const RE::Actor* a) {
    const auto it = g_instances.find(a->GetActorBase()->GetFormID());
    return it != g_instances.end() && it->second.tracked;
}
#include "transformation_reset.inc"

namespace bcn::frame_tasks {
using Channel = bcn::appearance::WorkChannel;
struct Job { Channel channel; std::function<void()> fn; };
std::vector<Job> jobs;
std::uint64_t epoch=1;
bool Active() { return true; }
std::uint64_t Epoch() { return epoch; }
bool IsCurrent(std::uint64_t e) { return e==epoch; }
bool QueueRestoration(unsigned, std::function<void()> work, unsigned = 1, Channel c = Channel::none) {
    std::erase_if(jobs, [c](const auto& j){ return c != Channel::none && j.channel==c; });
    jobs.push_back({c, std::move(work)}); return true;
}
void Step() { auto pending=std::move(jobs); jobs.clear(); for(auto& j:pending) j.fn(); }
}
unsigned refreshes{}, applies{}, clears{};
std::vector<std::function<void(bool)>> faces;
namespace bcn::face_skin {
void Clear(RE::Actor*, std::function<void(bool)> cb, bool, skin_transaction::Mode, bool unloaded, bool registryOnly) {
    Check(unloaded && registryOnly, "transition must clear saved face keys without writing creature head");
    ++clears; faces.push_back(std::move(cb));
}
}
namespace bcn::skin_session {
std::string tracked;
void TrackSkinSelection(unsigned, std::string id) { tracked=std::move(id); }
}
void RefreshNativeSkin3D(RE::Actor*) { ++refreshes; }
namespace bcn::skin_application {
int QueueApply(RE::Actor* a, const std::string& id, skin_transaction::Mode, skin_transaction::Selection) {
    if (probe::g_playerForeignOwner) return 1;
    Check(!transformation_skin::Suspended(a), "suspended actor was applied");
    auto& s = probe::g_instances[a->base.GetFormID()];
    if (!s.skin.armor) s = *probe::BuildInstance(a, SkinUvLayout::cbbe);
    s.sourceRace = a->race->id; s.tracked = true; s.appliedProfileId=id; s.desiredProfileId=id;
    s.skinAttached = true; a->base.skin = s.skin.armor; ++applies; return 0;
}
}
namespace {
#include "transformation_resume.inc"
}
namespace bcn::skin_application {
#include "transformation_reconcile.inc"
}
namespace face_probe {
struct Request { RE::ActorHandle handle; std::string profile="B"; unsigned race{}; std::uint64_t activeGeneration=1; bool registryOnly{}; };
std::mutex g_mutex;
std::uint64_t g_epoch=1;
std::unordered_map<unsigned, Request> g_requests;
using bcn::face_skin::OwnsActiveBatch;
struct Batch {
    Request request; unsigned actorId=0x14; std::uint64_t epoch=1, generation=1;
#include "transformation_face_current.inc"
};
}
void Drain() {
    for (unsigned i=0; i<150 && (!bcn::frame_tasks::jobs.empty() || !faces.empty()); ++i) {
        bcn::frame_tasks::Step();
        auto pending=std::move(faces); faces.clear(); for (auto& f:pending) f(true);
    }
    Check(bcn::frame_tasks::jobs.empty() && faces.empty(), "unbounded transition work");
}
void Reconcile(RE::Actor& a) { bcn::skin_application::QueueTransformationReconcile(&a); Drain(); }
int main() try {
    using namespace bcn;
    RE::TESObjectARMO humanSkin, vampireSkin, wolfSkin, explicitSkin, foreign;
    RE::TESRace human{1,&humanSkin}, vampire{0x70283A,&vampireSkin}, wolf{0xCDD84,&wolfSkin};
    RE::TESRace humanoid{4,&humanSkin,&vampire}, ordinaryVampire{5,&humanSkin};
    auto* data=RE::TESDataHandler::GetSingleton(); data->vampire=&vampire; data->wolf=&wolf;
    RE::Actor actor; actor.race=&human;
    Check(transformation_skin::Classify(&humanoid)==transformation_skin::Form::vampireLord, "humanoid armor ancestry");
    Check(transformation_skin::Classify(&ordinaryVampire)==transformation_skin::Form::ordinary, "ordinary vampire excluded");
    ordinaryVampire.armorParentRace=&ordinaryVampire;
    Check(transformation_skin::Classify(&ordinaryVampire)==transformation_skin::Form::ordinary, "race ancestry cycle");
    auto initial=probe::BuildInstance(&actor,SkinUvLayout::cbbe);
    Check(initial && !initial->originalSkin, "resolved race armor incorrectly captured as raw NPC skin");
    const auto selection = ActorRegistry::Get().chosen;
    ActorRegistry::Get().chosen.reset();
    skin_application::QueueApply(&actor, "preview", skin_transaction::Mode::preview,
        skin_transaction::Selection::direct);
    auto* previewGraph = actor.base.skin;
    const auto firstPreviewApplies = applies;
    Reconcile(actor);
    Check(actor.base.skin == previewGraph && applies == firstPreviewApplies && clears == 0 &&
        probe::g_instances[7].appliedProfileId == "preview",
        "first ordinary preview node event was treated as a transformation");
    native_skin::ResetSessionState();
    ActorRegistry::Get().chosen = selection;
    Reconcile(actor);
    auto* graph=actor.base.skin;
    Check(graph && graph!=&humanSkin && applies==firstPreviewApplies+1, "initial selected skin apply");
    for (auto* race : {&vampire, &wolf}) {
        actor.race=race; Reconcile(actor);
        Check(actor.GetSkin()==race->skin && !actor.base.skin, "creature did not use native race skin");
        Check(ActorRegistry::Get().chosen==selection, "suspend erased committed selection");
        const auto count=applies; Reconcile(actor);
        Check(applies==count, "duplicate node event re-applied while paused");
        actor.race=&human; Reconcile(actor);
        Check(actor.base.skin==graph && probe::g_instances[7].appliedProfileId=="B", "revert failed to reuse human B graph");
    }
    const auto allocated=probe::allocations.size();
    for(unsigned i=0;i<100;++i) { actor.race=&wolf; Reconcile(actor); actor.race=&human; Reconcile(actor); }
    Check(probe::allocations.size()==allocated, "one published graph allocated per transform");
    // Save/load in creature form: keep selection, reset transient state.
    actor.race=&wolf; Reconcile(actor); native_skin::ResetSessionState(); ++frame_tasks::epoch;
    Reconcile(actor); Check(actor.GetSkin()==&wolfSkin, "transformed load applied human skin");
    actor.race=&human; Reconcile(actor); Check(actor.base.skin==graph, "post-load revert did not restore B");
    // The two settings are independent, including live changes while transformed.
    Settings::Get().keepSkinDuringVampireLord=true;
    actor.race=&humanoid; Reconcile(actor);
    Check(actor.base.skin && actor.base.skin!=graph, "humanoid transform needs its own source graph");
    auto* humanoidGraph=actor.base.skin;
    Settings::Get().keepSkinDuringVampireLord=false; Reconcile(actor);
    Check(actor.GetSkin()==&humanSkin && !actor.base.skin, "keep-to-native live toggle");
    Settings::Get().keepSkinDuringVampireLord=true; Reconcile(actor);
    Check(actor.base.skin==humanoidGraph, "native-to-keep live toggle lost reusable graph");
    native_skin::ResetSessionState(); ++frame_tasks::epoch;
    Reconcile(actor); Check(actor.base.skin==humanoidGraph, "humanoid transformed load lost B");
    actor.race=&wolf; Reconcile(actor); Check(actor.GetSkin()==&wolfSkin, "VL option leaked to werewolf");
    Settings::Get().keepSkinDuringWerewolf=true; Reconcile(actor);
    Check(actor.base.skin && actor.base.skin!=graph, "werewolf keep option not connected");
    Settings::Get().keepSkinDuringWerewolf=false; Reconcile(actor);
    Check(actor.GetSkin()==&wolfSkin, "werewolf native option not connected");
    actor.race=&human; Reconcile(actor);
    // Old completion must not restore B in a different session or race.
    actor.race=&wolf; Reconcile(actor);
    actor.race=&human; skin_application::QueueTransformationReconcile(&actor);
    frame_tasks::Step(); Check(!faces.empty(), "missing asynchronous face cleanup");
    ++frame_tasks::epoch;
    const auto beforeStale=applies; Drain();
    Check(applies==beforeStale, "previous-session face completion re-applied B");
    native_skin::ResetSessionState(); Reconcile(actor);
    actor.race=&wolf; Reconcile(actor);
    actor.race=&human; actor.loaded=false;
    skin_application::QueueTransformationReconcile(&actor); frame_tasks::Step();
    auto cleanup=std::move(faces); faces.clear(); for(auto& f:cleanup) f(true);
    const auto beforeReady=applies; frame_tasks::Step();
    Check(applies==beforeReady, "revert applied before 3D was available");
    actor.loaded=true; Drain(); Check(applies==beforeReady+1, "delayed 3D did not resume B");
    // A reset/default during suspension must not resurrect the old selection.
    actor.race=&wolf; Reconcile(actor); ActorRegistry::Get().chosen.reset();
    actor.race=&human; Reconcile(actor); Check(!actor.base.skin, "default resurrected B");
    ActorRegistry::Get().chosen="B";
    // New-load normal base with explicit original skin, including creature->creature.
    native_skin::ResetSessionState(); probe::g_instances.clear(); probe::g_playerRaceGraphs.clear();
    actor.base.skin=&explicitSkin; Reconcile(actor); graph=actor.base.skin;
    Settings::Get().keepSkinDuringVampireLord=true;
    actor.race=&humanoid; Reconcile(actor);
    const auto explicitHumanoidGraph=actor.base.skin;
    Settings::Get().keepSkinDuringVampireLord=false; Reconcile(actor);
    Check(!actor.base.skin && actor.GetSkin()==&humanSkin, "explicit humanoid source was not released");
    Settings::Get().keepSkinDuringVampireLord=true; Reconcile(actor);
    Check(actor.base.skin==explicitHumanoidGraph && !probe::g_playerForeignOwner,
        "own temporary fallback was mistaken for a foreign provider on option toggle");
    actor.race=&human; Reconcile(actor);
    Check(actor.base.skin==graph, "explicit source failed to return after same-race option toggle");
    actor.race=&wolf; Reconcile(actor); Check(actor.GetSkin()==&wolfSkin, "explicit human baseline blocked race fallback");
    Settings::Get().keepSkinDuringVampireLord=false;
    actor.race=&vampire; Reconcile(actor); Check(actor.GetSkin()==&vampireSkin, "consecutive creature switch restored human baseline");
    actor.race=&human; Reconcile(actor); Check(actor.base.skin==graph, "explicit baseline return graph");
    actor.race=&wolf; Reconcile(actor); actor.base.skin=&foreign;
    actor.race=&human; Reconcile(actor);
    Check(actor.base.skin==&foreign, "foreign provider replaced during suspension was overwritten");
    // Race changed after a face batch started: no old writes; same-race clicks drain.
    face_probe::Batch batch; batch.request.handle=actor.GetHandle(); batch.request.race=human.id;
    face_probe::g_requests[0x14]=batch.request;
    Check(batch.Current(), "same-race batch should drain normally");
    actor.race=&wolf; Check(!batch.Current(), "old face batch survived race switch");
    batch.request.profile.clear();
    Check(!batch.Current(), "old Default preview must not repaint the creature head");
    batch.request.registryOnly=true;
    Check(batch.Current(), "owned face cleanup must work without humanoid head");
    actor.id=123; Check(!transformation_skin::Suspended(&actor), "NPC policy changed");
    std::cout << "Transformation skin production-policy/lifecycle checks passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
