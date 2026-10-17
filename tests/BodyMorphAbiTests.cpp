// Calls the extracted production declaration through independent public v4/v5
// provider layouts. No game or RaceMenu DLL is loaded by this fixture.
#include "BodyChangeNG/RaceMenuCompatibility.h"
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include "body_morph_abi.inc"

#if !defined(EXCLUSIVE_SKYRIM_FLAT) || defined(ENABLE_SKYRIM_VR)
#error BCNG verification requires SE/AE exclusive flat layout
#endif
static_assert(sizeof(RE::NiAVObject) == 0x110);
static_assert(sizeof(RE::BSLightingShaderMaterialBase) == 0xA0);
static_assert(sizeof(RE::GFxCharEvent) == 0x0C);

namespace public_contract {
struct Base {
    virtual ~Base() = default;
    virtual std::uint32_t GetVersion() = 0;
    virtual void Revert() = 0;
};
// Public interface history: SKSE64Plugins 9ebcb733e17be695f994cd2e9cc383043446bc02,
// IBodyMorphInterface prefix. CacheLimit is UInt32 in early v4, skee_u64 in v5.
template<class CacheLimit> struct Body : Base {
    struct KeyVisitor { virtual void Visit(const char*, float) = 0; };
    struct StringVisitor { virtual void Visit(const char*) = 0; };
    struct ActorVisitor { virtual void Visit(RE::TESObjectREFR*) = 0; };
    struct ValueVisitor { virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float) = 0; };
    struct MorphVisitor { virtual void Visit(RE::TESObjectREFR*, const char*) = 0; };
    std::uint32_t version{};
    unsigned refreshes{};
    bool deferred{};
    std::map<std::pair<std::string,std::string>, float> values;
    std::uint32_t GetVersion() override { return version; }
    void Revert() override { values.clear(); }
    virtual void SetMorph(RE::TESObjectREFR*, const char* name, const char* key, float value) { values[{name,key}] = value; }
    virtual float GetMorph(RE::TESObjectREFR*, const char* name, const char* key) { return values[{name,key}]; }
    virtual void ClearMorph(RE::TESObjectREFR*, const char* name, const char* key) { values.erase({name,key}); }
    virtual float GetBodyMorphs(RE::TESObjectREFR*, const char* name) {
        float sum=0; for(const auto& [key,value]:values) if(key.first==name) sum+=value; return sum;
    }
    virtual void ClearBodyMorphNames(RE::TESObjectREFR*, const char* name) {
        std::erase_if(values,[&](const auto& entry){ return entry.first.first==name; });
    }
    virtual void VisitMorphs(RE::TESObjectREFR*, MorphVisitor&) {}
    virtual void VisitKeys(RE::TESObjectREFR*, const char*, KeyVisitor&) {}
    virtual void VisitMorphValues(RE::TESObjectREFR* actor, ValueVisitor& visitor) {
        for (const auto& [key,value] : values) visitor.Visit(actor, key.first.c_str(), key.second.c_str(), value);
    }
    virtual void ClearMorphs(RE::TESObjectREFR*) { values.clear(); }
    virtual void ApplyVertexDiff(RE::TESObjectREFR*, RE::NiAVObject*, bool) { throw std::runtime_error("wrong slot"); }
    virtual void ApplyBodyMorphs(RE::TESObjectREFR*, bool defer) { ++refreshes; deferred = defer; }
    virtual void UpdateModelWeight(RE::TESObjectREFR*, bool) { throw std::runtime_error("wrong slot"); }
    virtual void SetCacheLimit(CacheLimit) { throw std::runtime_error("unused width-dependent method called"); }
    virtual bool HasMorphs(RE::TESObjectREFR*) { return !values.empty(); }
    virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR*) { return 0; }
    virtual bool HasBodyMorph(RE::TESObjectREFR*, const char* name, const char* key) { return values.contains({name,key}); }
    virtual bool HasBodyMorphName(RE::TESObjectREFR*, const char* name) {
        return std::ranges::any_of(values, [&](const auto& entry) { return entry.first.first == name; });
    }
    virtual bool HasBodyMorphKey(RE::TESObjectREFR*, const char* key) {
        return std::ranges::any_of(values, [&](const auto& entry) { return entry.first.second == key; });
    }
    virtual void ClearBodyMorphKeys(RE::TESObjectREFR*, const char* key) {
        std::erase_if(values, [&](const auto& entry) { return entry.first.second == key; });
    }
    virtual void VisitStrings(StringVisitor&) {}
    virtual void VisitActors(ActorVisitor&) {}
};
}
void Check(bool result) { if (!result) throw std::runtime_error("BodyMorph public-prefix ABI regression"); }
__declspec(noinline) skee::IBodyMorphInterface* Consumer(void* provider) {
    return static_cast<skee::IBodyMorphInterface*>(provider);
}
template<class Width> void Exercise(std::uint32_t revision) {
    public_contract::Body<Width> provider;
    provider.version = revision;
    auto* api = Consumer(&provider);
    Check(api->GetVersion() == revision);
    for (unsigned n = 1; n <= 1000; ++n) {
        api->SetMorph(nullptr, "Breasts", "BodyChangeNG", 0.75F);
        api->SetMorph(nullptr, "Breasts", "foreign", 0.25F);
        Check(api->GetMorph(nullptr, "Breasts", "BodyChangeNG") == 0.75F);
        Check(api->HasBodyMorphKey(nullptr, "BodyChangeNG"));
        struct Visitor : skee::IBodyMorphInterface::MorphValueVisitor {
            unsigned count{};
            void Visit(RE::TESObjectREFR*, const char*, const char*, float) override { ++count; }
        } visitor;
        api->SetMorph(nullptr,"BCNGABIProbe","BodyChangeNGEditorBaseline",.25F);
        api->SetMorph(nullptr,"BCNGABIProbe","BodyChangeNGEditorOffset",.75F);
        Check(api->GetBodyMorphs(nullptr,"BCNGABIProbe")==1.F);
        api->ClearBodyMorphNames(nullptr,"BCNGABIProbe");
        Check(!provider.values.contains({"BCNGABIProbe","BodyChangeNGEditorBaseline"}) &&
            !provider.values.contains({"BCNGABIProbe","BodyChangeNGEditorOffset"}));
        api->VisitMorphValues(nullptr, visitor);
        Check(visitor.count == 2);
        api->ApplyBodyMorphs(nullptr, false);
        Check(provider.refreshes == n && !provider.deferred);
        api->ClearBodyMorphKeys(nullptr, "BodyChangeNG");
        Check(!api->HasBodyMorphKey(nullptr, "BodyChangeNG"));
        Check(api->GetMorph(nullptr, "Breasts", "foreign") == 0.25F);
    }
}
int main() try {
    const std::array versions = {REL::Version{1,5,97,0}, REL::Version{1,6,317,0},
        REL::Version{1,6,318,0}, REL::Version{1,6,323,0}, REL::Version{1,6,342,0},
        REL::Version{1,6,353,0}, REL::Version{1,6,629,0}, REL::Version{1,6,640,0},
        REL::Version{1,6,659,0}, REL::Version{1,6,1130,0}, REL::Version{1,6,1170,0}, REL::Version{1,6,1179,0}};
    for (const auto runtime : versions) {
        const auto branch = bcn::runtime::ResolveGameBranch(runtime);
        Check(bcn::racemenu_compat::ResolveBodyMorphAbi(4,branch) == bcn::racemenu_compat::BodyMorphAbi::v4);
        Check(bcn::racemenu_compat::ResolveBodyMorphAbi(5,branch) == bcn::racemenu_compat::BodyMorphAbi::v5);
        Exercise<std::uint32_t>(4); Exercise<std::uint64_t>(5);
    }
    std::cout << "BodyMorphAbiTests: 12 runtime admissions x v4/v5 provider layouts x 1000 cycles PASS (offline)\n";
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
