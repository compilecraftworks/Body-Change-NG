// Compile the production begin/finish functions against a value-only SKEE
// fixture. No installed script, DLL, save, or game process is modified.
#include "BodyChangeNG/BodySliderEdit.h"
#include "BodyChangeNG/BodyMorphKeys.h"
#include <memory>
#include <mutex>
#include <optional>
#include <map>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <new>

namespace allocation_probe {
    constinit std::atomic_size_t blocks{}, bytes{};
    struct alignas(std::max_align_t) Header { std::size_t size; };
}
void* operator new(std::size_t size) {
    auto* h=static_cast<allocation_probe::Header*>(std::malloc(sizeof(allocation_probe::Header)+size));
    if(!h) throw std::bad_alloc{};
    h->size=size; ++allocation_probe::blocks; allocation_probe::bytes+=size;
    return h+1;
}
void operator delete(void* memory) noexcept {
    if(!memory) return; auto* h=static_cast<allocation_probe::Header*>(memory)-1;
    --allocation_probe::blocks; allocation_probe::bytes-=h->size; std::free(h);
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete(void* memory,std::size_t) noexcept { ::operator delete(memory); }
void operator delete[](void* memory,std::size_t) noexcept { ::operator delete(memory); }


unsigned checks{};
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool Near(float a, float b) { return std::abs(a-b) < 0.00001F; }
namespace RE {
class TESObjectREFR {};
class Actor;
struct ActorHandle {
    Actor* value{};
    std::shared_ptr<Actor> get() const { return {value, [](Actor*){}}; }
    explicit operator bool() const { return value != nullptr; }
    bool operator==(const ActorHandle&) const = default;
};
class Actor : public TESObjectREFR {
public:
    unsigned id=14; bool loaded=true;
    explicit Actor(unsigned value=14) : id(value) {}
    unsigned GetFormID() const { return id; }
    bool Is3DLoaded() const { return loaded; }
    ActorHandle GetHandle() { return {this}; }
};
Actor player, npc{15};
struct PlayerCharacter { static Actor* GetSingleton() { return &player; } };
}
namespace SKSE::log {
template<class... Args> void warn(Args&&...) {}
template<class... Args> void info(Args&&...) {}
}
namespace bcn {
struct Settings {
    bool removal{};
    static Settings& Get() { static Settings x; return x; }
    bool RemovalMode() const { return removal; }
};
struct Preset { unsigned family=body_family::Bit(body_family::Family::cbbe); bool male{}; };
struct PresetCatalog {
    bool available=true; Preset preset;
    static PresetCatalog& Get() { static PresetCatalog x; return x; }
    std::optional<Preset> Find(const std::string&) const { return available ? std::optional{preset} : std::nullopt; }
};
struct ActorRegistry {
    unsigned session=1, marks=0, outfitInvalidations=0; bool needs=false;
    static ActorRegistry& Get() { static ActorRegistry x; return x; }
    unsigned SessionGeneration() const { return session; }
    bool NeedsBodyApply(RE::Actor*, const std::string&, bool) const { return needs; }
    std::optional<std::string> AppliedBodyId(RE::Actor*) const { return std::string{"preset"}; }
    void MarkBodyApplied(RE::Actor*, const std::string&, bool) { ++marks; }
    void InvalidateOutfit(RE::Actor*) { ++outfitInvalidations; }
};
namespace body_family {
Mask PresetMask(Mask family, bool) { return family; }
Mask ResolveActor(RE::Actor*) { return PresetCatalog::Get().preset.family; }
}
namespace racemenu {
namespace skee {
struct IBodyMorphInterface {
    struct MorphValueVisitor { virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float)=0; };
    // Independent provider ordering (SKEE is case insensitive).
    struct NameLess { bool operator()(const std::string& a,const std::string& b) const { return _stricmp(a.c_str(),b.c_str())<0; } };
    std::map<std::string,std::map<std::string,float>,NameLess> values;
    unsigned mode{};
    void SetMorph(RE::Actor*,const char* name,const char* key,float value) { values[name][key]=value; }
    float GetBodyMorphs(RE::Actor*,const char* name) {
        auto it=values.find(name); if(it==values.end()) return 0;
        if(mode==1) return Sum(name)/static_cast<float>(it->second.size());
        if(mode==2) { float highest=0; for(auto& [key,value]:it->second) highest=(std::max)(highest,value); return highest; }
        return Sum(name);
    }
    void ClearBodyMorphNames(RE::Actor*,const char* name) { values.erase(name); }
    void ClearBodyMorphKeys(RE::Actor*,const char* key) { for(auto& [name, layers]:values) layers.erase(key); }
    bool HasBodyMorph(RE::Actor*,const char* name,const char* key) { auto n=values.find(name); return n!=values.end() && n->second.contains(key); }
    bool HasBodyMorphKey(RE::Actor*,const char* key) { for(auto& [name,layers]:values) if(layers.contains(key)) return true; return false; }
    void VisitMorphValues(RE::Actor* actor,MorphValueVisitor& visitor) {
        for(auto& [name,layers]:values) for(auto& [key,value]:layers) visitor.Visit(actor,name.c_str(),key.c_str(),value);
    }
    float Get(const char* name,const char* key) const {
        auto n=values.find(name); if(n==values.end()) return 0.F;
        auto k=n->second.find(key); return k==n->second.end()?0.F:k->second;
    }
    float Sum(const char* name) const { auto n=values.find(name); float sum=0; if(n!=values.end()) for(auto& [key,v]:n->second) sum+=v; return sum; }
};
}
skee::IBodyMorphInterface morphs;
auto* Interface() { return &morphs; }
constexpr auto kCommittedKey=keys::body, kLegacyCommittedKey=keys::legacyBody;
constexpr auto kPreviewKey=keys::preview, kLegacyPreviewKey=keys::legacyPreview;
constexpr auto kOutfitKey=keys::outfit, kLegacyOutfitKey=keys::legacyOutfit;
std::optional<std::string> testPresetId="preset";
std::optional<std::string> CurrentPresetId(const RE::Actor*) { return testPresetId; }
RE::ActorHandle CancelPreviewTracking(RE::ActorHandle = {}) { return {}; }
#include "body_editor_state.inc"
#include "body_owned_collector.inc"
#include "body_migrate_keys.inc"
#include "body_editor_active.inc"
#include "body_editor_mode.inc"
#include "body_editor_begin.inc"
#include "body_editor_finish.inc"
}
}

int main() try {
    using namespace bcn;
    using namespace bcn::racemenu;
    auto* actor=&RE::player;
    for (auto family : {body_family::Family::cbbe, body_family::Family::himbo, body_family::Family::ube}) {
        PresetCatalog::Get().preset.family=body_family::Bit(family);
        const auto* key=editor::Key(PresetCatalog::Get().preset.family);
        morphs.values.clear();
        morphs.SetMorph(actor,"Breasts",keys::body,.65F);
        morphs.SetMorph(actor,"Hip",keys::body,-.3F);
        morphs.SetMorph(actor,"Breasts",key,.1F);
        morphs.SetMorph(actor,"Waist",key,.25F);
        morphs.SetMorph(actor,"Breasts","Pregnancy",.2F);
        morphs.SetMorph(actor,"Breasts",keys::outfit,-.2F);
        Check(BeginBodySliderEdit(actor),"editor did not begin");
        Check(IsBodySliderEditing(actor),"editor session not tracked");
        Check(!BeginBodySliderEdit(&RE::npc),"NPC gained player editor ownership");
        Check(Near(morphs.Get("Breasts",key),.75F),"slider still shows zero or doubled baseline");
        Check(Near(morphs.Get("Hip",key),-.3F),"signed XML value lost");
        Check(Near(morphs.Sum("Breasts"),.95F),"preset doubled beneath editor");
        Check(morphs.HasBodyMorphKey(actor,keys::body) && morphs.HasBodyMorphKey(actor,keys::editorOffset),
            "provider snapshot cannot recover an editing session");
        Check(!morphs.HasBodyMorphKey(actor,keys::outfit),"clothing was included in editable body");
        Check(Near(morphs.Get("Breasts","Pregnancy"),.2F),"foreign effect was changed");
        Check(BeginBodySliderEdit(actor) && Near(morphs.Get("Breasts",key),.75F),"duplicate open stacked values");
        morphs.SetMorph(actor,"bReAsTs",key,.9F);
        morphs.SetMorph(actor,"NewSlider",key,1.35F);
        morphs.ClearBodyMorphKeys(actor,key); // RaceMenu Reset is a valid complete edit.
        morphs.SetMorph(actor,"Breasts",key,.9F);
        morphs.SetMorph(actor,"NewSlider",key,1.35F);
        FinishBodySliderEdit(actor);
        Check(!IsBodySliderEditing(actor) && !g_bodyEditor,"close retained a session");
        Check(Near(morphs.Get("Breasts",keys::body),.8F),"edited slider reverted to XML");
        Check(Near(morphs.Get("Breasts",key),.1F),"original editor contribution was erased");
        Check(Near(morphs.Get("Waist",keys::body),-.25F),"Reset failed to cancel an original editor value");
        Check(Near(morphs.Sum("Waist"),0.F) && Near(morphs.Sum("Hip"),0.F),"removed slider resurrected");
        Check(Near(morphs.Get("NewSlider",keys::body),1.35F),"new or over-100 slider lost");
        Check(Near(morphs.Get("Breasts",keys::outfit),-.2F),"clothing contaminated committed body");
        auto stable=morphs.values;
        const auto liveBlocks=allocation_probe::blocks.load(), liveBytes=allocation_probe::bytes.load();
        for(unsigned i=0;i<20000;++i) { Check(BeginBodySliderEdit(actor),"reopen failed"); FinishBodySliderEdit(actor); }
        Check(morphs.values==stable && !g_bodyEditor,"repeated reopen accumulated morphs or state");
        Check(allocation_probe::blocks==liveBlocks && allocation_probe::bytes==liveBytes,
            "repeated editor sessions leaked allocation blocks or bytes");
        Check(BeginBodySliderEdit(actor),"invalid-value test did not begin");
        morphs.SetMorph(actor,"Breasts",key,std::numeric_limits<float>::quiet_NaN());
        FinishBodySliderEdit(actor);
        Check(morphs.values==stable,"invalid slider poisoned saved morphs");
        // Both possible plugin save-callback orders. Save SKEE either before
        // BCNG finishes editing, or after. Loading discards runtime sessions.
        for (bool providerFirst : {true, false}) {
            Check(BeginBodySliderEdit(actor),"save-order test did not begin");
            morphs.SetMorph(actor,"Breasts",key,.95F);
            auto savedProvider=morphs.values;
            FinishBodySliderEdit(actor);
            if (!providerFirst) savedProvider=morphs.values;
            morphs.values=savedProvider; g_bodyEditor.reset();
            FinishBodySliderEdit(actor);
            Check(Near(morphs.Get("Breasts",keys::body),.85F) && Near(morphs.Get("Breasts",key),.1F),
                "save callback order duplicated or lost an edit");
            Check(!morphs.HasBodyMorphKey(actor,keys::editorOffset) &&
                !morphs.HasBodyMorphKey(actor,keys::editorBaseline),"saved transient keys were retained");
        }
        Check(BeginBodySliderEdit(actor),"detach test did not begin"); actor->loaded=false;
        morphs.SetMorph(actor,"Breasts",key,1.2F); FinishBodySliderEdit(actor); actor->loaded=true;
        Check(Near(morphs.Get("Breasts",keys::body),1.1F),"detached close discarded values");
        Check(BeginBodySliderEdit(actor),"session-reset test did not begin");
        ++ActorRegistry::Get().session; auto untouched=morphs.values; FinishBodySliderEdit(actor);
        Check(!g_bodyEditor && morphs.values==untouched,"old world wrote into a new session");
    }
    morphs.values.clear(); PresetCatalog::Get().preset.family=body_family::Bit(body_family::Family::cbbe);
    ActorRegistry::Get().needs=true;
    Check(!BeginBodySliderEdit(actor),"unapplied selection was marked as an edited body"); ActorRegistry::Get().needs=false;
    Check(BeginBodySliderEdit(actor),"zeroed preset did not begin");
    const auto* key=editor::Key(PresetCatalog::Get().preset.family);
    morphs.SetMorph(actor,"Breasts",key,.4F); FinishBodySliderEdit(actor);
    Check(Near(morphs.Get("Breasts",keys::body),.4F),"zeroed preset customization was discarded");
    Check(editor::Key(0)==nullptr && editor::Key(body_family::Bit(body_family::Family::unp))==nullptr &&
        editor::Key(body_family::Bit(body_family::Family::cbbe)|body_family::Bit(body_family::Family::ube))==nullptr,
        "unverified editor key was guessed");
    for (unsigned mode : {1U,2U}) {
        morphs.mode=mode; auto original=morphs.values;
        Check(!BeginBodySliderEdit(actor) && morphs.values==original && !g_bodyEditor,
            "non-additive provider was changed or a probe map leaked");
    }
    morphs.mode=0;
    // A save in an empty editor is recoverable without any preset catalog.
    morphs.values.clear(); Check(BeginBodySliderEdit(actor),"empty saved editor did not begin");
    morphs.SetMorph(actor,"Breasts",key,.55F); g_bodyEditor.reset();
    PresetCatalog::Get().available=false;
    FinishBodySliderEdit(actor); PresetCatalog::Get().available=true;
    Check(Near(morphs.Get("Breasts",keys::body),.55F) && !morphs.HasBodyMorphKey(actor,keys::editorOffset),
        "empty editor recovery needed a catalog or lost its edit");
    std::cout << "Body slider editor production lifecycle: " << checks << " checks passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
