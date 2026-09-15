#include "BodyChangeNG/FaceSkinOverrides.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/NativeTexturePath.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/FaceSkinSerialization.h"
#include "BodyChangeNG/FaceSkinNodeAccess.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include <RE/P/PackUnpack.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderMaterialFacegen.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/N/NiRTTI.h>
#include <RE/B/BGSBodyPartDefs.h>
#include <RE/M/MiddleHighProcessData.h>
#include <RE/R/RaceSexMenu.h>
#include <SKSE/Logger.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace
{
    using namespace bcn::face_skin;
    using VM = RE::BSScript::Internal::VirtualMachine;
    using Result = RE::BSScript::Variable;
    struct Request
    {
        RE::ActorHandle handle;
        Paths paths;
        std::string profile;
        bcn::skin_transaction::Mode mode{ bcn::skin_transaction::Mode::commit };
        Paths goodPaths;
        std::string goodProfile;
        bcn::skin_transaction::Mode goodMode{ bcn::skin_transaction::Mode::commit };
        std::function<void(bool)> completion;
        std::uint64_t generation{};
        std::uint64_t activeGeneration{};
        bool running{};
        bool complete{};
        bool hasSelection{};
        RebuildGate rebuild;
        std::function<bool()> rebuildDispatch;
        std::uint64_t rebuildTicket{};
        std::chrono::steady_clock::time_point rebuildStarted;
        bool nativeReturned{}, checkQueued{}, eventTaskQueued{};
    };
    std::mutex g_mutex;
    std::unordered_map<std::uint32_t, Request> g_requests;
    std::vector<Baseline> g_baselines;
    std::uint64_t g_epoch{ 1 }, g_generation{};
    static_assert(offsetof(RE::MiddleHighProcessData, update3DModel) == 0x311);

    // Read-only, game-thread observation. Never retain a geometry/material or
    // texture pointer in a request or in the serialized baseline.
    std::string VisibleTextureName(RE::Actor* actor, const std::string& node, unsigned channel)
    {
        if (!actor || bcn::runtime::ResolveGameBranch(REL::Module::get().version()) ==
                bcn::runtime::GameBranch::unsupported) return {};
        auto* root = actor->Get3D(false);
        auto* object = root ? root->GetObjectByName(RE::BSFixedString(node)) : nullptr;
        auto* geometry = object ? object->AsGeometry() : nullptr;
        auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
        auto* raw = shader ? shader->material : nullptr;
        if (!raw || raw->GetType() != RE::BSShaderMaterial::Type::kLighting) return {};
        auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(raw);
        const auto feature = material->GetFeature();
        if (feature != RE::BSShaderMaterial::Feature::kFaceGen &&
            feature != RE::BSShaderMaterial::Feature::kFaceGenRGBTint) return {};
        RE::NiTexture* texture{};
        switch (channel) {
        case 0: texture = material->diffuseTexture.get(); break;
        case 1: texture = material->normalTexture.get(); break;
        case 2: texture = feature == RE::BSShaderMaterial::Feature::kFaceGen ?
            static_cast<RE::BSLightingShaderMaterialFacegen*>(material)->subsurfaceTexture.get() :
            material->rimSoftLightingTexture.get(); break;
        case 3:
            if (feature == RE::BSShaderMaterial::Feature::kFaceGen)
                texture = static_cast<RE::BSLightingShaderMaterialFacegen*>(material)->detailTexture.get();
            break;
        case 7: texture = material->specularBackLightingTexture.get(); break;
        }
        const auto* source = netimmerse_cast<RE::NiSourceTexture*>(texture);
        return source && !source->name.empty() ? std::string(source->name.c_str()) : std::string{};
    }

    std::string OriginalDetailPath(RE::TESNPC* base)
    {
        if (!base) return {};
        // Repair old baselines which mistook an empty live TXST string for
        // an absent complexion. BCNG does not mutate these native face forms.
        auto* form = base->headRelatedData ? base->headRelatedData->faceDetails : nullptr;
        auto* root = base->GetRootFaceNPC();
        if (!form && root && root != base && root->headRelatedData) form = root->headRelatedData->faceDetails;
        auto* race = base->GetRace();
        const auto sex = static_cast<std::size_t>(base->GetSex());
        if (!form && race && sex < RE::SEXES::kTotal && race->faceRelatedData[sex])
            form = race->faceRelatedData[sex]->defaultFaceDetailsTextureSet;
        const auto* path = form ? form->GetTexturePath(RE::BSTextureSet::Textures::kDetailMap) : nullptr;
        return path ? ReloadableTexturePath(path) : std::string{};
    }

    bool SameTarget(const Baseline& a, const Baseline& b)
    {
        return a.actor == b.actor && a.base == b.base && a.node == b.node && a.female == b.female;
    }
    bool Store(const Baseline& value)
    {
        std::scoped_lock lock(g_mutex);
        auto it = std::ranges::find_if(g_baselines, [&](const auto& other) { return SameTarget(value, other); });
        if (it == g_baselines.end()) {
            if (g_baselines.size() >= kMaxBaselines) return false;
            g_baselines.push_back(value);
        }
        else *it = value;
        return true;
    }
    void Pump(std::uint32_t actor);



    class Callback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        explicit Callback(std::function<void(Result)> fn) : fn_(std::move(fn)) {}
        void operator()(Result result) override
        {
            // Papyrus completion is not guaranteed to run on the game thread.
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([fn = std::move(fn_), result]() mutable { fn(result); });
            }
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    private:
        std::function<void(Result)> fn_;
    };

    struct Batch : std::enable_shared_from_this<Batch>
    {
        std::uint32_t actorId{};
        std::uint64_t epoch{}, generation{};
        Request request;
        NodeAccess nodeAccess;
        Baseline baseline;
        Baseline beforeBaseline;
        Paths beforeVisible, beforeSaved;
        std::uint8_t mutated{};
        bool hadBaseline{}, rollingBack{}, rollbackDone{}, rollbackFailed{};
        std::vector<Baseline> oldTargets;
        std::size_t oldTarget{}, channel{};
        bool capturing{}, persistent{}, finished{}, cleaningOld{};

        bool Current()
        {
            std::scoped_lock lock(g_mutex);
            auto it = g_requests.find(actorId);
            // A newer click replaces the pending request, not the batch that
            // is already writing the five face channels.  Cancelling that
            // batch between diffuse and normal leaves a mixed/renderer-less
            // face (purple).  Finish the active generation, then Finish()
            // pumps only the newest request.
            return it != g_requests.end() && OwnsActiveBatch(
                epoch, g_epoch, generation, it->second.activeGeneration);
        }
        RE::NiPointer<RE::Actor> Actor()
        {
            auto actor = request.handle.get();
            if (!actor || actor->GetFormID() != actorId || !actor->GetActorBase() ||
                actor->GetActorBase()->GetFormID() != baseline.base) return {};
            return actor;
        }
        void Finish(bool success)
        {
            if (finished) return;
            if (!success && Current() && !cleaningOld && mutated != 0U && !rollbackDone) {
                if (rollingBack) {
                    rollbackFailed = true;
                    ++channel;
                } else {
                    rollingBack = true;
                    channel = 0;
                }
                RollbackChannel();
                return;
            }
            finished = true;
            bool newer{};
            std::function<void(bool)> completion;
            {
                std::scoped_lock lock(g_mutex);
                auto it = g_requests.find(actorId);
                if (it == g_requests.end() || !OwnsActiveBatch(epoch, g_epoch, generation, it->second.activeGeneration)) return;
                it->second.running = false;
                newer = it->second.generation != generation;
                if (success) {
                    it->second.goodPaths = request.paths;
                    it->second.goodProfile = request.profile;
                    it->second.goodMode = request.mode;
                }
                if (!newer) {
                    it->second.complete = success;
                    completion = std::move(it->second.completion);
                    if (!success) {
                        // A recovery rebuild must replay the last completed face,
                        // never repeat the failed texture batch indefinitely.
                        it->second.paths = it->second.goodPaths;
                        it->second.profile = it->second.goodProfile;
                        it->second.mode = it->second.goodMode;
                        it->second.complete = false;
                    }
                }
            }
            if (!newer) {
                if (!success) {
                    if (auto actor = Actor()) bcn::ActorRegistry::Get().InvalidateSkin(actor.get());
                    SKSE::log::warn("BCNG face NiOverride transaction incomplete actor={:08X} profile='{}'", actorId, request.profile);
                }
                if (completion) completion(success);
            } else {
                // Publish the drained generation's result before the next batch
                // so its body/face rollback snapshots describe the same skin.
                if (request.completion) request.completion(success);
                Pump(actorId);
            }
        }
        void RollbackChannel()
        {
            if (!Current()) { rollbackDone = true; Finish(false); return; }
            while (channel < kChannels.size() && (mutated & (1U << channel)) == 0U) ++channel;
            if (channel == kChannels.size()) {
                rollbackDone = true;
                if (!rollbackFailed) {
                    if (hadBaseline) Store(beforeBaseline);
                    else {
                        std::scoped_lock lock(g_mutex);
                        std::erase_if(g_baselines, [&](const auto& row) { return SameTarget(row, baseline); });
                    }
                }
                Finish(false);
                return;
            }
            Read(true, [self = shared_from_this()](std::string current) {
                const auto index = self->channel;
                // Do not undo a later provider's key while an asynchronous call
                // was in flight. The snapshot contains strings, not engine refs.
                if (!CanRollbackKey(current, self->beforeSaved[index], self->request.paths[index],
                        OwnedValue(self->baseline, index, current))) {
                    self->rollbackFailed = true;
                    ++self->channel;
                    self->RollbackChannel();
                    return;
                }
                auto visible = [self, index] {
                    auto next = [self] { ++self->channel; self->RollbackChannel(); };
                    if (self->beforeVisible[index].empty()) {
                        // An absent optional map is restored by the native rebuild;
                        // LoadTexture("") would install an engine fallback instead.
                        next();
                    } else self->Write(self->beforeVisible[index], false,
                        [self, index, next] { self->Verify(self->beforeVisible[index], next); });
                };
                if (current == self->beforeSaved[index]) visible();
                else if (self->beforeSaved[index].empty()) self->Remove(std::move(visible));
                else self->Write(self->beforeSaved[index], true, std::move(visible));
            });
        }
        template<class... Args>
        void Call(const char* name, std::function<void(Result)> continuation, Args... args)
        {
            if (!Current()) { Finish(false); return; }
            auto actor = Actor();
            auto* vm = VM::GetSingleton();
            if (!actor || !vm || !bcn::frame_tasks::Active()) { Finish(false); return; }
            auto self = shared_from_this();
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new Callback(
                [self, continuation = std::move(continuation)](Result result) {
                    if (!self->Current()) { self->Finish(false); return; }
                    continuation(result);
                }));
            if (!vm->DispatchStaticCall("NiOverride", name, RE::MakeFunctionArguments(std::move(args)...), callback)) {
                SKSE::log::error("BCNG face could not dispatch NiOverride.{} actor={:08X}", name, actorId);
                Finish(false);
            }
        }
        void Read(bool saved, std::function<void(std::string)> next)
        {
            if (!Current()) { Finish(false); return; }
            auto actor = Actor();
            if (!actor) { Finish(false); return; }
            if (nodeAccess && !cleaningOld) {
                std::string value;
                if (!bcn::frame_tasks::Active() || !nodeAccess.Read(actor.get(), baseline.female,
                        baseline.node, kChannels[channel], saved, value)) { Finish(false); return; }
                next(std::move(value));
                return;
            }
            Call(saved ? "GetNodeOverrideString" : "GetNodePropertyString",
                [self = shared_from_this(), next = std::move(next)](Result result) {
                    if (!result.IsString()) { self->Finish(false); return; }
                    next(std::string(result.GetString()));
                }, static_cast<RE::TESObjectREFR*>(actor.get()), saved ? baseline.female : false,
                RE::BSFixedString(baseline.node), std::int32_t{9}, std::int32_t{kChannels[channel]});
        }
        void Write(const std::string& path, bool persist, std::function<void()> next)
        {
            if (!Current()) { Finish(false); return; }
            // NiOverride's empty string is LoadTexture(""), not texture removal.
            // TuLED returned BSShader_DefNormalMap for this call (slot 3).
            if (path.empty()) {
                SKSE::log::warn("BCNG face has no reloadable restoration path actor={:08X} channel={}; refusing empty texture load",
                    actorId, kChannels[channel]);
                Finish(false);
                return;
            }
            auto actor = Actor();
            if (!actor) { Finish(false); return; }
            if (nodeAccess && !cleaningOld) {
                if (!bcn::frame_tasks::Active() || !nodeAccess.Write(actor.get(), baseline.female,
                        baseline.node, kChannels[channel], path, persist)) { Finish(false); return; }
                next();
                return;
            }
            Call("AddNodeOverrideString", [next = std::move(next)](Result) { next(); },
                static_cast<RE::TESObjectREFR*>(actor.get()), baseline.female,
                RE::BSFixedString(baseline.node), std::int32_t{9}, std::int32_t{kChannels[channel]},
                RE::BSFixedString(path), persist);
        }
        void Remove(std::function<void()> next)
        {
            if (!Current()) { Finish(false); return; }
            auto actor = Actor();
            if (!actor) { Finish(false); return; }
            if (nodeAccess && !cleaningOld) {
                if (!bcn::frame_tasks::Active() || !nodeAccess.Remove(actor.get(), baseline.female,
                        baseline.node, kChannels[channel])) { Finish(false); return; }
                next();
                return;
            }
            Call("RemoveNodeOverride", [next = std::move(next)](Result) { next(); },
                static_cast<RE::TESObjectREFR*>(actor.get()), baseline.female,
                RE::BSFixedString(baseline.node), std::int32_t{9}, std::int32_t{kChannels[channel]});
        }
        void Capture()
        {
            // Unchanged optional channels need no extra provider calls on a
            // subsequent selection. The first baseline still captures all five.
            while (channel < kChannels.size() && !capturing && request.paths[channel].empty() &&
                (baseline.touched & (1U << channel)) == 0U) ++channel;
            if (channel == kChannels.size()) {
                channel = 0;
                beforeBaseline = baseline;
                ApplyChannel();
                return;
            }
            Read(true, [self = shared_from_this()](std::string saved) {
                self->beforeSaved[self->channel] = saved;
                if (self->capturing) self->baseline.saved[self->channel] = std::move(saved);
                self->Read(false, [self](std::string visible) {
                    const auto actor = self->Actor();
                    visible = CaptureVisiblePath(visible,
                        VisibleTextureName(actor.get(), self->baseline.node, kChannels[self->channel]));
                    // A broken/missing current texture must not prevent a valid
                    // replacement from repairing it. Never load an empty path
                    // during rollback; the native rebuild handles absent maps.
                    self->beforeVisible[self->channel] = visible;
                    if (self->capturing) self->baseline.visible[self->channel] = std::move(visible);
                    ++self->channel;
                    self->Capture();
                });
            });
        }
        void Next()
        {
            ++channel;
            ApplyChannel();
        }
        void Verify(const std::string& expected, std::function<void()> verified = {})
        {
            Read(false, [self = shared_from_this(), expected, verified = std::move(verified)](std::string actual) {
                if (PathIdentity(actual) != PathIdentity(expected)) {
                    SKSE::log::warn("BCNG face DDS readback mismatch actor={:08X} channel={} expected='{}' actual='{}'",
                        self->actorId, kChannels[self->channel], expected, actual);
                    self->Finish(false);
                    return;
                }
                // GetNodePropertyString reads the TEXTURE SET PATH, even when
                // NiOverride's subsequent LoadTexture returned null. Inspect
                // the current head's required texture objects as well. Borrow
                // only inside this game-thread callback; never retain geometry.
                const auto actor = self->Actor();
                const auto textureName = VisibleTextureName(actor.get(), self->baseline.node, kChannels[self->channel]);
                if (!expected.empty() && !textureName.empty() && !Owns(textureName, expected)) {
                    SKSE::log::warn("BCNG face actual texture mismatch actor={:08X} channel={} expected='{}' texture='{}'",
                        self->actorId, kChannels[self->channel], expected, textureName);
                    self->Finish(false);
                    return;
                }
                auto* root = actor ? actor->Get3D(false) : nullptr;
                auto* object = root ? root->GetObjectByName(RE::BSFixedString(self->baseline.node)) : nullptr;
                auto* geometry = object ? object->AsGeometry() : nullptr;
                auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
                auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                const auto shaderChannel = kChannels[self->channel];
                const auto* texture = material ? (shaderChannel == 0 ? material->diffuseTexture.get() :
                    shaderChannel == 1 ? material->normalTexture.get() : nullptr) : nullptr;
                if (!geometry || !RequiredTextureReady(shaderChannel, !expected.empty(),
                        texture != nullptr, texture && texture->rendererTexture)) {
                    SKSE::log::warn("BCNG face texture not loaded actor={:08X} node='{}' channel={} path='{}' texture={} renderer={}",
                        self->actorId, self->baseline.node, shaderChannel, expected,
                        texture != nullptr, texture && texture->rendererTexture);
                    self->Finish(false);
                    return;
                }
                if (verified) verified();
                else self->Next();
            });
        }
        void ApplyChannel()
        {
            if (!Current()) { Finish(false); return; }
            if (channel == kChannels.size()) {
                // Successful Default/old-node cleanup no longer needs a saved
                // restoration snapshot. Do not retain one for every past NPC.
                if (baseline.touched == 0) {
                    std::scoped_lock lock(g_mutex);
                    std::erase_if(g_baselines, [&](const auto& row) { return SameTarget(row, baseline); });
                }
                if (cleaningOld) {
                    ++oldTarget;
                    BeginTarget();
                } else {
                    Finish(true);
                }
                return;
            }
            auto desired = cleaningOld ? std::string{} : request.paths[channel];
            if (!desired.empty()) {
                Read(true, [self = shared_from_this(), desired](std::string current) {
                    const auto index = self->channel;
                    if (!current.empty() && !Owns(current, OwnedValue(self->baseline, index, current))) {
                        // A provider may change its key between asynchronous
                        // capture and this write. Roll back to that newer key.
                        self->beforeBaseline.saved[index] = current;
                        if (current != self->beforeSaved[index]) self->beforeVisible[index] = current;
                    }
                    self->beforeSaved[index] = current;
                    PrepareWrite(self->baseline, index, current, desired, self->persistent);
                    if (!Store(self->baseline)) { self->Finish(false); return; }
                    self->mutated |= static_cast<std::uint8_t>(1U << index);
                    self->Write(desired, self->persistent, [self, desired] { self->Verify(desired); });
                });
                return;
            }
            if ((baseline.touched & (1U << channel)) == 0U) { Next(); return; }
            // Only restore touched channels. Do not add blank keys for an
            // absent detail map and do not remove a later provider's key.
            Read(true, [self = shared_from_this()](std::string current) {
                const auto index = self->channel;
                const auto owned = std::string(OwnedValue(self->baseline, index, current));
                auto visible = RestoreVisible(current, owned,
                    self->baseline.saved[index], self->baseline.visible[index]);
                if (visible.empty() && kChannels[index] == 3 && !self->cleaningOld) {
                    const auto actor = self->Actor();
                    visible = OriginalDetailPath(actor ? actor->GetActorBase() : nullptr);
                    if (!visible.empty()) {
                        self->baseline.visible[index] = visible;
                        if (!Store(self->baseline)) { self->Finish(false); return; }
                    }
                }
                if (!CanRestoreChannel(kChannels[index], !visible.empty())) {
                    SKSE::log::warn("BCNG face refuses empty required Default actor={:08X} channel={}",
                        self->actorId, kChannels[index]);
                    self->Finish(false);
                    return;
                }
                if (visible.empty() && !self->cleaningOld) {
                    // Preserve restoration ownership instead of deleting its
                    // key and then discovering that there is no loadable DDS.
                    SKSE::log::warn("BCNG face missing original optional DDS actor={:08X} channel={}; baseline retained",
                        self->actorId, kChannels[index]);
                    self->Finish(false);
                    return;
                }
                auto finish = [self, visible] {
                    auto commit = [self] {
                        if (self->request.mode == bcn::skin_transaction::Mode::preview && !self->cleaningOld) {
                            // Default preview changes only live material. Keep the
                            // committed key and its restoration ownership intact.
                            self->Next();
                            return;
                        }
                        self->baseline.owned[self->channel].clear();
                        self->baseline.pending[self->channel].clear();
                        self->baseline.touched &= static_cast<std::uint8_t>(~(1U << self->channel));
                        if (!Store(self->baseline)) { self->Finish(false); return; }
                        self->Next();
                    };
                    if (self->cleaningOld) commit();
                    else self->Write(visible, false, [self, visible, commit = std::move(commit)] {
                        self->Verify(visible, commit);
                    });
                };
                self->mutated |= static_cast<std::uint8_t>(1U << index);
                if (self->request.mode == bcn::skin_transaction::Mode::preview && !self->cleaningOld) finish();
                else if (Owns(current, owned)) {
                    if (self->baseline.saved[index].empty()) self->Remove(std::move(finish));
                    else self->Write(self->baseline.saved[index], true, std::move(finish));
                } else finish();
            });
        }
        void BeginTarget()
        {
            channel = 0;
            if (oldTarget < oldTargets.size()) {
                baseline = oldTargets[oldTarget];
                cleaningOld = true;
                ApplyChannel();
                return;
            }
            cleaningOld = false;
            mutated = 0;
            auto actor = request.handle.get();
            auto* base = actor ? actor->GetActorBase() : nullptr;
            auto* head = base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
            if (!actor || !base || !head || head->formEditorID.empty() || !actor->Is3DLoaded()) { Finish(false); return; }
            baseline = { .actor = actorId, .base = base->GetFormID(), .node = head->formEditorID.c_str(),
                .female = base->GetSex() == RE::SEX::kFemale };
            persistent = bcn::skin_transaction::PersistsFace(actor->IsPlayerRef(), request.mode);
            capturing = true;
            {
                std::scoped_lock lock(g_mutex);
                const auto it = std::ranges::find_if(g_baselines, [&](const auto& value) { return SameTarget(value, baseline); });
                if (it != g_baselines.end()) { baseline = *it; capturing = false; }
            }
            hadBaseline = !capturing;
            if (capturing && std::ranges::all_of(request.paths, [](const auto& path) { return path.empty(); })) {
                Finish(true);
                return;
            }
            Capture();
        }
    };

    void FailRebuild(std::uint32_t actorId, std::uint64_t ticket, std::uint64_t epoch)
    {
        std::function<void(bool)> completion;
        RE::ActorHandle handle;
        {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(actorId);
            if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != ticket) return;
            auto& request = it->second;
            request.rebuild.Cancel();
            request.rebuildDispatch = {};
            request.nativeReturned = request.checkQueued = false;
            // Consume once: recovery refresh must not recursively call this
            // same failed transaction's completion again.
            completion = std::move(request.completion);
            handle = request.handle;
            request.paths = request.goodPaths;
            request.profile = request.goodProfile;
            request.mode = request.goodMode;
            request.complete = false;
            if (!request.hasSelection && !request.running) g_requests.erase(it);
        }
        if (auto actor = handle.get()) bcn::ActorRegistry::Get().InvalidateSkin(actor.get());
        if (completion) completion(false);
        SKSE::log::warn("BCNG native skin rebuild did not complete actor={:08X}; selection not marked applied", actorId);
    }

    // Game-task-thread observation, after the native DoReset3D call returns.
    // No geometry/material pointers are kept across callbacks. The pending
    // byte belongs to the pinned flat SE/AE AIProcess layout, not a VR layout.
    RebuildObservation ReadRebuildState(RE::Actor* actor, bool needsFace, bool expired)
    {
        if (!actor || !actor->Is3DLoaded() ||
            bcn::runtime::ResolveGameBranch(REL::Module::get().version()) ==
                bcn::runtime::GameBranch::unsupported) return RebuildObservation::failed;
        auto* process = actor->GetActorRuntimeData().currentProcess;
        if (!process || !process->middleHigh) return RebuildObservation::failed;
        bool pending = process->middleHigh->update3DModel.underlying() != 0;
        if (actor->IsPlayerRef()) {
            auto* ui = RE::UI::GetSingleton();
            pending |= ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
        }
        bool ready = !needsFace;
        if (needsFace && !pending) {
            auto* base = actor->GetActorBase();
            auto* head = base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
            auto* root = actor->Get3D(false);
            auto* object = head && root && !head->formEditorID.empty() ?
                root->GetObjectByName(head->formEditorID) : nullptr;
            auto* geometry = object ? object->AsGeometry() : nullptr;
            auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
            ready = shader && shader->material &&
                shader->material->GetType() == RE::BSShaderMaterial::Type::kLighting;
            // Missing current DDS is not a reason to reject its replacement.
            // The applied diffuse/normal GPU resources are verified by Batch.
        }
        return ObserveRebuild(true, pending, ready, expired);
    }

    void CheckRebuild(std::uint32_t actorId, std::uint64_t ticket, std::uint64_t epoch)
    {
        RE::ActorHandle handle;
        bool needsFace{}, expired{};
        {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(actorId);
            if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != ticket ||
                !it->second.rebuild.inFlight || !it->second.nativeReturned) return;
            handle = it->second.handle;
            needsFace = it->second.hasSelection;
            expired = std::chrono::steady_clock::now() - it->second.rebuildStarted >= std::chrono::seconds(5);
        }
        const auto actor = handle.get();
        const auto state = ReadRebuildState(actor.get(), needsFace, expired);
        if (state == RebuildObservation::failed) { FailRebuild(actorId, ticket, epoch); return; }
        bool schedule{};
        {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(actorId);
            if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != ticket ||
                !it->second.rebuild.inFlight) return;
            auto& request = it->second;
            if (state == RebuildObservation::ready) {
                request.rebuild.Complete();
                request.nativeReturned = false;
                if (!request.hasSelection && !request.rebuild.Blocked() && !request.running) {
                    g_requests.erase(it);
                    return;
                }
            } else if (!request.checkQueued) {
                request.checkQueued = true;
                schedule = true;
            }
        }
        if (state == RebuildObservation::ready) {
            // Synchronous native completion reaches this in the SAME game
            // task as the body update; no VM or extra input-frame round trip.
            Pump(actorId);
        } else if (schedule) {
            // At most one check per rebuild, paced by external input ticks.
            // Do not re-enqueue directly in SKSE's live-draining task FIFO.
            if (!bcn::frame_tasks::Queue(0, [actorId, ticket, epoch] {
                    {
                        std::scoped_lock lock(g_mutex);
                        const auto it = g_requests.find(actorId);
                        if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != ticket) return;
                        it->second.checkQueued = false;
                    }
                    CheckRebuild(actorId, ticket, epoch);
                }, 1U, bcn::appearance::WorkChannel::none, true)) FailRebuild(actorId, ticket, epoch);
        }
    }

    void Pump(std::uint32_t actorId)
    {
        auto batch = std::make_shared<Batch>();
        std::function<bool()> rebuild;
        std::uint64_t rebuildTicket{}, epoch{};
        {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(actorId);
            if (it == g_requests.end() || it->second.running) return;
            auto& request = it->second;
            if (request.rebuild.Begin(false, static_cast<bool>(request.rebuildDispatch))) {
                rebuild = std::move(request.rebuildDispatch);
                rebuildTicket = request.rebuildTicket = ++g_generation;
                request.rebuildStarted = std::chrono::steady_clock::now();
                request.nativeReturned = request.checkQueued = false;
                epoch = g_epoch;
            } else {
                if (!request.rebuild.CanApply(request.running, request.hasSelection, request.complete)) return;
                auto actor = it->second.handle.get();
                auto* base = actor ? actor->GetActorBase() : nullptr;
                auto* head = base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
                if (!actor || !actor->Is3DLoaded() || !head || head->formEditorID.empty()) return;
                it->second.running = true;
                it->second.activeGeneration = it->second.generation;
                batch->actorId = actorId;
                batch->epoch = g_epoch;
                batch->generation = it->second.generation;
                batch->request = it->second;
                for (const auto& value : g_baselines) {
                    if (value.actor == actorId && value.base == base->GetFormID() &&
                        (value.node != head->formEditorID.c_str() || value.female != (base->GetSex() == RE::SEX::kFemale))) {
                        batch->oldTargets.push_back(value);
                    }
                }
            }
        }
        if (rebuild) {
            if (!rebuild()) {
                FailRebuild(actorId, rebuildTicket, epoch);
            } else {
                {
                    std::scoped_lock lock(g_mutex);
                    const auto it = g_requests.find(actorId);
                    if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != rebuildTicket) return;
                    it->second.nativeReturned = true;
                }
                CheckRebuild(actorId, rebuildTicket, epoch);
            }
            return;
        }
        batch->nodeAccess = NodeAccess::Connect();
        batch->BeginTarget();
    }
    void Submit(RE::Actor* actor, Paths paths, std::string profile, std::function<void(bool)> completion,
        bool deferForRebuild, bcn::skin_transaction::Mode mode)
    {
        if (!actor || !bcn::frame_tasks::Active()) { if (completion) completion(false); return; }
        bool capacityExceeded{};
        {
            std::scoped_lock lock(g_mutex);
            capacityExceeded = !g_requests.contains(actor->GetFormID()) && g_requests.size() >= kMaxBaselines;
            if (!capacityExceeded) {
                auto& request = g_requests[actor->GetFormID()];
                request.handle = actor->GetHandle();
                request.paths = std::move(paths);
                request.profile = std::move(profile);
                request.mode = mode;
                request.completion = std::move(completion);
                request.generation = ++g_generation;
                request.complete = false;
                request.hasSelection = true;
                if (deferForRebuild) request.rebuild.Request();
            }
        }
        if (capacityExceeded) {
            SKSE::log::warn("BCNG face request capacity reached actor={:08X}", actor->GetFormID());
            if (completion) completion(false);
            return;
        }
        Pump(actor->GetFormID());
    }
}

namespace bcn::face_skin
{
    void Apply(RE::Actor* actor, const std::vector<SkinTextureLayer>& layers,
        std::string profileId, std::function<void(bool)> completion, bool deferForRebuild,
        skin_transaction::Mode mode)
    {
        Paths paths;
        for (const auto& layer : layers) {
            const auto found = std::ranges::find(kChannels, layer.shaderTextureIndex);
            if (found == kChannels.end()) continue;
            const auto cached = runtime_assets::ExpectedTexturePathFromGameRelative(layer.path, "skin-face");
            const auto converted = CacheOverridePath(cached);
            if (!converted || !runtime_assets::CachedTextureExists(cached)) {
                if (completion) completion(false);
                return;
            }
            paths[static_cast<std::size_t>(found - kChannels.begin())] = *converted;
        }
        Submit(actor, std::move(paths), std::move(profileId), std::move(completion), deferForRebuild, mode);
    }
    void Clear(RE::Actor* actor, std::function<void(bool)> completion, bool deferForRebuild,
        skin_transaction::Mode mode)
    {
        Submit(actor, {}, {}, std::move(completion), deferForRebuild, mode);
    }
    void QueueRebuild(RE::Actor* actor, std::function<bool()> dispatch)
    {
        if (!actor || !dispatch || !bcn::frame_tasks::Active()) return;
        bool capacityExceeded{};
        {
            std::scoped_lock lock(g_mutex);
            capacityExceeded = !g_requests.contains(actor->GetFormID()) && g_requests.size() >= kMaxBaselines;
            if (!capacityExceeded) {
                auto& request = g_requests[actor->GetFormID()];
                request.handle = actor->GetHandle();
                request.rebuildDispatch = std::move(dispatch);
                request.rebuild.Request();
                request.generation = ++g_generation;
                request.complete = false;
            }
        }
        if (capacityExceeded) {
            // No face work exists for this actor, so there is nothing to drain.
            static_cast<void>(dispatch());
            return;
        }
        Pump(actor->GetFormID());
    }
    void OnNiNodeUpdate(RE::Actor* actor)
    {
        if (!actor) return;
        std::uint64_t epoch{};
        const auto id = actor->GetFormID();
        {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(id);
            if (it == g_requests.end()) return;
            auto& request = it->second;
            request.generation = ++g_generation;
            request.complete = false;
            epoch = g_epoch;
            if (!request.hasSelection && !request.rebuild.Blocked() && !request.running) {
                g_requests.erase(it);
                return;
            }
            // DoReset3D can send this event before its native call returns.
            // It invalidates the face, but must not prematurely open the gate.
            if (request.eventTaskQueued) return;
            request.eventTaskQueued = true;
        }
        // This event can arrive outside the game thread, so never touch the
        // rebuilt head here. Coalesce notifications onto the game task queue.
        // BCNG's own native-return path may already have completed the face
        // before this runs; Pump then observes complete and does nothing.
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([id, epoch] {
                std::uint64_t ticket{};
                bool rebuilding{};
                {
                    std::scoped_lock lock(g_mutex);
                    const auto it = g_requests.find(id);
                    if (g_epoch != epoch || it == g_requests.end()) return;
                    it->second.eventTaskQueued = false;
                    rebuilding = it->second.rebuild.inFlight;
                    ticket = it->second.rebuildTicket;
                }
                if (rebuilding) CheckRebuild(id, ticket, epoch);
                else Pump(id);
            });
        } else {
            std::scoped_lock lock(g_mutex);
            const auto it = g_requests.find(id);
            if (g_epoch == epoch && it != g_requests.end()) it->second.eventTaskQueued = false;
        }
    }
    void Reset(bool preserveBaselines)
    {
        std::scoped_lock lock(g_mutex);
        ++g_epoch;
        decltype(g_requests){}.swap(g_requests);
        if (!preserveBaselines) decltype(g_baselines){}.swap(g_baselines);
    }
    void Forget(std::uint32_t actor)
    {
        std::scoped_lock lock(g_mutex);
        g_requests.erase(actor);
        // Cell detach discards jobs, not the original appearance needed by Default.
    }
    bool Matches(const RE::Actor* actor, std::string_view profileId, skin_transaction::Mode mode)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_mutex);
        const auto it = g_requests.find(actor->GetFormID());
        return it != g_requests.end() && it->second.complete && it->second.profile == profileId && it->second.mode == mode;
    }
    bool Pending(const RE::Actor* actor, std::string_view profileId, skin_transaction::Mode mode)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_mutex);
        const auto it = g_requests.find(actor->GetFormID());
        // Includes a newer request waiting behind its cancelled batch. Do not
        // keep replacing that pending generation on every reconciliation pass.
        return it != g_requests.end() && (it->second.running || it->second.rebuild.Blocked()) &&
            !it->second.complete && it->second.profile == profileId && it->second.mode == mode;
    }
    std::vector<Baseline> SnapshotBaselines()
    {
        std::scoped_lock lock(g_mutex);
        return g_baselines;
    }
    void RestoreBaselines(std::vector<Baseline> values)
    {
        std::scoped_lock lock(g_mutex);
        g_baselines = std::move(values);
    }
}
