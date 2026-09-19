// Executes selected UNMODIFIED production functions against a fake VM/form
// allocator. This is fault injection, not an engine allocator/leak measurement.
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/NativeSkinRouting.h"
#include "BodyChangeNG/NativeTextureConstruction.h"
#include "BodyChangeNG/NativeModelArray.h"
#include "BodyChangeNG/NativeGraphConstruction.h"
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <span>
#include <unordered_set>
#include <cstdint>
#ifdef BCNG_OFFLINE_MEMORY_PROBE
#include "OfflineAllocationMeter.h"
#include <cstdio>
#endif

namespace RE {
    template<class T> using BSTSmartPointer = std::shared_ptr<T>;
    namespace BSScript {
        using Variable = int;
        struct Object {};
        struct IStackCallbackFunctor {
            virtual ~IStackCallbackFunctor() = default;
            virtual void operator()(Variable) = 0;
            virtual void SetObject(const BSTSmartPointer<Object>&) = 0;
        };
    }
    template<class... T> int MakeFunctionArguments(T&&...) { return 0; }
    struct DecalData { int value{}; };
    bool failAllocation{};
    template<class T> T* malloc() { return failAllocation ? nullptr : new T{}; }
    struct BSTextureSet { enum Textures { kDiffuse = 0 }; };
    struct BGSTextureSet;
    std::vector<BGSTextureSet*> engineForms;
    bool failFactory{};
    struct BGSTextureSet {
        static constexpr int FORMTYPE = 7;
        enum class Flag { kHasModelSpaceNormalMap = 4 };
        struct Flags {
            int value{};
            Flags& operator=(int v) { value = v; return *this; }
            void set(Flag v) { value |= static_cast<int>(v); }
            bool operator==(const Flags&) const = default;
        } flags;
        int boundData{}, formType{FORMTYPE};
        DecalData* decalData{};
        unsigned refs{1};
        std::array<std::string, 8> paths;
        ~BGSTextureSet() { delete decalData; }
        int GetFormType() const { return formType; }
        unsigned GetFormID() const { return 0xFF000001; }
        void DecRefCount() {
            if (--refs == 0) { std::erase(engineForms, this); delete this; }
        }
        BGSTextureSet* CreateDuplicateForm(bool, void*) {
            return engineForms.emplace_back(new BGSTextureSet{});
        }
    };
    struct IFormFactory {
        template<class T> struct Factory {
            T* Create() { return failFactory ? nullptr : engineForms.emplace_back(new T{}); }
        };
        template<class T> static Factory<T>* GetConcreteFormFactoryByType() { static Factory<T> factory; return &factory; }
    };
    using BSFixedString = std::string;
    struct TESModelTextureSwap {
        struct AlternateTexture {
            BGSTextureSet* textureSet{};
            std::uint32_t index3D{}, unk0C{};
            BSFixedString name3D;
        };
        AlternateTexture* alternateTextures{};
        std::uint32_t numAlternateTextures{};
    };
    struct TESForm {
        static inline std::unordered_set<TESForm*> owned;
        static inline bool ordered{true};
        BGSTextureSet* texture{};
        virtual ~TESForm() {
            if (texture && std::ranges::find(engineForms, texture) == engineForms.end()) ordered = false;
            owned.erase(this);
        }
    };
    struct TESObjectARMO : TESForm {};
    struct TESNPC {
        TESObjectARMO* skin{};
        TESObjectARMO* farSkin{};
        int GetSex() const { return 1; }
    };
    class Actor {
    public:
        TESNPC* base{};
        TESNPC* GetActorBase() const { return base; }
        TESObjectARMO* GetSkin() const { return base ? base->skin : nullptr; }
        void* GetRace() const { return nullptr; }
        unsigned GetFormID() const { return 1; }
    };
}
namespace SKSE {
    namespace log { template<class... T> void error(T&&...) {} template<class... T> void warn(T&&...) {} }
    struct Tasks {
        std::vector<std::function<void()>> jobs;
        void AddTask(std::function<void()> job) { jobs.push_back(std::move(job)); }
        void Drain() {
            auto pending = std::move(jobs); jobs.clear();
            for (auto& job : pending) job();
        }
    } tasks;
    Tasks* GetTaskInterface() { return &tasks; }
}
namespace bcn::frame_tasks {
    bool Active() { return true; }
    std::vector<std::function<void()>> cleanup;
    bool Queue(unsigned, std::function<void()> fn) { cleanup.push_back(std::move(fn)); return true; }
    void Drain() {
        auto jobs = std::move(cleanup); cleanup.clear();
        for (auto& job : jobs) job();
    }
}

static void Check(bool condition, const char* reason)
{ if (!condition) throw std::runtime_error(reason); }

namespace vm_probe {
    using Result = RE::BSScript::Variable;
    struct VM {
        bool accepted{true}, drop{};
        std::vector<RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>> pending;
        static VM* GetSingleton() { static VM vm; return &vm; }
        bool DispatchStaticCall(const char*, const char*, int,
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback) {
            if (accepted && !drop) pending.push_back(std::move(callback));
            return accepted;
        }
        void Deliver(bool twice = false) {
            auto callbacks = std::move(pending); pending.clear();
            for (auto& callback : callbacks) { (*callback)(1); if (twice) (*callback)(1); }
        }
    };
    #include "face_callback.inc"
    struct State { std::uint64_t epoch{1}, active{1}; bool running{true}; unsigned writes{}; };
    struct Batch : std::enable_shared_from_this<Batch> {
        unsigned actorId{1};
        State& state;
        const std::uint64_t epoch, generation;
        bool finished{};
        explicit Batch(State& value) : state(value), epoch(value.epoch), generation(value.active) {}
        bool Current() { return bcn::face_skin::OwnsActiveBatch(epoch, state.epoch, generation, state.active); }
        bool Actor() { return true; }
        // Non-mutating call probe: omit the production rollback path. Keep
        // Finish's ownership gate so an old callback cannot end a new batch.
        void Finish(bool) { finished = true; if (Current()) state.running = false; }
        #include "face_call.inc"
    };
    void Run() {
        auto& vm = *VM::GetSingleton();
        for (int scenario{}; scenario < 10; ++scenario) {
            State state;
            vm.accepted = scenario != 0;
            vm.drop = scenario == 1;
            auto batch = std::make_shared<Batch>(state);
            std::weak_ptr retained = batch;
            batch->Call("GetNodeOverrideString", [batch](Result) {
                ++batch->state.writes; batch->Finish(true);
            });
            batch.reset();
            if (scenario == 0) {
                Check(!state.running && retained.expired(), "immediate dispatch failure retained work");
            } else if (scenario == 1) {
                Check(state.running && !retained.expired(), "abandonment ran outside the game queue");
                bcn::frame_tasks::Drain();
                Check(!state.running && retained.expired(), "dropped callback retained a running batch");
            } else if (scenario == 6 || scenario == 9) {
                if (scenario == 9) ++state.epoch;
                vm.pending.clear(); // VM later discards an accepted callback.
                bcn::frame_tasks::Drain();
                Check(state.running == (scenario == 9) && retained.expired(), "late discard crossed session ownership");
            } else {
                Check(state.running && !retained.expired(), "accepted callback lost its batch owner");
                if (scenario == 3) ++state.epoch; // load boundary
                if (scenario == 4) ++state.active; // detach/re-attach
                if (scenario == 5) {
                    // Arbitrarily long VM delay cannot be treated as success.
                    for (int i{}; i < 1000; ++i) Check(state.running, "delay silently completed work");
                }
                vm.Deliver(scenario == 8);
                Check(state.writes == 0U, "Papyrus callback wrote before game-task dispatch");
                if (scenario == 7) SKSE::tasks.jobs.clear(); // queued delivery discarded
                SKSE::tasks.Drain();
                bcn::frame_tasks::Drain();
                const bool stale = scenario == 3 || scenario == 4;
                Check(state.writes == (stale || scenario == 7 ? 0U : 1U) && state.running == stale && retained.expired(),
                    "late callback crossed ownership or failed to release the batch");
            }
        }
    }
}

namespace bcn {
    struct ActorRegistry {
        unsigned invalidations{};
        static ActorRegistry& Get() { static ActorRegistry registry; return registry; }
        void InvalidateSkin(bool*) { ++invalidations; }
    };
}
namespace finish_probe {
    using bcn::face_skin::OwnsActiveBatch;
    struct Request {
        unsigned generation{1}, activeGeneration{1};
        bool running{true}, complete{};
        int paths{2}, goodPaths{1}, mode{2}, goodMode{1};
        std::string profile{"new"}, goodProfile{"good"};
        std::function<void(bool)> completion;
    };
    std::unordered_map<unsigned, Request> g_requests;
    std::mutex g_mutex;
    unsigned g_epoch{1}, pumps{};
    void Pump(unsigned) { ++pumps; }
    struct Batch {
        unsigned actorId{1}, epoch{1}, generation{1}, channel{}, rollbacks{};
        bool finished{}, cleaningOld{}, rollbackDone{}, rollingBack{}, rollbackFailed{};
        unsigned mutated{};
        Request request;
        bool Current() { return OwnsActiveBatch(epoch, g_epoch, generation, g_requests.at(actorId).activeGeneration); }
        std::shared_ptr<bool> Actor() { return std::make_shared<bool>(true); }
        #include "face_finish.inc"
        // This probes actual Finish routing. Per-channel engine writes are
        // deliberately not simulated by this immediate rollback boundary.
        void RollbackChannel() { ++rollbacks; rollbackDone = true; Finish(false); }
    };
    void Run() {
        for (unsigned scenario{}; scenario < 4; ++scenario) {
            g_requests.clear(); g_epoch = 1; pumps = 0;
            Batch batch;
            unsigned completions{};
            bool successSeen{};
            batch.request.completion = [&](bool success) { ++completions; successSeen |= success; };
            g_requests[1] = batch.request;
            if (scenario == 1 || scenario == 2) batch.mutated = 3;
            if (scenario == 2) ++g_requests[1].generation; // queued newer request
            if (scenario == 3) ++g_epoch; // obsolete session
            batch.Finish(false); batch.Finish(false);
            const auto& result = g_requests.at(1);
            Check(result.running == (scenario == 3) && !result.complete && !successSeen,
                "abandonment incorrectly completed or unlocked another face batch");
            Check(completions == (scenario == 3 ? 0U : 1U) &&
                batch.rollbacks == (scenario == 1 || scenario == 2 ? 1U : 0U) &&
                pumps == (scenario == 2 ? 1U : 0U), "Finish rollback/continuation was lost or repeated");
            if (scenario < 2) Check(result.paths == result.goodPaths && result.profile == "good",
                "failed face replayed the rejected selection");
        }
    }
}

namespace clone_probe {
    constexpr std::size_t kTextureCount = 8;
    struct TextureBinding {
        RE::BGSTextureSet* textureSet{};
        std::uint32_t slotMask{};
        std::array<std::string, kTextureCount> originalPaths;
        bcn::native_skin::TextureRole role{};
        std::optional<bcn::native_skin::TextureRole> fixedRole;
    };
    bool slotsReady = true;
    int failWrite = -1;
    bool PrepareFormSlots(TextureBinding&) { return slotsReady; }
    std::string TexturePath(RE::BGSTextureSet* source, std::size_t index) { return source->paths[index]; }
    bool WriteTexturePath(TextureBinding& binding, std::size_t index, const std::string& path) {
        if (static_cast<int>(index) == failWrite) return false;
        binding.textureSet->paths[index] = path; return true;
    }
    #include "duplicate_form.inc"
    #include "clone_texture.inc"
    struct ModelTextureTarget {
        RE::BGSTextureSet* provider{};
        bcn::native_skin::TextureRole role{};
        bool modelSpaceNormals{};
        std::array<std::string, 8> paths;
        unsigned index3D{};
        std::string name3D;
    };
    #include "model_texture.inc"
    void Run() {
        RE::BGSTextureSet source;
        source.boundData = 17; source.flags = 4; source.decalData = new RE::DecalData{9};
        source.paths.fill("original.dds");
        for (int repetition{}; repetition < 100; ++repetition) for (int failure = -4; failure < 8; ++failure) {
            RE::failFactory = failure == -4;
            RE::failAllocation = failure == -3;
            slotsReady = failure != -2;
            failWrite = failure >= 0 ? failure : -1;
            const auto before = RE::engineForms.size();
            const auto clone = CloneTexture(&source, 4U, bcn::SkinUvLayout::cbbe);
            Check(clone.has_value() == (failure == -1), "unexpected clone failure boundary");
            Check(RE::engineForms.size() == before + (clone ? 1U : 0U),
                "unpublished failed clone leaked its construction reference");
            Check(source.paths.front() == "original.dds" && source.decalData->value == 9,
                "failed clone mutated provider texture/decal");
            if (clone) Check(clone->textureSet->decalData != source.decalData &&
                clone->textureSet->flags == source.flags && clone->textureSet->boundData == source.boundData,
                "clone aliases provider-owned mutable data");
        }
        // Only fake successful forms are dropped here: published engine graphs
        // can still have raw TNG/ARMA references and are NOT freed by BCNG.
        while (!RE::engineForms.empty()) RE::engineForms.back()->DecRefCount();
        ModelTextureTarget target{ .modelSpaceNormals = true };
        target.paths.fill("embedded.dds");
        for (int repetition{}; repetition < 100; ++repetition) for (int failure = -3; failure < 8; ++failure) {
            RE::failFactory = failure == -3;
            slotsReady = failure != -2;
            failWrite = failure >= 0 ? failure : -1;
            const auto before = RE::engineForms.size();
            const auto texture = CreateModelTexture(target, 4U);
            Check(texture.has_value() == (failure == -1) &&
                RE::engineForms.size() == before + (texture ? 1U : 0U), "embedded TXST failure retained a construction reference");
            if (texture) Check(texture->textureSet->flags.value == 4 &&
                texture->textureSet->paths[7] == "embedded.dds", "embedded flags/paths changed");
        }
        while (!RE::engineForms.empty()) RE::engineForms.back()->DecRefCount();
    }
}

namespace detach_probe {
    struct Armor { Armor* parent{}; };
    struct Graph { Armor* armor{}; bool restoreFails{}; unsigned restores{}; };
    struct Base { Armor* skin{}; Armor* farSkin{}; unsigned GetFormID() const { return 1; } };
    struct BaseInstance {
        Base* base{};
        bool skinAttached{true}, farSkinAttached{true};
        Graph skin, farSkin;
        Armor* originalSkin{}; Armor* originalFarSkin{};
    };
    bool IsTngChildOfGraph(Armor* current, const Graph& graph) {
        return current && current->parent == graph.armor;
    }
    bool RestorePrivateTextures(Graph& graph) { ++graph.restores; return !graph.restoreFails; }
    #include "restore_owned.inc"
    void Run() {
        for (unsigned ownership{}; ownership < 3; ++ownership) for (unsigned failure{}; failure < 4; ++failure) {
            Armor original, originalFar, privateBody, privateFar, tngChild{ &privateBody }, foreign;
            Base base{ ownership == 0 ? &privateBody : ownership == 1 ? &tngChild : &foreign, &privateFar };
            BaseInstance instance{ &base, true, true, { &privateBody, (failure & 1U) != 0 },
                { &privateFar, (failure & 2U) != 0 }, &original, &originalFar };
            Check(RestoreOwnedPointers(instance) == (failure == 0), "partial restore reported success");
            Check(base.skin == (ownership == 2 ? &foreign : &original) && base.farSkin == &originalFar,
                "detach overwrote a foreign provider or missed its TNG child");
            Check(instance.skin.restores == 1 && instance.farSkin.restores == 1 &&
                !instance.skinAttached && !instance.farSkinAttached, "partial failure skipped the remaining graph");
            instance.skin.restoreFails = instance.farSkin.restoreFails = false;
            Check(RestoreOwnedPointers(instance) && instance.skin.restores == 2 && instance.farSkin.restores == 2,
                "retained graph cannot retry restoration after partial failure");
        }
    }
}

namespace model_array_probe {
    using TextureBinding = clone_probe::TextureBinding;
    using ModelTextureTarget = clone_probe::ModelTextureTarget;
    #include "pending_model.inc"
    struct ModelArrayHeap {
        static inline std::unordered_set<void*> allocations;
        static inline bool fail{};
        static void* Allocate(std::size_t size) {
            if (fail) return nullptr;
            auto* result = ::operator new(size);
            allocations.insert(result);
            return result;
        }
        static void Free(void* memory) {
            Check(allocations.erase(memory) == 1, "freed an array interior or freed it twice");
            ::operator delete(memory);
        }
    };
    #include "model_array.inc"
    using Entry = RE::TESModelTextureSwap::AlternateTexture;
    using Array = bcn::native_skin::ModelArrayConstruction<Entry, ModelArrayHeap>;
    void Run() {
        for (unsigned repeat{}; repeat < 100; ++repeat)
        for (unsigned oldCount{}; oldCount < 3; ++oldCount)
        for (unsigned newCount{}; newCount < 3; ++newCount)
        for (bool fail : {false, true}) {
            ModelArrayHeap::fail = false;
            Array initial;
            if (oldCount) Check(initial.Allocate(oldCount), "initial allocation failed");
            RE::BGSTextureSet provider;
            for (unsigned i{}; i < oldCount; ++i)
                Check(initial.Append({ &provider, i, 0, "node" + std::to_string(i) }), "initial entry failed");
            RE::TESModelTextureSwap model{ initial.release(), oldCount };
            auto* original = model.alternateTextures;
            std::vector<PendingModelTexture> pending;
            for (unsigned i{}; i < newCount; ++i) {
                auto* created = RE::engineForms.emplace_back(new RE::BGSTextureSet{});
                pending.emplace_back(ModelTextureTarget{ .index3D = i, .name3D = "node" + std::to_string(i) },
                    TextureBinding{ .textureSet = created });
            }
            // Duplicate target intentionally overestimates allocation capacity;
            // the cookie must still count only constructed elements.
            if (newCount) {
                auto* created = RE::engineForms.emplace_back(new RE::BGSTextureSet{});
                pending.emplace_back(ModelTextureTarget{ .index3D = 0, .name3D = "node0" },
                    TextureBinding{ .textureSet = created });
            }
            ModelArrayHeap::fail = fail;
            const auto success = SetModelAlternateTextures(model, pending);
            Check(success == (newCount == 0 || !fail), "array failure boundary changed");
            if (!success) Check(model.alternateTextures == original && model.numAlternateTextures == oldCount,
                "failed array construction mutated original model");
            else {
                Check(model.numAlternateTextures == std::max(oldCount, newCount), "duplicate model target added an extra row");
                if (newCount) Check(model.alternateTextures[0].textureSet == pending.back().binding.textureSet,
                    "last duplicate target not applied");
                for (auto& item : pending) static_cast<void>(item.construction.release());
            }
            pending.clear();
            Check(success || RE::engineForms.empty(), "unpublished model TXST leaked after array failure");
            // Reproduce the independently observed engine destructor contract,
            // not ModelArrayConstruction::Destroy, to check the exported layout.
            if (model.alternateTextures) {
                auto* head = reinterpret_cast<std::size_t*>(model.alternateTextures) - 1;
                Check(*head == model.numAlternateTextures, "engine array cookie includes unconstructed entries");
                std::destroy_n(model.alternateTextures, *head);
                ModelArrayHeap::Free(head);
            }
            while (!RE::engineForms.empty()) RE::engineForms.back()->DecRefCount();
            Check(ModelArrayHeap::allocations.empty(), "model array allocation retained after cleanup");
        }
        ModelArrayHeap::fail = false;
        Array overflow;
        Check(!overflow.Allocate(std::numeric_limits<std::size_t>::max()), "array byte size overflow accepted");
    }
}

namespace graph_probe {
    using GraphConstruction = bcn::native_skin::GraphConstruction<RE::TESForm, RE::BGSTextureSet>;
    struct ArmorGraph { RE::TESObjectARMO* armor{}; };
    struct BaseInstance {
        RE::TESNPC* base{};
        unsigned ownerActor{};
        RE::TESObjectARMO* originalSkin{};
        RE::TESObjectARMO* originalFarSkin{};
        ArmorGraph skin, farSkin;
    };
    int failStep{}, step{};
    bool IsTngSkinArmor(RE::TESObjectARMO*) { return false; }
    std::optional<ArmorGraph> CloneArmorGraph(RE::TESObjectARMO*, int, bcn::SkinUvLayout,
        void*, GraphConstruction& construction, bool = false) {
        auto* armor = new RE::TESObjectARMO;
        RE::TESForm::owned.insert(armor); construction.OwnForm(armor);
        if (step++ == failStep) return std::nullopt;
        for (unsigned i{}; i < 2; ++i) {
            auto* form = new RE::TESForm;
            RE::TESForm::owned.insert(form); construction.OwnForm(form);
            if (step++ == failStep) return std::nullopt;
            auto* texture = RE::engineForms.emplace_back(new RE::BGSTextureSet{});
            construction.OwnTexture(texture); form->texture = texture;
            if (step++ == failStep) return std::nullopt;
        }
        return ArmorGraph{ armor };
    }
    #include "build_instance.inc"
    void Run() {
        for (unsigned repeat{}; repeat < 100; ++repeat)
        for (failStep = -1; failStep < 10; ++failStep) {
            RE::TESObjectARMO body, far;
            RE::TESNPC base{ &body, &far };
            RE::Actor actor{ &base };
            step = 0;
            auto instance = BuildInstance(&actor, bcn::SkinUvLayout::cbbe);
            Check(instance.has_value() == (failStep == -1), "graph construction failure not propagated");
            Check(base.skin == &body && base.farSkin == &far, "construction published a partial graph");
            if (instance) {
                instance.reset(); // Published/persistent forms must not be deleted here.
                Check(RE::TESForm::owned.size() == 6 && RE::engineForms.size() == 4,
                    "persistent graph lost forms/texture references");
                while (!RE::TESForm::owned.empty()) delete *RE::TESForm::owned.begin();
                while (!RE::engineForms.empty()) RE::engineForms.back()->DecRefCount();
            }
            Check(RE::TESForm::owned.empty() && RE::engineForms.empty() && RE::TESForm::ordered,
                "body/far construction failed to reclaim forms before texture references");
        }
    }
}

int main() try {
#ifdef BCNG_OFFLINE_MEMORY_PROBE
    // Positive control: prove the meter observes retained bytes before using
    // zero deltas as evidence. Release the control allocation immediately.
    const auto controlBefore = allocation_meter::Read();
    auto* control = ::operator new(4096);
    const auto controlHeld = allocation_meter::Read();
    Check(controlHeld.bytes - controlBefore.bytes == 4096 && controlHeld.blocks - controlBefore.blocks == 1,
        "allocation meter positive control failed");
    ::operator delete(control);
    Check(allocation_meter::Read().bytes == controlBefore.bytes, "positive control retained bytes");
    std::printf("POSITIVE_CONTROL retained=4096 bytes blocks=1; after-free=0 bytes\n");
    // Isolate callback infrastructure cost from actor captures/continuations.
    // Same host callback base on both sides; mirrors the previous field layout.
    struct LegacyCallback final : RE::BSScript::IStackCallbackFunctor {
        std::function<void(int)> fn;
        void operator()(int) override {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };
    const auto layoutBefore = allocation_meter::Read();
    auto* oldCallback = new LegacyCallback;
    const auto oldLayout = allocation_meter::Read().bytes - layoutBefore.bytes;
    delete oldCallback;
    auto* newCallback = new vm_probe::Callback({}, {});
    const auto newLayout = allocation_meter::Read().bytes - layoutBefore.bytes;
    delete newCallback;
    Check(allocation_meter::Read().bytes == layoutBefore.bytes, "callback infrastructure retained bytes");
    std::printf("CALLBACK_INFRASTRUCTURE old=%zu new=%zu incremental=%zu bytes per live callback\n",
        oldLayout, newLayout, newLayout - oldLayout);
    const std::pair<const char*, void(*)()> probes[] = {
        {"face-callback", vm_probe::Run}, {"face-finish", finish_probe::Run},
        {"txst-construction", clone_probe::Run}, {"detach-retry", detach_probe::Run},
        {"model-array", model_array_probe::Run}, {"body-far-construction", graph_probe::Run}
    };
    for (const auto& [name, run] : probes) {
        for (unsigned warm{}; warm < 3; ++warm) run();
        allocation_meter::ResetPeak();
        const auto before = allocation_meter::Read();
        for (unsigned cycle{}; cycle < 100; ++cycle) run();
        const auto after = allocation_meter::Read();
        const auto byteDelta = static_cast<long long>(after.bytes) - static_cast<long long>(before.bytes);
        const auto blockDelta = static_cast<long long>(after.blocks) - static_cast<long long>(before.blocks);
        std::printf("%s cycles=100 allocations=%zu peak-extra-bytes=%zu retained-bytes=%lld retained-blocks=%lld\n",
            name, after.allocations - before.allocations, after.peak - before.bytes, byteDelta, blockDelta);
        Check(byteDelta == 0 && blockDelta == 0, "offline probe has residual allocation growth");
    }
    std::printf("SIZES callback=%zu function=%zu shared-pointer=%zu mutex=%zu graph-construction=%zu\n",
        sizeof(vm_probe::Callback), sizeof(std::function<void(int)>), sizeof(std::shared_ptr<int>),
        sizeof(std::mutex), sizeof(graph_probe::GraphConstruction));
    std::printf("SCOPE: real host C++ allocations; fake VM/engine; successful persistent test forms explicitly cleaned by harness. Not whole-game leak bytes.\n");
#else
    vm_probe::Run(); finish_probe::Run(); clone_probe::Run(); detach_probe::Run(); model_array_probe::Run(); graph_probe::Run();
    std::cout << "Failure path probes passed: 10 VM scenarios, 4 Finish/rollback routes, 2300 TXST attempts, 12 detach/retry scenarios, 1800 model-array cases, 1100 body/far graph cases (fake VM/allocator, not engine proof)\n";
#endif
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
