#include "BodyChangeNG/SkinOverrides.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariRouting.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/RaceMenuOverrideRouting.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/NativeSkinBackend.h"
#include "BodyChangeNG/SkinOverrideBackend.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinSessionState.h"
#include "BodyChangeNG/SkinTargetResolver.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/SkinOverrideOwnership.h"

#include <RE/P/PackUnpack.h>
#include <RE/B/BipedAnim.h>
#include <RE/B/BipedObjects.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSTextureSet.h>
#include <RE/B/BSVisit.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <SKSE/Logger.h>

#include <array>
#include <atomic>
#include <bit>
#include <cctype>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    namespace skee_override = bcn::skin_backend;

    constexpr auto kUbeBodySlot = bcn::skin_target::kUbeBodySlot;
    constexpr auto kSosMaleGenitalSlot = bcn::skin_target::kSosMaleGenitalSlot;

    using LoadedPartView = bcn::skin_target::LoadedPartView;
    using LoadedPartTarget = bcn::skin_target::LoadedPartTarget;
    using LoadedFutanariRoute = bcn::skin_target::LoadedFutanariRoute;
    using LoadedProfileBodyRoute = bcn::skin_target::LoadedProfileBodyRoute;
    using FaceNodeInfo = bcn::skin_target::FaceNodeInfo;
    using bcn::skin_target::ActiveAddonModelPath;
    using bcn::skin_target::FaceNode;
    using bcn::skin_target::FindLoadedFutanariRoute;
    using bcn::skin_target::FindLoadedPartTargets;
    using bcn::skin_target::FindLoadedProfileBodyRoute;
    using bcn::skin_target::GeometryDiffuseTexture;
    using bcn::skin_target::IsSkinGeometry;
    using bcn::skin_target::StableTextureSet;
    using bcn::skin_target::ViewContainsNode;

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
            for (const auto& node : target.persistentNodes) {
                for (const auto character : node) {
                    appendByte(static_cast<std::uint8_t>(character));
                }
                appendByte(0xFFU);
            }
            appendByte(0xFEU);
        }
        return hash == 0U ? 1U : hash;
    }

    [[nodiscard]] std::uint64_t MaleGenitalTargetSignature(RE::Actor* actor)
    {
        return AddonTargetSignature(FindLoadedPartTargets(actor, kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals, false), 0x4D414C45ULL);
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

    [[nodiscard]] bool ProfileMatchesActor(RE::Actor* actor, const bcn::SkinProfile& profile)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return false;
        const auto actorSex = base->GetSex() == RE::SEX::kFemale ?
            bcn::SkinSex::female : bcn::SkinSex::male;
        return bcn::SkinProfileCompatibility(profile, actorSex,
            bcn::ResolveActorSkinRace(actor),
            bcn::body_family::ResolveActor(actor)).Compatible();
    }

    [[nodiscard]] skee_override::IPluginInterface* OverrideInterface() noexcept
    {
        return skee_override::Interface();
    }

    [[nodiscard]] bcn::racemenu_override::Route OverrideRoute() noexcept
    {
        return skee_override::ActiveRoute();
    }

    [[nodiscard]] skee_override::IOverrideInterfaceV2* OverrideInterfaceV2() noexcept
    {
        return skee_override::NativeV2();
    }

    [[nodiscard]] bool UsesLegacyOverride() noexcept
    {
        return skee_override::UsesPapyrus();
    }

    [[nodiscard]] std::uint64_t BeginSkinChange(const RE::FormID actorFormID)
    {
        return bcn::skin_session::BeginSkinChange(actorFormID);
    }

    [[nodiscard]] bool IsCurrentSkinChange(const RE::FormID actorFormID, const std::uint64_t generation)
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

    [[nodiscard]] std::uint64_t BeginRsvFaceRefresh(const RE::FormID actorFormID)
    {
        return bcn::skin_session::BeginFaceRefresh(actorFormID);
    }

    [[nodiscard]] bool IsCurrentRsvFaceRefresh(
        const RE::FormID actorFormID, const std::uint64_t generation)
    {
        return bcn::skin_session::IsCurrentFaceRefresh(actorFormID, generation);
    }

    [[nodiscard]] bool ReleaseRsvTransientFace(const RE::FormID actorFormID)
    {
        return bcn::skin_session::ReleaseTransientFace(actorFormID);
    }

    [[nodiscard]] bool HasRsvTransientFace(const RE::FormID actorFormID)
    {
        return bcn::skin_session::HasTransientFace(actorFormID);
    }

    [[nodiscard]] bool ClaimLegacyCleanup(const RE::FormID actorFormID)
    {
        return bcn::skin_session::ClaimLegacyCleanup(actorFormID);
    }

    constexpr std::uint32_t kShaderTextureProperty = 9;
    // BSTextureSet slots owned by a complete skin profile: diffuse, normal,
    // subsurface/skin tint, face detail and specular.
    constexpr std::array<std::uint32_t, 5> kTextureIndices{ 0U, 1U, 2U, 3U, 7U };
    constexpr std::uint32_t kFaceDetailTextureIndex = 3U;

    [[nodiscard]] constexpr std::string_view TextureIndexName(const std::uint32_t index) noexcept
    {
        switch (index) {
        case 0U: return "diffuse";
        case 1U: return "normal";
        case 2U: return "subsurface";
        case 3U: return "detail";
        case 7U: return "specular";
        default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view SkinPartName(
        RE::BGSBipedObjectForm::BipedObjectSlot slot) noexcept;

    void ApplyLegacyNow(RE::ActorHandle actorHandle, bcn::SkinProfile profile,
        std::uint64_t generation, bool settledRepaint = false,
        std::uint8_t remainingVerificationRepairs = 1U);
    void ApplyNow(RE::ActorHandle actorHandle, bcn::SkinProfile profile,
        std::uint64_t generation, bool settledRepaint = false,
        std::uint8_t remainingVerificationRepairs = 1U);
    void QueueSettledSkinAudit(RE::ActorHandle actorHandle, std::uint64_t generation,
        std::uint32_t remainingTaskHops, bool repairAfterRebuild = false,
        std::uint8_t remainingVerificationRepairs = 1U);

    class NodeUpdateCallback final : public RE::BSScript::IStackCallbackFunctor
    {
        struct Payload
        {
            RE::ActorHandle actor;
            std::uint64_t generation{}, epoch{};
            bool auditSkin{ true };
            bool repairDefaultSkin{};
            bcn::frame_tasks::Lease lease;
        };
    public:
        NodeUpdateCallback(RE::ActorHandle actorHandle, const std::uint64_t generation,
            const bool auditSkin, const bool repairDefaultSkin) :
            payload_(Payload{std::move(actorHandle), generation, bcn::frame_tasks::Epoch(), auditSkin,
                repairDefaultSkin,
                bcn::frame_tasks::CurrentLease()}) {}

        void operator()(RE::BSScript::Variable) override
        {
            // Actor.QueueNiNodeUpdate is asynchronous with respect to the
            // scene graph used by RaceMenu BodyMorph.  Start the post-rebuild
            // barrier only after Papyrus reports that the call completed.
            auto payload = payload_.Take();
            if (!payload || !bcn::frame_tasks::IsCurrent(payload->epoch) ||
                !bcn::frame_tasks::ValidLease(payload->lease)) return;
            if (payload->repairDefaultSkin) {
                bcn::frame_tasks::Continue(std::move(payload->lease),
                    [handle = payload->actor, generation = payload->generation] {
                        const auto actor = handle.get();
                        if (!actor || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
                        const auto clean = bcn::skin_override::LiveSkinStateMatches(actor.get(), {}, true,
                            bcn::skin_override::LiveCheckScope::fullProfile);
                        if (!clean.value_or(true)) {
                            // RaceMenu removed the persistent keys but left its
                            // already-loaded armor clones painted. Rebuild only
                            // this actor, and only on that verified fallback.
                            actor->DoReset3D(false);
                            SKSE::log::info(
                                "Body Change NG rebuilt actor {:08X} after a stale live default-skin clone",
                                actor->GetFormID());
                        }
                        QueueSettledSkinAudit(handle, generation, 1U);
                    }, 2U);
                return;
            }
            if (payload->auditSkin) {
                bcn::frame_tasks::Continue(std::move(payload->lease),
                    [handle = payload->actor, generation = payload->generation] {
                        QueueSettledSkinAudit(handle, generation, 2U, true);
                    });
            }
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        bcn::async_work::CompletionPayload<Payload> payload_;
    };

    [[nodiscard]] bool QueueNiNodeUpdate(RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor,
        RE::ActorHandle actorHandle, const std::uint64_t generation, const bool auditSkin = true,
        const bool repairDefaultSkin = false)
    {
        if (!actor) return false;
        auto* policy = vm.GetObjectHandlePolicy();
        if (!policy) return false;
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(actor->GetFormType()), actor);
        if (handle == policy->EmptyHandle()) return false;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
            new NodeUpdateCallback(std::move(actorHandle), generation, auditSkin, repairDefaultSkin));
        return vm.DispatchMethodCall(handle, "Actor", "QueueNiNodeUpdate",
            RE::MakeFunctionArguments(), callback);
    }

    [[nodiscard]] std::vector<std::pair<std::uint8_t, std::string>> ResolveTextureLayers(
        const std::vector<bcn::SkinTextureLayer>& layers, const std::string_view cacheNamespace)
    {
        std::vector<std::pair<std::uint8_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, cacheNamespace);
            if (!path.empty()) {
                resolvedLayers.emplace_back(
                    static_cast<std::uint8_t>(layer.shaderTextureIndex), std::move(path));
            }
        }
        return resolvedLayers;
    }

    [[nodiscard]] bool ApplyLoadedPart(skee_override::IOverrideInterfaceV2& overrides,
                                 RE::Actor* actor, const bool female,
                                 const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                                 const std::vector<bcn::SkinTextureLayer>& layers,
                                 const std::vector<LoadedPartTarget>& targets,
                                 const std::string_view cacheNamespace = "skin",
                                 const std::string_view partName = {})
    {
        if (!actor) return false;
        if (targets.empty()) return false;

        const auto resolvedLayers = ResolveTextureLayers(layers, cacheNamespace);
        if (resolvedLayers.empty()) return false;

        std::size_t appliedTargets{};
        std::size_t submitted{};
        for (const auto& target : targets) {
            for (const auto& node : target.persistentNodes) {
                for (const auto& [textureIndex, path] : resolvedLayers) {
                    skee_override::StringVisitor current;
                    const auto exists = overrides.GetArmorOverride(actor, female, target.armor,
                        target.addon, node.c_str(),
                        static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, current);
                    if (!bcn::skin_override::ownership::MayReplace(exists, current.Value())) {
                        SKSE::log::warn(
                            "SkinOverride persistent-register skipped actor={:08X} part={} armor={:08X} addon={:08X} node='{}' index={} reason=foreign-owner current='{}'",
                            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
                            target.armor->GetFormID(), target.addon->GetFormID(), node,
                            static_cast<std::uint32_t>(textureIndex), current.Value());
                        continue;
                    }
                    skee_override::StringVariant value{ path };
                    overrides.AddArmorOverride(actor, female, target.armor, target.addon,
                        node.c_str(), static_cast<std::uint16_t>(kShaderTextureProperty),
                        textureIndex, value);
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part={} armor={:08X} addon={:08X} node='{}' index={}({}) action={} value='{}'",
                        actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
                        target.armor->GetFormID(), target.addon->GetFormID(), node,
                        static_cast<std::uint32_t>(textureIndex), TextureIndexName(textureIndex),
                        exists ? "replace-owned" : "add", path);
                    ++submitted;
                }
            }
            for (const auto& view : target.views) {
                overrides.ApplyArmorOverrides(
                    actor, target.armor, target.addon, view.object, true);
                ++appliedTargets;
            }
        }
        SKSE::log::info("SkinAudit actor={:08X} part={} stored-keys={} exact-targets={} mode=RaceMenu-v2-exact-persistent",
            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
            submitted, appliedTargets);
        return submitted != 0U && appliedTargets != 0U;
    }

    [[nodiscard]] bool ApplySkinSlotPart(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::string_view cacheNamespace = "skin",
        const std::string_view partName = {},
        const bool updateLoaded = true)
    {
        if (!actor) return false;
        const auto mask = static_cast<std::uint32_t>(slot);
        if (mask == 0U || !std::has_single_bit(mask)) return false;
        const auto resolvedLayers = ResolveTextureLayers(layers, cacheNamespace);
        if (resolvedLayers.empty()) return false;

        std::size_t stored{};
        std::size_t transient{};
        bool thirdPersonApplied{};
        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            for (const auto& [textureIndex, path] : resolvedLayers) {
                skee_override::StringVisitor current;
                const auto exists = overrides.GetSkinOverride(actor, female, firstPerson, mask,
                    static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, current);
                const auto mayPersist = bcn::skin_override::ownership::MayReplace(exists, current.Value());
                const auto rsvTransient = exists &&
                    bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(current.Value());
                if (!mayPersist && !rsvTransient) {
                    SKSE::log::warn(
                        "SkinOverride skin-slot skipped actor={:08X} part={} view={} mask={:08X} index={} reason=foreign-owner current='{}'",
                        actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
                        firstPerson ? "1p" : "3p", mask,
                        static_cast<std::uint32_t>(textureIndex), current.Value());
                    continue;
                }

                skee_override::StringVariant value{ path };
                if (mayPersist) {
                    overrides.AddSkinOverride(actor, female, firstPerson, mask,
                        static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, value);
                    ++stored;
                } else {
                    ++transient;
                }
                if (updateLoaded) {
                    // Shared-atlas layouts can safely repaint the live Skin
                    // Armor. Conventional hands and feet use store-only mode
                    // regardless of equipment; exact targets paint whatever
                    // skin is currently visible without repainting other parts.
                    overrides.SetSkinProperty(actor, firstPerson, mask,
                        static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, value, true);
                    if (!firstPerson) thirdPersonApplied = true;
                }
            }
        }
        SKSE::log::info(
            "SkinAudit actor={:08X} part={} stored-keys={} transient-rsv-keys={} mode={}",
            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
            stored, transient, updateLoaded ? "RaceMenu-v2-single-skin-slot" :
                "RaceMenu-v2-conventional-limb-store-only");
        return updateLoaded ? thirdPersonApplied : stored != 0U;
    }

    [[nodiscard]] bool ApplyPart(skee_override::IOverrideInterfaceV2& overrides, RE::Actor* actor,
                                 const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                                 const std::vector<bcn::SkinTextureLayer>& layers,
                                 const bcn::skin_geometry::BodySelection selection =
                                     bcn::skin_geometry::BodySelection::all)
    {
        if (!actor) return false;
        const auto targets = FindLoadedPartTargets(actor, slot, selection);
        return ApplyLoadedPart(overrides, actor, female, slot, layers, targets);
    }

    [[nodiscard]] bool ApplyProfileBodyPart(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female, const bcn::SkinProfile& profile,
        const std::vector<bcn::SkinTextureLayer>& layers)
    {
        if (!actor) return false;
        const auto route = FindLoadedProfileBodyRoute(actor, profile);
        return ApplyLoadedPart(overrides, actor, female, route.slot, layers, route.targets);
    }

    [[nodiscard]] bool ClearArmorAddonTargets(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female, const std::vector<LoadedPartTarget>& targets)
    {
        bool removed{};
        for (const auto& target : targets) {
            auto cleanupNodes = target.persistentNodes;
            // v0.2.16 also stored a compatibility entry under the empty node
            // name. Probe it during cleanup only; never create it again.
            if (std::ranges::find(cleanupNodes, std::string{}) == cleanupNodes.end()) {
                cleanupNodes.emplace_back();
            }
            for (const auto& node : cleanupNodes) {
                for (const auto textureIndex : kTextureIndices) {
                    skee_override::StringVisitor current;
                    const auto exists = overrides.GetArmorOverride(actor, female, target.armor, target.addon,
                        node.c_str(), static_cast<std::uint16_t>(kShaderTextureProperty),
                        static_cast<std::uint8_t>(textureIndex), current);
                    if (!bcn::skin_override::ownership::MayRemove(exists, current.Value())) continue;
                    overrides.RemoveArmorOverride(actor, female, target.armor, target.addon, node.c_str(),
                        static_cast<std::uint16_t>(kShaderTextureProperty), static_cast<std::uint8_t>(textureIndex));
                    SKSE::log::info(
                        "SkinOverride persistent-remove actor={:08X} armor={:08X} addon={:08X} node='{}' index={}({}) owner=BCNG",
                        actor->GetFormID(), target.armor->GetFormID(), target.addon->GetFormID(), node,
                        textureIndex, TextureIndexName(textureIndex));
                    removed = true;
                }
            }
        }
        return removed;
    }

    [[nodiscard]] bool ClearArmorAddonPart(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const bcn::skin_geometry::BodySelection selection =
            bcn::skin_geometry::BodySelection::all)
    {
        const auto limb = slot == RE::BGSBipedObjectForm::BipedObjectSlot::kHands ||
            slot == RE::BGSBipedObjectForm::BipedObjectSlot::kFeet;
        return ClearArmorAddonTargets(overrides, actor, female,
            FindLoadedPartTargets(actor, slot, selection, true, limb));
    }

    [[nodiscard]] bool ApplyFacePart(skee_override::IOverrideInterfaceV2& overrides, RE::Actor* actor,
                                     const bool female, const FaceNodeInfo& face,
                                     const std::vector<bcn::SkinTextureLayer>& layers)
    {
        if (!face.object) return false;
        bool stored{};
        std::vector<std::pair<std::uint8_t, std::string>> transientLayers;
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, "skin-face");
            if (path.empty()) continue;
            SKSE::log::info(
                "SkinAudit expected actor={:08X} part=face node='{}' index={}({}) source='{}' cache='{}'",
                actor->GetFormID(), face.nodeName, static_cast<std::uint32_t>(layer.shaderTextureIndex),
                TextureIndexName(layer.shaderTextureIndex), layer.path, path);
            skee_override::StringVisitor current;
            const auto index = static_cast<std::uint8_t>(layer.shaderTextureIndex);
            const auto exists = overrides.GetNodeOverride(actor, female, face.nodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty), index, current);
            if (!bcn::skin_override::ownership::MayReplace(exists, current.Value())) {
                if (bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(current.Value())) {
                    // RSV reasserts these serialized node keys after every
                    // NiNode rebuild. Leave its saved ownership intact, then
                    // paint the selected BCNG face onto this live geometry.
                    // Removing BCNG therefore reveals RSV again naturally.
                    transientLayers.emplace_back(index, std::move(path));
                    continue;
                }
                SKSE::log::warn(
                    "SkinOverride persistent-register skipped actor={:08X} part=face node='{}' index={} reason=foreign-owner current='{}'",
                    actor->GetFormID(), face.nodeName, static_cast<std::uint32_t>(index), current.Value());
                continue;
            }
            skee_override::StringVariant value{ path };
            overrides.AddNodeOverride(actor, female, face.nodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty), index, value);
            SKSE::log::info(
                "SkinOverride persistent-register actor={:08X} part=face node='{}' index={}({}) action={} value='{}'",
                actor->GetFormID(), face.nodeName, static_cast<std::uint32_t>(index),
                TextureIndexName(index), exists ? "replace-owned" : "add", path);
            stored = true;
        }
        if (stored) overrides.ApplyNodeOverrides(actor, face.object, true);
        for (const auto& [index, path] : transientLayers) {
            skee_override::StringVariant value{ path };
            overrides.SetNodeProperty(actor, false, face.nodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty), index, value, true);
            SKSE::log::debug(
                "SkinOverride live-apply actor={:08X} part=face node='{}' index={} provider=RSV value='{}'",
                actor->GetFormID(), face.nodeName, static_cast<std::uint32_t>(index), path);
        }
        if (!transientLayers.empty()) {
            bcn::skin_session::MarkTransientFace(actor->GetFormID());
        }
        return stored || !transientLayers.empty();
    }

    void ApplyRsvFaceTransientV2(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female, const FaceNodeInfo& face,
        const std::vector<bcn::SkinTextureLayer>& layers)
    {
        if (!actor || !face.object) return;
        for (const auto& layer : layers) {
            const auto path = bcn::runtime_assets::TexturePathFromGameRelative(
                layer.path, "skin-face");
            if (path.empty()) continue;
            const auto index = static_cast<std::uint8_t>(layer.shaderTextureIndex);
            skee_override::StringVisitor current;
            const auto exists = overrides.GetNodeOverride(actor, female,
                face.nodeName.c_str(), static_cast<std::uint16_t>(kShaderTextureProperty),
                index, current);
            if (!bcn::skin_override::ownership::MayTransientlyPaintRsv(
                    exists, current.Value())) continue;
            skee_override::StringVariant value{ path };
            // This bridge never claims a serialized key. RSV remains the
            // persistent owner and a native Face TXST/3D reset reveals it
            // immediately when BCNG returns to Default.
            overrides.SetNodeProperty(actor, false, face.nodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty), index, value, true);
        }
    }

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    void LogLiveSkinGeometry(RE::Actor* actor, const bool firstPerson)
    {
        if (!actor) return;
        auto* root = actor->Get3D(firstPerson);
        if (!root) {
            SKSE::log::info("SkinAudit actual actor={:08X} view={} unavailable",
                actor->GetFormID(), firstPerson ? "first-person" : "third-person");
            return;
        }

        std::size_t logged{};
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
            if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
            auto* shader = geometry->lightingShaderProp_cast();
            auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
            const auto textureSet = StableTextureSet(material);
            if (!material || !textureSet) return RE::BSVisit::BSVisitControl::kContinue;

            const auto* rawName = geometry->name.c_str();
            const auto name = rawName && rawName[0] != '\0' ? std::string{ rawName } : std::string{ "<unnamed>" };
            const auto loweredName = LowerAscii(name);
            bool relevant = loweredName.contains("body") || loweredName.contains("hand") ||
                loweredName.contains("feet") || loweredName.contains("foot") ||
                loweredName.contains("head") || loweredName.contains("face");
            std::array<std::string, kTextureIndices.size()> paths;
            for (std::size_t position{}; position < kTextureIndices.size(); ++position) {
                const auto index = kTextureIndices[position];
                const auto* rawPath = textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(index));
                paths[position] = rawPath ? rawPath : "";
                auto loweredPath = LowerAscii(paths[position]);
                std::ranges::replace(loweredPath, '/', '\\');
                relevant = relevant || loweredPath.contains("bodychangeng\\cache\\skin\\") ||
                    loweredPath.contains("bodychangeng\\cache\\skin-face\\") ||
                    loweredPath.contains("femalebody") || loweredPath.contains("femalehands") ||
                    loweredPath.contains("femalefeet") || loweredPath.contains("femalehead") ||
                    loweredPath.contains("malebody") || loweredPath.contains("malehands") ||
                    loweredPath.contains("malefeet") || loweredPath.contains("malehead");
            }
            if (!relevant) return RE::BSVisit::BSVisitControl::kContinue;

            SKSE::log::info(
                "SkinAudit actual actor={:08X} view={} node='{}' feature={} diffuse='{}' normal='{}' subsurface='{}' detail='{}' specular='{}'",
                actor->GetFormID(), firstPerson ? "first-person" : "third-person", name,
                static_cast<std::uint32_t>(material->GetFeature()), paths[0], paths[1], paths[2], paths[3], paths[4]);
            ++logged;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        SKSE::log::info("SkinAudit actual actor={:08X} view={} relevant-geometries={}",
            actor->GetFormID(), firstPerson ? "first-person" : "third-person", logged);
    }

    void AuditLiveSkinProfile(RE::Actor* actor, const bcn::SkinProfile& profile);

    void QueueSettledSkinAudit(RE::ActorHandle actorHandle, const std::uint64_t generation,
        const std::uint32_t remainingTaskHops, const bool repairAfterRebuild,
        const std::uint8_t remainingVerificationRepairs)
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        bcn::frame_tasks::Continue(bcn::frame_tasks::CurrentLease(),
            [actorHandle, generation, repairAfterRebuild, remainingVerificationRepairs] {
            if (!bcn::frame_tasks::ValidLease(bcn::frame_tasks::CurrentLease())) return;
            const auto actor = actorHandle.get();
            if (!actor || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
            if (repairAfterRebuild) {
                const auto profileID = bcn::skin_override::CurrentProfileId(actor.get());
                const auto profile = profileID ? bcn::SkinProfiles::Get().Find(*profileID) :
                    std::optional<bcn::SkinProfile>{};
                if (!profile || !ProfileMatchesActor(actor.get(), *profile)) return;

                // QueueNiNodeUpdate can replace the live NPC Biped clones
                // after the first exact repaint. Repaint that final clone
                // once, under the same generation, without requesting a
                // second rebuild. This path performs no catalog scan or poll.
                if (bcn::racemenu_override::UsesPapyrus(OverrideRoute())) {
                    ApplyLegacyNow(actorHandle, *profile, generation, true,
                        remainingVerificationRepairs);
                } else {
                    ApplyNow(actorHandle, *profile, generation, true,
                        remainingVerificationRepairs);
                }
                return;
            }
            if (const auto profileID = bcn::skin_override::CurrentProfileId(actor.get())) {
                if (const auto profile = bcn::SkinProfiles::Get().Find(*profileID)) {
                    // Accepted RaceMenu calls are not proof that the final
                    // Biped clone uses the requested textures. Verify this one
                    // actor once after the quiet interval. The check uses only
                    // the in-memory profile and fixed loaded Biped entries; it
                    // does not rescan the catalog or poll all NPCs.
                    const auto liveMatches = bcn::skin_override::LiveSkinStateMatches(
                        actor.get(), *profileID, false,
                        bcn::skin_override::LiveCheckScope::fullProfile);
                    if (!liveMatches.value_or(false)) {
                        bcn::ActorRegistry::Get().InvalidateSkin(actor.get());
                        if (remainingVerificationRepairs != 0U) {
                            SKSE::log::info(
                                "SkinAudit final verification requested one exact repair actor={:08X} profile='{}' generation={}",
                                actor->GetFormID(), profile->name, generation);
                            if (bcn::racemenu_override::UsesPapyrus(OverrideRoute())) {
                                ApplyLegacyNow(actorHandle, *profile, generation, true,
                                    remainingVerificationRepairs - 1U);
                            } else {
                                ApplyNow(actorHandle, *profile, generation, true,
                                    remainingVerificationRepairs - 1U);
                            }
                            return;
                        }
                        SKSE::log::warn(
                            "Body Change NG could not verify final skin '{}' on actor {:08X}; the desired selection remains pending for the next actor/equipment refresh",
                            profile->name, actor->GetFormID());
                    }
                    if (spdlog::should_log(spdlog::level::debug)) {
                        AuditLiveSkinProfile(actor.get(), *profile);
                    }
                }
            }
            if (spdlog::should_log(spdlog::level::debug)) {
                SKSE::log::info(
                    "SkinAudit settled actor={:08X} generation={} body-reapply=false",
                    actor->GetFormID(), generation);
            }
        }, std::max(1U, remainingTaskHops));
    }

    [[nodiscard]] std::vector<std::string> LegacyMisdirectedFaceNodes(RE::Actor* actor)
    {
        std::vector<std::string> nodes;
        if (!actor) return nodes;

        // v0.2.6 searched the actor's entire scene graph for the first
        // FaceGenRGBTint material. Some body meshes use that shader feature,
        // so a face texture override could be persisted against a naked-body
        // geometry. RaceMenu serializes node overrides and reapplies them on
        // every 3D rebuild, which means merely fixing face-node discovery does
        // not repair an already affected save.
        std::unordered_set<const RE::BSGeometry*> faceGeometry;
        if (auto* faceRoot = actor->GetFaceNodeSkinned()) {
            RE::BSVisit::TraverseScenegraphGeometries(faceRoot, [&](RE::BSGeometry* geometry) {
                if (geometry) faceGeometry.insert(geometry);
                return RE::BSVisit::BSVisitControl::kContinue;
            });
        }

        if (auto* root = actor->Get3D()) {
            RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
                if (!geometry || faceGeometry.contains(geometry)) {
                    return RE::BSVisit::BSVisitControl::kContinue;
                }
                auto* shader = geometry->lightingShaderProp_cast();
                auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                const auto textureSet = StableTextureSet(material);
                if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;

                bool hasMisdirectedFaceTexture{};
                for (const auto index : kTextureIndices) {
                    const auto* path = textureSet->GetTexturePath(
                        static_cast<RE::BSTextureSet::Texture>(index));
                    if (!path || path[0] == '\0') continue;
                    auto lowered = LowerAscii(path);
                    std::ranges::replace(lowered, '/', '\\');
                    if (lowered.contains("bodychangeng\\cache\\skin-face\\")) {
                        hasMisdirectedFaceTexture = true;
                        break;
                    }
                }
                if (hasMisdirectedFaceTexture) {
                    if (const auto* name = geometry->name.c_str(); name && name[0] != '\0' &&
                        std::ranges::find(nodes, name) == nodes.end()) {
                        nodes.emplace_back(name);
                    }
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });
        }
        return nodes;
    }

    [[nodiscard]] bool ClearLegacyMisdirectedFaceNodes(
        skee_override::IOverrideInterfaceV2& overrides, RE::Actor* actor, const bool female)
    {
        bool removed{};
        const auto nodes = LegacyMisdirectedFaceNodes(actor);
        for (const auto& nodeName : nodes) {
            for (const auto textureIndex : kTextureIndices) {
                skee_override::StringVisitor current;
                const auto exists = overrides.GetNodeOverride(actor, female, nodeName.c_str(),
                    static_cast<std::uint16_t>(kShaderTextureProperty),
                    static_cast<std::uint8_t>(textureIndex), current);
                if (!bcn::skin_override::ownership::MayRemove(exists, current.Value())) continue;
                overrides.RemoveNodeOverride(actor, female, nodeName.c_str(),
                    static_cast<std::uint16_t>(kShaderTextureProperty), static_cast<std::uint8_t>(textureIndex));
                removed = true;
            }
        }
        if (!nodes.empty()) {
            SKSE::log::info(
                "Body Change NG removed {} legacy face-texture override node(s) from non-face geometry on actor {:08X}",
                nodes.size(), actor->GetFormID());
        }
        return removed;
    }

    [[nodiscard]] bool IsVampireRace(RE::TESNPC* base)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        if (!editorID) return false;
        std::string lowered{ editorID };
        std::ranges::transform(lowered, lowered.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return lowered.find("vampire") != std::string::npos;
    }

    [[nodiscard]] bcn::HumanoidSkinRace ActorHumanoidSkinRace(RE::TESNPC* base)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        return bcn::HumanoidSkinRaceFromEditorID(
            editorID ? std::string_view{ editorID } : std::string_view{});
    }

    [[nodiscard]] bcn::skin_plan::ApplicationPlan EffectiveSkinPlan(
        const bcn::SkinProfile& profile, RE::Actor* actor, RE::TESNPC* base,
        const std::string_view currentDetailFilename = {})
    {
        return bcn::skin_plan::Build(profile, {
            .elder = bcn::IsElderActor(base),
            .vampire = IsVampireRace(base),
            .humanoidRace = ActorHumanoidSkinRace(base),
            .faceDetailFilename = currentDetailFilename,
            .bodyFamily = bcn::body_family::ResolveActor(actor)
        });
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
        if (!actor || !base || profile.sex != bcn::SkinSex::male || profile.maleGenitals.empty()) return {};
        const auto modelPath = ActiveAddonModelPath(actor, kSosMaleGenitalSlot, false);
        if (modelPath.empty()) return {};
        const auto matches = [&modelPath](const bcn::MaleGenitalTextureVariant& variant) {
            auto directory = variant.addonDirectory;
            std::ranges::transform(directory, directory.begin(), [](const unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            return !directory.empty() && modelPath.contains("\\sos\\" + directory + "\\");
        };
        const auto selected = std::ranges::find_if(profile.maleGenitals, matches);
        if (selected == profile.maleGenitals.end()) return {};

        auto layers = std::vector<bcn::SkinTextureLayer>{};
        // The Muscular NIF deliberately uses Regular diffuse/subsurface/
        // specular and replaces only its normal map. Mirror that material
        // composition inside one skin pack before overlaying Muscular files.
        if (modelPath.contains("\\sos\\vectorplexus muscular\\")) {
            const auto regular = std::ranges::find_if(profile.maleGenitals, [](const auto& variant) {
                auto name = variant.addonDirectory;
                std::ranges::transform(name, name.begin(), [](const unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
                return name == "vectorplexus regular";
            });
            if (regular != profile.maleGenitals.end()) {
                layers = MaleGenitalVariantLayers(*regular, base);
            }
        }
        bcn::skin_plan::OverlayLayers(layers, MaleGenitalVariantLayers(*selected, base));
        return layers;
    }

    [[nodiscard]] std::vector<bcn::runtime_assets::TexturePreparation> EffectiveTexturePreparations(
        const bcn::SkinProfile& profile, RE::Actor* actor, RE::TESNPC* base)
    {
        std::vector<bcn::runtime_assets::TexturePreparation> paths;
        const auto add = [&](const std::vector<bcn::SkinTextureLayer>& layers,
            const std::string_view nameSpace) {
            for (const auto& layer : layers) {
                paths.push_back({ layer.path, std::string{ nameSpace } });
            }
        };
        const auto face = FaceNode(actor, base);
        const auto plan = EffectiveSkinPlan(
            profile, actor, base, face ? face->detailFilename : std::string_view{});
        add(plan.body, "skin");
        add(plan.cbbeGenitalAnal, "skin");
        add(plan.unpGenitalAnal, "skin");
        add(EffectiveMaleGenitalLayers(profile, actor, base), "skin");
        // UBE hands and feet reuse the Body atlas already queued above.
        if (!plan.broadSharedAtlas) {
            add(plan.hands, "skin");
            add(profile.feet, "skin");
        }
        if (face) {
            add(plan.face, "skin-face");
        }
        return paths;
    }

    [[nodiscard]] std::string NormalizedTexturePath(std::string path)
    {
        std::ranges::replace(path, '/', '\\');
        std::ranges::transform(path, path.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return path;
    }

    [[nodiscard]] bool HasPotentialFaceLayers(const bcn::SkinProfile& profile)
    {
        if (!profile.face.empty() || !profile.vampireFace.empty() ||
            !profile.elderFace.empty() || !profile.faceDetails.empty()) return true;
        return std::ranges::any_of(profile.raceFace,
            [](const auto& layers) { return !layers.empty(); });
    }

    [[nodiscard]] bool ActorHasRsvFaceOwnership(RE::Actor* actor)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        const auto face = FaceNode(actor, base);
        if (!face || !face->object) return false;

        // RaceMenu v2 exposes the persistent owner directly. This remains the
        // authoritative probe even when the current live material has not yet
        // caught up with RSV's deferred FaceGen update.
        if (auto* overrides = OverrideInterfaceV2()) {
            const auto female = base && base->GetSex() == RE::SEX::kFemale;
            for (const auto textureIndex : kTextureIndices) {
                skee_override::StringVisitor current;
                if (overrides->GetNodeOverride(actor, female, face->nodeName.c_str(),
                        static_cast<std::uint16_t>(kShaderTextureProperty),
                        static_cast<std::uint8_t>(textureIndex), current) &&
                    bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(
                        current.Value())) return true;
            }
        }

        // RaceMenu v0/v1 exposes the same query only through asynchronous
        // Papyrus. The live FaceGen texture is a safe bounded fallback: no
        // directory scan, equipment inspection, or arbitrary foreign-owner
        // takeover is involved.
        bool found{};
        RE::BSVisit::TraverseScenegraphGeometries(face->object,
            [&found](RE::BSGeometry* geometry) {
                if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                auto* shader = geometry->lightingShaderProp_cast();
                auto* material = shader ?
                    static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                const auto textureSet = StableTextureSet(material);
                if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;
                for (const auto textureIndex : kTextureIndices) {
                    const auto* path = textureSet->GetTexturePath(
                        static_cast<RE::BSTextureSet::Texture>(textureIndex));
                    if (path && bcn::skin_override::ownership::
                            IsRacialSkinVarianceTexturePath(path)) {
                        found = true;
                        return RE::BSVisit::BSVisitControl::kStop;
                    }
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });
        return found;
    }

    void AuditLiveSkinProfile(RE::Actor* actor, const bcn::SkinProfile& profile)
    {
        if (!actor) return;
        auto* base = actor->GetActorBase();
        const auto faceNode = FaceNode(actor, base);
        const auto plan = EffectiveSkinPlan(
            profile, actor, base, faceNode ? faceNode->detailFilename : std::string_view{});
        const auto& bodyLayers = plan.body;
        const auto maleGenitalLayers = EffectiveMaleGenitalLayers(profile, actor, base);
        const auto& faceLayers = plan.face;
        const auto& selectedHandsLayers = plan.hands;
        const auto& selectedFeetLayers = plan.feet;
        const auto handsTargets = selectedHandsLayers.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
            bcn::skin_geometry::BodySelection::all, true, true);
        const auto feetTargets = selectedFeetLayers.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
            bcn::skin_geometry::BodySelection::all, true, true);
        const auto verifyPart = [&](const std::string_view part,
            const std::vector<bcn::SkinTextureLayer>& layers,
            const std::optional<RE::BGSBipedObjectForm::BipedObjectSlot> slot,
            const bcn::skin_geometry::BodySelection selection = bcn::skin_geometry::BodySelection::all,
            const std::vector<LoadedPartTarget>* loadedTargets = nullptr) {
            std::vector<LoadedPartView> views;
            if (loadedTargets) {
                for (const auto& target : *loadedTargets) {
                    views.insert(views.end(), target.views.begin(), target.views.end());
                }
            } else if (slot) {
                for (const auto& target : FindLoadedPartTargets(actor, *slot, selection)) {
                    views.insert(views.end(), target.views.begin(), target.views.end());
                }
            } else if (faceNode && faceNode->object) {
                views.push_back({ .firstPerson = false, .object = faceNode->object });
            }
            for (const auto& layer : layers) {
                const auto cacheNamespace = part == "face" ? "skin-face" : "skin";
                const auto expected = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, cacheNamespace);
                if (expected.empty()) continue;
                const auto normalizedExpected = NormalizedTexturePath(expected);
                std::vector<std::string> matchingNodes;
                for (const auto& view : views) {
                    if (!view.object) continue;
                    RE::BSVisit::TraverseScenegraphGeometries(view.object, [&](RE::BSGeometry* geometry) {
                        if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                        const auto* rawName = geometry->name.c_str();
                        const std::string_view geometryName = rawName && rawName[0] != '\0' ? rawName : "";
                        if (!ViewContainsNode(view, geometryName)) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        if (!bcn::skin_geometry::Matches(
                                geometryName, selection, GeometryDiffuseTexture(geometry))) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        auto* shader = geometry->lightingShaderProp_cast();
                        auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                        const auto textureSet = StableTextureSet(material);
                        if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;
                        const auto* actual = textureSet->GetTexturePath(
                            static_cast<RE::BSTextureSet::Texture>(layer.shaderTextureIndex));
                        if (!actual || NormalizedTexturePath(actual) != normalizedExpected) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        matchingNodes.push_back(std::string{ view.firstPerson ? "1p:" : "3p:" } +
                            (rawName && rawName[0] != '\0' ? rawName : "<unnamed>"));
                        return RE::BSVisit::BSVisitControl::kContinue;
                    });
                }
                std::string nodeList;
                for (const auto& node : matchingNodes) {
                    if (!nodeList.empty()) nodeList += ',';
                    nodeList += node;
                }
                SKSE::log::info(
                    "SkinAudit verify actor={:08X} profile='{}' part={} index={}({}) source='{}' cache='{}' matches={} nodes='{}'",
                    actor->GetFormID(), profile.name, part,
                    static_cast<std::uint32_t>(layer.shaderTextureIndex),
                    TextureIndexName(layer.shaderTextureIndex), layer.path, expected,
                    matchingNodes.size(), nodeList);
            }
        };
        const auto bodyRoute = FindLoadedProfileBodyRoute(actor, profile);
        verifyPart(SkinPartName(bodyRoute.slot), bodyLayers, bodyRoute.slot,
            bodyRoute.selection, &bodyRoute.targets);
        verifyPart("cbbe-genital-anal", plan.cbbeGenitalAnal,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        verifyPart("unp-genital-anal", plan.unpGenitalAnal,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::unpGenitalAnal);
        verifyPart("sos-male-genitals", maleGenitalLayers, kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (plan.beastTail) {
            verifyPart("tail-body-atlas", bodyLayers,
                RE::BGSBipedObjectForm::BipedObjectSlot::kTail);
        }
        verifyPart(plan.broadSharedAtlas ? "hands-ube-body-atlas" : "hands", selectedHandsLayers,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
            bcn::skin_geometry::BodySelection::all, &handsTargets);
        verifyPart(plan.broadSharedAtlas ? "feet-ube-body-atlas" : "feet-body-atlas", selectedFeetLayers,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
            bcn::skin_geometry::BodySelection::all, &feetTargets);
        verifyPart("face", faceLayers, std::nullopt);
    }

    [[nodiscard]] bool ClearTexturePart(skee_override::IOverrideInterfaceV2& overrides,
                                        RE::Actor* actor, const bool female,
                                        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                                        const bool includeLegacyTargetMasks = true,
                                        const bcn::skin_geometry::BodySelection selection =
                                            bcn::skin_geometry::BodySelection::all)
    {
        if (!actor) return false;
        std::unordered_set<std::uint64_t> masks;
        const auto addMask = [&](const bool firstPerson, const std::uint32_t mask) {
            if (mask != 0U) masks.insert((static_cast<std::uint64_t>(firstPerson) << 32U) | mask);
        };
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        addMask(false, requestedMask);
        if (actor == RE::PlayerCharacter::GetSingleton()) addMask(true, requestedMask);
        if (includeLegacyTargetMasks) {
            for (const auto& target : FindLoadedPartTargets(actor, slot, selection)) {
                for (const auto& view : target.views) addMask(view.firstPerson, target.slotMask);
            }
        }

        // Do not call RemoveAllSkinOverrides here: that would erase unrelated
        // shader properties written by mods such as Wet Function Redux.
        bool removed{};
        for (const auto identity : masks) {
            const auto firstPerson = (identity >> 32U) != 0U;
            const auto mask = static_cast<std::uint32_t>(identity);
            for (const auto textureIndex : kTextureIndices) {
                skee_override::StringVisitor current;
                const auto exists = overrides.GetSkinOverride(actor, female, firstPerson, mask,
                    static_cast<std::uint16_t>(kShaderTextureProperty),
                    static_cast<std::uint8_t>(textureIndex), current);
                if (!bcn::skin_override::ownership::MayRemove(exists, current.Value())) continue;
                overrides.RemoveSkinOverride(actor, female, firstPerson, mask,
                    static_cast<std::uint16_t>(kShaderTextureProperty), static_cast<std::uint8_t>(textureIndex));
                SKSE::log::info(
                    "SkinOverride persistent-remove actor={:08X} legacy=skin-slot view={} mask={:08X} index={}({}) owner=BCNG",
                    actor->GetFormID(), firstPerson ? "1p" : "3p", mask, textureIndex,
                    TextureIndexName(textureIndex));
                removed = true;
            }
        }
        return removed;
    }

    [[nodiscard]] bool ClearFaceTextures(skee_override::IOverrideInterfaceV2& overrides,
                                          RE::Actor* actor, const bool female,
                                          const std::string_view nodeName, const bool clearDetail)
    {
        bool removed{};
        const std::string ownedNodeName{ nodeName };
        for (const auto textureIndex : kTextureIndices) {
            // A skin pack can contain several FaceGen detail alternatives
            // (freckles, rough, blank). If none matches the actor's live
            // detail texture, leave slot 3 alone instead of silently erasing
            // the actor's existing FaceGen choice.
            if (textureIndex == kFaceDetailTextureIndex && !clearDetail) continue;
            skee_override::StringVisitor current;
            const auto exists = overrides.GetNodeOverride(actor, female, ownedNodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty),
                static_cast<std::uint8_t>(textureIndex), current);
            if (!bcn::skin_override::ownership::MayRemove(exists, current.Value())) continue;
            overrides.RemoveNodeOverride(actor, female, ownedNodeName.c_str(),
                static_cast<std::uint16_t>(kShaderTextureProperty), static_cast<std::uint8_t>(textureIndex));
            SKSE::log::info(
                "SkinOverride persistent-remove actor={:08X} part=face node='{}' index={}({}) owner=BCNG",
                actor->GetFormID(), ownedNodeName, textureIndex, TextureIndexName(textureIndex));
            removed = true;
        }
        return removed;
    }

    // Override v0/v1 predates RaceMenu's public SetVariant wrapper. Constructing
    // its internal OverrideVariant in another DLL is not serialization-safe
    // because strings must be interned in RaceMenu's private StringTable.
    // Use the stable NiOverride Papyrus natives for v0/v1 strings instead. Skin
    // overrides below always receive exactly one biped bit; combined masks are
    // never used, so body, hands and feet cannot overwrite one another. Exact
    // Armor + ArmorAddon + node keys cover skin embedded by current outfits.
    struct LegacyOverrideBatch final
    {
        bcn::frame_tasks::Lease lease;
        std::uint64_t epoch{};
        RE::ActorHandle actor;
        RE::FormID actorFormID{};
        const RE::Actor* identity{};
        const RE::BSScript::Internal::VirtualMachine* vmIdentity{};
        std::uint64_t generation{};
        std::uint64_t session{};
        bool female{};
        bool futanari{};
        bool independentCleanup{};
        std::atomic_uint32_t pending{ 1U };  // submission sentinel
        std::atomic_uint32_t accepted{};
        std::atomic_bool timedOut{};
        std::chrono::steady_clock::time_point deadline;
        std::function<void(std::uint32_t)> completion;
    };

    std::mutex g_legacyWatchdogLock;
    std::vector<std::weak_ptr<LegacyOverrideBatch>> g_legacyWatchdogs;
    bool g_legacyWatchdogArmed{};
    constexpr auto kLegacyCallbackTimeout = std::chrono::seconds(10);
    constexpr std::uint32_t kLegacyWatchdogFrames = 60U;
    constexpr auto kLegacyWatchdogChannel =
        bcn::appearance::WorkChannel::legacySkinWatchdog;

    void QueueLegacyWatchdogSweep();

    void SweepLegacyWatchdogs()
    {
        const auto now = std::chrono::steady_clock::now();
        bool remaining{};
        {
            std::scoped_lock lock(g_legacyWatchdogLock);
            std::erase_if(g_legacyWatchdogs, [&](const auto& weak) {
                const auto batch = weak.lock();
                if (!batch || batch->pending.load(std::memory_order_acquire) == 0U) return true;
                if (now < batch->deadline) {
                    remaining = true;
                    return false;
                }
                if (!batch->timedOut.exchange(true, std::memory_order_acq_rel)) {
                    if (batch->lease) batch->lease->cancelled.store(true, std::memory_order_release);
                    SKSE::log::error(
                        "Body Change NG cancelled an unreturned RaceMenu v0/v1 callback batch for actor {:08X}; pending={} accepted={}",
                        batch->actorFormID, batch->pending.load(std::memory_order_acquire),
                        batch->accepted.load(std::memory_order_acquire));
                }
                return true;
            });
            g_legacyWatchdogArmed = remaining;
        }
        if (remaining) QueueLegacyWatchdogSweep();
    }

    void QueueLegacyWatchdogSweep()
    {
        if (!bcn::frame_tasks::Queue(0U, [] { SweepLegacyWatchdogs(); },
                kLegacyWatchdogFrames, kLegacyWatchdogChannel)) {
            std::scoped_lock lock(g_legacyWatchdogLock);
            g_legacyWatchdogArmed = false;
        }
    }

    [[nodiscard]] constexpr std::string_view SkinPartName(
        const RE::BGSBipedObjectForm::BipedObjectSlot slot) noexcept
    {
        switch (slot) {
        case RE::BGSBipedObjectForm::BipedObjectSlot::kBody: return "body";
        case RE::BGSBipedObjectForm::BipedObjectSlot::kHands: return "hands";
        case RE::BGSBipedObjectForm::BipedObjectSlot::kFeet: return "feet";
        case RE::BGSBipedObjectForm::BipedObjectSlot::kTail: return "tail";
        case kUbeBodySlot: return "ube-body-slot-53";
        case kSosMaleGenitalSlot: return "sos-male-genitals-slot-52";
        default: return "unknown";
        }
    }

    void ArmLegacyWatchdog(const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        bool queueSweep{};
        {
            std::scoped_lock lock(g_legacyWatchdogLock);
            g_legacyWatchdogs.emplace_back(batch);
            queueSweep = !std::exchange(g_legacyWatchdogArmed, true);
        }
        if (queueSweep) QueueLegacyWatchdogSweep();
    }

    struct LegacyMutationTracker final
    {
        std::atomic_uint32_t accepted{};
    };

    [[nodiscard]] auto MakeLegacyBatch(RE::Actor* actor, const std::uint64_t generation,
        const bool futanari = false, const bool independentCleanup = false)
    {
        auto batch = std::make_shared<LegacyOverrideBatch>();
        batch->lease = bcn::frame_tasks::CurrentLease();
        batch->epoch = bcn::frame_tasks::Epoch();
        batch->actor = actor->GetHandle();
        batch->actorFormID = actor->GetFormID();
        batch->identity = actor;
        batch->vmIdentity = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        batch->generation = generation;
        batch->futanari = futanari;
        batch->independentCleanup = independentCleanup;
        batch->session = bcn::ActorRegistry::Get().SessionGeneration();
        batch->female = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        batch->deadline = std::chrono::steady_clock::now() + kLegacyCallbackTimeout;
        ArmLegacyWatchdog(batch);
        return batch;
    }

    [[nodiscard]] bool IsCurrentLegacyChange(const LegacyOverrideBatch& batch)
    {
        if (batch.independentCleanup) return true;
        return batch.futanari ? IsCurrentFutanariChange(batch.actorFormID, batch.generation) :
            IsCurrentSkinChange(batch.actorFormID, batch.generation);
    }

    // Keep an owning actor reference alive throughout an intermediate query
    // callback. Stale callbacks must not reach their captured native arguments.
    [[nodiscard]] RE::NiPointer<RE::Actor> ResolveLegacyBatch(const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        if (!batch || batch->timedOut.load(std::memory_order_acquire) ||
            !bcn::frame_tasks::ValidLease(batch->lease) || !bcn::frame_tasks::IsCurrent(batch->epoch) ||
            batch->session != bcn::ActorRegistry::Get().SessionGeneration() ||
            batch->vmIdentity != RE::BSScript::Internal::VirtualMachine::GetSingleton()) return {};
        const auto actor = batch->actor.get();
        if (!actor || actor.get() != batch->identity || !actor->Is3DLoaded() ||
            !IsCurrentLegacyChange(*batch)) return {};
        const auto* base = actor->GetActorBase();
        if (!base || (base->GetSex() == RE::SEX::kFemale) != batch->female) return {};
        return actor;
    }

    void CompleteLegacyBatch(const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        if (!batch || batch->pending.fetch_sub(1U, std::memory_order_acq_rel) != 1U) return;
        if (batch->timedOut.load(std::memory_order_acquire) ||
            !bcn::frame_tasks::IsCurrent(batch->epoch) || !bcn::frame_tasks::ValidLease(batch->lease) ||
            !IsCurrentLegacyChange(*batch)) return;
        const auto accepted = batch->accepted.load(std::memory_order_acquire);
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto completion = batch->completion;
            bcn::frame_tasks::Continue(batch->lease, [batch, completion, accepted] {
                const auto actor = ResolveLegacyBatch(batch);
                if (actor && completion) completion(accepted);
            });
        }
    }

    class LegacyOverrideCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        explicit LegacyOverrideCallback(std::shared_ptr<LegacyOverrideBatch> batch) : payload_(std::move(batch)) {}
        ~LegacyOverrideCallback() override { Finish(); }
        void operator()(RE::BSScript::Variable) override { Finish(); }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        void Finish()
        {
            if (auto batch = payload_.Take()) CompleteLegacyBatch(*batch);
        }
        bcn::async_work::CompletionPayload<std::shared_ptr<LegacyOverrideBatch>> payload_;
    };

    [[nodiscard]] bool DispatchLegacy(RE::BSScript::Internal::VirtualMachine& vm, const char* function,
        RE::BSScript::IFunctionArguments* arguments, const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        batch->pending.fetch_add(1U, std::memory_order_relaxed);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new LegacyOverrideCallback(batch));
        const auto dispatched = vm.DispatchStaticCall("NiOverride", function, arguments, callback);
        if (dispatched) batch->accepted.fetch_add(1U, std::memory_order_release);
        // On failed dispatch the local callback destructor balances pending.
        return dispatched;
    }

    class LegacyOwnershipQueryCallback final : public RE::BSScript::IStackCallbackFunctor
    {
        struct Payload
        {
            std::shared_ptr<LegacyOverrideBatch> batch;
            std::function<void(std::string)> completion;
        };
    public:
        LegacyOwnershipQueryCallback(std::shared_ptr<LegacyOverrideBatch> batch,
            std::function<void(std::string)> completion) :
            payload_(Payload{std::move(batch), std::move(completion)}) {}

        ~LegacyOwnershipQueryCallback() override
        {
            if (auto payload = payload_.Take()) CompleteLegacyBatch(payload->batch);
        }

        void operator()(RE::BSScript::Variable result) override
        {
            auto payload = payload_.Take();
            if (!payload) return;
            auto batch = std::move(payload->batch);
            // Only inspect locked generation/session data on the VM thread.
            // Stale query results cannot submit another ownership mutation.
            if (!bcn::frame_tasks::IsCurrent(batch->epoch) || !bcn::frame_tasks::ValidLease(batch->lease) ||
                !IsCurrentLegacyChange(*batch)) {
                CompleteLegacyBatch(batch);
                return;
            }
            std::string current;
            if (result.IsString()) current = result.GetString();
            // VM callbacks are not an engine mutation boundary. Hand the
            // follow-up back to the game thread and revalidate there, not only
            // after the entire clear/apply batch has already changed textures.
            if (const auto* tasks = SKSE::GetTaskInterface()) {
                const auto queued = bcn::frame_tasks::Continue(batch->lease,
                    [batch, completion = std::move(payload->completion), current = std::move(current)]() mutable {
                    const auto actor = ResolveLegacyBatch(batch);
                    if (actor && completion) completion(std::move(current));
                    CompleteLegacyBatch(batch);
                });
                if (!queued) CompleteLegacyBatch(batch);
            } else CompleteLegacyBatch(batch);
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        bcn::async_work::CompletionPayload<Payload> payload_;
    };

    void DispatchLegacyOwnershipQuery(RE::BSScript::Internal::VirtualMachine& vm, const char* function,
        RE::BSScript::IFunctionArguments* arguments, const std::shared_ptr<LegacyOverrideBatch>& batch,
        std::function<void(std::string)> completion)
    {
        batch->pending.fetch_add(1U, std::memory_order_relaxed);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
            new LegacyOwnershipQueryCallback(batch, std::move(completion)));
        if (!vm.DispatchStaticCall("NiOverride", function, arguments, callback)) {
            SKSE::log::warn("SkinOverride ownership-query dispatch failed function={}", function);
            // Destructor completes a callback that was never dispatched.
        }
    }

    void DispatchLegacyPartClear(RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor,
        const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::shared_ptr<LegacyOverrideBatch>& batch,
        const bcn::skin_geometry::BodySelection selection =
            bcn::skin_geometry::BodySelection::all)
    {
        std::unordered_set<std::uint64_t> masks;
        const auto addMask = [&](const bool firstPerson, const std::uint32_t mask) {
            if (mask != 0U) masks.insert((static_cast<std::uint64_t>(firstPerson) << 32U) | mask);
        };
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        addMask(false, requestedMask);
        if (actor == RE::PlayerCharacter::GetSingleton()) addMask(true, requestedMask);

        const auto targets = FindLoadedPartTargets(actor, slot, selection);
        for (const auto& target : targets) {
            for (const auto& view : target.views) addMask(view.firstPerson, target.slotMask);
            auto cleanupNodes = target.persistentNodes;
            if (std::ranges::find(cleanupNodes, std::string{}) == cleanupNodes.end()) {
                cleanupNodes.emplace_back();
            }
            for (const auto& node : cleanupNodes) {
                for (const auto textureIndex : kTextureIndices) {
                    auto* armor = target.armor;
                    auto* addon = target.addon;
                    DispatchLegacyOwnershipQuery(vm, "GetOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                        static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                        std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex)), batch,
                        [&vm, actor, female, armor, addon, node, textureIndex, batch](std::string current) {
                            if (!bcn::skin_override::ownership::MayRemove(!current.empty(), current)) return;
                            static_cast<void>(DispatchLegacy(vm, "RemoveOverride", RE::MakeFunctionArguments(
                                static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                                static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                                std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                                static_cast<std::uint32_t>(textureIndex)), batch));
                            SKSE::log::info(
                                "SkinOverride persistent-remove actor={:08X} armor={:08X} addon={:08X} node='{}' index={}({}) owner=BCNG mode=RaceMenu-v0-v1-Papyrus",
                                actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                textureIndex, TextureIndexName(textureIndex));
                        });
                }
            }
        }

        // Clean the broad skin-slot keys written by Body Change NG v0.2.15
        // and earlier without touching non-texture properties from other mods.
        for (const auto identity : masks) {
            const auto firstPerson = (identity >> 32U) != 0U;
            const auto mask = static_cast<std::uint32_t>(identity);
            for (const auto textureIndex : kTextureIndices) {
                DispatchLegacyOwnershipQuery(vm, "GetSkinOverrideString", RE::MakeFunctionArguments(
                    static_cast<RE::TESObjectREFR*>(actor), bool{ female }, bool{ firstPerson },
                    static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(kShaderTextureProperty),
                    static_cast<std::uint32_t>(textureIndex)), batch,
                    [&vm, actor, female, firstPerson, mask, textureIndex, batch](std::string current) {
                        if (!bcn::skin_override::ownership::MayRemove(!current.empty(), current)) return;
                        static_cast<void>(DispatchLegacy(vm, "RemoveSkinOverride", RE::MakeFunctionArguments(
                            static_cast<RE::TESObjectREFR*>(actor), bool{ female }, bool{ firstPerson },
                            static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(kShaderTextureProperty),
                            static_cast<std::uint32_t>(textureIndex)), batch));
                        SKSE::log::info(
                            "SkinOverride persistent-remove actor={:08X} legacy=skin-slot view={} mask={:08X} index={} owner=BCNG mode=RaceMenu-v0-v1-Papyrus",
                            actor->GetFormID(), firstPerson ? "1p" : "3p", mask, textureIndex);
                    });
            }
        }
    }

    void DispatchLegacyTargetsClear(RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const std::vector<LoadedPartTarget>& targets,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        for (const auto& target : targets) {
            auto cleanupNodes = target.persistentNodes;
            if (std::ranges::find(cleanupNodes, std::string{}) == cleanupNodes.end()) {
                cleanupNodes.emplace_back();
            }
            for (const auto& node : cleanupNodes) {
                for (const auto textureIndex : kTextureIndices) {
                    auto* armor = target.armor;
                    auto* addon = target.addon;
                    DispatchLegacyOwnershipQuery(vm, "GetOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                        static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                        std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex)), batch,
                        [&vm, actor, female, armor, addon, node, textureIndex, batch](std::string current) {
                            if (!bcn::skin_override::ownership::MayRemove(!current.empty(), current)) return;
                            static_cast<void>(DispatchLegacy(vm, "RemoveOverride", RE::MakeFunctionArguments(
                                static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                                static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                                std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                                static_cast<std::uint32_t>(textureIndex)), batch));
                            SKSE::log::info(
                                "SkinOverride futanari-remove actor={:08X} armor={:08X} addon={:08X} node='{}' index={} owner=BCNG mode=RaceMenu-v0-v1-Papyrus",
                                actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                textureIndex);
                        });
                }
            }
        }
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacyLoadedPartApply(
        RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch,
        const std::vector<LoadedPartTarget>& targets,
        const std::string_view cacheNamespace = "skin",
        const std::string_view partName = {})
    {
        std::vector<std::pair<std::uint32_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, cacheNamespace);
            if (!path.empty()) resolvedLayers.emplace_back(layer.shaderTextureIndex, std::move(path));
        }
        if (resolvedLayers.empty()) return {};

        auto mutation = std::make_shared<LegacyMutationTracker>();
        std::size_t submitted{};
        for (const auto& target : targets) {
            for (const auto& node : target.persistentNodes) {
                for (const auto& [textureIndex, path] : resolvedLayers) {
                    auto* armor = target.armor;
                    auto* addon = target.addon;
                    DispatchLegacyOwnershipQuery(vm, "GetOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                        static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                        std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex)), batch,
                        [&vm, actor, female, armor, addon, node, textureIndex, path, batch, mutation](std::string current) {
                            const auto exists = !current.empty();
                            if (!bcn::skin_override::ownership::MayReplace(exists, current)) {
                                SKSE::log::warn(
                                    "SkinOverride persistent-register skipped actor={:08X} armor={:08X} addon={:08X} node='{}' index={} reason=foreign-owner mode=RaceMenu-v0-v1-Papyrus current='{}'",
                                    actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                    textureIndex, current);
                                return;
                            }
                            const auto dispatched = DispatchLegacy(vm, "AddOverrideString", RE::MakeFunctionArguments(
                                static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                                static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                                std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                                static_cast<std::uint32_t>(textureIndex), std::string{ path }, true), batch);
                            if (dispatched) mutation->accepted.fetch_add(1U, std::memory_order_release);
                            SKSE::log::info(
                                "SkinOverride persistent-register actor={:08X} armor={:08X} addon={:08X} node='{}' index={}({}) action={} value='{}' mode=RaceMenu-v0-v1-Papyrus",
                                actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                textureIndex, TextureIndexName(textureIndex), exists ? "replace-owned" : "add", path);
                        });
                    ++submitted;
                }
            }
        }
        SKSE::log::info("SkinAudit actor={:08X} part={} stored-keys={} mode=RaceMenu-v0-v1-Papyrus-exact",
            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName, submitted);
        return submitted != 0U ? mutation : std::shared_ptr<LegacyMutationTracker>{};
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacySkinSlotApply(
        RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor, const bool female,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch,
        const std::string_view cacheNamespace = "skin",
        const std::string_view partName = {},
        const bool requirePersistence = false)
    {
        if (!actor) return {};
        const auto mask = static_cast<std::uint32_t>(slot);
        if (mask == 0U || !std::has_single_bit(mask)) return {};

        std::vector<std::pair<std::uint32_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, cacheNamespace);
            if (!path.empty()) resolvedLayers.emplace_back(layer.shaderTextureIndex, std::move(path));
        }
        if (resolvedLayers.empty()) return {};

        auto mutation = std::make_shared<LegacyMutationTracker>();
        std::size_t submitted{};
        const std::string loggedPart{ partName.empty() ? SkinPartName(slot) : partName };
        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            for (const auto& [textureIndex, path] : resolvedLayers) {
                DispatchLegacyOwnershipQuery(vm, "GetSkinOverrideString", RE::MakeFunctionArguments(
                    static_cast<RE::TESObjectREFR*>(actor), bool{ female }, bool{ firstPerson },
                    static_cast<std::uint32_t>(mask),
                    static_cast<std::uint32_t>(kShaderTextureProperty),
                    static_cast<std::uint32_t>(textureIndex)), batch,
                    [&vm, actor, female, firstPerson, mask, textureIndex, path, loggedPart,
                        batch, mutation, requirePersistence](std::string current) {
                        const auto exists = !current.empty();
                        const auto mayPersist =
                            bcn::skin_override::ownership::MayReplace(exists, current);
                        const auto rsvTransient = exists &&
                            bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(current);
                        if (!mayPersist && !rsvTransient) {
                            SKSE::log::warn(
                                "SkinOverride skin-slot skipped actor={:08X} part={} view={} mask={:08X} index={} reason=foreign-owner mode=RaceMenu-v0-v1-Papyrus current='{}'",
                                actor->GetFormID(), loggedPart, firstPerson ? "1p" : "3p",
                                mask, textureIndex, current);
                            return;
                        }
                        if (requirePersistence && !mayPersist) {
                            SKSE::log::info(
                                "SkinOverride conventional-limb slot skipped actor={:08X} part={} view={} mask={:08X} index={} reason=persistence-required current='{}' mode=RaceMenu-v0-v1-Papyrus",
                                actor->GetFormID(), loggedPart, firstPerson ? "1p" : "3p",
                                mask, textureIndex, current);
                            return;
                        }
                        // Persist our own single-slot key. If RSV owns this
                        // channel, paint only the live node and leave RSV's
                        // serialized value untouched.
                        const auto dispatched = DispatchLegacy(vm, "AddSkinOverrideString",
                            RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(actor),
                                bool{ female }, bool{ firstPerson }, static_cast<std::uint32_t>(mask),
                                static_cast<std::uint32_t>(kShaderTextureProperty),
                                static_cast<std::uint32_t>(textureIndex),
                                std::string{ path }, bool{ mayPersist }), batch);
                        if (dispatched) mutation->accepted.fetch_add(1U, std::memory_order_release);
                        SKSE::log::info(
                            "SkinOverride skin-slot-register actor={:08X} part={} view={} mask={:08X} index={}({}) mode={} value='{}' RaceMenu-v0-v1-Papyrus",
                            actor->GetFormID(), loggedPart, firstPerson ? "1p" : "3p", mask,
                            textureIndex, TextureIndexName(textureIndex),
                            mayPersist ? "persistent" : "transient-rsv", path);
                    });
                ++submitted;
            }
        }
        SKSE::log::info(
            "SkinAudit actor={:08X} part={} queried-keys={} mode=RaceMenu-v0-v1-Papyrus-single-skin-slot",
            actor->GetFormID(), loggedPart, submitted);
        return submitted != 0U ? mutation : std::shared_ptr<LegacyMutationTracker>{};
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacyPartApply(
        RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch,
        const bcn::skin_geometry::BodySelection selection = bcn::skin_geometry::BodySelection::all,
        const bool allowExplicitLimbNode = false)
    {
        const auto targets = FindLoadedPartTargets(
            actor, slot, selection, true, allowExplicitLimbNode);
        return DispatchLegacyLoadedPartApply(vm, actor, female, slot, layers, batch, targets);
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacyProfileBodyApply(
        RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor, const bool female,
        const bcn::SkinProfile& profile, const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        const auto route = FindLoadedProfileBodyRoute(actor, profile);
        return DispatchLegacyLoadedPartApply(
            vm, actor, female, route.slot, layers, batch, route.targets);
    }

    void DispatchLegacyFaceClear(RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor,
        const bool female, const std::string_view faceNode, const bool clearDetail,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        auto nodes = LegacyMisdirectedFaceNodes(actor);
        if (std::ranges::find(nodes, faceNode) == nodes.end()) nodes.emplace_back(faceNode);
        for (const auto& node : nodes) {
            for (const auto textureIndex : kTextureIndices) {
                if (node == faceNode && textureIndex == kFaceDetailTextureIndex && !clearDetail) continue;
                DispatchLegacyOwnershipQuery(vm, "GetNodeOverrideString", RE::MakeFunctionArguments(
                    static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ node },
                    static_cast<std::uint32_t>(kShaderTextureProperty),
                    static_cast<std::uint32_t>(textureIndex)), batch,
                    [&vm, actor, female, node, textureIndex, batch](std::string current) {
                        if (!bcn::skin_override::ownership::MayRemove(!current.empty(), current)) return;
                        static_cast<void>(DispatchLegacy(vm, "RemoveNodeOverride", RE::MakeFunctionArguments(
                            static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ node },
                            static_cast<std::uint32_t>(kShaderTextureProperty),
                            static_cast<std::uint32_t>(textureIndex)), batch));
                        SKSE::log::info(
                            "SkinOverride persistent-remove actor={:08X} part=face node='{}' index={}({}) owner=BCNG mode=RaceMenu-v0-v1-Papyrus",
                            actor->GetFormID(), node, textureIndex, TextureIndexName(textureIndex));
                    });
            }
        }
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacyFaceApply(
        RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const FaceNodeInfo& face,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        auto mutation = std::make_shared<LegacyMutationTracker>();
        std::size_t submitted{};
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, "skin-face");
            if (path.empty()) continue;
            SKSE::log::info(
                "SkinAudit expected actor={:08X} part=face node='{}' index={}({}) source='{}' cache='{}' mode=RaceMenu-v0-v1-Papyrus-exact",
                actor->GetFormID(), face.nodeName, static_cast<std::uint32_t>(layer.shaderTextureIndex),
                TextureIndexName(layer.shaderTextureIndex), layer.path, path);
            const auto textureIndex = static_cast<std::uint32_t>(layer.shaderTextureIndex);
            DispatchLegacyOwnershipQuery(vm, "GetNodeOverrideString", RE::MakeFunctionArguments(
                static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ face.nodeName },
                static_cast<std::uint32_t>(kShaderTextureProperty),
                static_cast<std::uint32_t>(textureIndex)), batch,
                [&vm, actor, female, node = face.nodeName, textureIndex, path, batch, mutation](std::string current) {
                    const auto exists = !current.empty();
                    if (!bcn::skin_override::ownership::MayReplace(exists, current)) {
                        if (bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(current)) {
                            // RaceMenu v0/v1 AddNodeOverrideString applies to
                            // the live node even when persistence is false.
                            // That lets RSV retain ownership of its serialized
                            // key while BCNG paints the selected skin now.
                            const auto dispatched = DispatchLegacy(vm, "AddNodeOverrideString",
                                RE::MakeFunctionArguments(
                                    static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                                    std::string{ node },
                                    static_cast<std::uint32_t>(kShaderTextureProperty),
                                    static_cast<std::uint32_t>(textureIndex),
                                    std::string{ path }, false), batch);
                            if (dispatched) {
                                mutation->accepted.fetch_add(1U, std::memory_order_release);
                                bcn::skin_session::MarkTransientFace(actor->GetFormID());
                            }
                            SKSE::log::debug(
                                "SkinOverride live-apply actor={:08X} part=face node='{}' index={} provider=RSV mode=RaceMenu-v0-v1-Papyrus value='{}'",
                                actor->GetFormID(), node, textureIndex, path);
                            return;
                        }
                        SKSE::log::warn(
                            "SkinOverride persistent-register skipped actor={:08X} part=face node='{}' index={} reason=foreign-owner mode=RaceMenu-v0-v1-Papyrus current='{}'",
                            actor->GetFormID(), node, textureIndex, current);
                        return;
                    }
                    const auto dispatched = DispatchLegacy(vm, "AddNodeOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ node },
                        static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex), std::string{ path }, true), batch);
                    if (dispatched) mutation->accepted.fetch_add(1U, std::memory_order_release);
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part=face node='{}' index={}({}) action={} value='{}' mode=RaceMenu-v0-v1-Papyrus",
                        actor->GetFormID(), node, textureIndex, TextureIndexName(textureIndex),
                        exists ? "replace-owned" : "add", path);
                });
            ++submitted;
        }
        return submitted != 0U ? mutation : std::shared_ptr<LegacyMutationTracker>{};
    }

    void DispatchLegacyRsvFaceTransient(
        RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor,
        const bool female, const FaceNodeInfo& face,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        for (const auto& layer : layers) {
            const auto path = bcn::runtime_assets::TexturePathFromGameRelative(
                layer.path, "skin-face");
            if (path.empty()) continue;
            const auto textureIndex = static_cast<std::uint32_t>(layer.shaderTextureIndex);
            DispatchLegacyOwnershipQuery(vm, "GetNodeOverrideString",
                RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(actor),
                    bool{ female }, std::string{ face.nodeName },
                    static_cast<std::uint32_t>(kShaderTextureProperty),
                    static_cast<std::uint32_t>(textureIndex)),
                batch, [&vm, actor, female, node = face.nodeName,
                           textureIndex, path, batch](std::string current) {
                    if (!bcn::skin_override::ownership::MayTransientlyPaintRsv(
                            !current.empty(), current)) return;
                    // persist=false is the public v0/v1 live-only route. It
                    // never replaces RSV's serialized owner.
                    static_cast<void>(DispatchLegacy(vm, "AddNodeOverrideString",
                        RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(actor),
                            bool{ female }, std::string{ node },
                            static_cast<std::uint32_t>(kShaderTextureProperty),
                            static_cast<std::uint32_t>(textureIndex),
                            std::string{ path }, false), batch));
                });
        }
    }

    void MarkCurrentSkinContent(RE::Actor* actor, const bcn::SkinProfile& profile, std::uint64_t generation)
    {
        if (!actor || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        if (profile.contentHash != bcn::SkinProfiles::Get().ContentHash(profile.id)) {
            [[maybe_unused]] const auto refreshed = bcn::skin_override::QueueApply(actor, profile.id);
            return;
        }
        bcn::ActorRegistry::Get().MarkSkinApplied(actor, profile.id, false);
    }

    void ApplyLegacyNow(RE::ActorHandle actorHandle, const bcn::SkinProfile profile,
        const std::uint64_t generation, const bool settledRepaint,
        const std::uint8_t remainingVerificationRepairs)
    {
        const auto actor = actorHandle.get();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !vm ||
            !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        auto* base = actor->GetActorBase();
        if (!base) return;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        if ((female && profile.sex != bcn::SkinSex::female) ||
            (!female && profile.sex != bcn::SkinSex::male)) return;
        if (!ProfileMatchesActor(actor.get(), profile)) return;
        // Cancel any delayed reconciliation belonging to the previous profile.
        // The new face apply records this actor again only when an RSV-owned
        // persistent face channel is actually encountered.
        static_cast<void>(ReleaseRsvTransientFace(actor->GetFormID()));
        const auto faceNode = FaceNode(actor.get(), base);
        const auto initialPlan = EffectiveSkinPlan(
            profile, actor.get(), base, faceNode ? faceNode->detailFilename : std::string_view{});
        if (initialPlan.requiresFaceGeometry && !faceNode) {
            SKSE::log::warn(
                "Body Change NG skipped face layers from skin '{}' on actor {:08X}: no live FaceGen geometry was found",
                profile.name, actor->GetFormID());
            return;
        }

        auto clearBatch = MakeLegacyBatch(actor.get(), generation);
        clearBatch->completion = [actorHandle, profile, generation, settledRepaint,
                                     remainingVerificationRepairs](const std::uint32_t) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentActor->Is3DLoaded() || !currentVM ||
                !IsCurrentSkinChange(currentActor->GetFormID(), generation)) return;
            auto* currentBase = currentActor->GetActorBase();
            if (!currentBase || !ProfileMatchesActor(currentActor.get(), profile)) return;
            const auto currentFace = FaceNode(currentActor.get(), currentBase);
            const auto currentPlan = EffectiveSkinPlan(profile, currentActor.get(), currentBase,
                currentFace ? currentFace->detailFilename : std::string_view{});
            if (currentPlan.requiresFaceGeometry && !currentFace) return;
            const auto currentFemale = currentBase->GetSex() == RE::SEX::kFemale;
            const auto& currentFaceLayers = currentPlan.face;
            const auto& currentBodyLayers = currentPlan.body;
            const auto& currentHandsLayers = currentPlan.hands;
            const auto& currentFeetLayers = currentPlan.feet;
            const auto currentMaleGenitalLayers = EffectiveMaleGenitalLayers(
                profile, currentActor.get(), currentBase);

            auto applyBatch = MakeLegacyBatch(currentActor.get(), generation);
            auto requiredParts = std::make_shared<
                std::vector<std::shared_ptr<LegacyMutationTracker>>>();
            auto needsExactRepair = std::make_shared<std::atomic_bool>(false);
            applyBatch->completion = [actorHandle, profile, generation, settledRepaint,
                remainingVerificationRepairs, requiredParts,
                needsExactRepair](const std::uint32_t) {
                const auto settledActor = actorHandle.get();
                if (!settledActor || !IsCurrentSkinChange(settledActor->GetFormID(), generation)) return;
                const auto appliedParts = static_cast<std::size_t>(std::ranges::count_if(
                    *requiredParts, [](const auto& part) {
                        return part && part->accepted.load(std::memory_order_acquire) != 0U;
                    }));
                const auto complete = !requiredParts->empty() && appliedParts == requiredParts->size();
                const auto finish = [actorHandle, profile, generation, settledRepaint,
                    remainingVerificationRepairs,
                    complete, appliedParts, requestedParts = requiredParts->size()] {
                    const auto finalActor = actorHandle.get();
                    if (!finalActor || !IsCurrentSkinChange(finalActor->GetFormID(), generation)) return;
                    auto* finalVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    const auto rebuildQueued = !settledRepaint && finalVM &&
                        QueueNiNodeUpdate(*finalVM, finalActor.get(), actorHandle, generation);
                    if (bcn::skin_override::CanFinalizeSkinApply(complete, rebuildQueued)) {
                        MarkCurrentSkinContent(finalActor.get(), profile, generation);
                        SKSE::log::info(
                            "Body Change NG applied texture skin profile '{}' to actor {:08X} through RaceMenu Override v0/v1 Papyrus{}",
                            profile.name, finalActor->GetFormID(),
                            settledRepaint ? " after the final Biped rebuild" : "");
                    } else if (!complete) {
                        SKSE::log::warn(
                            "Body Change NG dispatched only {}/{} currently available parts from skin '{}' to actor {:08X} through RaceMenu Override v0/v1 Papyrus; the desired selection remains pending",
                            appliedParts, requestedParts, profile.name, finalActor->GetFormID());
                    }
                    if (!rebuildQueued) {
                        QueueSettledSkinAudit(actorHandle, generation, 3U, false,
                            remainingVerificationRepairs);
                    }
                };

                if (!needsExactRepair->load(std::memory_order_acquire)) {
                    finish();
                    return;
                }

                // NiOverride v0/v1 has no native store-only operation. Its
                // persistent skin-slot call can repaint every addon on the
                // selected Skin Armor while reserving a hidden hand or foot.
                // Restore all currently visible conventional surfaces through
                // exact Armor+Addon+node keys after those callbacks finish.
                auto* repairVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                auto* repairBase = settledActor->GetActorBase();
                if (!repairVM || !repairBase ||
                    !ProfileMatchesActor(settledActor.get(), profile)) {
                    finish();
                    return;
                }
                const auto repairFemale = repairBase->GetSex() == RE::SEX::kFemale;
                const auto repairFace = FaceNode(settledActor.get(), repairBase);
                const auto repairPlan = EffectiveSkinPlan(profile, settledActor.get(), repairBase,
                    repairFace ? repairFace->detailFilename : std::string_view{});
                auto repairBatch = MakeLegacyBatch(settledActor.get(), generation);
                repairBatch->completion = [finish](const std::uint32_t) { finish(); };
                if (!repairPlan.body.empty()) {
                    static_cast<void>(DispatchLegacyProfileBodyApply(*repairVM,
                        settledActor.get(), repairFemale, profile,
                        repairPlan.body, repairBatch));
                }
                if (!repairPlan.cbbeGenitalAnal.empty()) {
                    static_cast<void>(DispatchLegacyPartApply(*repairVM,
                        settledActor.get(), repairFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                        repairPlan.cbbeGenitalAnal, repairBatch,
                        bcn::skin_geometry::BodySelection::cbbeGenitalAnal));
                }
                if (!repairPlan.unpGenitalAnal.empty()) {
                    static_cast<void>(DispatchLegacyPartApply(*repairVM,
                        settledActor.get(), repairFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                        repairPlan.unpGenitalAnal, repairBatch,
                        bcn::skin_geometry::BodySelection::unpGenitalAnal));
                }
                if (!repairPlan.hands.empty()) {
                    static_cast<void>(DispatchLegacyPartApply(*repairVM,
                        settledActor.get(), repairFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
                        repairPlan.hands, repairBatch,
                        bcn::skin_geometry::BodySelection::all, true));
                }
                if (!repairPlan.feet.empty()) {
                    static_cast<void>(DispatchLegacyPartApply(*repairVM,
                        settledActor.get(), repairFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
                        repairPlan.feet, repairBatch,
                        bcn::skin_geometry::BodySelection::all, true));
                }
                if (repairPlan.beastTail && !repairPlan.body.empty()) {
                    static_cast<void>(DispatchLegacyPartApply(*repairVM,
                        settledActor.get(), repairFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kTail,
                        repairPlan.body, repairBatch));
                }
                CompleteLegacyBatch(repairBatch);
            };

            const auto submitPart = [&](const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                const std::vector<bcn::SkinTextureLayer>& layers,
                const bcn::skin_geometry::BodySelection selection =
                    bcn::skin_geometry::BodySelection::all,
                const bool allowExplicitLimbNode = false,
                const bcn::SkinLayout layout = bcn::SkinLayout::unknown) {
                if (layers.empty()) return;
                auto exact = DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                    slot, layers, applyBatch, selection, allowExplicitLimbNode);
                const auto slotRoute = bcn::ResolveLimbSkinSlotRoute(
                    layout, exact ? 1U : 0U);
                if (slotRoute == bcn::LimbSkinSlotRoute::broadLive) {
                    requiredParts->push_back(DispatchLegacySkinSlotApply(*currentVM,
                        currentActor.get(), currentFemale, slot, layers, applyBatch));
                } else if (slotRoute == bcn::LimbSkinSlotRoute::persistentOnly ||
                    slotRoute == bcn::LimbSkinSlotRoute::persistentAndExact) {
                    needsExactRepair->store(true, std::memory_order_release);
                    requiredParts->push_back(DispatchLegacySkinSlotApply(*currentVM,
                        currentActor.get(), currentFemale, slot, layers, applyBatch,
                        "skin", SkinPartName(slot), true));
                } else {
                    requiredParts->push_back(std::move(exact));
                }
            };
            const auto hasPrimaryParts = !currentBodyLayers.empty() || !currentHandsLayers.empty() ||
                !currentFeetLayers.empty() || !currentFaceLayers.empty();
            if (!currentBodyLayers.empty()) {
                auto exact = DispatchLegacyProfileBodyApply(*currentVM,
                    currentActor.get(), currentFemale, profile, currentBodyLayers, applyBatch);
                if (currentPlan.broadSharedAtlas) {
                    requiredParts->push_back(DispatchLegacySkinSlotApply(*currentVM,
                        currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                        currentBodyLayers, applyBatch, "skin", "body-slot-32"));
                    requiredParts->push_back(DispatchLegacySkinSlotApply(*currentVM,
                        currentActor.get(), currentFemale, kUbeBodySlot,
                        currentBodyLayers, applyBatch, "skin", "ube-body-slot-53"));
                } else {
                    requiredParts->push_back(std::move(exact));
                }
            }
            const auto submitGenitalAnal = [&](const std::vector<bcn::SkinTextureLayer>& layers,
                const bcn::skin_geometry::BodySelection selection) {
                if (layers.empty()) return;
                if (hasPrimaryParts) {
                    static_cast<void>(DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, applyBatch, selection));
                } else {
                    requiredParts->push_back(DispatchLegacyPartApply(*currentVM,
                        currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                        layers, applyBatch, selection));
                }
            };
            submitGenitalAnal(currentPlan.cbbeGenitalAnal,
                bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
            submitGenitalAnal(currentPlan.unpGenitalAnal,
                bcn::skin_geometry::BodySelection::unpGenitalAnal);
            submitPart(kSosMaleGenitalSlot, currentMaleGenitalLayers,
                bcn::skin_geometry::BodySelection::maleGenitals);
            if (currentPlan.beastTail && !currentBodyLayers.empty()) {
                    // Tail availability is auxiliary: a tail-hiding outfit or
                    // custom race setup must not keep an otherwise complete
                    // skin selection perpetually pending.
                    static_cast<void>(DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kTail, currentBodyLayers, applyBatch));
            }
            submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands, currentHandsLayers,
                bcn::skin_geometry::BodySelection::all, true, currentPlan.layout);
            submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, currentFeetLayers,
                bcn::skin_geometry::BodySelection::all, true, currentPlan.layout);
            if (!currentFaceLayers.empty()) {
                requiredParts->push_back(currentFace ?
                    DispatchLegacyFaceApply(*currentVM, currentActor.get(), currentFemale,
                        *currentFace, currentFaceLayers, applyBatch) :
                    std::shared_ptr<LegacyMutationTracker>{});
            }
            if (std::ranges::none_of(*requiredParts, [](const auto& part) { return part != nullptr; })) {
                SKSE::log::warn("Body Change NG found no RaceMenu v0/v1 skin targets for '{}'", profile.name);
            }
            CompleteLegacyBatch(applyBatch);
        };

        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch,
            bcn::skin_geometry::BodySelection::regular);
        DispatchLegacyPartClear(*vm, actor.get(), female, kUbeBodySlot, clearBatch,
            bcn::skin_geometry::BodySelection::regular);
        // Female SOS/TNG/TRX/ERF geometry is owned by the independent
        // Futanari tab. A normal BodySkin reapply must not erase that choice.
        // Male actors continue to use BodySkin's SOS addon textures.
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) DispatchLegacyPartClear(
            *vm, actor.get(), female, kSosMaleGenitalSlot, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail, clearBatch);
        if (faceNode) DispatchLegacyFaceClear(*vm, actor.get(), female, faceNode->nodeName, true, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void ClearLegacyNow(RE::ActorHandle actorHandle, const std::uint64_t generation,
        std::string unavailableProfileId = {})
    {
        const auto actor = actorHandle.get();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !vm ||
            !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        auto* base = actor->GetActorBase();
        if (!base) return;
        static_cast<void>(ReleaseRsvTransientFace(actor->GetFormID()));
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto faceNode = FaceNode(actor.get(), base);

        auto clearBatch = MakeLegacyBatch(actor.get(), generation);
        clearBatch->completion = [actorHandle, generation,
                                     unavailableProfileId = std::move(unavailableProfileId)](
                                     const std::uint32_t accepted) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentVM ||
                !IsCurrentSkinChange(currentActor->GetFormID(), generation)) return;
            // RaceMenu v0/v1 can remove every serialized key yet leave an
            // already-loaded armor clone painted. Inspect this actor once at
            // completion so an idempotent second Default click can repair the
            // live body/hands/feet as well as the face.
            const auto liveDefault = bcn::skin_override::LiveSkinStateMatches(
                currentActor.get(), {}, true,
                bcn::skin_override::LiveCheckScope::fullProfile);
            const auto staleLiveClone = liveDefault.has_value() && !*liveDefault;
            if (accepted == 0U && !staleLiveClone) {
                SKSE::log::info("Body Change NG found no remaining RaceMenu v0/v1 skin texture overrides for actor {:08X}",
                    currentActor->GetFormID());
                const auto useDefault = unavailableProfileId.empty();
                bcn::ActorRegistry::Get().MarkSkinApplied(
                    currentActor.get(), unavailableProfileId, useDefault);
                return;
            }
            if (!QueueNiNodeUpdate(*currentVM, currentActor.get(), actorHandle, generation, true, true)) {
                if (staleLiveClone) currentActor->DoReset3D(false);
                QueueSettledSkinAudit(actorHandle, generation, 2U);
            }
            SKSE::log::info("Body Change NG removed its RaceMenu v0/v1 skin texture overrides for actor {:08X}",
                currentActor->GetFormID());
            const auto useDefault = unavailableProfileId.empty();
            bcn::ActorRegistry::Get().MarkSkinApplied(
                currentActor.get(), unavailableProfileId, useDefault);
        };
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch,
            bcn::skin_geometry::BodySelection::regular);
        DispatchLegacyPartClear(*vm, actor.get(), female, kUbeBodySlot, clearBatch,
            bcn::skin_geometry::BodySelection::regular);
        // Default BodySkin and Default Futanari Skin are independent choices.
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) DispatchLegacyPartClear(
            *vm, actor.get(), female, kSosMaleGenitalSlot, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail, clearBatch);
        if (faceNode) DispatchLegacyFaceClear(*vm, actor.get(), female, faceNode->nodeName, true, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void ApplyNow(RE::ActorHandle actorHandle, const bcn::SkinProfile profile,
        const std::uint64_t generation, const bool settledRepaint,
        const std::uint8_t remainingVerificationRepairs)
    {
        const auto actor = actorHandle.get();
        if (!actor || !actor->Is3DLoaded()) return;
        if (!IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        auto* base = actor->GetActorBase();
        if (!base) return;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        if ((female && profile.sex != bcn::SkinSex::female) || (!female && profile.sex != bcn::SkinSex::male)) return;
        if (!ProfileMatchesActor(actor.get(), profile)) return;
        static_cast<void>(ReleaseRsvTransientFace(actor->GetFormID()));
        auto* overrides = OverrideInterfaceV2();
        if (!overrides) return;

        const auto faceNode = FaceNode(actor.get(), base);
        const auto plan = EffectiveSkinPlan(
            profile, actor.get(), base, faceNode ? faceNode->detailFilename : std::string_view{});
        if (plan.requiresFaceGeometry && !faceNode) {
            SKSE::log::warn(
                "Body Change NG skipped face layers from skin '{}' on actor {:08X}: no live FaceGen geometry was found",
                profile.name, actor->GetFormID());
            return;
        }
        const auto& faceLayers = plan.face;
        const auto& bodyLayers = plan.body;
        const auto& handsLayers = plan.hands;
        const auto& feetLayers = plan.feet;
        const auto maleGenitalLayers = EffectiveMaleGenitalLayers(profile, actor.get(), base);
        // Single-bit skin-slot keys cover the actor's underlying body, hands
        // and feet across naked/equipped rebuilds. Exact armor/addon keys remain
        // complementary for outfits that embed visible skin in their own NIF.
        bool removed = ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female);
        const auto includeLegacyTargetMasks = ClaimLegacyCleanup(actor->GetFormID());
        removed = ClearTexturePart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, includeLegacyTargetMasks,
            bcn::skin_geometry::BodySelection::regular) || removed;
        removed = ClearTexturePart(*overrides, actor.get(), female, kUbeBodySlot,
            includeLegacyTargetMasks, bcn::skin_geometry::BodySelection::regular) || removed;
        // Never let a female BodySkin selection clear the independently owned
        // futanari material on slot 52. Male SOS skins remain BodySkin-owned.
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) {
            removed = ClearTexturePart(*overrides, actor.get(), female, kSosMaleGenitalSlot,
                includeLegacyTargetMasks) || removed;
        }
        removed = ClearTexturePart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, includeLegacyTargetMasks) || removed;
        removed = ClearTexturePart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, includeLegacyTargetMasks) || removed;
        removed = ClearTexturePart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail, includeLegacyTargetMasks) || removed;
        // Reconcile every owned exact key before writing the new profile.
        // This restores the actor's underlying texture for absent parts and
        // absent diffuse/normal/subsurface/detail/specular channels.
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::regular) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female, kUbeBodySlot,
            bcn::skin_geometry::BodySelection::regular) || removed;
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) {
            removed = ClearArmorAddonPart(
                *overrides, actor.get(), female, kSosMaleGenitalSlot) || removed;
        }
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || removed;
        if (faceNode) {
            removed = ClearFaceTextures(*overrides, actor.get(), female, faceNode->nodeName, true) || removed;
        }

        // Discover every currently loaded target before the first live
        // RaceMenu repaint. ApplyArmorOverrides replaces shader materials in
        // place; traversing the same clone again immediately afterwards can
        // otherwise observe a detached/transitional texture set. Keeping this
        // read phase ahead of the write phase also makes one selection a
        // deterministic snapshot of the actor's current equipment.
        LoadedProfileBodyRoute bodyRoute;
        if (!bodyLayers.empty()) bodyRoute = FindLoadedProfileBodyRoute(actor.get(), profile);
        const auto cbbeGenitalTargets = plan.cbbeGenitalAnal.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor.get(),
                RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        const auto unpGenitalTargets = plan.unpGenitalAnal.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor.get(),
                RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                bcn::skin_geometry::BodySelection::unpGenitalAnal);
        const auto maleGenitalTargets = maleGenitalLayers.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor.get(),
                kSosMaleGenitalSlot, bcn::skin_geometry::BodySelection::maleGenitals);
        const auto tailTargets = plan.beastTail && !bodyLayers.empty() ?
            FindLoadedPartTargets(actor.get(), RE::BGSBipedObjectForm::BipedObjectSlot::kTail) :
            std::vector<LoadedPartTarget>{};
        const auto& selectedHandsLayers = plan.hands;
        const auto& selectedFeetLayers = plan.feet;
        const auto handsTargets = selectedHandsLayers.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor.get(),
                RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
                bcn::skin_geometry::BodySelection::all, true, true);
        const auto feetTargets = selectedFeetLayers.empty() ?
            std::vector<LoadedPartTarget>{} : FindLoadedPartTargets(actor.get(),
                RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
                bcn::skin_geometry::BodySelection::all, true, true);

        std::size_t requestedParts{};
        std::size_t appliedParts{};
        const auto applyPart = [&](const RE::BGSBipedObjectForm::BipedObjectSlot slot,
            const std::vector<bcn::SkinTextureLayer>& layers,
            const std::vector<LoadedPartTarget>& targets,
            const bcn::SkinLayout layout = bcn::SkinLayout::unknown) {
            if (layers.empty()) return;
            ++requestedParts;
            const auto exactApplied = ApplyLoadedPart(
                *overrides, actor.get(), female, slot, layers, targets);
            auto durableApplied = exactApplied;
            const auto slotRoute = bcn::ResolveLimbSkinSlotRoute(
                layout, targets.size());
            if (slotRoute == bcn::LimbSkinSlotRoute::broadLive) {
                durableApplied = ApplySkinSlotPart(
                    *overrides, actor.get(), female, slot, layers);
            } else if (slotRoute == bcn::LimbSkinSlotRoute::persistentOnly ||
                slotRoute == bcn::LimbSkinSlotRoute::persistentAndExact) {
                // The actor Skin Armor slot is authoritative. Exact worn-addon
                // targets only mirror that value onto verified visible skin;
                // they never replace the actor's durable hand/foot record.
                const auto stored = ApplySkinSlotPart(*overrides, actor.get(), female,
                    slot, layers, "skin", SkinPartName(slot), false);
                durableApplied = stored &&
                    (slotRoute == bcn::LimbSkinSlotRoute::persistentOnly || exactApplied);
            }
            if (durableApplied) ++appliedParts;
        };
        const auto hasPrimaryParts = !bodyLayers.empty() || !handsLayers.empty() ||
            !feetLayers.empty() || !faceLayers.empty();
        if (!bodyLayers.empty()) {
            ++requestedParts;
            auto durableApplied = ApplyLoadedPart(*overrides, actor.get(), female, bodyRoute.slot,
                bodyLayers, bodyRoute.targets);
            if (plan.broadSharedAtlas) {
                durableApplied = ApplySkinSlotPart(*overrides, actor.get(), female,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                    bodyLayers, "skin", "body-slot-32");
                durableApplied = ApplySkinSlotPart(*overrides, actor.get(), female,
                    kUbeBodySlot, bodyLayers, "skin", "ube-body-slot-53") && durableApplied;
            }
            if (durableApplied) {
                ++appliedParts;
            }
        }
        const auto applyGenitalAnal = [&](const std::vector<bcn::SkinTextureLayer>& layers,
            const std::vector<LoadedPartTarget>& targets) {
            if (layers.empty()) return;
            if (hasPrimaryParts) {
                static_cast<void>(ApplyLoadedPart(*overrides, actor.get(), female,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, targets));
            } else {
                ++requestedParts;
                if (ApplyLoadedPart(*overrides, actor.get(), female,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                        layers, targets)) {
                    ++appliedParts;
                }
            }
        };
        applyGenitalAnal(plan.cbbeGenitalAnal, cbbeGenitalTargets);
        applyGenitalAnal(plan.unpGenitalAnal, unpGenitalTargets);
        applyPart(kSosMaleGenitalSlot, maleGenitalLayers, maleGenitalTargets);
        if (plan.beastTail && !bodyLayers.empty()) {
                static_cast<void>(ApplyLoadedPart(*overrides, actor.get(), female,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kTail, bodyLayers, tailTargets));
        }
        applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
            selectedHandsLayers, handsTargets, plan.layout);
        applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
            selectedFeetLayers, feetTargets, plan.layout);
        if (!faceLayers.empty()) {
            ++requestedParts;
            if (faceNode && ApplyFacePart(*overrides, actor.get(), female, *faceNode, faceLayers)) {
                ++appliedParts;
            }
        }
        const auto complete = requestedParts != 0U && appliedParts == requestedParts;
        bool rebuildQueued{};
        if (!settledRepaint && removed) {
            if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
                rebuildQueued = QueueNiNodeUpdate(*vm, actor.get(), actorHandle, generation);
            }
        }
        if (bcn::skin_override::CanFinalizeSkinApply(complete, rebuildQueued)) {
            MarkCurrentSkinContent(actor.get(), profile, generation);
            SKSE::log::info(
                "Body Change NG applied texture skin profile '{}' to actor {:08X} synchronously{}",
                profile.name, actor->GetFormID(),
                settledRepaint ? " after the final Biped rebuild" : "");
        } else if (!complete) {
            SKSE::log::warn(
                "Body Change NG applied only {}/{} currently available parts from skin '{}' to actor {:08X}; the desired selection remains pending for a later 3D/equipment refresh",
                appliedParts, requestedParts, profile.name, actor->GetFormID());
        }
        if (rebuildQueued) return;
        if (!settledRepaint && removed) {
            QueueSettledSkinAudit(actorHandle, generation, 2U, false,
                remainingVerificationRepairs);
        } else {
            QueueSettledSkinAudit(actorHandle, generation, 0U, false,
                remainingVerificationRepairs);
        }
    }

    void ClearNow(RE::ActorHandle actorHandle, const std::uint64_t generation,
        std::string unavailableProfileId = {})
    {
        const auto actor = actorHandle.get();
        if (!actor || !actor->Is3DLoaded()) return;
        if (!IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        auto* base = actor->GetActorBase();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* overrides = OverrideInterfaceV2();
        if (!base || !vm || !overrides) return;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto faceNode = FaceNode(actor.get(), base);
        const auto hadTransientRsvFace = ReleaseRsvTransientFace(actor->GetFormID());
        bool cleared{};
        cleared = ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, true,
            bcn::skin_geometry::BodySelection::regular) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, kUbeBodySlot, true,
            bcn::skin_geometry::BodySelection::regular) || cleared;
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) {
            cleared = ClearTexturePart(
                *overrides, actor.get(), female, kSosMaleGenitalSlot) || cleared;
        }
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::regular) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, kUbeBodySlot,
            bcn::skin_geometry::BodySelection::regular) || cleared;
        if (bcn::futanari::BodySkinOwnsSosSlot(female)) {
            cleared = ClearArmorAddonPart(
                *overrides, actor.get(), female, kSosMaleGenitalSlot) || cleared;
        }
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || cleared;
        if (faceNode) cleared = ClearFaceTextures(*overrides, actor.get(), female, faceNode->nodeName, true) || cleared;
        const auto liveDefault = bcn::skin_override::LiveSkinStateMatches(actor.get(), {}, true,
            bcn::skin_override::LiveCheckScope::fullProfile);
        const auto staleLiveClone = liveDefault.has_value() && !*liveDefault;
        if (!cleared && !hadTransientRsvFace && !staleLiveClone) {
            SKSE::log::info("Body Change NG found no remaining skin texture overrides for actor {:08X}",
                actor->GetFormID());
            const auto useDefault = unavailableProfileId.empty();
            bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), unavailableProfileId, useDefault);
            return;
        }
        if (!QueueNiNodeUpdate(*vm, actor.get(), actorHandle, generation, true, true)) {
            if (staleLiveClone) actor->DoReset3D(false);
            QueueSettledSkinAudit(actorHandle, generation, 2U);
        }
        SKSE::log::info("Body Change NG removed its RaceMenu skin texture overrides for actor {:08X}",
            actor->GetFormID());
        const auto useDefault = unavailableProfileId.empty();
        bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), unavailableProfileId, useDefault);
    }

    bcn::skin_override::ApplyResult QueueClearInternal(RE::Actor* actor,
        std::string unavailableProfileId)
    {
        if (!bcn::frame_tasks::Active()) return bcn::skin_override::ApplyResult::noTaskInterface;
        if (!actor) return bcn::skin_override::ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return bcn::skin_override::ApplyResult::actor3DUnavailable;
        const auto* overrideInterface = OverrideInterface();
        const auto legacyOverride = bcn::racemenu_override::UsesPapyrus(OverrideRoute());
        if (!overrideInterface ||
            (legacyOverride && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
            return bcn::skin_override::ApplyResult::unavailable;
        }
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return bcn::skin_override::ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginSkinChange(actor->GetFormID());
        // An unavailable desired profile is intentionally retained in the
        // Actor Registry, while the runtime selection cache stays empty so
        // equipment refreshes do not repeatedly retry a missing folder.
        bcn::skin_session::TrackSkinSelection(actor->GetFormID(), {});
        bcn::frame_tasks::Queue(actor->GetFormID(),
            [handle, generation, legacyOverride,
                unavailableProfileId = std::move(unavailableProfileId)]() mutable {
                if (legacyOverride) {
                    ClearLegacyNow(handle, generation, std::move(unavailableProfileId));
                } else {
                    ClearNow(handle, generation, std::move(unavailableProfileId));
                }
            }, 1, bcn::appearance::WorkChannel::skinApply);
        return bcn::skin_override::ApplyResult::queued;
    }

    [[nodiscard]] std::optional<std::uint64_t> CurrentSkinGeneration(
        const RE::FormID actorFormID)
    {
        return bcn::skin_session::CurrentSkinGeneration(actorFormID);
    }

    void QueueSettledFaceRefresh(RE::ActorHandle actorHandle, const RE::FormID actorFormID,
        std::string profileId, const std::uint64_t refreshGeneration,
        const std::chrono::steady_clock::time_point notBefore)
    {
        bcn::frame_tasks::Queue(actorFormID,
            [actorHandle, actorFormID, profileId = std::move(profileId),
                refreshGeneration, notBefore]() mutable {
                if (!IsCurrentRsvFaceRefresh(actorFormID, refreshGeneration)) return;
                if (std::chrono::steady_clock::now() < notBefore) {
                    QueueSettledFaceRefresh(actorHandle, actorFormID, std::move(profileId),
                        refreshGeneration, notBefore);
                    return;
                }
                const auto actor = actorHandle.get();
                if (!actor || !actor->Is3DLoaded() || actor->GetFormID() != actorFormID) return;
                const auto currentProfile = bcn::skin_override::CurrentProfileId(actor.get());
                if (!currentProfile || *currentProfile != profileId) return;
                const auto profile = bcn::SkinProfiles::Get().Find(profileId);
                auto* base = actor->GetActorBase();
                if (!profile || !base || !ProfileMatchesActor(actor.get(), *profile)) return;
                const auto face = FaceNode(actor.get(), base);
                if (!face) return;
                const auto layers = EffectiveSkinPlan(
                    *profile, actor.get(), base, face->detailFilename).face;
                if (layers.empty()) return;
                const auto female = base->GetSex() == RE::SEX::kFemale;
                if (UsesLegacyOverride()) {
                    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    const auto skinGeneration = CurrentSkinGeneration(actorFormID);
                    if (!vm || !skinGeneration) return;
                    auto batch = MakeLegacyBatch(actor.get(), *skinGeneration);
                    DispatchLegacyRsvFaceTransient(
                        *vm, actor.get(), female, *face, layers, batch);
                    CompleteLegacyBatch(batch);
                } else if (auto* overrides = OverrideInterfaceV2()) {
                    ApplyRsvFaceTransientV2(
                        *overrides, actor.get(), female, *face, layers);
                }
            }, 8U, bcn::appearance::WorkChannel::rsvFaceReconcile);
    }

    void CleanupLegacyBodySkinV2Now(const RE::ActorHandle& actorHandle)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* overrides = OverrideInterfaceV2();
        if (!actor || !actor->Is3DLoaded() || !base || !overrides ||
            !ClaimLegacyCleanup(actor->GetFormID())) return;

        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto faceNode = FaceNode(actor.get(), base);
        bool removed = ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female);
        const auto clearPart = [&](const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                                   const bcn::skin_geometry::BodySelection selection =
                                       bcn::skin_geometry::BodySelection::all) {
            removed = ClearTexturePart(*overrides, actor.get(), female, slot, true, selection) || removed;
            removed = ClearArmorAddonPart(*overrides, actor.get(), female, slot, selection) || removed;
        };
        clearPart(RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::regular);
        clearPart(kUbeBodySlot, bcn::skin_geometry::BodySelection::regular);
        clearPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
        clearPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        clearPart(RE::BGSBipedObjectForm::BipedObjectSlot::kTail);
        // Slot 52 is deliberately excluded. Male SOS/TNG and female futanari
        // are independent adapters with their own apply/clear ownership.
        if (faceNode) {
            removed = ClearFaceTextures(
                *overrides, actor.get(), female, faceNode->nodeName, true) || removed;
        }
        if (removed) {
            actor->DoReset3D(false);
            SKSE::log::info(
                "Body Change NG removed owned 1.1.x BodySkin NiOverride keys before native TXST migration for actor {:08X}",
                actor->GetFormID());
        }
    }

    void CleanupLegacyBodySkinPapyrusNow(const RE::ActorHandle& actorHandle)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !base || !vm ||
            !ClaimLegacyCleanup(actor->GetFormID())) return;

        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto faceNode = FaceNode(actor.get(), base);
        auto batch = MakeLegacyBatch(actor.get(), 0U, false, true);
        batch->completion = [actorHandle](const std::uint32_t removed) {
            const auto settledActor = actorHandle.get();
            if (!settledActor || removed == 0U) return;
            settledActor->DoReset3D(false);
            SKSE::log::info(
                "Body Change NG removed owned 1.1.x BodySkin NiOverride keys before native TXST migration for actor {:08X} through RaceMenu v0/v1",
                settledActor->GetFormID());
        };
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, batch,
            bcn::skin_geometry::BodySelection::regular);
        DispatchLegacyPartClear(*vm, actor.get(), female, kUbeBodySlot, batch,
            bcn::skin_geometry::BodySelection::regular);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, batch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, batch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail, batch);
        if (faceNode) {
            DispatchLegacyFaceClear(
                *vm, actor.get(), female, faceNode->nodeName, true, batch);
        }
        CompleteLegacyBatch(batch);
    }

    void QueueLegacyBodySkinCleanup(RE::Actor* actor)
    {
        if (!actor || !actor->Is3DLoaded() || !bcn::frame_tasks::Active()) return;
        const auto route = OverrideRoute();
        if (route == bcn::racemenu_override::Route::unsupported) return;
        const auto handle = actor->GetHandle();
        const auto legacy = bcn::racemenu_override::UsesPapyrus(route);
        // NativeSkinBackend invokes this admission hook before it submits form
        // graph work. Actor serialization then keeps the old Armor/Addon
        // identities stable until their owned exact keys have been inspected.
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, legacy] {
            if (legacy) CleanupLegacyBodySkinPapyrusNow(handle);
            else CleanupLegacyBodySkinV2Now(handle);
        }, 0U, bcn::appearance::WorkChannel::legacyBodySkinCleanup, true);
    }

    void ApplyMaleGenitalsV2Now(RE::ActorHandle actorHandle,
        const bcn::SkinProfile profile, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !actor->Is3DLoaded() || !base || base->GetSex() != RE::SEX::kMale ||
            !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        const auto layers = EffectiveMaleGenitalLayers(profile, actor.get(), base);
        if (layers.empty()) return;
        const auto targets = FindLoadedPartTargets(actor.get(), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        auto* overrides = OverrideInterfaceV2();
        if (!overrides || targets.empty()) return;
        static_cast<void>(ClearArmorAddonTargets(*overrides, actor.get(), false, targets));
        if (ApplyLoadedPart(*overrides, actor.get(), false, kSosMaleGenitalSlot,
                layers, targets, "skin", "male-genitals")) {
            bcn::skin_session::MarkAddonApplied(actor->GetFormID(),
                bcn::skin_session::AddonTextureChannel::maleGenitals,
                AddonTargetSignature(targets, 0x4D414C45ULL));
            SKSE::log::info(
                "Body Change NG applied the selected male BodySkin to the active SOS/TNG addon for actor {:08X}",
                actor->GetFormID());
        }
    }

    void ClearMaleGenitalsV2Now(
        RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !actor->Is3DLoaded() || !base || base->GetSex() != RE::SEX::kMale ||
            !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        const auto targets = FindLoadedPartTargets(actor.get(), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::maleGenitals);
        auto* overrides = OverrideInterfaceV2();
        if (!overrides || targets.empty()) return;
        const auto cleared = ClearArmorAddonTargets(*overrides, actor.get(), false, targets);
        if (cleared) {
            if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
                static_cast<void>(QueueNiNodeUpdate(
                    *vm, actor.get(), actorHandle, generation, false));
            }
        }
    }

    void ApplyMaleGenitalsLegacyNow(RE::ActorHandle actorHandle,
        const bcn::SkinProfile profile, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !base || base->GetSex() != RE::SEX::kMale ||
            !vm || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        const auto layers = EffectiveMaleGenitalLayers(profile, actor.get(), base);
        const auto targets = FindLoadedPartTargets(actor.get(), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (layers.empty() || targets.empty()) return;

        auto clearBatch = MakeLegacyBatch(actor.get(), generation);
        clearBatch->completion = [actorHandle, profile, generation](const std::uint32_t) {
            const auto current = actorHandle.get();
            auto* currentBase = current ? current->GetActorBase() : nullptr;
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!current || !current->Is3DLoaded() || !currentBase || !currentVM ||
                currentBase->GetSex() != RE::SEX::kMale ||
                !IsCurrentSkinChange(current->GetFormID(), generation)) return;
            const auto currentLayers = EffectiveMaleGenitalLayers(
                profile, current.get(), currentBase);
            const auto currentTargets = FindLoadedPartTargets(current.get(),
                kSosMaleGenitalSlot, bcn::skin_geometry::BodySelection::maleGenitals);
            if (currentLayers.empty() || currentTargets.empty()) return;
            auto applyBatch = MakeLegacyBatch(current.get(), generation);
            applyBatch->completion = [actorHandle, generation](const std::uint32_t accepted) {
                const auto settled = actorHandle.get();
                auto* settledVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (!settled || !settledVM || accepted == 0U ||
                    !IsCurrentSkinChange(settled->GetFormID(), generation)) return;
                bcn::skin_session::MarkAddonApplied(settled->GetFormID(),
                    bcn::skin_session::AddonTextureChannel::maleGenitals,
                    MaleGenitalTargetSignature(settled.get()));
                static_cast<void>(QueueNiNodeUpdate(
                    *settledVM, settled.get(), actorHandle, generation, false));
            };
            static_cast<void>(DispatchLegacyLoadedPartApply(*currentVM, current.get(), false,
                kSosMaleGenitalSlot, currentLayers, applyBatch, currentTargets,
                "skin", "male-genitals"));
            CompleteLegacyBatch(applyBatch);
        };
        DispatchLegacyTargetsClear(*vm, actor.get(), false, targets, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void ClearMaleGenitalsLegacyNow(
        RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !base || base->GetSex() != RE::SEX::kMale ||
            !vm || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        const auto targets = FindLoadedPartTargets(actor.get(), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::maleGenitals);
        if (targets.empty()) return;
        auto clearBatch = MakeLegacyBatch(actor.get(), generation);
        clearBatch->completion = [actorHandle, generation](const std::uint32_t accepted) {
            const auto current = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!current || !currentVM || accepted == 0U ||
                !IsCurrentSkinChange(current->GetFormID(), generation)) return;
            static_cast<void>(QueueNiNodeUpdate(
                *currentVM, current.get(), actorHandle, generation, false));
        };
        DispatchLegacyTargetsClear(*vm, actor.get(), false, targets, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void QueueMaleGenitalApply(RE::Actor* actor, const bcn::SkinProfile& profile,
        const std::uint64_t generation)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base || base->GetSex() != RE::SEX::kMale ||
            profile.maleGenitals.empty()) return;
        const auto layers = EffectiveMaleGenitalLayers(profile, actor, base);
        if (layers.empty()) return;
        std::vector<bcn::runtime_assets::TexturePreparation> paths;
        paths.reserve(layers.size());
        for (const auto& layer : layers) paths.push_back({ layer.path, "skin" });
        const auto handle = actor->GetHandle();
        const auto legacy = UsesLegacyOverride();
        bcn::frame_tasks::Queue(actor->GetFormID(),
            [handle, profile, paths = std::move(paths), generation, legacy]() mutable {
                const auto current = handle.get();
                if (!current || !IsCurrentSkinChange(
                        current->GetFormID(), generation)) return;
                const auto lease = bcn::frame_tasks::CurrentLease();
                auto continueApply =
                    [lease, handle, profile, generation, legacy](const bool prepared) mutable {
                        if (!prepared) return;
                        static_cast<void>(bcn::frame_tasks::Continue(lease,
                            [handle, profile, generation, legacy] {
                                if (legacy) {
                                    ApplyMaleGenitalsLegacyNow(handle, profile, generation);
                                } else {
                                    ApplyMaleGenitalsV2Now(handle, profile, generation);
                                }
                            }));
                    };
                if (!bcn::runtime_assets::PrepareTexturePathsAsync(
                        (static_cast<std::uint64_t>(current->GetFormID()) << 2U) | 2U,
                        std::move(paths), continueApply,
                        bcn::async_work::FrameTaskQueue::InteractiveLease(lease))) {
                    continueApply(false);
                }
            }, 1U, bcn::appearance::WorkChannel::maleGenitalSkinApply);
    }

    void QueueMaleGenitalClear(RE::Actor* actor, const std::uint64_t generation)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base || base->GetSex() != RE::SEX::kMale) return;
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::maleGenitals);
        const auto handle = actor->GetHandle();
        const auto legacy = UsesLegacyOverride();
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, generation, legacy] {
            if (legacy) ClearMaleGenitalsLegacyNow(handle, generation);
            else ClearMaleGenitalsV2Now(handle, generation);
        }, 1U, bcn::appearance::WorkChannel::maleGenitalSkinApply);
    }

    void ApplyFutanariV2Now(RE::ActorHandle actorHandle,
        const bcn::FutanariSkinProfile profile, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        if (!actor || !actor->Is3DLoaded() ||
            !IsCurrentFutanariChange(actor->GetFormID(), generation)) return;
        const auto route = FindLoadedFutanariRoute(actor.get());
        if (!route.type || *route.type != profile.type || route.targets.empty()) return;
        auto* overrides = OverrideInterfaceV2();
        if (!overrides) return;
        static_cast<void>(ClearArmorAddonTargets(*overrides, actor.get(), true, route.targets));
        if (ApplyLoadedPart(*overrides, actor.get(), true, kSosMaleGenitalSlot,
                profile.layers, route.targets, "futanari", "futanari-genitals")) {
            bcn::skin_session::MarkAddonApplied(actor->GetFormID(),
                bcn::skin_session::AddonTextureChannel::futanari,
                FutanariTargetSignature(route));
            SKSE::log::info(
                "Body Change NG applied futanari skin '{}' ({}) to actor {:08X}",
                profile.name, bcn::FutanariSkinTypeLabel(profile.type), actor->GetFormID());
        }
    }

    void ClearFutanariV2Now(RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        if (!actor || !actor->Is3DLoaded() ||
            !IsCurrentFutanariChange(actor->GetFormID(), generation)) return;
        const auto route = FindLoadedFutanariRoute(actor.get());
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::futanari);
        auto* overrides = OverrideInterfaceV2();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!overrides || route.targets.empty()) return;
        const auto cleared = ClearArmorAddonTargets(*overrides, actor.get(), true, route.targets);
        if (cleared && vm) {
            static_cast<void>(QueueNiNodeUpdate(*vm, actor.get(), actorHandle, generation, false));
        }
        SKSE::log::info("Body Change NG restored the default futanari skin for actor {:08X}",
            actor->GetFormID());
    }

    void ApplyFutanariLegacyNow(RE::ActorHandle actorHandle,
        const bcn::FutanariSkinProfile profile, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !vm ||
            !IsCurrentFutanariChange(actor->GetFormID(), generation)) return;
        const auto route = FindLoadedFutanariRoute(actor.get());
        if (!route.type || *route.type != profile.type || route.targets.empty()) return;

        auto clearBatch = MakeLegacyBatch(actor.get(), generation, true);
        clearBatch->completion = [actorHandle, profile, generation](const std::uint32_t) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentActor->Is3DLoaded() || !currentVM ||
                !IsCurrentFutanariChange(currentActor->GetFormID(), generation)) return;
            const auto currentRoute = FindLoadedFutanariRoute(currentActor.get());
            if (!currentRoute.type || *currentRoute.type != profile.type ||
                currentRoute.targets.empty()) return;
            auto applyBatch = MakeLegacyBatch(currentActor.get(), generation, true);
            applyBatch->completion = [actorHandle, profile, generation](const std::uint32_t accepted) {
                const auto settledActor = actorHandle.get();
                if (!settledActor || !IsCurrentFutanariChange(
                        settledActor->GetFormID(), generation)) return;
                if (accepted != 0U) {
                    bcn::skin_session::MarkAddonApplied(settledActor->GetFormID(),
                        bcn::skin_session::AddonTextureChannel::futanari,
                        FutanariTargetSignature(FindLoadedFutanariRoute(
                            settledActor.get(), false)));
                    SKSE::log::info(
                        "Body Change NG applied futanari skin '{}' ({}) to actor {:08X} through RaceMenu Override v0/v1 Papyrus",
                        profile.name, bcn::FutanariSkinTypeLabel(profile.type),
                        settledActor->GetFormID());
                    // The legacy Papyrus route stores exact persistent keys but
                    // does not repaint an already loaded addon clone. Rebuild
                    // this actor once after the batch, just like BodySkin, so
                    // UBE SOS/TNG, UBE/CBBE TRX, and CBBE ERF all update now.
                    if (auto* settledVM = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
                        static_cast<void>(QueueNiNodeUpdate(
                            *settledVM, settledActor.get(), actorHandle, generation, false));
                    }
                }
            };
            static_cast<void>(DispatchLegacyLoadedPartApply(*currentVM, currentActor.get(), true,
                kSosMaleGenitalSlot, profile.layers, applyBatch, currentRoute.targets,
                "futanari", "futanari-genitals"));
            CompleteLegacyBatch(applyBatch);
        };
        DispatchLegacyTargetsClear(*vm, actor.get(), true, route.targets, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void ClearFutanariLegacyNow(RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !vm ||
            !IsCurrentFutanariChange(actor->GetFormID(), generation)) return;
        const auto route = FindLoadedFutanariRoute(actor.get());
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::futanari);
        if (route.targets.empty()) return;
        auto clearBatch = MakeLegacyBatch(actor.get(), generation, true);
        clearBatch->completion = [actorHandle, generation](const std::uint32_t) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentVM || !IsCurrentFutanariChange(
                    currentActor->GetFormID(), generation)) return;
            static_cast<void>(QueueNiNodeUpdate(
                *currentVM, currentActor.get(), actorHandle, generation, false));
            SKSE::log::info(
                "Body Change NG restored the default futanari skin for actor {:08X} through RaceMenu Override v0/v1 Papyrus",
                currentActor->GetFormID());
        };
        DispatchLegacyTargetsClear(*vm, actor.get(), true, route.targets, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }
}

namespace bcn::skin_override
{
    void ResetSessionState()
    {
        bcn::native_skin::ResetSessionState();
        bcn::runtime_assets::CancelTexturePreparations();
        {
            std::scoped_lock lock(g_legacyWatchdogLock);
            for (const auto& weak : g_legacyWatchdogs) {
                if (const auto batch = weak.lock()) {
                    batch->timedOut.store(true, std::memory_order_release);
                    if (batch->lease) batch->lease->cancelled.store(true, std::memory_order_release);
                }
            }
            g_legacyWatchdogs.clear();
            g_legacyWatchdogArmed = false;
        }
        bcn::skin_session::Reset();
    }

    ApplyResult QueueApply(RE::Actor* actor, std::string profileId)
    {
        const auto actorFormID = actor ? actor->GetFormID() : 0U;
        const auto profile = bcn::SkinProfiles::Get().Find(profileId);
        const auto needsRsvFaceBridge = actor && profile && HasPotentialFaceLayers(*profile) &&
            ActorHasRsvFaceOwnership(actor);
        const auto result = bcn::native_skin::QueueApply(actor, profileId,
            [actor] { QueueLegacyBodySkinCleanup(actor); });
        if (result == ApplyResult::queued && actor) {
            const auto generation = BeginSkinChange(actorFormID);
            bcn::skin_session::TrackSkinSelection(actorFormID, std::move(profileId));
            if (needsRsvFaceBridge) {
                bcn::skin_session::MarkTransientFace(actorFormID);
                SKSE::log::info(
                    "Body Change NG enabled the isolated RSV face bridge for actor {:08X}",
                    actorFormID);
            }
            if (profile) {
                if (profile->maleGenitals.empty()) QueueMaleGenitalClear(actor, generation);
                else QueueMaleGenitalApply(actor, *profile, generation);
            }
        }
        if (result == ApplyResult::missingProfile && actor) {
            [[maybe_unused]] const auto cleared = bcn::native_skin::QueueClear(actor,
                [actor] { QueueLegacyBodySkinCleanup(actor); });
            const auto generation = BeginSkinChange(actorFormID);
            bcn::skin_session::TrackSkinSelection(actorFormID, {});
            static_cast<void>(ReleaseRsvTransientFace(actorFormID));
            QueueMaleGenitalClear(actor, generation);
        }
        return result;
    }

    ApplyResult QueueClear(RE::Actor* actor)
    {
        const auto result = bcn::native_skin::QueueClear(actor,
            [actor] { QueueLegacyBodySkinCleanup(actor); });
        if (result == ApplyResult::queued && actor) {
            const auto generation = BeginSkinChange(actor->GetFormID());
            bcn::skin_session::TrackSkinSelection(actor->GetFormID(), {});
            static_cast<void>(ReleaseRsvTransientFace(actor->GetFormID()));
            QueueMaleGenitalClear(actor, generation);
        }
        return result;
    }

    std::optional<std::string> CurrentProfileId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        if (const auto native = bcn::native_skin::CurrentProfileId(actor)) return native;
        if (const auto selected = bcn::ActorRegistry::Get().SelectedSkinId(actor)) return selected;
        return bcn::ActorRegistry::Get().AppliedSkinId(actor);
    }

    bool HasTrackedSelection(const RE::Actor* actor)
    {
        return bcn::native_skin::HasTrackedSelection(actor);
    }

    bool HasCurrentMaleGenitalSkin(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base || base->GetSex() != RE::SEX::kMale) return false;
        const auto profileId = CurrentProfileId(actor);
        const auto profile = profileId ? bcn::SkinProfiles::Get().Find(*profileId) :
            std::optional<bcn::SkinProfile>{};
        return profile && !profile->maleGenitals.empty();
    }

    void QueueReapplyCurrentMaleGenitals(RE::Actor* actor, const bool onlyIfAddonChanged)
    {
        if (!actor || !HasCurrentMaleGenitalSkin(actor)) return;
        if (onlyIfAddonChanged) {
            const auto signature = MaleGenitalTargetSignature(actor);
            if (signature == 0U) {
                // Forget a removed addon so equipping the same Form again is
                // still treated as a new loaded target (required by the
                // legacy node route, whose override lived on the old clone).
                bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
                    bcn::skin_session::AddonTextureChannel::maleGenitals);
                return;
            }
            if (!bcn::skin_session::NeedsAddonReapply(
                    bcn::skin_session::AppliedAddonSignature(actor->GetFormID(),
                        bcn::skin_session::AddonTextureChannel::maleGenitals),
                    signature)) {
                return;
            }
        }
        const auto profileId = CurrentProfileId(actor);
        const auto profile = profileId ? bcn::SkinProfiles::Get().Find(*profileId) :
            std::optional<bcn::SkinProfile>{};
        if (!profile) return;
        auto generation = CurrentSkinGeneration(actor->GetFormID());
        if (!generation) {
            generation = BeginSkinChange(actor->GetFormID());
            bcn::skin_session::TrackSkinSelection(actor->GetFormID(), profile->id);
        }
        QueueMaleGenitalApply(actor, *profile, *generation);
    }

    std::optional<bcn::FutanariSkinType> CurrentFutanariType(
        RE::Actor* actor, const bool refresh)
    {
        if (!actor || !actor->Is3DLoaded()) return std::nullopt;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return std::nullopt;
        if (!refresh) {
            const auto cached = bcn::skin_session::CachedFutanariType(actor->GetFormID());
            if (cached.cached) return cached.type;
        }
        const auto detected = FindLoadedFutanariRoute(actor, false).type;
        bcn::skin_session::CacheFutanariType(actor->GetFormID(), detected);
        return detected;
    }

    ApplyResult QueueApplyFutanari(RE::Actor* actor, std::string profileId)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return ApplyResult::incompatibleSex;
        const auto* overrideInterface = OverrideInterface();
        const auto legacyOverride = bcn::racemenu_override::UsesPapyrus(OverrideRoute());
        if (!overrideInterface ||
            (legacyOverride && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
            return ApplyResult::unavailable;
        }
        const auto profile = bcn::FutanariSkinProfiles::Get().Find(profileId);
        if (!profile) return ApplyResult::missingProfile;
        const auto actorType = CurrentFutanariType(actor, true);
        if (!actorType) return ApplyResult::futanariGeometryUnavailable;
        if (*actorType != profile->type) return ApplyResult::incompatibleFutanariType;
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;

        bcn::ActorRegistry::Get().SetFutanariSkin(actor, profile->id);
        const auto handle = actor->GetHandle();
        const auto generation = BeginFutanariChange(actor->GetFormID());
        bcn::frame_tasks::Queue(actor->GetFormID(),
            [handle, profile = *profile, generation, legacyOverride] {
                const auto current = handle.get();
                if (!current || !IsCurrentFutanariChange(current->GetFormID(), generation)) return;
                if (profile.contentHash != bcn::FutanariSkinProfiles::Get().ContentHash(profile.id)) {
                    QueueReapplyCurrentFutanari(current.get());
                    return;
                }
                std::vector<bcn::runtime_assets::TexturePreparation> paths;
                paths.reserve(profile.layers.size());
                for (const auto& layer : profile.layers) {
                    paths.push_back({ layer.path, "futanari" });
                }
                const auto lease = bcn::frame_tasks::CurrentLease();
                const auto continueApply = [lease, handle, profile, generation, legacyOverride](const bool prepared) {
                    if (!prepared) {
                        SKSE::log::warn(
                            "Body Change NG could not prepare every futanari texture for '{}'; unavailable channels remain unchanged",
                            profile.name);
                    }
                    static_cast<void>(bcn::frame_tasks::Continue(lease,
                        [handle, profile, generation, legacyOverride] {
                            const auto resolved = handle.get();
                            if (!resolved || !IsCurrentFutanariChange(
                                    resolved->GetFormID(), generation)) return;
                            if (legacyOverride) {
                                ApplyFutanariLegacyNow(handle, profile, generation);
                            } else {
                                ApplyFutanariV2Now(handle, profile, generation);
                            }
                        }));
                };
                if (!bcn::runtime_assets::PrepareTexturePathsAsync(
                        (static_cast<std::uint64_t>(current->GetFormID()) << 1U) | 1U,
                        std::move(paths), continueApply,
                        bcn::async_work::FrameTaskQueue::InteractiveLease(lease))) {
                    continueApply(false);
                }
            }, 1U, bcn::appearance::WorkChannel::futanariSkinApply);
        return ApplyResult::queued;
    }

    ApplyResult QueueClearFutanari(RE::Actor* actor)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        bcn::ActorRegistry::Get().ClearFutanariSkin(actor);
        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
            bcn::skin_session::AddonTextureChannel::futanari);
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        const auto* overrideInterface = OverrideInterface();
        const auto legacyOverride = bcn::racemenu_override::UsesPapyrus(OverrideRoute());
        if (!overrideInterface ||
            (legacyOverride && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
            return ApplyResult::unavailable;
        }
        if (!CurrentFutanariType(actor, true)) return ApplyResult::futanariGeometryUnavailable;
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginFutanariChange(actor->GetFormID());
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, generation, legacyOverride] {
            if (legacyOverride) ClearFutanariLegacyNow(handle, generation);
            else ClearFutanariV2Now(handle, generation);
        }, 1U, bcn::appearance::WorkChannel::futanariSkinApply);
        return ApplyResult::queued;
    }

    std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* actor)
    {
        return bcn::ActorRegistry::Get().SelectedFutanariSkinId(actor);
    }

    void QueueReapplyCurrentFutanari(RE::Actor* actor, const bool onlyIfAddonChanged)
    {
        if (const auto profile = CurrentFutanariProfileId(actor)) {
            if (onlyIfAddonChanged) {
                const auto signature = actor ?
                    FutanariTargetSignature(FindLoadedFutanariRoute(actor, false)) : 0U;
                if (signature == 0U) {
                    if (actor) {
                        bcn::skin_session::ClearAddonApplied(actor->GetFormID(),
                            bcn::skin_session::AddonTextureChannel::futanari);
                    }
                    return;
                }
                if (!bcn::skin_session::NeedsAddonReapply(
                        bcn::skin_session::AppliedAddonSignature(actor->GetFormID(),
                            bcn::skin_session::AddonTextureChannel::futanari),
                        signature)) {
                    return;
                }
            }
            [[maybe_unused]] const auto result = QueueApplyFutanari(actor, *profile);
        }
    }

    void InvalidateFutanariDetection(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0U) return;
        bcn::skin_session::InvalidateFutanariType(actorFormID);
    }

    void NotifyNiNodeUpdated(RE::Actor* actor)
    {
        if (!actor || !actor->Is3DLoaded() || !bcn::frame_tasks::Active()) return;
        // A NiNode update is not an equip/unequip boundary. During the
        // rebuild the genital partClone can be absent even though its biped
        // slot and ArmorAddon remain equipped. TESEquipEvent, actor selection
        // and actor teardown are the exact cache invalidation points for a
        // real slot ownership change.
        const auto profileId = CurrentProfileId(actor);
        if (!profileId) return;
        // A cell detach intentionally drops reference-scoped transient state,
        // while the native Face TXST remains owned at ActorBase scope. Rebuild
        // the bridge marker only from current, bounded evidence so a reloaded
        // RSV face is not left behind and an unrelated NiNode update still
        // performs no queued work.
        if (!HasRsvTransientFace(actor->GetFormID())) {
            const auto profile = bcn::SkinProfiles::Get().Find(*profileId);
            if (!profile || !HasPotentialFaceLayers(*profile) ||
                !ActorHasRsvFaceOwnership(actor)) return;
            bcn::skin_session::MarkTransientFace(actor->GetFormID());
        }
        if (!CurrentSkinGeneration(actor->GetFormID())) {
            static_cast<void>(BeginSkinChange(actor->GetFormID()));
            bcn::skin_session::TrackSkinSelection(actor->GetFormID(), *profileId);
        }
        // RSV's head effect intentionally waits 0.1 seconds before restoring
        // its serialized face keys. Apply BCNG immediately on selection, then
        // reconcile only this already-selected actor once after that boundary.
        // Channel coalescing collapses repeated NiNode events; there is no
        // polling, catalog scan, or all-NPC pass.
        const auto generation = BeginRsvFaceRefresh(actor->GetFormID());
        QueueSettledFaceRefresh(actor->GetHandle(), actor->GetFormID(), *profileId,
            generation, std::chrono::steady_clock::now() + std::chrono::milliseconds(150));
    }

    [[maybe_unused]] std::optional<bool> LegacyLiveSkinStateMatches(RE::Actor* actor, const std::string_view profileId,
        const bool expectDefault, const LiveCheckScope scope)
    {
        if (!actor || !actor->Is3DLoaded()) return std::nullopt;

        const auto hasOwnedLiveTexture = [&] {
            bool hasOwnedTexture{};
            const auto inspectRoot = [&](RE::NiAVObject* root) {
                if (!root || hasOwnedTexture) return;
                RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    auto* shader = geometry->lightingShaderProp_cast();
                    auto* material = shader ?
                        static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                    const auto textureSet = StableTextureSet(material);
                    if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;
                    for (const auto textureIndex : kTextureIndices) {
                        const auto* path = textureSet->GetTexturePath(
                            static_cast<RE::BSTextureSet::Texture>(textureIndex));
                        if (path && bcn::skin_override::ownership::IsOwnedBodySkinTexturePath(path)) {
                            hasOwnedTexture = true;
                            return RE::BSVisit::BSVisitControl::kStop;
                        }
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            };
            inspectRoot(actor->Get3D(false));
            if (actor == RE::PlayerCharacter::GetSingleton()) inspectRoot(actor->Get3D(true));
            return hasOwnedTexture;
        };
        if (expectDefault) return !hasOwnedLiveTexture();

        const auto profile = bcn::SkinProfiles::Get().Find(profileId);
        auto* base = actor->GetActorBase();
        if (!profile) {
            const auto cleanFallback = !hasOwnedLiveTexture();
            if (cleanFallback) {
                // Keep the unavailable desired ID in the persistent registry,
                // but suppress repeated equipment-event retries this session.
                bcn::skin_session::TrackSkinSelection(actor->GetFormID(), {});
            }
            return cleanFallback;
        }
        if (!base || !ProfileMatchesActor(actor, *profile)) return false;
        const auto faceNode = scope == LiveCheckScope::fullProfile ?
            FaceNode(actor, base) : std::optional<FaceNodeInfo>{};
        std::size_t comparableLayers{};
        bool mismatch{};
        const auto inspectPart = [&](const std::vector<bcn::SkinTextureLayer>& layers,
            const std::optional<RE::BGSBipedObjectForm::BipedObjectSlot> slot,
            const bcn::skin_geometry::BodySelection selection = bcn::skin_geometry::BodySelection::all,
            const std::string_view cacheNamespace = "skin",
            const std::vector<LoadedPartTarget>* loadedTargets = nullptr,
            const bool allowExplicitLimbNode = false) {
            std::vector<LoadedPartView> views;
            if (loadedTargets) {
                for (const auto& target : *loadedTargets) {
                    views.insert(views.end(), target.views.begin(), target.views.end());
                }
            } else if (slot) {
                for (const auto& target : FindLoadedPartTargets(
                         actor, *slot, selection, false, allowExplicitLimbNode)) {
                    views.insert(views.end(), target.views.begin(), target.views.end());
                }
            } else if (faceNode && faceNode->object) {
                views.push_back({ .firstPerson = false, .object = faceNode->object });
            }
            if (views.empty()) return;

            struct LayerExpectation final
            {
                std::uint8_t textureIndex{};
                std::string normalizedPath;
                bool available{};
                bool sawMatch{};
                bool sawDifferent{};
            };
            std::vector<LayerExpectation> expectations;
            expectations.reserve(layers.size());
            for (const auto& layer : layers) {
                const auto expected = bcn::runtime_assets::ExpectedTexturePathFromGameRelative(
                    layer.path, cacheNamespace);
                const auto expectedAvailable = !expected.empty() &&
                    bcn::runtime_assets::CachedTextureExists(expected);
                expectations.push_back({
                    .textureIndex = layer.shaderTextureIndex,
                    .normalizedPath = expectedAvailable ? NormalizedTexturePath(expected) : std::string{},
                    .available = expectedAvailable
                });
            }
            bool sawComparableGeometry{};
            for (const auto& view : views) {
                if (!view.object) continue;
                RE::BSVisit::TraverseScenegraphGeometries(view.object, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto* rawName = geometry->name.c_str();
                    const std::string_view geometryName = rawName && rawName[0] != '\0' ? rawName : "";
                    if (!ViewContainsNode(view, geometryName)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (!bcn::skin_geometry::Matches(
                            geometryName, selection, GeometryDiffuseTexture(geometry))) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (slot && selection != bcn::skin_geometry::BodySelection::maleGenitals &&
                        !IsSkinGeometry(geometry, view.actorSkinArmor)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    auto* shader = geometry->lightingShaderProp_cast();
                    auto* material = shader ?
                        static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                    const auto textureSet = StableTextureSet(material);
                    if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;
                    sawComparableGeometry = true;
                    for (auto& expectation : expectations) {
                        const auto* actual = textureSet->GetTexturePath(
                            static_cast<RE::BSTextureSet::Texture>(expectation.textureIndex));
                        if (actual && expectation.available &&
                            NormalizedTexturePath(actual) == expectation.normalizedPath) {
                            expectation.sawMatch = true;
                        } else {
                            expectation.sawDifferent = true;
                        }
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            }
            if (!sawComparableGeometry) return;
            for (const auto& expectation : expectations) {
                ++comparableLayers;
                if (!expectation.sawMatch || expectation.sawDifferent) mismatch = true;
            }
        };

        const auto plan = EffectiveSkinPlan(
            *profile, actor, base, faceNode ? faceNode->detailFilename : std::string_view{});
        const auto bodyRoute = FindLoadedProfileBodyRoute(actor, *profile, false);
        inspectPart(plan.body, bodyRoute.slot,
            bodyRoute.selection, "skin", &bodyRoute.targets);
        inspectPart(plan.cbbeGenitalAnal, RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        inspectPart(plan.unpGenitalAnal, RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::unpGenitalAnal);
        inspectPart(EffectiveMaleGenitalLayers(*profile, actor, base), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (plan.beastTail) {
            inspectPart(plan.body,
                RE::BGSBipedObjectForm::BipedObjectSlot::kTail);
        }
        inspectPart(plan.hands, RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
            bcn::skin_geometry::BodySelection::all, "skin", nullptr, true);
        inspectPart(plan.feet, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
            bcn::skin_geometry::BodySelection::all, "skin", nullptr, true);
        if (scope == LiveCheckScope::fullProfile) {
            inspectPart(plan.face, std::nullopt,
                bcn::skin_geometry::BodySelection::all, "skin-face");
        }
        return comparableLayers != 0U && !mismatch;
    }

    std::optional<bool> LiveSkinStateMatches(RE::Actor* actor,
        const std::string_view profileId, const bool expectDefault,
        const LiveCheckScope)
    {
        return bcn::native_skin::LiveStateMatches(actor, profileId, expectDefault);
    }

    void AuditNow(RE::Actor* actor, const std::string_view reason)
    {
        if (!actor) return;
        SKSE::log::info("SkinAudit manual begin actor={:08X} reason='{}'", actor->GetFormID(), reason);
        LogLiveSkinGeometry(actor, false);
        if (actor == RE::PlayerCharacter::GetSingleton()) LogLiveSkinGeometry(actor, true);
        SKSE::log::info("SkinAudit manual end actor={:08X} reason='{}'", actor->GetFormID(), reason);
    }

    void ForgetActorState(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0) return;
        bcn::skin_session::Forget(actorFormID);
    }
}
