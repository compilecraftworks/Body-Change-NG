// Exact production observer/verifier/live matcher/registry bodies, fake engine boundaries.
#include "BodyChangeNG/BodyTextureRecovery.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/ActorState.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ranges>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

unsigned checks{};
void Check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
bool resources = true, workAllowed = true, faceReady = true, facePending{};
namespace REL {
struct Module { int version() const { return game; } static Module& get() { static Module m; return m; } int game=1170; };
}
namespace bcn::runtime {
enum class GameBranch { unsupported, se, ae };
GameBranch ResolveGameBranch(int game) { return game == 597 ? GameBranch::se : game == 629 || game == 1170 ? GameBranch::ae : GameBranch::unsupported; }
}
namespace RE {
using FormID = unsigned;
struct NiSourceTexture { void* rendererTexture = reinterpret_cast<void*>(1); };
struct TextureSet { std::array<std::string,8> paths; const char* GetTexturePath(unsigned i) const { return paths[i].c_str(); } };
struct BSTextureSet { enum Texture : unsigned { kDiffuse, kNormal }; };
struct BGSTextureSet : TextureSet {};
struct BSLightingShaderMaterialBase {
    TextureSet* textureSet{};
    NiSourceTexture* diffuseTexture{}, *normalTexture{};
    bool skin = true, stable = true;
};
struct Shader { BSLightingShaderMaterialBase* material{}; };
struct Name { const char* c_str() const { return text; } const char* text="Body"; };
struct BSGeometry {
    Name name; Shader* shader{};
    Shader* lightingShaderProp_cast() { return shader; }
};
struct Part { std::vector<BSGeometry*> geometries; };
struct Addon {};
struct Object { Addon* addon{}; struct { Part* value{}; Part* get() const { return value; } } partClone; BGSTextureSet* skinTexture{}; };
struct Biped { std::array<Object,40> objects; };
struct Flag { unsigned value{}; unsigned underlying() const { return value; } };
struct MiddleHigh { Flag update3DModel; };
struct Process { MiddleHigh* middleHigh{}; };
struct Armor { std::vector<Addon*> armorAddons; };
struct Base { unsigned id=2; Armor* skin{}; unsigned GetFormID() const { return id; } };
struct Race { unsigned id=1; unsigned GetFormID() const { return id; } };
class Actor;
struct ActorHandle { Actor* value{}; std::shared_ptr<Actor> get() const { return value ? std::shared_ptr<Actor>{value, [](Actor*){}} : nullptr; } };
class Actor {
public:
    bool loaded=true; Base base; unsigned id=14;
    struct Data { Process* currentProcess{}; } data;
    std::shared_ptr<Biped> biped;
    Base* GetActorBase() { return &base; }
    const Base* GetActorBase() const { return &base; }
    bool Is3DLoaded() const { return loaded; }
    Data& GetActorRuntimeData() { return data; }
    const std::shared_ptr<Biped>& GetBiped(bool) const { return biped; }
    unsigned GetFormID() const { return id; }
    ActorHandle GetHandle() { return {this}; }
    Race race;
    Race* GetRace() { return &race; }
};
namespace BSVisit {
enum class BSVisitControl { kContinue, kStop };
template<class F> void TraverseScenegraphGeometries(Part* part, F&& visit) {
    for (auto* g : part->geometries) if (visit(g) == BSVisitControl::kStop) break;
}
}
struct BSResourceNiBinaryStream {
    explicit BSResourceNiBinaryStream(const std::string&) {}
    bool good() const { return resources; }
    bool read(char* data, unsigned n) { if (!resources) return false; std::memcpy(data,"DDS ",n); return true; }
};
}
namespace bcn {
struct Settings { bool removal{}; static Settings& Get() { static Settings s; return s; } bool RemovalMode() const { return removal; } };
struct SkinProfiles { static SkinProfiles& Get() { static SkinProfiles s; return s; } std::uint64_t hash=99; std::uint64_t ContentHash(std::string_view) { return hash; } };
struct ActorRegistry {
    ActorState dataState;
    bool exists=true;
    unsigned marked{}, invalidated{};
    std::mutex lock_;
    static ActorRegistry& Get() { static ActorRegistry r; return r; }
    const ActorState* FindValidatedLocked(RE::Actor*) { return exists ? &dataState : nullptr; }
    std::uint64_t SkinSignature(std::string_view,bool) const { return 99; }
    bool NeedsSkinApply(RE::Actor*,std::string_view,bool);
    void MarkSkinApplied(RE::Actor*,std::string,bool) { ++marked; dataState.skin.application.applied=true; dataState.skin.application.verifiedThisSession=true; }
    void InvalidateSkin(RE::Actor*) { ++invalidated; dataState.skin.application.applied=false; }
};
}
namespace bcn::transformation_skin {
bool suspended{};
bool Suspended(const RE::Actor*) { return suspended; }
}
namespace bcn::skin_target {
bool IsSkinGeometry(RE::BSGeometry* g,bool) { return g && g->shader && g->shader->material && g->shader->material->skin; }
RE::TextureSet* StableTextureSet(RE::BSLightingShaderMaterialBase* m) { return m && m->stable ? m->textureSet : nullptr; }
}
namespace bcn::face_skin {
bool Matches(const RE::Actor*,std::string_view,skin_transaction::Mode = skin_transaction::Mode::commit) { return faceReady; }
bool Pending(const RE::Actor*,std::string_view,skin_transaction::Mode) { return facePending; }
}
namespace SKSE::log { template<class... T> void warn(T&&...) {} }
namespace bcn::frame_tasks {
std::deque<std::function<void()>> jobs;
bool CurrentWorkAllowed() { return workAllowed; }
unsigned CurrentLease() { return 1; }
bool Continue(unsigned,std::function<void()> fn,unsigned) { jobs.push_back(std::move(fn)); return true; }
void Drain() { unsigned n{}; while (!jobs.empty()) { Check(++n < 100,"unbounded continuation"); auto fn=std::move(jobs.front()); jobs.pop_front(); fn(); } }
}
namespace probe {
struct TextureBinding { RE::BGSTextureSet* textureSet{}; };
struct Graph { RE::Armor* armor{}; std::vector<TextureBinding> textures; };
struct BaseInstance {
    RE::Base* base{}; Graph skin;
    unsigned sourceRace=1;
    std::uint64_t generation=7, appliedContentHash=99, desiredContentHash=99;
    std::string appliedProfileId="custom",desiredProfileId="custom";
    bool tracked=true,skinAttached=true,appliedDefault=false;
    bcn::skin_transaction::Mode desiredMode=bcn::skin_transaction::Mode::commit;
};
std::mutex g_lock;
std::unordered_map<unsigned,BaseInstance> g_instances;
std::unordered_map<unsigned,bcn::native_skin::BodyTextureRecoveryBudget> g_bodyRecoveries;
constexpr std::size_t kMaxBodyRecoveries = 4096;
std::string TexturePath(RE::BGSTextureSet* txst,std::size_t n) { return txst ? txst->paths[n] : ""; }
bool OwnsCurrentPointers(const BaseInstance& v) { return v.base && (!v.skinAttached || v.base->skin == v.skin.armor); }
#include "body_texture_observer.inc"
#include "body_texture_verify.inc"
}
namespace bcn::native_skin {
using namespace probe;
#include "body_texture_live.inc"
}
namespace bcn::skin_application {
std::optional<bool> LiveSkinStateMatches(RE::Actor* a,std::string_view id,bool d) { return native_skin::LiveStateMatches(a,id,d); }
}
namespace bcn {
#include "body_texture_registry.inc"
}
struct Fixture {
    RE::NiSourceTexture diffuse,normal;
    RE::BGSTextureSet native;
    RE::TextureSet set;
    RE::BSLightingShaderMaterialBase material;
    RE::Shader shader;
    RE::BSGeometry geometry;
    RE::Part part;
    RE::Addon addon,foreignAddon;
    RE::Armor armor,foreignArmor;
    RE::MiddleHigh middle;
    RE::Process process;
    RE::Actor actor;
    unsigned refreshes{};
    Fixture() {
        native.paths[0]="BodyChangeNG\\Cache\\skin\\hash\\body.dds";
        native.paths[1]="BodyChangeNG\\Cache\\skin\\hash\\normal.dds";
        set.paths=native.paths;
        material={&set,&diffuse,&normal};
        shader={&material}; geometry.shader=&shader;
        part.geometries={&geometry}; armor.armorAddons={&addon};
        actor.base.skin=&armor; process.middleHigh=&middle; actor.data.currentProcess=&process;
        actor.biped=std::make_shared<RE::Biped>();
        actor.biped->objects[2]={&addon,{&part},&native};
        probe::g_instances.clear(); probe::g_bodyRecoveries.clear();
        auto& v=probe::g_instances[2]; v.base=&actor.base; v.skin={&armor,{{&native}}};
        bcn::frame_tasks::jobs.clear(); workAllowed=resources=faceReady=true; facePending=false;
        REL::Module::get().game=1170;
        bcn::Settings::Get().removal=false; bcn::SkinProfiles::Get().hash=99;
        auto& r=bcn::ActorRegistry::Get(); r.exists=true; r.marked=r.invalidated=0;
        r.dataState={}; r.dataState.skin.application.applied=true; r.dataState.skin.application.signature=99;
        r.dataState.skin.application.verifiedThisSession=true;
    }
    void Verify(unsigned left=30) { probe::VerifyBodyTexturesNow(actor.GetHandle(),2,7,[this](auto*) { ++refreshes; },left); }
    bcn::native_skin::BodyTextureState Read() { return probe::ReadBodyTextures(&actor,probe::g_instances.at(2)).state; }
};

void Run() {
    using namespace bcn::native_skin;
    for (bool selected : {false,true}) for(bool matches : {false,true})
        for(bool texture : {false,true}) for(bool renderer : {false,true})
            Check(MissingOwnedBodyResource(selected,matches,texture,renderer) ==
                (selected && matches && (!texture || !renderer)),"resource admission matrix");

    for (int game : {597,629,1170}) {
        Fixture f; REL::Module::get().game=game;
        Check(f.Read()==BodyTextureState::ready,"healthy SE/AE material failed");
        Check(!bcn::ActorRegistry::Get().NeedsSkinApply(&f.actor,"custom",false),"healthy registry reapply");
        f.material.diffuseTexture=nullptr;
        Check(!LiveStateMatches(&f.actor,"custom",false).value(),"missing diffuse accepted");
        Check(bcn::ActorRegistry::Get().NeedsSkinApply(&f.actor,"custom",false),"verified session skipped missing resource");
        f.Verify(); Check(f.refreshes==1,"first recovery missing");
        f.material.diffuseTexture=&f.diffuse;
        bcn::frame_tasks::Drain();
        Check(f.refreshes==1 && probe::g_bodyRecoveries.empty(),"successful recovery repeated/retained budget");
        Check(bcn::ActorRegistry::Get().marked==1,"successful retry not recorded");
    }
    {
        Fixture f; f.diffuse.rendererTexture=nullptr;
        Check(f.Read()==BodyTextureState::missing,"GPU resource missing accepted");
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==2 && bcn::frame_tasks::jobs.empty(),"retry limit did not terminate");
        f.Verify(); Check(f.refreshes==2,"new NiNode event reset exhausted generation");
        Check(!bcn::ActorRegistry::Get().dataState.skin.application.applied,"failed recovery recorded");
        ++probe::g_instances.at(2).generation;
        probe::VerifyBodyTexturesNow(f.actor.GetHandle(),2,8,[&](auto*){++f.refreshes;},30);
        Check(f.refreshes==3,"new selection cannot retry");
        bcn::frame_tasks::jobs.clear();
    }
    {
        Fixture f; f.material.normalTexture=nullptr;
        Check(f.Read()==BodyTextureState::missing,"missing normal accepted");
        f.Verify(0); Check(f.refreshes==0 && bcn::frame_tasks::jobs.empty(),"expired budget dispatched unchecked repair");
    }
    for (unsigned failure : {0U,1U,2U,3U,4U,5U,6U}) {
        Fixture f; f.material.diffuseTexture=nullptr;
        if (failure==0) f.set.paths[0]="other\\body.dds";
        if (failure==1) f.actor.biped->objects[2].addon=&f.foreignAddon;
        if (failure==2) f.actor.biped->objects[2].skinTexture=nullptr;
        if (failure==3) f.material.skin=false;
        if (failure==4) f.geometry.name.text="3BBB_Anus";
        if (failure==5) f.native.paths[0]=f.set.paths[0]="original\\body.dds";
        if (failure==6) f.actor.base.skin=&f.foreignArmor;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0,"foreign/outfit/genital/source target was repaired");
    }
    {
        Fixture f; f.set.paths[0]="foreign\\body.dds"; f.material.normalTexture=nullptr;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0,"mixed foreign diffuse/owned missing normal was overwritten");
    }
    {
        Fixture f; RE::BGSTextureSet foreign=f.native;
        RE::Part other=f.part;
        f.actor.biped->objects[7]={&f.addon,{&other},&foreign};
        f.material.diffuseTexture=nullptr;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0,"foreign TXST in another part did not veto whole-actor refresh");
    }
    for (unsigned stop : {0U,1U,2U,3U,4U,5U,6U}) {
        Fixture f; f.material.diffuseTexture=nullptr; f.Verify();
        if (stop==0) workAllowed=false;
        if (stop==1) ++probe::g_instances.at(2).generation;
        if (stop==2) f.actor.base.id=3;
        if (stop==3) f.actor.base.skin=&f.foreignArmor;
        if (stop==4) bcn::Settings::Get().removal=true;
        if (stop==5) bcn::SkinProfiles::Get().hash=100;
        if (stop==6) probe::g_instances.at(2).desiredProfileId="new";
        bcn::frame_tasks::Drain();
        Check(f.refreshes==1,"stale/detached/foreign/removal continuation wrote");
    }
    {
        Fixture f; f.material.diffuseTexture=nullptr; resources=false;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0,"unavailable DDS caused native reset loop");
    }
    for (unsigned transient : {0U,1U,2U,3U,4U,5U}) {
        Fixture f; f.material.diffuseTexture=nullptr;
        if (transient==0) f.middle.update3DModel.value=1;
        if (transient==1) f.actor.loaded=false;
        if (transient==2) f.actor.biped.reset();
        if (transient==3) f.material.stable=false;
        if (transient==4) facePending=true;
        if (transient==5) REL::Module::get().game=1180;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0,"transitional/unsupported layout was mutated");
    }
    {
        Fixture f; f.material.diffuseTexture=nullptr;
        f.actor.biped->objects[3]=f.actor.biped->objects[2];
        f.actor.biped->objects[7]=f.actor.biped->objects[2];
        unsigned visits{};
        VisitOwnedBodyParts(*f.actor.biped,[&](auto* a){return a==&f.addon;},[&](auto&,auto*){++visits;});
        Check(visits==1,"shared clone visited repeatedly");
        f.actor.biped->objects[2]={}; f.actor.biped->objects[3]={}; f.actor.biped->objects[7]={};
        Check(f.Read()==BodyTextureState::unobserved,"clothed/absent body treated as missing");
    }
    {
        Fixture f;
        f.set.paths[0]="BodyChangeNG\\Cache\\skin\\older\\body.dds";
        Check(f.Read()==BodyTextureState::missing,"previous BCNG material mistaken for foreign");
        f.Verify(); Check(f.refreshes==1,"stale owned cache path did not refresh");
        f.set.paths[0]=f.native.paths[0]; bcn::frame_tasks::Drain();
        Check(probe::g_bodyRecoveries.empty(),"stale owned material retained budget");
    }
    {
        Fixture f; f.material.diffuseTexture=nullptr;
        f.Verify(); bcn::frame_tasks::Drain();
        auto& object=f.actor.biped->objects[2]; auto* saved=object.partClone.value;
        object.partClone.value=nullptr; f.Verify();
        Check(probe::g_bodyRecoveries.at(14).attempts==2,"absent part reset failed budget");
        object.partClone.value=saved; f.Verify();
        Check(f.refreshes==2,"temporary absent part caused another reset cycle");
    }
    {
        Fixture f; probe::g_bodyRecoveries.reserve(4096);
        for (unsigned id=100;id<4196;++id) probe::g_bodyRecoveries[id]={7,2};
        f.material.diffuseTexture=nullptr; f.Verify();
        Check(f.refreshes==0 && probe::g_bodyRecoveries.size()==4096,"recovery state cap exceeded");
    }
    {
        Fixture f; f.material.diffuseTexture=nullptr;
        bcn::transformation_skin::suspended=true;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0, "suspended creature triggered human body recovery");
        bcn::transformation_skin::suspended=false;
        f.actor.race.id=2;
        f.Verify(); bcn::frame_tasks::Drain();
        Check(f.refreshes==0, "old-race graph recovery survived race switch");
    }
    {
        Fixture f;
        for (unsigned cycle{}; cycle<50000; ++cycle) {
            f.material.diffuseTexture=nullptr; f.Verify();
            f.material.diffuseTexture=&f.diffuse; bcn::frame_tasks::Drain();
            Check(probe::g_bodyRecoveries.empty() && bcn::frame_tasks::jobs.empty(),"healthy cycle retained actor/work state");
        }
        Check(f.refreshes==50000,"cycle recovery count");
    }
    std::cout << "Body texture recovery: " << checks << " checks passed; exact production functions, 50,000 cycles.\n";
}
int main() {
    try { Run(); return 0; }
    catch (const std::exception& e) { std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n"; return 1; }
}
