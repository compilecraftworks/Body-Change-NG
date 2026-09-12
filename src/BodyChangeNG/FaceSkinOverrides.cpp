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
#include <SKSE/Logger.h>
#include <algorithm>
#include <cctype>
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
        std::function<void(bool)> completion;
        std::uint64_t generation{};
        std::uint64_t activeGeneration{};
        bool running{};
        bool complete{};
        bool hasSelection{};
        RebuildGate rebuild;
        std::function<bool()> rebuildDispatch;
        std::uint64_t rebuildTicket{};
    };
    std::mutex g_mutex;
    std::unordered_map<std::uint32_t, Request> g_requests;
    std::vector<Baseline> g_baselines;
    std::uint64_t g_epoch{ 1 }, g_generation{};

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
            finished = true;
            bool newer{};
            {
                std::scoped_lock lock(g_mutex);
                auto it = g_requests.find(actorId);
                if (it == g_requests.end() || !OwnsActiveBatch(epoch, g_epoch, generation, it->second.activeGeneration)) return;
                it->second.running = false;
                newer = it->second.generation != generation;
                if (!newer) it->second.complete = success;
            }
            if (!newer) {
                if (!success) {
                    if (auto actor = Actor()) bcn::ActorRegistry::Get().InvalidateSkin(actor.get());
                    SKSE::log::warn("BCNG face NiOverride transaction incomplete actor={:08X} profile='{}'", actorId, request.profile);
                }
                if (request.completion) request.completion(success);
            } else Pump(actorId);
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
            if (channel == kChannels.size()) {
                channel = 0;
                ApplyChannel();
                return;
            }
            Read(true, [self = shared_from_this()](std::string saved) {
                self->baseline.saved[self->channel] = std::move(saved);
                self->Read(false, [self](std::string visible) {
                    const auto actor = self->Actor();
                    const auto property = visible;
                    visible = CaptureVisiblePath(visible,
                        VisibleTextureName(actor.get(), self->baseline.node, kChannels[self->channel]));
                    if (property.empty() && !visible.empty())
                    if (!CanRestoreChannel(kChannels[self->channel], !visible.empty())) {
                        SKSE::log::warn("BCNG face baseline unavailable actor={:08X} node='{}' channel={}; no face write",
                            self->actorId, self->baseline.node, kChannels[self->channel]);
                        self->Finish(false);
                        return;
                    }
                    self->baseline.visible[self->channel] = std::move(visible);
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
                        self->baseline.saved[index] = current;
                    }
                    self->baseline.touched |= static_cast<std::uint8_t>(1U << index);
                    if (self->persistent) {
                        if (Owns(current, OwnedValue(self->baseline, index, current))) self->baseline.owned[index] = current;
                        self->baseline.pending[index] = desired;
                    }
                    if (!Store(self->baseline)) { self->Finish(false); return; }
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
                if (Owns(current, owned)) {
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
            auto actor = request.handle.get();
            auto* base = actor ? actor->GetActorBase() : nullptr;
            auto* head = base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
            if (!actor || !base || !head || head->formEditorID.empty() || !actor->Is3DLoaded()) { Finish(false); return; }
            baseline = { .actor = actorId, .base = base->GetFormID(), .node = head->formEditorID.c_str(),
                .female = base->GetSex() == RE::SEX::kFemale };
            persistent = Persistent(actor->IsPlayerRef());
            capturing = true;
            {
                std::scoped_lock lock(g_mutex);
                const auto it = std::ranges::find_if(g_baselines, [&](const auto& value) { return SameTarget(value, baseline); });
                if (it != g_baselines.end()) { baseline = *it; capturing = false; }
            }
            if (capturing && std::ranges::all_of(request.paths, [](const auto& path) { return path.empty(); })) {
                Finish(true);
                return;
            }
            if (capturing) Capture();
            else ApplyChannel();
        }
    };

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
                std::function<void(bool)> completion;
                RE::ActorHandle handle;
                {
                    std::scoped_lock lock(g_mutex);
                    const auto it = g_requests.find(actorId);
                    if (g_epoch != epoch || it == g_requests.end() || it->second.rebuildTicket != rebuildTicket) return;
                    it->second.rebuild.Complete();
                    completion = it->second.completion;
                    handle = it->second.handle;
                    if (!it->second.hasSelection && !it->second.rebuild.Blocked()) g_requests.erase(it);
                }
                if (auto actor = handle.get()) bcn::ActorRegistry::Get().InvalidateSkin(actor.get());
                if (completion) completion(false);
                SKSE::log::warn("BCNG face rebuild dispatch failed actor={:08X}; face not marked applied", actorId);
            }
            return;
        }
        batch->nodeAccess = NodeAccess::Connect();
        batch->BeginTarget();
    }
    void Submit(RE::Actor* actor, Paths paths, std::string profile, std::function<void(bool)> completion,
        bool deferForRebuild)
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
        std::string profileId, std::function<void(bool)> completion, bool deferForRebuild)
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
        Submit(actor, std::move(paths), std::move(profileId), std::move(completion), deferForRebuild);
    }
    void Clear(RE::Actor* actor, std::function<void(bool)> completion, bool deferForRebuild)
    {
        Submit(actor, {}, {}, std::move(completion), deferForRebuild);
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
            request.rebuild.Complete();
            request.generation = ++g_generation;
            request.complete = false;
            epoch = g_epoch;
            if (!request.hasSelection && !request.rebuild.Blocked() && !request.running) {
                g_requests.erase(it);
                return;
            }
        }
        // This event can arrive outside the game thread, so never touch the
        // rebuilt head here. Queue directly onto SKSE's game task queue. Going
        // back through BCNG's input-tick actor queue added a quiet lease tick
        // after the already-completed rebuild, which was the remaining visible
        // gap between body and face. Generation/epoch checks still collapse
        // repeated NiNode events and newer skin selections supersede this task.
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([id, epoch] {
                { std::scoped_lock lock(g_mutex); if (g_epoch != epoch) return; }
                Pump(id);
            });
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
    bool Matches(const RE::Actor* actor, std::string_view profileId)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_mutex);
        const auto it = g_requests.find(actor->GetFormID());
        return it != g_requests.end() && it->second.complete && it->second.profile == profileId;
    }
    bool Pending(const RE::Actor* actor, std::string_view profileId)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_mutex);
        const auto it = g_requests.find(actor->GetFormID());
        // Includes a newer request waiting behind its cancelled batch. Do not
        // keep replacing that pending generation on every reconciliation pass.
        return it != g_requests.end() && (it->second.running || it->second.rebuild.Blocked()) &&
            !it->second.complete && it->second.profile == profileId;
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
