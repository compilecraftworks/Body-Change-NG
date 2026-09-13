#include "BodyChangeNG/SkinApplication.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/NativeAddonPolicy.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariSupport.h"
#include "BodyChangeNG/FaceSkinOverrides.h"
#include "BodyChangeNG/NativeAddonSkinBackend.h"
#include "BodyChangeNG/NativeSkinBackend.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinSessionState.h"
#include "BodyChangeNG/SkinTargetResolver.h"

#include <SKSE/Logger.h>

#include <cctype>
#include <optional>
#include <vector>

namespace
{
    constexpr auto kSosMaleGenitalSlot = bcn::skin_target::kSosMaleGenitalSlot;
    using LoadedPartTarget = bcn::skin_target::LoadedPartTarget;
    using LoadedPartView = bcn::skin_target::LoadedPartView;
    using LoadedFutanariRoute = bcn::skin_target::LoadedFutanariRoute;
    using bcn::skin_target::ActiveAddonModelPath;
    using bcn::skin_target::ActiveAddonTextureEvidence;
    using bcn::skin_target::FindLoadedFutanariRoute;
    using bcn::skin_target::FindLoadedPartTargets;

    [[nodiscard]] std::uint64_t AddonTargetSignature(
        const std::vector<LoadedPartTarget>& targets, const std::uint64_t discriminator)
    {
        if (targets.empty()) return 0U;
        std::uint64_t hash = 1469598103934665603ULL;
        const auto appendByte = [&hash](const std::uint8_t value) {
            hash = (hash ^ value) * 1099511628211ULL;
        };
        const auto appendInteger = [&appendByte](const std::uint64_t value) {
            for (std::uint32_t shift{}; shift < 64U; shift += 8U) {
                appendByte(static_cast<std::uint8_t>(value >> shift));
            }
        };
        appendInteger(discriminator);
        for (const auto& target : targets) {
            appendInteger(target.armor ? target.armor->GetFormID() : 0U);
            appendInteger(target.addon ? target.addon->GetFormID() : 0U);
            appendInteger(target.slotMask);
            // Native construction/reuse consumes the published TXST selection
            // for every new geometry. A different heap address is NOT a new
            // source and must not recursively request another NiNode update.
            for (const auto& view : target.views) {
                appendByte(view.firstPerson ? 1U : 0U);
                appendByte(view.actorSkinArmor ? 1U : 0U);
                for (const auto& node : view.nodes) {
                    for (const auto character : node) appendByte(static_cast<std::uint8_t>(character));
                    appendByte(0);
                }
            }
            appendByte(0xFEU);
        }
        return hash == 0U ? 1U : hash;
    }

    [[nodiscard]] std::uint64_t MaleGenitalTargetSignature(RE::Actor* actor)
    {
        return AddonTargetSignature(FindLoadedPartTargets(actor, kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals), 0x4D414C45ULL);
    }

    [[nodiscard]] std::uint64_t FutanariTargetSignature(
        const LoadedFutanariRoute& route)
    {
        const auto discriminator = 0x46555441ULL |
            (static_cast<std::uint64_t>(route.addonKind) << 32U) |
            (static_cast<std::uint64_t>(route.type.value_or(
                bcn::FutanariSkinType::cbbeTrx)) << 40U);
        return route.type ? AddonTargetSignature(route.targets, discriminator) : 0U;
    }

    [[nodiscard]] std::uint64_t BeginSkinChange(const RE::FormID actorFormID)
    {
        return bcn::skin_session::BeginSkinChange(actorFormID);
    }

    [[nodiscard]] bool IsCurrentSkinChange(
        const RE::FormID actorFormID, const std::uint64_t generation)
    {
        return bcn::skin_session::IsCurrentSkinChange(actorFormID, generation);
    }

    [[nodiscard]] std::uint64_t BeginFutanariChange(const RE::FormID actorFormID)
    {
        return bcn::skin_session::BeginFutanariChange(actorFormID);
    }

    [[nodiscard]] bool IsCurrentFutanariChange(
        const RE::FormID actorFormID, const std::uint64_t generation)
    {
        return bcn::skin_session::IsCurrentFutanariChange(actorFormID, generation);
    }

    class NodeUpdateCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        void operator()(RE::BSScript::Variable) override {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    [[nodiscard]] bool QueueNiNodeUpdate(
        RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor)
    {
        if (!actor) return false;
        auto* policy = vm.GetObjectHandlePolicy();
        if (!policy) return false;
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(actor->GetFormType()), actor);
        if (handle == policy->EmptyHandle()) return false;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
            new NodeUpdateCallback());
        return vm.DispatchMethodCall(handle, "Actor", "QueueNiNodeUpdate",
            RE::MakeFunctionArguments(), callback);
    }

    void RefreshNativeSkin3D(RE::Actor* actor)
    {
        if (!actor) return;
        const auto handle = actor->GetHandle();
        bcn::face_skin::QueueRebuild(actor, [handle] {
            const auto actor = handle.get();
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!actor || !actor->Is3DLoaded() || !vm) return false;
            const auto queued = QueueNiNodeUpdate(*vm, actor.get());
            if (queued) {
            } else {
                SKSE::log::warn(
                    "Body Change NG could not queue Actor.QueueNiNodeUpdate after native Skin Armor mutation for actor {:08X}",
                    actor->GetFormID());
            }
            return queued;
        });
    }

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    [[nodiscard]] std::string ActiveVariantNeedle(
        const bcn::MaleGenitalTextureVariant& variant)
    {
        const auto directory = LowerAscii(variant.addonDirectory);
        return directory.empty() ? std::string{} : "\\sos\\" + directory + "\\";
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> MaleGenitalVariantLayers(
        const bcn::MaleGenitalTextureVariant& variant, RE::TESNPC* base)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        const auto actorRace = bcn::SkinRaceFromEditorID(
            editorID ? std::string_view{ editorID } : std::string_view{});
        std::vector<bcn::SkinTextureLayer> layers;
        switch (actorRace) {
        case bcn::SkinRace::argonian: layers = variant.argonian; break;
        case bcn::SkinRace::khajiit: layers = variant.khajiit; break;
        default: layers = variant.humanoid; break;
        }
        if (actorRace == bcn::SkinRace::humanoid && bcn::IsElderActor(base)) {
            bcn::skin_plan::OverlayLayers(layers, variant.elder);
        }
        return layers;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> EffectiveMaleGenitalLayers(
        const bcn::SkinProfile& profile, RE::Actor* actor, RE::TESNPC* base)
    {
        if (!actor || !base || profile.sex != bcn::SkinSex::male ||
            profile.maleGenitals.empty()) return {};
        const auto modelPath = ActiveAddonModelPath(actor, kSosMaleGenitalSlot, false);
        const auto textureEvidence = ActiveAddonTextureEvidence(
            actor, kSosMaleGenitalSlot, false);
        if (modelPath.empty() && textureEvidence.empty()) return {};
        const auto selected = std::ranges::find_if(profile.maleGenitals,
            [&modelPath, &textureEvidence](const auto& variant) {
                const auto needle = ActiveVariantNeedle(variant);
                return !needle.empty() &&
                    (modelPath.contains(needle) || textureEvidence.contains(needle));
            });
        if (selected == profile.maleGenitals.end()) return {};

        std::vector<bcn::SkinTextureLayer> layers;
        if (modelPath.contains("\\sos\\vectorplexus muscular\\") ||
            textureEvidence.contains("\\sos\\vectorplexus muscular\\")) {
            const auto regular = std::ranges::find_if(profile.maleGenitals,
                [](const auto& variant) {
                    return LowerAscii(variant.addonDirectory) == "vectorplexus regular";
                });
            if (regular != profile.maleGenitals.end()) {
                layers = MaleGenitalVariantLayers(*regular, base);
            }
        }
        bcn::skin_plan::OverlayLayers(layers, MaleGenitalVariantLayers(*selected, base));
        return layers;
    }

    [[nodiscard]] std::vector<LoadedPartTarget> NativeAddonTargets(
        RE::Actor* actor, const bool female)
    {
        auto targets = female ? FindLoadedFutanariRoute(actor).targets :
            FindLoadedPartTargets(actor, kSosMaleGenitalSlot,
                bcn::skin_geometry::BodySelection::maleGenitals);
        const auto slot = static_cast<std::uint32_t>(kSosMaleGenitalSlot);
        for (auto& target : targets) {
            std::erase_if(target.views, [&target, slot](const LoadedPartView& view) {
                // SOS and TNG intentionally use this same exact native slot-52
                // admission path. Provider identity never changes ownership.
                return !bcn::native_addon::AcceptTargetView(
                    (target.slotMask & slot) != 0U,
                    view.object != nullptr, !view.nodes.empty());
            });
        }
        std::erase_if(targets, [slot](const LoadedPartTarget& target) {
            return target.views.empty() || (target.slotMask & slot) == 0U;
        });
        return targets;
    }

    [[nodiscard]] bool CurrentAddonChange(
        RE::Actor* actor, const std::uint64_t generation, const bool female)
    {
        if (bcn::runtime::ResolveGameBranch(REL::Module::get().version()) ==
            bcn::runtime::GameBranch::unsupported) return false;
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return base && actor->Is3DLoaded() &&
            (base->GetSex() == RE::SEX::kFemale) == female &&
            (female ? IsCurrentFutanariChange(actor->GetFormID(), generation) :
                IsCurrentSkinChange(actor->GetFormID(), generation));
    }

    void ApplyMaleGenitalsNativeNow(RE::ActorHandle handle,
        const bcn::SkinProfile& profile, const std::uint64_t generation)
    {
        const auto actor = handle.get();
        if (!CurrentAddonChange(actor.get(), generation, false) ||
            profile.contentHash != bcn::SkinProfiles::Get().ContentHash(profile.id)) return;
        const auto targets = NativeAddonTargets(actor.get(), false);
        const auto layers = EffectiveMaleGenitalLayers(
            profile, actor.get(), actor->GetActorBase());
        if (layers.empty()) {
            if (!targets.empty() && !profile.maleGenitals.empty()) {
                SKSE::log::warn(
                    "Native genital TXST: pack '{}' has no verified variant for actor {:08X}, model='{}', source='{}'; keeping provider textures",
                    profile.name, actor->GetFormID(),
                    ActiveAddonModelPath(actor.get(), kSosMaleGenitalSlot, false),
                    ActiveAddonTextureEvidence(actor.get(), kSosMaleGenitalSlot, false));
            }
            if (bcn::native_addon::Clear(actor->GetFormID()) && !targets.empty())
                RefreshNativeSkin3D(actor.get());
            return;
        }
        if (const auto applied = bcn::native_addon::Apply(actor->GetFormID(),
                bcn::native_addon::Channel::maleGenitals, targets, layers, "skin")) {
            bcn::skin_session::MarkAddonReconciled(actor->GetFormID(),
                bcn::skin_session::AddonTextureChannel::maleGenitals,
                MaleGenitalTargetSignature(actor.get()));
            if (applied.changed) RefreshNativeSkin3D(actor.get());
        }
    }

    void ApplyFutanariNativeNow(RE::ActorHandle handle,
        const bcn::FutanariSkinProfile& profile, const std::uint64_t generation)
    {
        const auto actor = handle.get();
        if (!CurrentAddonChange(actor.get(), generation, true) ||
            profile.contentHash != bcn::FutanariSkinProfiles::Get().ContentHash(profile.id)) return;
        const auto route = FindLoadedFutanariRoute(actor.get());
        if (!route.type || *route.type != profile.type) return;
        if (const auto applied = bcn::native_addon::Apply(actor->GetFormID(),
                bcn::native_addon::Channel::futanari,
                NativeAddonTargets(actor.get(), true), profile.layers, "futanari")) {
            bcn::skin_session::MarkAddonReconciled(actor->GetFormID(),
                bcn::skin_session::AddonTextureChannel::futanari,
                bcn::skin_session::FutanariSelectionSignature(
                    FutanariTargetSignature(route), profile.id, profile.contentHash));
            if (applied.changed) RefreshNativeSkin3D(actor.get());
        }
    }

    void RestoreAddonNativeNow(RE::ActorHandle handle,
        const std::uint64_t generation, const bool female)
    {
        const auto actor = handle.get();
        if (!CurrentAddonChange(actor.get(), generation, female)) return;
        const auto sessionChannel = female ?
            bcn::skin_session::AddonTextureChannel::futanari :
            bcn::skin_session::AddonTextureChannel::maleGenitals;
        const auto targets = NativeAddonTargets(actor.get(), female);
        bcn::native_addon::Clear(actor->GetFormID());
        if (!targets.empty()) RefreshNativeSkin3D(actor.get());
        const auto signature = female ?
            FutanariTargetSignature(FindLoadedFutanariRoute(actor.get())) :
            MaleGenitalTargetSignature(actor.get());
        if (signature == 0U) {
            bcn::skin_session::ClearAddonReconciled(actor->GetFormID(), sessionChannel);
        } else {
            bcn::skin_session::MarkAddonReconciled(
                actor->GetFormID(), sessionChannel, signature);
        }
    }

    void QueueMaleGenitalApply(RE::Actor* actor, const bcn::SkinProfile& profile,
        const std::uint64_t generation)
    {
        if (!bcn::native_addon::Available()) {
            SKSE::log::warn("Native genital TXST unavailable; body/face selection remains independent");
            return;
        }
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base || base->GetSex() != RE::SEX::kMale ||
            profile.maleGenitals.empty()) return;
        const auto handle = actor->GetHandle();
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, profile, generation] {
            const auto current = handle.get();
            if (!CurrentAddonChange(current.get(), generation, false)) return;
            std::vector<bcn::runtime_assets::TexturePreparation> paths;
            for (const auto& layer : EffectiveMaleGenitalLayers(
                    profile, current.get(), current->GetActorBase())) {
                paths.push_back({ layer.path, "skin" });
            }
            const auto lease = bcn::frame_tasks::CurrentLease();
            auto ready = [lease, handle, profile, generation](const bool prepared) {
                if (!prepared) return;
                static_cast<void>(bcn::frame_tasks::Continue(lease,
                    [handle, profile, generation] {
                        ApplyMaleGenitalsNativeNow(handle, profile, generation);
                    }));
            };
            if (!bcn::runtime_assets::PrepareTexturePathsAsync(
                    (static_cast<std::uint64_t>(current->GetFormID()) << 2U) | 2U,
                    std::move(paths), ready,
                    bcn::async_work::FrameTaskQueue::InteractiveLease(lease))) {
                ready(false);
            }
        }, 1U, bcn::appearance::WorkChannel::maleGenitalSkinApply);
    }

    void QueueMaleGenitalClear(RE::Actor* actor, const std::uint64_t generation)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base || base->GetSex() != RE::SEX::kMale) return;
        // Clear desired runtime supply even while SOS/TNG is unequipped.
        // Persistent selection is managed separately by ActorRegistry.
        bcn::native_addon::Clear(actor->GetFormID());
        bcn::skin_session::ClearAddonReconciled(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::maleGenitals);
        const auto handle = actor->GetHandle();
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, generation] {
            RestoreAddonNativeNow(handle, generation, false);
        }, 1U, bcn::appearance::WorkChannel::maleGenitalSkinApply);
    }
}

namespace bcn::skin_application
{
    void ResetSessionState()
    {
        face_skin::Reset(true); // PostLoadGame must retain the loaded restoration baseline.
        native_skin::ResetSessionState();
        runtime_assets::CancelTexturePreparations();
        skin_session::Reset();
        native_addon::Reset();
    }

    ApplyResult QueueApply(RE::Actor* actor, std::string profileId)
    {
        const auto actorFormID = actor ? actor->GetFormID() : 0U;
        const auto profile = SkinProfiles::Get().Find(profileId);
        const auto result = native_skin::QueueApply(actor, profileId,
            [](RE::Actor* refreshed) { RefreshNativeSkin3D(refreshed); });
        if (result == ApplyResult::queued && actor) {
            const auto generation = BeginSkinChange(actorFormID);
            skin_session::TrackSkinSelection(actorFormID, std::move(profileId));
            if (profile) {
                if (profile->maleGenitals.empty()) QueueMaleGenitalClear(actor, generation);
                else QueueMaleGenitalApply(actor, *profile, generation);
            }
        }
        if (result == ApplyResult::missingProfile && actor) {
            [[maybe_unused]] const auto cleared = native_skin::QueueClear(actor,
                [](RE::Actor* refreshed) { RefreshNativeSkin3D(refreshed); });
            const auto generation = BeginSkinChange(actorFormID);
            skin_session::TrackSkinSelection(actorFormID, {});
            QueueMaleGenitalClear(actor, generation);
        }
        return result;
    }

    ApplyResult QueueClear(RE::Actor* actor)
    {
        const auto result = native_skin::QueueClear(actor,
            [](RE::Actor* refreshed) { RefreshNativeSkin3D(refreshed); });
        if (result == ApplyResult::queued && actor) {
            const auto generation = BeginSkinChange(actor->GetFormID());
            skin_session::TrackSkinSelection(actor->GetFormID(), {});
            QueueMaleGenitalClear(actor, generation);
        }
        return result;
    }

    std::optional<std::string> CurrentProfileId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        if (skin_session::HasTrackedSelection(actor->GetFormID())) {
            return skin_session::RuntimeProfileId(actor->GetFormID());
        }
        if (native_skin::HasTrackedSelection(actor)) {
            return native_skin::CurrentProfileId(actor);
        }
        if (const auto state = ActorRegistry::Get().Snapshot(actor);
            state && state->skin.selection.useDefault) return std::nullopt;
        if (const auto selected = ActorRegistry::Get().SelectedSkinId(actor)) return selected;
        return ActorRegistry::Get().AppliedSkinId(actor);
    }

    bool HasTrackedSelection(const RE::Actor* actor)
    {
        return native_skin::HasTrackedSelection(actor);
    }

    bool HasCurrentMaleGenitalSkin(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base || base->GetSex() != RE::SEX::kMale) return false;
        const auto profileId = CurrentProfileId(actor);
        const auto profile = profileId ? SkinProfiles::Get().Find(*profileId) :
            std::optional<SkinProfile>{};
        return profile && !profile->maleGenitals.empty();
    }

    void QueueReapplyCurrentMaleGenitals(
        RE::Actor* actor, const bool onlyIfAddonChanged)
    {
        if (!native_addon::Available()) return;
        if (!actor || !actor->GetActorBase() ||
            actor->GetActorBase()->GetSex() != RE::SEX::kMale) return;
        if (!HasCurrentMaleGenitalSkin(actor)) {
            if (ActorRegistry::Get().Snapshot(actor)) {
                const auto signature = MaleGenitalTargetSignature(actor);
                if (signature == 0U) {
                    skin_session::ClearAddonReconciled(actor->GetFormID(),
                        skin_session::AddonTextureChannel::maleGenitals);
                    return;
                }
                if (onlyIfAddonChanged && !skin_session::NeedsAddonReconcile(
                        skin_session::ReconciledAddonSignature(actor->GetFormID(),
                            skin_session::AddonTextureChannel::maleGenitals), signature)) return;
                auto generation = skin_session::CurrentSkinGeneration(actor->GetFormID());
                if (!generation) generation = BeginSkinChange(actor->GetFormID());
                QueueMaleGenitalClear(actor, *generation);
            }
            return;
        }
        const auto profileId = CurrentProfileId(actor);
        const auto profile = profileId ? SkinProfiles::Get().Find(*profileId) :
            std::optional<SkinProfile>{};
        if (!profile) return;
        if (onlyIfAddonChanged) {
            const auto signature = MaleGenitalTargetSignature(actor);
            if (signature == 0U) {
                skin_session::ClearAddonReconciled(actor->GetFormID(),
                    skin_session::AddonTextureChannel::maleGenitals);
                return;
            }
            if (!skin_session::NeedsAddonReconcile(
                    skin_session::ReconciledAddonSignature(actor->GetFormID(),
                        skin_session::AddonTextureChannel::maleGenitals), signature,
                    native_addon::HasSelection(actor->GetFormID(),
                        native_addon::Channel::maleGenitals))) return;
        }
        auto generation = skin_session::CurrentSkinGeneration(actor->GetFormID());
        if (!generation) {
            generation = BeginSkinChange(actor->GetFormID());
            skin_session::TrackSkinSelection(actor->GetFormID(), profile->id);
        }
        QueueMaleGenitalApply(actor, *profile, *generation);
    }

    std::optional<FutanariSkinType> CurrentFutanariType(
        RE::Actor* actor, const bool refresh)
    {
        if (!actor || !actor->Is3DLoaded()) return std::nullopt;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return std::nullopt;
        if (!refresh) {
            const auto cached = skin_session::CachedFutanariType(actor->GetFormID());
            if (cached.cached) return cached.type;
        }
        const auto detected = FindLoadedFutanariRoute(actor).type;
        skin_session::CacheFutanariType(actor->GetFormID(), detected);
        return detected;
    }

    ApplyResult QueueApplyFutanari(RE::Actor* actor, std::string profileId,
        const FutanariSelectionMode mode)
    {
        if (!native_addon::Available()) return ApplyResult::unsupportedRuntime;
        if (!frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        if (runtime::ResolveGameBranch(REL::Module::get().version()) ==
            runtime::GameBranch::unsupported) return ApplyResult::unsupportedRuntime;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return ApplyResult::incompatibleSex;
        const auto profile = FutanariSkinProfiles::Get().Find(profileId);
        if (!profile) return ApplyResult::missingProfile;
        if (!FutanariSkinTypeMatchesActor(
                profile->type, body_family::ResolveActor(actor))) {
            return ApplyResult::incompatibleFutanariType;
        }
        const auto actorType = futanari_support::RegisteredType(actor);
        if (!actorType || *actorType != profile->type) {
            return ApplyResult::incompatibleFutanariType;
        }
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;

        if (mode == FutanariSelectionMode::manual) {
            ActorRegistry::Get().SetManualFutanariSkin(actor, profile->id, false);
        } else if (mode == FutanariSelectionMode::automatic) {
            ActorRegistry::Get().SetAutomaticFutanariSkin(actor, profile->id);
        }
        const auto handle = actor->GetHandle();
        const auto generation = BeginFutanariChange(actor->GetFormID());
        // The desired selection is persistent and independent of whether a
        // provider currently has a slot-52 addon attached.
        if (!actor->Is3DLoaded()) return ApplyResult::queued;
        frame_tasks::Queue(actor->GetFormID(),
            [handle, profile = *profile, generation] {
                const auto current = handle.get();
                if (!current || !IsCurrentFutanariChange(
                        current->GetFormID(), generation)) return;
                if (profile.contentHash !=
                    FutanariSkinProfiles::Get().ContentHash(profile.id)) {
                    QueueReapplyCurrentFutanari(current.get());
                    return;
                }
                std::vector<runtime_assets::TexturePreparation> paths;
                paths.reserve(profile.layers.size());
                for (const auto& layer : profile.layers) {
                    paths.push_back({ layer.path, "futanari" });
                }
                const auto lease = frame_tasks::CurrentLease();
                const auto continueApply = [lease, handle, profile, generation](
                    const bool prepared) {
                    if (!prepared) {
                        SKSE::log::warn(
                            "Body Change NG could not prepare every futanari texture for '{}'; unavailable channels remain unchanged",
                            profile.name);
                        return;
                    }
                    static_cast<void>(frame_tasks::Continue(lease,
                        [handle, profile, generation] {
                            ApplyFutanariNativeNow(handle, profile, generation);
                        }));
                };
                if (!runtime_assets::PrepareTexturePathsAsync(
                        (static_cast<std::uint64_t>(current->GetFormID()) << 1U) | 1U,
                        std::move(paths), continueApply,
                        async_work::FrameTaskQueue::InteractiveLease(lease))) {
                    continueApply(false);
                }
            }, 1U, appearance::WorkChannel::futanariSkinApply);
        return ApplyResult::queued;
    }

    ApplyResult QueueClearFutanari(RE::Actor* actor, const FutanariSelectionMode mode)
    {
        if (!frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        if (runtime::ResolveGameBranch(REL::Module::get().version()) ==
            runtime::GameBranch::unsupported) return ApplyResult::unsupportedRuntime;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return ApplyResult::incompatibleSex;
        if (mode == FutanariSelectionMode::manual) {
            ActorRegistry::Get().SetManualFutanariSkin(actor, {}, true);
        }
        native_addon::Clear(actor->GetFormID());
        const auto generation = BeginFutanariChange(actor->GetFormID());
        skin_session::ClearAddonReconciled(actor->GetFormID(),
            skin_session::AddonTextureChannel::futanari);
        if (!actor->Is3DLoaded()) return ApplyResult::queued;
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actor->GetFormID(), [handle, generation] {
            RestoreAddonNativeNow(handle, generation, true);
        }, 1U, appearance::WorkChannel::futanariSkinApply);
        return ApplyResult::queued;
    }

    std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* actor)
    {
        return ActorRegistry::Get().SelectedFutanariSkinId(actor);
    }

    void QueueReapplyCurrentFutanari(
        RE::Actor* actor, const bool onlyIfAddonChanged)
    {
        if (!native_addon::Available()) return;
        if (!actor || !frame_tasks::Active() || frame_tasks::HasPreview(actor->GetFormID())) return;
        // Registration / addon switches can occur after the actor-load pass.
        // The player and manual selections are excluded by this narrow evaluator.
        Distribution::Get().RefreshFutanariSelection(actor);
        if (const auto profileId = CurrentFutanariProfileId(actor)) {
            const auto profile = FutanariSkinProfiles::Get().Find(*profileId);
            if (!profile) return;
            if (onlyIfAddonChanged) {
                const auto signature = skin_session::FutanariSelectionSignature(
                    FutanariTargetSignature(FindLoadedFutanariRoute(actor)), profile->id, profile->contentHash);
                if (signature == 0U) {
                    skin_session::ClearAddonReconciled(actor->GetFormID(),
                        skin_session::AddonTextureChannel::futanari);
                    return;
                }
                if (!skin_session::NeedsAddonReconcile(
                        skin_session::ReconciledAddonSignature(actor->GetFormID(),
                            skin_session::AddonTextureChannel::futanari), signature,
                        native_addon::HasSelection(actor->GetFormID(),
                            native_addon::Channel::futanari))) return;
            }
            [[maybe_unused]] const auto result = QueueApplyFutanari(
                actor, *profileId, FutanariSelectionMode::restore);
        } else if (ActorRegistry::Get().FutanariUsesDefault(actor)) {
            const auto signature =
                FutanariTargetSignature(FindLoadedFutanariRoute(actor));
            if (signature == 0U) {
                skin_session::ClearAddonReconciled(actor->GetFormID(),
                    skin_session::AddonTextureChannel::futanari);
                return;
            }
            if (onlyIfAddonChanged && !skin_session::NeedsAddonReconcile(
                    skin_session::ReconciledAddonSignature(actor->GetFormID(),
                        skin_session::AddonTextureChannel::futanari), signature)) return;
            [[maybe_unused]] const auto result = QueueClearFutanari(
                actor, FutanariSelectionMode::restore);
        }
    }

    void InvalidateFutanariDetection(const std::uint32_t actorFormID)
    {
        if (actorFormID != 0U) skin_session::InvalidateFutanariType(actorFormID);
    }

    std::optional<bool> LiveSkinStateMatches(RE::Actor* actor,
        const std::string_view profileId, const bool expectDefault)
    {
        return native_skin::LiveStateMatches(actor, profileId, expectDefault);
    }

    void ForgetActorState(const std::uint32_t actorFormID)
    {
        face_skin::Forget(actorFormID);
        native_addon::Forget(actorFormID);
        if (actorFormID != 0U) skin_session::Forget(actorFormID);
    }
}
