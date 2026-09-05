#include "BodyChangeNG/SkinOverrides.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariRouting.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
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
    namespace skee_override
    {
        class IPluginInterface
        {
        public:
            virtual ~IPluginInterface() = default;
            virtual std::uint32_t GetVersion() = 0;
            virtual void Revert() = 0;
        };

        // RaceMenu's public Override interface v2. Keep the complete virtual
        // surface in upstream order: omitting any earlier entry would shift
        // AddSkinOverride/ApplySkinOverrides to the wrong vtable slots.
        class IOverrideInterfaceV2 : public IPluginInterface
        {
        public:
            class GetVariant
            {
            public:
                virtual void Int(std::int32_t) = 0;
                virtual void Float(float) = 0;
                virtual void String(const char*) = 0;
                virtual void Bool(bool) = 0;
                virtual void TextureSet(const RE::BGSTextureSet*) = 0;
            };

            class SetVariant
            {
            public:
                enum class Type { None, Int, Float, String, Bool, TextureSet };
                virtual Type GetType() { return Type::None; }
                virtual std::int32_t Int() { return 0; }
                virtual float Float() { return 0.0F; }
                virtual const char* String() { return nullptr; }
                virtual bool Bool() { return false; }
                virtual RE::BGSTextureSet* TextureSet() { return nullptr; }
            };

            virtual bool HasArmorAddonNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, bool) = 0;
            virtual bool HasArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) = 0;
            virtual void AddArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&) = 0;
            virtual bool GetArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void RemoveArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) = 0;
            virtual void SetArmorProperties(RE::TESObjectREFR*, bool) = 0;
            virtual void SetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
            virtual bool GetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void ApplyArmorOverrides(RE::TESObjectREFR*, RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool) = 0;
            virtual void RemoveAllArmorOverrides() = 0;
            virtual void RemoveAllArmorOverridesByReference(RE::TESObjectREFR*) = 0;
            virtual void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR*, bool, RE::TESObjectARMO*) = 0;
            virtual void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*) = 0;
            virtual void RemoveAllArmorOverridesByNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*) = 0;
            virtual bool HasNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t) = 0;
            virtual void AddNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, SetVariant&) = 0;
            virtual bool GetNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void RemoveNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t) = 0;
            virtual void SetNodeProperties(RE::TESObjectREFR*, bool) = 0;
            virtual void SetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
            virtual bool GetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void ApplyNodeOverrides(RE::TESObjectREFR*, RE::NiAVObject*, bool) = 0;
            virtual void RemoveAllNodeOverrides() = 0;
            virtual void RemoveAllNodeOverridesByReference(RE::TESObjectREFR*) = 0;
            virtual void RemoveAllNodeOverridesByNode(RE::TESObjectREFR*, bool, const char*) = 0;
            virtual bool HasSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t) = 0;
            virtual void AddSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t, SetVariant&) = 0;
            virtual bool GetSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void RemoveSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t) = 0;
            virtual void SetSkinProperties(RE::TESObjectREFR*, bool) = 0;
            virtual void SetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
            virtual bool GetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, GetVariant&) = 0;
            virtual void ApplySkinOverrides(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, std::uint32_t, RE::NiAVObject*, bool) = 0;
            virtual void RemoveAllSkinOverrides() = 0;
            virtual void RemoveAllSkinOverridesByReference(RE::TESObjectREFR*) = 0;
            virtual void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR*, bool, bool, std::uint32_t) = 0;
        };

        class StringVariant final : public IOverrideInterfaceV2::SetVariant
        {
        public:
            explicit StringVariant(std::string value) : value_(std::move(value)) {}
            Type GetType() override { return Type::String; }
            const char* String() override { return value_.c_str(); }

        private:
            std::string value_;
        };

        class StringVisitor final : public IOverrideInterfaceV2::GetVariant
        {
        public:
            void Int(std::int32_t) override {}
            void Float(float) override {}
            void String(const char* value) override { value_ = value ? value : ""; }
            void Bool(bool) override {}
            void TextureSet(const RE::BGSTextureSet*) override {}

            [[nodiscard]] const std::string& Value() const noexcept { return value_; }

        private:
            std::string value_;
        };
    }

    std::mutex g_selectionLock;
    std::mutex g_generationLock;
    std::mutex g_legacyCleanupLock;
    std::mutex g_rsvFaceLock;
    std::mutex g_futanariLock;
    std::unordered_map<RE::FormID, std::string> g_currentProfileIds;
    std::unordered_map<RE::FormID, std::uint64_t> g_applyGenerations;
    std::unordered_map<RE::FormID, std::uint64_t> g_rsvFaceGenerations;
    std::unordered_set<RE::FormID> g_rsvTransientFaces;
    struct CachedFutanariType final
    {
        std::optional<bcn::FutanariSkinType> type;
    };
    std::unordered_map<RE::FormID, CachedFutanariType> g_futanariTypes;
    std::unordered_map<RE::FormID, std::uint64_t> g_futanariApplyGenerations;
    std::atomic_uint64_t g_nextApplyGeneration{ 1U };
    std::atomic_uint64_t g_nextRsvFaceGeneration{ 1U };
    std::atomic_uint64_t g_nextFutanariApplyGeneration{ 1U };
    std::unordered_set<RE::FormID> g_legacyCleanupComplete;
    std::atomic<skee_override::IPluginInterface*> g_overrideInterface{};
    std::atomic_uint32_t g_overrideVersion{};
    // UBE's naked body is authored on Skyrim biped slot 53. CommonLib names
    // the corresponding bit kModLegRight (bit 23).
    constexpr auto kUbeBodySlot = RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight;
    // Schlongs of Skyrim ArmorAddons use biped slot 52.
    constexpr auto kSosMaleGenitalSlot = RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary;

    [[nodiscard]] bool UsesUbeBodySlot(const bcn::SkinProfile& profile) noexcept
    {
        return (profile.bodyFamilies & bcn::body_family::Bit(
            bcn::body_family::Family::ube)) != 0U;
    }

    [[nodiscard]] bool UsesBeastTail(const bcn::SkinProfile& profile) noexcept
    {
        // Vanilla Argonian/Khajiit tail NIFs deliberately reference the same
        // sex-specific body atlas. This is a real second geometry target for
        // the body channels, not a missing-part fallback such as body->feet.
        return profile.race == bcn::SkinRace::argonian ||
            profile.race == bcn::SkinRace::khajiit;
    }

    [[nodiscard]] bool ProfileMatchesActor(RE::Actor* actor, const bcn::SkinProfile& profile)
    {
        return !actor || (bcn::SkinRaceMatchesActor(
            profile.race, bcn::ResolveActorSkinRace(actor)) &&
            bcn::SkinMatchesActor(profile.bodyFamilies, bcn::body_family::ResolveActor(actor)));
    }

    [[nodiscard]] skee_override::IPluginInterface* OverrideInterface() noexcept
    {
        if (auto* existing = g_overrideInterface.load(std::memory_order_acquire)) return existing;
        auto* candidate = static_cast<skee_override::IPluginInterface*>(
            bcn::racemenu::QueryInterface("Override"));
        if (!candidate) return nullptr;
        const auto version = candidate->GetVersion();
        // RaceMenu SE 0.4.14-0.4.16 exposes the internal Override v1 ABI.
        // RaceMenu AE exposes the public wrapper ABI introduced as v2. A
        // future ABI must be audited before it is called; fail closed instead
        // of treating a changed vtable as v2.
        if (version < 1U || version > 2U) {
            SKSE::log::error("Body Change NG rejected unsupported RaceMenu Override interface version {}", version);
            return nullptr;
        }
        g_overrideVersion.store(version, std::memory_order_release);
        g_overrideInterface.store(candidate, std::memory_order_release);
        SKSE::log::info("Body Change NG received RaceMenu Override interface version {} path={}", version,
            version == 1U ? "Papyrus-NiOverride-exact-persistent" : "native-v2-exact-persistent");
        return candidate;
    }

    [[nodiscard]] std::uint32_t OverrideVersion() noexcept
    {
        if (!OverrideInterface()) return 0U;
        return g_overrideVersion.load(std::memory_order_acquire);
    }

    [[nodiscard]] skee_override::IOverrideInterfaceV2* OverrideInterfaceV2() noexcept
    {
        auto* overrideBase = OverrideInterface();
        return overrideBase && OverrideVersion() == 2U ?
            static_cast<skee_override::IOverrideInterfaceV2*>(overrideBase) : nullptr;
    }

    [[nodiscard]] std::uint64_t BeginSkinChange(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_generationLock);
        const auto generation = g_nextApplyGeneration.fetch_add(1U, std::memory_order_relaxed);
        g_applyGenerations.insert_or_assign(actorFormID, generation);
        return generation;
    }

    [[nodiscard]] bool IsCurrentSkinChange(const RE::FormID actorFormID, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_generationLock);
        const auto found = g_applyGenerations.find(actorFormID);
        return found != g_applyGenerations.end() && found->second == generation;
    }

    [[nodiscard]] std::uint64_t BeginFutanariChange(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_futanariLock);
        const auto generation = g_nextFutanariApplyGeneration.fetch_add(1U, std::memory_order_relaxed);
        g_futanariApplyGenerations.insert_or_assign(actorFormID, generation);
        return generation;
    }

    [[nodiscard]] bool IsCurrentFutanariChange(
        const RE::FormID actorFormID, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_futanariLock);
        const auto found = g_futanariApplyGenerations.find(actorFormID);
        return found != g_futanariApplyGenerations.end() && found->second == generation;
    }

    [[nodiscard]] std::uint64_t BeginRsvFaceRefresh(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_rsvFaceLock);
        const auto generation = g_nextRsvFaceGeneration.fetch_add(1U, std::memory_order_relaxed);
        g_rsvFaceGenerations[actorFormID] = generation;
        return generation;
    }

    [[nodiscard]] bool IsCurrentRsvFaceRefresh(
        const RE::FormID actorFormID, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_rsvFaceLock);
        const auto found = g_rsvFaceGenerations.find(actorFormID);
        return found != g_rsvFaceGenerations.end() && found->second == generation;
    }

    [[nodiscard]] bool ReleaseRsvTransientFace(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_rsvFaceLock);
        g_rsvFaceGenerations.erase(actorFormID);
        return g_rsvTransientFaces.erase(actorFormID) != 0U;
    }

    [[nodiscard]] bool HasRsvTransientFace(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_rsvFaceLock);
        return g_rsvTransientFaces.contains(actorFormID);
    }

    [[nodiscard]] bool ClaimLegacyCleanup(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_legacyCleanupLock);
        return g_legacyCleanupComplete.insert(actorFormID).second;
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

    struct LoadedPartView final
    {
        bool firstPerson{};
        bool actorSkinArmor{};
        RE::NiAVObject* object{};
        // Exact geometry nodes for this requested body part inside object.
        // A naked Skin Armor can expose the same multi-slot clone through
        // body, hands and feet entries, so callers must not inspect the whole
        // clone again without this boundary.
        std::vector<std::string> nodes;
    };

    struct LoadedPartTarget final
    {
        RE::TESObjectARMO* armor{};
        RE::TESObjectARMA* addon{};
        std::uint32_t slotMask{};
        std::vector<LoadedPartView> views;
        std::vector<std::string> immediateNodes;
        std::vector<std::string> persistentNodes;
    };

    struct LoadedFutanariRoute final
    {
        bcn::futanari::AddonKind addonKind{ bcn::futanari::AddonKind::none };
        std::optional<bcn::FutanariSkinType> type;
        std::vector<LoadedPartTarget> targets;
    };

    [[nodiscard]] std::string_view GeometryDiffuseTexture(RE::BSGeometry* geometry);

    [[nodiscard]] std::string AddonModelPath(RE::TESObjectARMA* addon, const bool firstPerson)
    {
        if (!addon) return {};
        const auto* rawPath = firstPerson ? addon->bipedModel1stPersons[1U].GetModel() :
            addon->bipedModels[1U].GetModel();
        auto path = rawPath ? std::string{ rawPath } : std::string{};
        std::ranges::replace(path, '/', '\\');
        return path;
    }

    [[nodiscard]] std::optional<bcn::FutanariSkinType> FutanariTypeFor(
        const bcn::futanari::AddonKind kind, const bcn::body_family::Mask actorFamily)
    {
        const auto ube = bcn::body_family::Bit(bcn::body_family::Family::ube);
        const auto cbbe = bcn::body_family::Bit(bcn::body_family::Family::cbbe);
        if (kind == bcn::futanari::AddonKind::ube && (actorFamily & ube) != 0U) {
            return bcn::FutanariSkinType::ubeTrx;
        }
        if (kind == bcn::futanari::AddonKind::trx) {
            if ((actorFamily & ube) != 0U) return bcn::FutanariSkinType::ubeTrx;
            if ((actorFamily & cbbe) != 0U) return bcn::FutanariSkinType::cbbeTrx;
        } else if (kind == bcn::futanari::AddonKind::erf && (actorFamily & cbbe) != 0U) {
            return bcn::FutanariSkinType::erf;
        }
        return std::nullopt;
    }

    [[nodiscard]] LoadedFutanariRoute FindLoadedFutanariRoute(
        RE::Actor* actor, const bool logTargets = true)
    {
        LoadedFutanariRoute result;
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base || base->GetSex() != RE::SEX::kFemale || !actor->Is3DLoaded()) return result;

        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            const auto& biped = actor->GetBiped(firstPerson);
            if (!biped) continue;
            std::unordered_set<RE::NiAVObject*> inspectedClones;
            for (std::size_t index{}; index < RE::BIPED_OBJECTS::kEditorTotal; ++index) {
                const auto& object = biped->objects[index];
                auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
                auto* addon = object.addon;
                auto* partClone = object.partClone.get();
                if (!armor || !addon || !partClone || !addon->IsValidRace(actor->GetRace()) ||
                    !inspectedClones.insert(partClone).second) continue;

                const auto modelPath = AddonModelPath(addon, firstPerson);
                const auto modelKind = bcn::futanari::ClassifyEvidence(modelPath);
                std::vector<std::string> matchingNodes;
                auto targetKind = modelKind;
                RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto* rawName = geometry->name.c_str();
                    const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                    if (name.empty()) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto geometryKind = bcn::futanari::ClassifyEvidence(
                        {}, name, GeometryDiffuseTexture(geometry));
                    if (geometryKind == bcn::futanari::AddonKind::none ||
                        (modelKind != bcn::futanari::AddonKind::none && geometryKind != modelKind)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (targetKind == bcn::futanari::AddonKind::none) targetKind = geometryKind;
                    if (geometryKind == targetKind &&
                        std::ranges::find(matchingNodes, name) == matchingNodes.end()) {
                        matchingNodes.push_back(name);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (targetKind == bcn::futanari::AddonKind::none || matchingNodes.empty()) continue;
                if (result.addonKind != bcn::futanari::AddonKind::none &&
                    result.addonKind != targetKind) {
                    SKSE::log::warn(
                        "Body Change NG found simultaneous TRX and ERF futanari targets on actor {:08X}; refusing an ambiguous texture route",
                        actor->GetFormID());
                    return {};
                }
                result.addonKind = targetKind;

                auto target = std::ranges::find_if(result.targets, [&](const LoadedPartTarget& candidate) {
                    return candidate.armor == armor && candidate.addon == addon;
                });
                if (target == result.targets.end()) {
                    result.targets.push_back({
                        .armor = armor,
                        .addon = addon,
                        .slotMask = armor->GetSlotMask().underlying() & addon->GetSlotMask().underlying()
                    });
                    target = std::prev(result.targets.end());
                }
                target->views.push_back({
                    .firstPerson = firstPerson,
                    .actorSkinArmor = false,
                    .object = partClone,
                    .nodes = matchingNodes
                });
                for (const auto& node : matchingNodes) {
                    if (std::ranges::find(target->immediateNodes, node) == target->immediateNodes.end()) {
                        target->immediateNodes.push_back(node);
                        target->persistentNodes.push_back(node);
                    }
                }
            }
        }
        result.type = FutanariTypeFor(result.addonKind, bcn::body_family::ResolveActor(actor));
        if (!result.type) result.targets.clear();
        if (logTargets && result.type) {
            SKSE::log::info(
                "SkinAudit futanari target actor={:08X} type={} addon-targets={}",
                actor->GetFormID(), bcn::FutanariSkinTypeLabel(*result.type), result.targets.size());
        }
        return result;
    }

    [[nodiscard]] bool IsSkinGeometry(RE::BSGeometry* geometry, const bool actorSkinArmor)
    {
        if (!geometry) return false;
        // A naked Skin Armor is already an exact semantic target.  For worn
        // armor, only repaint embedded skin geometry (revealing outfits often
        // ship a copy of the body in the slot-32 NIF).  Repainting every
        // geometry would replace the outfit's own fabric/metal textures.
        if (actorSkinArmor) return true;
        auto* shader = geometry->lightingShaderProp_cast();
        auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        return material && material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint;
    }

    [[nodiscard]] std::string_view GeometryDiffuseTexture(RE::BSGeometry* geometry)
    {
        if (!geometry) return {};
        auto* shader = geometry->lightingShaderProp_cast();
        auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        const auto textureSet = material ? material->GetTextureSet() : nullptr;
        if (!textureSet) return {};
        const auto* path = textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse);
        return path ? std::string_view{ path } : std::string_view{};
    }

    [[nodiscard]] bool MatchesFallbackPart(
        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::string_view nodeName, const std::string_view texturePath)
    {
        switch (slot) {
        case RE::BGSBipedObjectForm::BipedObjectSlot::kHands:
            return bcn::skin_geometry::MatchesLimb(
                bcn::skin_geometry::LimbSelection::hands, nodeName, texturePath);
        case RE::BGSBipedObjectForm::BipedObjectSlot::kFeet:
            return bcn::skin_geometry::MatchesLimb(
                bcn::skin_geometry::LimbSelection::feet, nodeName, texturePath);
        default:
            return false;
        }
    }

    [[nodiscard]] bool ViewContainsNode(
        const LoadedPartView& view, const std::string_view nodeName) noexcept
    {
        return view.nodes.empty() || std::ranges::any_of(view.nodes,
            [nodeName](const std::string& candidate) { return candidate == nodeName; });
    }

    void AppendCrossSlotBodyTargets(RE::Actor* actor,
        const bcn::skin_geometry::BodySelection selection,
        std::vector<LoadedPartTarget>& results)
    {
        if (!actor) return;
        auto* skinArmor = actor->GetSkin();
        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            const auto& biped = actor->GetBiped(firstPerson);
            if (!biped) continue;
            std::unordered_set<RE::NiAVObject*> inspectedClones;
            for (std::size_t index{}; index < RE::BIPED_OBJECTS::kEditorTotal; ++index) {
                const auto& object = biped->objects[index];
                auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
                auto* addon = object.addon;
                auto* partClone = object.partClone.get();
                if (!armor || armor == skinArmor || !addon || !partClone ||
                    !addon->IsValidRace(actor->GetRace()) || !inspectedClones.insert(partClone).second) {
                    continue;
                }

                std::vector<std::string> matchingNodes;
                RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                    if (!IsSkinGeometry(geometry, false)) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto* rawName = geometry->name.c_str();
                    const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                    const auto texturePath = GeometryDiffuseTexture(geometry);
                    if (!bcn::skin_geometry::IsBodyGeometryCandidate(name, texturePath) ||
                        !bcn::skin_geometry::Matches(name, selection, texturePath)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (std::ranges::find(matchingNodes, name) == matchingNodes.end()) {
                        matchingNodes.push_back(name);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (matchingNodes.empty()) continue;

                const auto armorMask = armor->GetSlotMask().underlying();
                const auto addonMask = addon->GetSlotMask().underlying();
                auto target = std::ranges::find_if(results, [&](const LoadedPartTarget& candidate) {
                    return candidate.armor == armor && candidate.addon == addon;
                });
                if (target == results.end()) {
                    results.push_back({ .armor = armor, .addon = addon,
                        .slotMask = armorMask & addonMask });
                    target = std::prev(results.end());
                }
                if (std::ranges::none_of(target->views, [partClone, firstPerson](const auto& view) {
                    return view.object == partClone && view.firstPerson == firstPerson;
                })) {
                    target->views.push_back({
                        .firstPerson = firstPerson,
                        .actorSkinArmor = false,
                        .object = partClone,
                        .nodes = matchingNodes
                    });
                }
                for (const auto& name : matchingNodes) {
                    if (std::ranges::find(target->immediateNodes, name) == target->immediateNodes.end()) {
                        target->immediateNodes.push_back(name);
                        target->persistentNodes.push_back(name);
                    }
                }
            }
        }
    }

    [[nodiscard]] std::vector<LoadedPartTarget> FindLoadedPartTargets(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const bcn::skin_geometry::BodySelection selection = bcn::skin_geometry::BodySelection::all,
        const bool logTargets = true)
    {
        std::vector<LoadedPartTarget> results;
        if (!actor) return results;
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        if (requestedMask == 0U || !std::has_single_bit(requestedMask)) return results;
        const auto objectIndex = static_cast<std::size_t>(std::countr_zero(requestedMask));
        if (objectIndex >= RE::BIPED_OBJECTS::kEditorTotal) return results;
        auto* skinArmor = actor->GetSkin();
        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            const auto& biped = actor->GetBiped(firstPerson);
            if (!biped) continue;
            const auto& object = biped->objects[objectIndex];
            auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
            auto* addon = object.addon;
            auto* partClone = object.partClone.get();
            if (!armor || !addon || !partClone || !addon->IsValidRace(actor->GetRace())) continue;
            const auto armorMask = armor->GetSlotMask().underlying();
            const auto addonMask = addon->GetSlotMask().underlying();
            if ((armorMask & requestedMask) == 0U || (addonMask & requestedMask) == 0U) continue;

            const auto actorSkinArmor = armor == skinArmor;
            std::vector<std::string> matchingNodes;
            RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                const auto* rawName = geometry->name.c_str();
                const std::string geometryName = rawName && rawName[0] != '\0' ? rawName : "";
                const auto texturePath = GeometryDiffuseTexture(geometry);
                if (!bcn::skin_geometry::MatchesRequestedPart(requestedMask,
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kBody),
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHands),
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet),
                        geometryName, texturePath) ||
                    !bcn::skin_geometry::Matches(geometryName, selection, texturePath) ||
                    (!IsSkinGeometry(geometry, actorSkinArmor) &&
                        selection != bcn::skin_geometry::BodySelection::maleGenitals)) {
                    return RE::BSVisit::BSVisitControl::kContinue;
                }
                if (std::ranges::find(matchingNodes, geometryName) == matchingNodes.end()) {
                    matchingNodes.push_back(geometryName);
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            if (matchingNodes.empty()) continue;

            auto target = std::ranges::find_if(results, [&](const LoadedPartTarget& candidate) {
                return candidate.armor == armor && candidate.addon == addon;
            });
            if (target == results.end()) {
                results.push_back({ .armor = armor, .addon = addon, .slotMask = armorMask & addonMask });
                target = std::prev(results.end());
            }
            target->views.push_back({
                .firstPerson = firstPerson,
                .actorSkinArmor = actorSkinArmor,
                .object = partClone,
                .nodes = matchingNodes
            });
            for (const auto& geometryName : matchingNodes) {
                if (std::ranges::find(target->immediateNodes, geometryName) == target->immediateNodes.end()) {
                    target->immediateNodes.push_back(geometryName);
                    target->persistentNodes.push_back(geometryName);
                }
            }
        }
        const auto requestedHandsOrFeet =
            slot == RE::BGSBipedObjectForm::BipedObjectSlot::kHands ||
            slot == RE::BGSBipedObjectForm::BipedObjectSlot::kFeet;

        const auto requestedBody =
            slot == RE::BGSBipedObjectForm::BipedObjectSlot::kBody || slot == kUbeBodySlot;
        const auto hasExactWornBody = requestedBody && std::ranges::any_of(results,
            [skinArmor](const LoadedPartTarget& target) { return target.armor != skinArmor; });
        if (requestedBody && !hasExactWornBody) {
            // Some revealing outfits anchor their embedded body clone to a
            // non-body biped slot. Scan the fixed 32-entry Biped array only
            // for this apply/verify operation and accept FaceGen skin
            // materials with explicit body evidence; never repaint the
            // outfit's fabric/metal geometries.
            AppendCrossSlotBodyTargets(actor, selection, results);
        }
        if (requestedHandsOrFeet) {
            // The exact slot may contain a sleeve/glove/boot clone with no
            // skin geometry.  Such a placeholder is not an applicable skin
            // target and previously blocked the naked Skin Armor fallback.
            std::erase_if(results, [](const LoadedPartTarget& target) {
                return target.immediateNodes.empty();
            });
        }
        // Some naked-skin armors are a multi-slot clone anchored at slot 32,
        // leaving the exact hands/feet biped array entry empty or unusable.
        // Keep the exact O(1) path above; only when it found no usable skin
        // target, inspect the fixed 32-entry biped array and accept geometry
        // whose node/material identifies the requested part. This avoids a
        // whole-actor scenegraph scan and body->hands/feet cross-routing.
        if (bcn::skin_geometry::NeedsFixedBipedFallback(requestedHandsOrFeet, results.size())) {
            for (const bool firstPerson : { false, true }) {
                if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
                const auto& biped = actor->GetBiped(firstPerson);
                if (!biped) continue;
                std::unordered_set<RE::NiAVObject*> inspectedClones;
                for (std::size_t index{}; index < RE::BIPED_OBJECTS::kEditorTotal; ++index) {
                    const auto& object = biped->objects[index];
                    auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
                    auto* addon = object.addon;
                    auto* partClone = object.partClone.get();
                    if (!armor || armor != skinArmor || !addon || !partClone ||
                        !addon->IsValidRace(actor->GetRace())) continue;
                    const auto armorMask = armor->GetSlotMask().underlying();
                    const auto addonMask = addon->GetSlotMask().underlying();
                    if ((armorMask & requestedMask) == 0U || (addonMask & requestedMask) == 0U) continue;
                    if (!inspectedClones.insert(partClone).second) continue;

                    std::vector<std::string> matchingNodes;
                    RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                        if (!IsSkinGeometry(geometry, true)) return RE::BSVisit::BSVisitControl::kContinue;
                        const auto* rawName = geometry->name.c_str();
                        const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                        const auto texturePath = GeometryDiffuseTexture(geometry);
                        if (!MatchesFallbackPart(slot, name, texturePath) ||
                            !bcn::skin_geometry::Matches(name, selection, texturePath)) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        if (std::ranges::find(matchingNodes, name) == matchingNodes.end()) {
                            matchingNodes.push_back(name);
                        }
                        return RE::BSVisit::BSVisitControl::kContinue;
                    });
                    if (matchingNodes.empty()) continue;

                    auto target = std::ranges::find_if(results, [&](const LoadedPartTarget& candidate) {
                        return candidate.armor == armor && candidate.addon == addon;
                    });
                    if (target == results.end()) {
                        results.push_back({ .armor = armor, .addon = addon,
                            .slotMask = armorMask & addonMask });
                        target = std::prev(results.end());
                    }
                    if (std::ranges::none_of(target->views, [partClone, firstPerson](const auto& view) {
                        return view.object == partClone && view.firstPerson == firstPerson;
                    })) {
                        target->views.push_back({
                            .firstPerson = firstPerson,
                            .actorSkinArmor = true,
                            .object = partClone,
                            .nodes = matchingNodes
                        });
                    }
                    for (const auto& name : matchingNodes) {
                        if (std::ranges::find(target->immediateNodes, name) == target->immediateNodes.end()) {
                            target->immediateNodes.push_back(name);
                            target->persistentNodes.push_back(name);
                        }
                    }
                }
            }
        }
        if (logTargets) {
            for (const auto& target : results) {
                SKSE::log::info(
                    "SkinAudit target actor={:08X} part={} armor={:08X} addon={:08X} addon-mask={:08X} source={} views={} skin-geometries={}",
                    actor->GetFormID(), SkinPartName(slot), target.armor->GetFormID(), target.addon->GetFormID(), target.slotMask,
                    target.armor == skinArmor ? "skin-armor" : "worn-armor", target.views.size(),
                    target.immediateNodes.size());
            }
        }
        return results;
    }

    struct LoadedProfileBodyRoute final
    {
        RE::BGSBipedObjectForm::BipedObjectSlot slot{
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody };
        bcn::skin_geometry::BodySelection selection{
            bcn::skin_geometry::BodySelection::regular };
        std::vector<LoadedPartTarget> targets;
    };

    void MergeLoadedPartTargets(std::vector<LoadedPartTarget>& destination,
        std::vector<LoadedPartTarget> source)
    {
        for (auto& incoming : source) {
            auto target = std::ranges::find_if(destination, [&](const LoadedPartTarget& candidate) {
                return candidate.armor == incoming.armor && candidate.addon == incoming.addon;
            });
            if (target == destination.end()) {
                destination.push_back(std::move(incoming));
                continue;
            }
            target->slotMask |= incoming.slotMask;
            for (auto& view : incoming.views) {
                if (std::ranges::none_of(target->views, [&](const LoadedPartView& candidate) {
                    return candidate.object == view.object && candidate.firstPerson == view.firstPerson;
                })) {
                    target->views.push_back(std::move(view));
                }
            }
            const auto appendNode = [](std::vector<std::string>& nodes, std::string node) {
                if (std::ranges::find(nodes, node) == nodes.end()) nodes.push_back(std::move(node));
            };
            for (auto& node : incoming.immediateNodes) appendNode(target->immediateNodes, std::move(node));
            for (auto& node : incoming.persistentNodes) appendNode(target->persistentNodes, std::move(node));
        }
    }

    [[nodiscard]] LoadedProfileBodyRoute FindLoadedProfileBodyRoute(
        RE::Actor* actor, const bcn::SkinProfile& profile, const bool logTargets = true)
    {
        if (!UsesUbeBodySlot(profile)) {
            return {
                .slot = RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                .selection = bcn::skin_geometry::BodySelection::regular,
                .targets = FindLoadedPartTargets(actor,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
                    bcn::skin_geometry::BodySelection::regular, logTargets)
            };
        }

        // UBE clothing can split the visible skin atlas across a slot-32 body
        // clone and a slot-53 anatomy/body clone.  They are complementary,
        // not alternatives: applying only the first slot found leaves the
        // other visible surface on its previous/default skin.
        auto ubeTargets = FindLoadedPartTargets(actor, kUbeBodySlot,
            bcn::skin_geometry::BodySelection::all, logTargets);
        auto standardTargets = FindLoadedPartTargets(actor,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::regular, logTargets);
        if (logTargets && ubeTargets.empty() && !standardTargets.empty()) {
            SKSE::log::info(
                "SkinAudit UBE body fallback actor={:08X} slot-53-targets=0 standard-body-targets={}",
                actor ? actor->GetFormID() : 0U, standardTargets.size());
        }
        MergeLoadedPartTargets(ubeTargets, std::move(standardTargets));
        return {
            .slot = kUbeBodySlot,
            .selection = bcn::skin_geometry::BodySelection::all,
            .targets = std::move(ubeTargets)
        };
    }

    void QueueSettledSkinAudit(RE::ActorHandle actorHandle, std::uint64_t generation,
        std::uint32_t remainingTaskHops);

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
                        QueueSettledSkinAudit(handle, generation, 2U);
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

        std::vector<std::pair<std::uint8_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, cacheNamespace);
            if (!path.empty()) {
                resolvedLayers.emplace_back(static_cast<std::uint8_t>(layer.shaderTextureIndex), std::move(path));
            }
        }
        if (resolvedLayers.empty()) return false;

        std::size_t appliedTargets{};
        std::size_t submitted{};
        for (const auto& target : targets) {
            for (const auto& node : target.persistentNodes) {
                for (const auto& [textureIndex, path] : resolvedLayers) {
                    skee_override::StringVisitor current;
                    const auto exists = overrides.GetArmorOverride(actor, female, target.armor, target.addon,
                        node.c_str(), static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, current);
                    if (!bcn::skin_override::ownership::MayReplace(exists, current.Value())) {
                        SKSE::log::warn(
                            "SkinOverride persistent-register skipped actor={:08X} part={} armor={:08X} addon={:08X} node='{}' index={} reason=foreign-owner current='{}'",
                            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
                            target.armor->GetFormID(),
                            target.addon->GetFormID(), node, static_cast<std::uint32_t>(textureIndex), current.Value());
                        continue;
                    }
                    skee_override::StringVariant value{ path };
                    overrides.AddArmorOverride(actor, female, target.armor, target.addon, node.c_str(),
                        static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, value);
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part={} armor={:08X} addon={:08X} node='{}' index={}({}) action={} value='{}'",
                        actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
                        target.armor->GetFormID(),
                        target.addon->GetFormID(), node, static_cast<std::uint32_t>(textureIndex),
                        TextureIndexName(textureIndex), exists ? "replace-owned" : "add", path);
                    ++submitted;
                }
            }
            for (const auto& view : target.views) {
                // The persistent map is armor/addon/node based; the exact
                // loaded addon clone is repainted immediately after storage.
                overrides.ApplyArmorOverrides(actor, target.armor, target.addon, view.object, true);
                ++appliedTargets;
            }
        }
        SKSE::log::info("SkinAudit actor={:08X} part={} stored-keys={} exact-targets={} mode=RaceMenu-v2-exact-persistent",
            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName,
            submitted, appliedTargets);
        return submitted != 0U && appliedTargets != 0U;
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
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot)
    {
        return ClearArmorAddonTargets(overrides, actor, female,
            FindLoadedPartTargets(actor, slot));
    }

    struct FaceNodeInfo final
    {
        std::string nodeName;
        std::string detailFilename;
        RE::NiAVObject* object{};
    };

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
            std::scoped_lock lock(g_rsvFaceLock);
            g_rsvTransientFaces.insert(actor->GetFormID());
        }
        return stored || !transientLayers.empty();
    }

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    [[nodiscard]] std::string LowerFilename(const std::string_view path)
    {
        const auto separator = path.find_last_of("\\/");
        std::string result{ separator == std::string_view::npos ? path : path.substr(separator + 1U) };
        std::ranges::transform(result, result.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return result;
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
            const auto textureSet = material ? material->GetTextureSet() : nullptr;
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
        const std::uint32_t remainingTaskHops)
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        bcn::frame_tasks::Continue(bcn::frame_tasks::CurrentLease(), [actorHandle, generation] {
            if (!bcn::frame_tasks::ValidLease(bcn::frame_tasks::CurrentLease())) return;
            const auto actor = actorHandle.get();
            if (!actor || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
            // Keep the quiet interval, but expensive per-DDS scene traversal
            // and diagnostic texture resolution are opt-in, not shipping work.
            if (!spdlog::should_log(spdlog::level::debug)) return;
            if (const auto profileID = bcn::skin_override::CurrentProfileId(actor.get())) {
                if (const auto profile = bcn::SkinProfiles::Get().Find(*profileID)) {
                    AuditLiveSkinProfile(actor.get(), *profile);
                }
            }
            SKSE::log::info(
                "SkinAudit settled actor={:08X} generation={} body-reapply=false",
                actor->GetFormID(), generation);
        }, std::max(1U, remainingTaskHops));
    }

    [[nodiscard]] std::optional<FaceNodeInfo> FaceNode(RE::Actor* actor, RE::TESNPC* base)
    {
        std::optional<FaceNodeInfo> result;
        int bestScore{};
        if (actor) {
            // Character exposes the exact live FaceGen subtree for both the
            // player and NPCs. Search only that subtree. Scanning the actor's
            // whole 3D is unsafe because naked body, hands and feet may also
            // use FaceGenRGBTint and can otherwise receive femalehead.dds.
            if (auto* root = actor->GetFaceNodeSkinned()) {
                RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    auto* shader = geometry->lightingShaderProp_cast();
                    auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                    if (!material) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto feature = material->GetFeature();
                    if (feature != RE::BSShaderMaterial::Feature::kFaceGen &&
                        feature != RE::BSShaderMaterial::Feature::kFaceGenRGBTint) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (const auto* name = geometry->name.c_str(); name && name[0] != '\0') {
                        const auto loweredName = LowerAscii(name);
                        const auto rejectedName = loweredName.contains("hair") || loweredName.contains("eye") ||
                            loweredName.contains("brow") || loweredName.contains("mouth") ||
                            loweredName.contains("teeth") || loweredName.contains("tongue") ||
                            loweredName.contains("body") || loweredName.contains("hand") ||
                            loweredName.contains("feet") || loweredName.contains("foot");

                        // A genuine FaceGen material is authoritative; the RGB
                        // tint fallback must additionally look like a head by
                        // node name or by its current diffuse texture.
                        int score = feature == RE::BSShaderMaterial::Feature::kFaceGen ? 100 : 0;
                        FaceNodeInfo info{ .nodeName = name, .object = geometry };
                        if (const auto textureSet = material->GetTextureSet()) {
                            if (const auto* diffuse = textureSet->GetTexturePath(
                                    RE::BSTextureSet::Textures::kDiffuse); diffuse && diffuse[0] != '\0') {
                                const auto loweredDiffuse = LowerAscii(diffuse);
                                if (loweredDiffuse.contains("facegendata\\facetint") ||
                                    loweredDiffuse.contains("femalehead") ||
                                    loweredDiffuse.contains("malehead")) {
                                    score = (std::max)(score, 90);
                                }
                            }
                            if (const auto* detail = textureSet->GetTexturePath(
                                    RE::BSTextureSet::Textures::kDetailMap); detail && detail[0] != '\0') {
                                info.detailFilename = LowerFilename(detail);
                                score = (std::max)(score, 80);
                            }
                        }
                        if (!rejectedName && (loweredName.contains("head") || loweredName.contains("face"))) {
                            score = (std::max)(score, 85);
                        }
                        if (!rejectedName && score > bestScore) {
                            bestScore = score;
                            result = std::move(info);
                        }
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            }
        }
        if (result) return result;
        // Do not guess an EditorID that was not found in the live scene graph.
        // Custom followers and NPC replacers can use entirely different
        // FaceGen node names; failing closed avoids a body-only skin and neck
        // seam.
        static_cast<void>(base);
        return std::nullopt;
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
                const auto textureSet = material ? material->GetTextureSet() : nullptr;
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

    [[nodiscard]] std::optional<bcn::SkinTextureLayer> MatchingFaceDetail(
        const bcn::SkinProfile& profile, const std::string_view currentFilename)
    {
        if (profile.faceDetails.empty()) return std::nullopt;
        if (!currentFilename.empty()) {
            const auto exact = std::ranges::find_if(profile.faceDetails, [&](const bcn::SkinTextureLayer& layer) {
                return LowerFilename(layer.path) == currentFilename;
            });
            if (exact != profile.faceDetails.end()) return *exact;
            for (const auto token : { std::string_view{ "frek" }, std::string_view{ "rough" }, std::string_view{ "blank" } }) {
                if (!currentFilename.contains(token)) continue;
                const auto semantic = std::ranges::find_if(profile.faceDetails, [&](const bcn::SkinTextureLayer& layer) {
                    return LowerFilename(layer.path).contains(token);
                });
                if (semantic != profile.faceDetails.end()) return *semantic;
            }
        }
        // A single detail file is unambiguous. With several alternatives and
        // no match, preserve the actor's current FaceGen detail choice.
        return profile.faceDetails.size() == 1U ? std::optional{ profile.faceDetails.front() } : std::nullopt;
    }

    void OverlayEffectiveLayers(std::vector<bcn::SkinTextureLayer>& base,
        const std::vector<bcn::SkinTextureLayer>& overlay)
    {
        for (const auto& layer : overlay) {
            const auto existing = std::ranges::find(base, layer.shaderTextureIndex,
                &bcn::SkinTextureLayer::shaderTextureIndex);
            if (existing != base.end()) *existing = layer;
            else base.push_back(layer);
        }
        std::ranges::sort(base, {}, &bcn::SkinTextureLayer::shaderTextureIndex);
    }

    [[nodiscard]] bcn::HumanoidSkinRace ActorHumanoidSkinRace(RE::TESNPC* base)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        return bcn::HumanoidSkinRaceFromEditorID(
            editorID ? std::string_view{ editorID } : std::string_view{});
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> EffectiveBodyLayers(
        const bcn::SkinProfile& profile, RE::TESNPC* base)
    {
        auto layers = profile.body;
        if (bcn::IsElderActor(base)) OverlayEffectiveLayers(layers, profile.elderBody);
        return layers;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> EffectiveHandsLayers(
        const bcn::SkinProfile& profile, RE::TESNPC* base)
    {
        auto layers = profile.hands;
        if (bcn::IsElderActor(base)) OverlayEffectiveLayers(layers, profile.elderHands);
        return layers;
    }

    [[nodiscard]] std::string ActiveAddonModelPath(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot, const bool female)
    {
        if (!actor) return {};
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        if (requestedMask == 0U || !std::has_single_bit(requestedMask)) return {};
        const auto objectIndex = static_cast<std::size_t>(std::countr_zero(requestedMask));
        if (objectIndex >= RE::BIPED_OBJECTS::kEditorTotal) return {};
        const auto& biped = actor->GetBiped(false);
        if (!biped) return {};
        auto* addon = biped->objects[objectIndex].addon;
        if (!addon) return {};
        const auto* rawPath = addon->bipedModels[female ? 1U : 0U].GetModel();
        auto path = rawPath ? std::string{ rawPath } : std::string{};
        std::ranges::replace(path, '/', '\\');
        std::ranges::transform(path, path.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return path;
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
            OverlayEffectiveLayers(layers, variant.elder);
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
        OverlayEffectiveLayers(layers, MaleGenitalVariantLayers(*selected, base));
        return layers;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> EffectiveFaceLayers(const bcn::SkinProfile& profile,
        RE::TESNPC* base, const std::string_view currentDetailFilename)
    {
        // Every actor-specific set can be partial. Start with the base face,
        // then replace only supplied channels in specificity order. A missing
        // race/elder channel therefore keeps the pack's base channel; if the
        // base channel is also absent, RaceMenu leaves the actor untouched.
        auto layers = profile.face;
        const auto raceIndex = static_cast<std::size_t>(ActorHumanoidSkinRace(base));
        if (raceIndex < profile.raceFace.size()) OverlayEffectiveLayers(layers, profile.raceFace[raceIndex]);
        if (IsVampireRace(base)) OverlayEffectiveLayers(layers, profile.vampireFace);
        if (bcn::IsElderActor(base)) OverlayEffectiveLayers(layers, profile.elderFace);
        if (std::ranges::find(layers, kFaceDetailTextureIndex,
                &bcn::SkinTextureLayer::shaderTextureIndex) == layers.end()) {
            if (const auto detail = MatchingFaceDetail(profile, currentDetailFilename)) layers.push_back(*detail);
        }
        return layers;
    }

    [[nodiscard]] bool ProfileUsesFace(const bcn::SkinProfile& profile, RE::TESNPC* base) noexcept
    {
        if (!profile.face.empty() || !profile.faceDetails.empty() ||
            (IsVampireRace(base) && !profile.vampireFace.empty()) ||
            (bcn::IsElderActor(base) && !profile.elderFace.empty())) return true;
        const auto raceIndex = static_cast<std::size_t>(ActorHumanoidSkinRace(base));
        return raceIndex < profile.raceFace.size() && !profile.raceFace[raceIndex].empty();
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
        add(EffectiveBodyLayers(profile, base), "skin");
        add(profile.cbbeGenitalAnal, "skin");
        add(profile.unpGenitalAnal, "skin");
        add(EffectiveMaleGenitalLayers(profile, actor, base), "skin");
        // UBE hands and feet reuse the Body atlas already queued above.
        if (!UsesUbeBodySlot(profile)) {
            add(EffectiveHandsLayers(profile, base), "skin");
            add(profile.feet, "skin");
        }
        if (const auto face = FaceNode(actor, base)) {
            add(EffectiveFaceLayers(profile, base, face->detailFilename), "skin-face");
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

    void AuditLiveSkinProfile(RE::Actor* actor, const bcn::SkinProfile& profile)
    {
        if (!actor) return;
        auto* base = actor->GetActorBase();
        const auto faceNode = FaceNode(actor, base);
        const auto bodyLayers = EffectiveBodyLayers(profile, base);
        const auto handsLayers = EffectiveHandsLayers(profile, base);
        const auto maleGenitalLayers = EffectiveMaleGenitalLayers(profile, actor, base);
        const auto faceLayers = faceNode ?
            EffectiveFaceLayers(profile, base, faceNode->detailFilename) :
            EffectiveFaceLayers(profile, base, {});
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
                        const auto textureSet = material ? material->GetTextureSet() : nullptr;
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
        verifyPart("cbbe-genital-anal", profile.cbbeGenitalAnal,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        verifyPart("unp-genital-anal", profile.unpGenitalAnal,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::unpGenitalAnal);
        verifyPart("sos-male-genitals", maleGenitalLayers, kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (UsesBeastTail(profile)) {
            verifyPart("tail-body-atlas", bodyLayers,
                RE::BGSBipedObjectForm::BipedObjectSlot::kTail);
        }
        if (UsesUbeBodySlot(profile)) {
            verifyPart("hands-ube-body-atlas", bodyLayers,
                RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            verifyPart("feet-ube-body-atlas", bodyLayers,
                RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        } else {
            verifyPart("hands", handsLayers, RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            verifyPart("feet", profile.feet, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        }
        verifyPart("face", faceLayers, std::nullopt);
    }

    [[nodiscard]] bool ClearTexturePart(skee_override::IOverrideInterfaceV2& overrides,
                                        RE::Actor* actor, const bool female,
                                        const RE::BGSBipedObjectForm::BipedObjectSlot slot)
    {
        if (!actor) return false;
        std::unordered_set<std::uint64_t> masks;
        const auto addMask = [&](const bool firstPerson, const std::uint32_t mask) {
            if (mask != 0U) masks.insert((static_cast<std::uint64_t>(firstPerson) << 32U) | mask);
        };
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        addMask(false, requestedMask);
        if (actor == RE::PlayerCharacter::GetSingleton()) addMask(true, requestedMask);
        for (const auto& target : FindLoadedPartTargets(actor, slot)) {
            for (const auto& view : target.views) addMask(view.firstPerson, target.slotMask);
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

    // Override v1 predates RaceMenu's public SetVariant wrapper. Constructing
    // its internal OverrideVariant in another DLL is not serialization-safe
    // because strings must be interned in RaceMenu's private StringTable.
    // Use the stable NiOverride Papyrus natives for v1 strings instead. The
    // exact Armor + ArmorAddon + geometry-node key avoids the broad legacy
    // AddSkinOverrideString behavior that repaints body, hands and feet in an
    // unspecified order.
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
    constexpr std::uint32_t kLegacyWatchdogChannel = 113U;

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
                        "Body Change NG cancelled an unreturned RaceMenu v1 callback batch for actor {:08X}; pending={} accepted={}",
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
        const bool futanari = false)
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
        batch->session = bcn::ActorRegistry::Get().SessionGeneration();
        batch->female = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        batch->deadline = std::chrono::steady_clock::now() + kLegacyCallbackTimeout;
        ArmLegacyWatchdog(batch);
        return batch;
    }

    [[nodiscard]] bool IsCurrentLegacyChange(const LegacyOverrideBatch& batch)
    {
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
                !IsCurrentSkinChange(batch->actorFormID, batch->generation)) {
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
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        std::unordered_set<std::uint64_t> masks;
        const auto addMask = [&](const bool firstPerson, const std::uint32_t mask) {
            if (mask != 0U) masks.insert((static_cast<std::uint64_t>(firstPerson) << 32U) | mask);
        };
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        addMask(false, requestedMask);
        if (actor == RE::PlayerCharacter::GetSingleton()) addMask(true, requestedMask);

        const auto targets = FindLoadedPartTargets(actor, slot);
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
                                "SkinOverride persistent-remove actor={:08X} armor={:08X} addon={:08X} node='{}' index={}({}) owner=BCNG mode=RaceMenu-v1",
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
                            "SkinOverride persistent-remove actor={:08X} legacy=skin-slot view={} mask={:08X} index={} owner=BCNG mode=RaceMenu-v1",
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
                                "SkinOverride futanari-remove actor={:08X} armor={:08X} addon={:08X} node='{}' index={} owner=BCNG mode=RaceMenu-v1",
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
                                    "SkinOverride persistent-register skipped actor={:08X} armor={:08X} addon={:08X} node='{}' index={} reason=foreign-owner mode=RaceMenu-v1 current='{}'",
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
                                "SkinOverride persistent-register actor={:08X} armor={:08X} addon={:08X} node='{}' index={}({}) action={} value='{}' mode=RaceMenu-v1",
                                actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                textureIndex, TextureIndexName(textureIndex), exists ? "replace-owned" : "add", path);
                        });
                    ++submitted;
                }
            }
        }
        SKSE::log::info("SkinAudit actor={:08X} part={} stored-keys={} mode=RaceMenu-v1-exact",
            actor->GetFormID(), partName.empty() ? SkinPartName(slot) : partName, submitted);
        return submitted != 0U ? mutation : std::shared_ptr<LegacyMutationTracker>{};
    }

    [[nodiscard]] std::shared_ptr<LegacyMutationTracker> DispatchLegacyPartApply(
        RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch,
        const bcn::skin_geometry::BodySelection selection = bcn::skin_geometry::BodySelection::all)
    {
        const auto targets = FindLoadedPartTargets(actor, slot, selection);
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
                            "SkinOverride persistent-remove actor={:08X} part=face node='{}' index={}({}) owner=BCNG mode=RaceMenu-v1",
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
                "SkinAudit expected actor={:08X} part=face node='{}' index={}({}) source='{}' cache='{}' mode=RaceMenu-v1-exact",
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
                            // RaceMenu v1's AddNodeOverrideString applies to
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
                                std::scoped_lock lock(g_rsvFaceLock);
                                g_rsvTransientFaces.insert(actor->GetFormID());
                            }
                            SKSE::log::debug(
                                "SkinOverride live-apply actor={:08X} part=face node='{}' index={} provider=RSV mode=RaceMenu-v1 value='{}'",
                                actor->GetFormID(), node, textureIndex, path);
                            return;
                        }
                        SKSE::log::warn(
                            "SkinOverride persistent-register skipped actor={:08X} part=face node='{}' index={} reason=foreign-owner mode=RaceMenu-v1 current='{}'",
                            actor->GetFormID(), node, textureIndex, current);
                        return;
                    }
                    const auto dispatched = DispatchLegacy(vm, "AddNodeOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ node },
                        static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex), std::string{ path }, true), batch);
                    if (dispatched) mutation->accepted.fetch_add(1U, std::memory_order_release);
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part=face node='{}' index={}({}) action={} value='{}' mode=RaceMenu-v1",
                        actor->GetFormID(), node, textureIndex, TextureIndexName(textureIndex),
                        exists ? "replace-owned" : "add", path);
                });
            ++submitted;
        }
        return submitted != 0U ? mutation : std::shared_ptr<LegacyMutationTracker>{};
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
        const std::uint64_t generation)
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
        if (ProfileUsesFace(profile, base) && !faceNode) {
            SKSE::log::warn(
                "Body Change NG skipped face layers from skin '{}' on actor {:08X}: no live FaceGen geometry was found",
                profile.name, actor->GetFormID());
            return;
        }
        const auto faceLayers = faceNode ?
            EffectiveFaceLayers(profile, base, faceNode->detailFilename) :
            std::vector<bcn::SkinTextureLayer>{};

        auto clearBatch = MakeLegacyBatch(actor.get(), generation);
        clearBatch->completion = [actorHandle, profile, generation](const std::uint32_t) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentActor->Is3DLoaded() || !currentVM ||
                !IsCurrentSkinChange(currentActor->GetFormID(), generation)) return;
            auto* currentBase = currentActor->GetActorBase();
            if (!currentBase || !ProfileMatchesActor(currentActor.get(), profile)) return;
            const auto currentFace = FaceNode(currentActor.get(), currentBase);
            if (ProfileUsesFace(profile, currentBase) && !currentFace) return;
            const auto currentFemale = currentBase->GetSex() == RE::SEX::kFemale;
            const auto currentFaceLayers = currentFace ? EffectiveFaceLayers(
                profile, currentBase, currentFace->detailFilename) :
                std::vector<bcn::SkinTextureLayer>{};
            const auto currentBodyLayers = EffectiveBodyLayers(profile, currentBase);
            const auto currentHandsLayers = EffectiveHandsLayers(profile, currentBase);
            const auto currentMaleGenitalLayers = EffectiveMaleGenitalLayers(
                profile, currentActor.get(), currentBase);

            auto applyBatch = MakeLegacyBatch(currentActor.get(), generation);
            auto requiredParts = std::make_shared<
                std::vector<std::shared_ptr<LegacyMutationTracker>>>();
            applyBatch->completion = [actorHandle, profile, generation, requiredParts](const std::uint32_t) {
                const auto settledActor = actorHandle.get();
                if (!settledActor || !IsCurrentSkinChange(settledActor->GetFormID(), generation)) return;
                const auto appliedParts = static_cast<std::size_t>(std::ranges::count_if(
                    *requiredParts, [](const auto& part) {
                        return part && part->accepted.load(std::memory_order_acquire) != 0U;
                    }));
                const auto complete = !requiredParts->empty() && appliedParts == requiredParts->size();
                if (complete) {
                    MarkCurrentSkinContent(settledActor.get(), profile, generation);
                    SKSE::log::info(
                        "Body Change NG applied texture skin profile '{}' to actor {:08X} through RaceMenu Override v1",
                        profile.name, settledActor->GetFormID());
                } else {
                    SKSE::log::warn(
                        "Body Change NG dispatched only {}/{} currently available parts from skin '{}' to actor {:08X} through RaceMenu Override v1; the desired selection remains pending",
                        appliedParts, requiredParts->size(), profile.name, settledActor->GetFormID());
                }
                auto* settledVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (!settledVM || !QueueNiNodeUpdate(*settledVM, settledActor.get(), actorHandle, generation)) {
                    QueueSettledSkinAudit(actorHandle, generation, 3U);
                }
            };

            const auto submitPart = [&](const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                const std::vector<bcn::SkinTextureLayer>& layers,
                const bcn::skin_geometry::BodySelection selection =
                    bcn::skin_geometry::BodySelection::all) {
                if (layers.empty()) return;
                requiredParts->push_back(DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                    slot, layers, applyBatch, selection));
            };
            const auto hasPrimaryParts = !currentBodyLayers.empty() || !currentHandsLayers.empty() ||
                !profile.feet.empty() || !currentFaceLayers.empty();
            if (!currentBodyLayers.empty()) {
                requiredParts->push_back(DispatchLegacyProfileBodyApply(*currentVM,
                    currentActor.get(), currentFemale, profile, currentBodyLayers, applyBatch));
            }
            const auto submitGenitalAnal = [&](const std::vector<bcn::SkinTextureLayer>& layers,
                const bcn::skin_geometry::BodySelection selection) {
                if (layers.empty()) return;
                if (hasPrimaryParts) {
                    static_cast<void>(DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, applyBatch, selection));
                } else {
                    submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, selection);
                }
            };
            submitGenitalAnal(profile.cbbeGenitalAnal,
                bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
            submitGenitalAnal(profile.unpGenitalAnal,
                bcn::skin_geometry::BodySelection::unpGenitalAnal);
            submitPart(kSosMaleGenitalSlot, currentMaleGenitalLayers,
                bcn::skin_geometry::BodySelection::maleGenitals);
            if (UsesBeastTail(profile) && !currentBodyLayers.empty()) {
                    // Tail availability is auxiliary: a tail-hiding outfit or
                    // custom race setup must not keep an otherwise complete
                    // skin selection perpetually pending.
                    static_cast<void>(DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                        RE::BGSBipedObjectForm::BipedObjectSlot::kTail, currentBodyLayers, applyBatch));
            }
            if (UsesUbeBodySlot(profile)) {
                submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands, currentBodyLayers);
                submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, currentBodyLayers);
            } else {
                submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands, currentHandsLayers);
                submitPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, profile.feet);
            }
            if (!currentFaceLayers.empty()) {
                requiredParts->push_back(currentFace ?
                    DispatchLegacyFaceApply(*currentVM, currentActor.get(), currentFemale,
                        *currentFace, currentFaceLayers, applyBatch) :
                    std::shared_ptr<LegacyMutationTracker>{});
            }
            if (std::ranges::none_of(*requiredParts, [](const auto& part) { return part != nullptr; })) {
                SKSE::log::warn("Body Change NG found no RaceMenu v1 skin targets for '{}'", profile.name);
            }
            CompleteLegacyBatch(applyBatch);
        };

        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female, kUbeBodySlot, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female, kSosMaleGenitalSlot, clearBatch);
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
            // RaceMenu v1 can remove every serialized key yet leave an
            // already-loaded armor clone painted. Inspect this actor once at
            // completion so an idempotent second Default click can repair the
            // live body/hands/feet as well as the face.
            const auto liveDefault = bcn::skin_override::LiveSkinStateMatches(
                currentActor.get(), {}, true,
                bcn::skin_override::LiveCheckScope::fullProfile);
            const auto staleLiveClone = liveDefault.has_value() && !*liveDefault;
            if (accepted == 0U && !staleLiveClone) {
                SKSE::log::info("Body Change NG found no remaining RaceMenu v1 skin texture overrides for actor {:08X}",
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
            SKSE::log::info("Body Change NG removed its RaceMenu v1 skin texture overrides for actor {:08X}",
                currentActor->GetFormID());
            const auto useDefault = unavailableProfileId.empty();
            bcn::ActorRegistry::Get().MarkSkinApplied(
                currentActor.get(), unavailableProfileId, useDefault);
        };
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female, kUbeBodySlot, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female, kSosMaleGenitalSlot, clearBatch);
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
        const std::uint64_t generation)
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
        if (ProfileUsesFace(profile, base) && !faceNode) {
            SKSE::log::warn(
                "Body Change NG skipped face layers from skin '{}' on actor {:08X}: no live FaceGen geometry was found",
                profile.name, actor->GetFormID());
            return;
        }
        const auto faceLayers = faceNode ?
            EffectiveFaceLayers(profile, base, faceNode->detailFilename) :
            std::vector<bcn::SkinTextureLayer>{};
        const auto bodyLayers = EffectiveBodyLayers(profile, base);
        const auto handsLayers = EffectiveHandsLayers(profile, base);
        const auto maleGenitalLayers = EffectiveMaleGenitalLayers(profile, actor.get(), base);
        // Store exact Armor + ArmorAddon + geometry keys, then repaint only
        // that loaded addon clone. Broad AddSkinOverrideString/skin-slot
        // registration is intentionally never used for new entries.
        bool removed = ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female);
        if (ClaimLegacyCleanup(actor->GetFormID())) {
            removed = ClearTexturePart(*overrides, actor.get(), female,
                RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || removed;
            removed = ClearTexturePart(*overrides, actor.get(), female, kUbeBodySlot) || removed;
            removed = ClearTexturePart(*overrides, actor.get(), female, kSosMaleGenitalSlot) || removed;
            removed = ClearTexturePart(*overrides, actor.get(), female,
                RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || removed;
            removed = ClearTexturePart(*overrides, actor.get(), female,
                RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || removed;
            removed = ClearTexturePart(*overrides, actor.get(), female,
                RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || removed;
        }
        // Reconcile every owned exact key before writing the new profile.
        // This restores the actor's underlying texture for absent parts and
        // absent diffuse/normal/subsurface/detail/specular channels.
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female, kUbeBodySlot) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female, kSosMaleGenitalSlot) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || removed;
        removed = ClearArmorAddonPart(*overrides, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || removed;
        if (faceNode) {
            removed = ClearFaceTextures(*overrides, actor.get(), female, faceNode->nodeName, true) || removed;
        }

        std::size_t requestedParts{};
        std::size_t appliedParts{};
        const auto applyPart = [&](const RE::BGSBipedObjectForm::BipedObjectSlot slot,
            const std::vector<bcn::SkinTextureLayer>& layers,
            const bcn::skin_geometry::BodySelection selection =
                bcn::skin_geometry::BodySelection::all) {
            if (layers.empty()) return;
            ++requestedParts;
            if (ApplyPart(*overrides, actor.get(), female, slot, layers, selection)) ++appliedParts;
        };
        const auto hasPrimaryParts = !bodyLayers.empty() || !handsLayers.empty() ||
            !profile.feet.empty() || !faceLayers.empty();
        if (!bodyLayers.empty()) {
            ++requestedParts;
            if (ApplyProfileBodyPart(*overrides, actor.get(), female, profile, bodyLayers)) {
                ++appliedParts;
            }
        }
        const auto applyGenitalAnal = [&](const std::vector<bcn::SkinTextureLayer>& layers,
            const bcn::skin_geometry::BodySelection selection) {
            if (layers.empty()) return;
            if (hasPrimaryParts) {
                static_cast<void>(ApplyPart(*overrides, actor.get(), female,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, selection));
            } else {
                applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kBody, layers, selection);
            }
        };
        applyGenitalAnal(profile.cbbeGenitalAnal,
            bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        applyGenitalAnal(profile.unpGenitalAnal,
            bcn::skin_geometry::BodySelection::unpGenitalAnal);
        applyPart(kSosMaleGenitalSlot, maleGenitalLayers,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (UsesBeastTail(profile) && !bodyLayers.empty()) {
                static_cast<void>(ApplyPart(*overrides, actor.get(), female,
                    RE::BGSBipedObjectForm::BipedObjectSlot::kTail, bodyLayers));
        }
        if (UsesUbeBodySlot(profile)) {
            applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands, bodyLayers);
            applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, bodyLayers);
        } else {
            applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kHands, handsLayers);
            applyPart(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, profile.feet);
        }
        if (!faceLayers.empty()) {
            ++requestedParts;
            if (faceNode && ApplyFacePart(*overrides, actor.get(), female, *faceNode, faceLayers)) {
                ++appliedParts;
            }
        }
        const auto complete = requestedParts != 0U && appliedParts == requestedParts;
        if (complete) {
            MarkCurrentSkinContent(actor.get(), profile, generation);
            SKSE::log::info("Body Change NG applied texture skin profile '{}' to actor {:08X} synchronously",
                profile.name, actor->GetFormID());
        } else {
            SKSE::log::warn(
                "Body Change NG applied only {}/{} currently available parts from skin '{}' to actor {:08X}; the desired selection remains pending for a later 3D/equipment refresh",
                appliedParts, requestedParts, profile.name, actor->GetFormID());
        }
        if (removed) {
            if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                vm && QueueNiNodeUpdate(*vm, actor.get(), actorHandle, generation)) return;
            QueueSettledSkinAudit(actorHandle, generation, 2U);
        } else {
            QueueSettledSkinAudit(actorHandle, generation, 0U);
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
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, kUbeBodySlot) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, kSosMaleGenitalSlot) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kTail) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, kUbeBodySlot) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, kSosMaleGenitalSlot) || cleared;
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
        const auto overrideVersion = OverrideVersion();
        if (overrideVersion == 0U || !RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
            return bcn::skin_override::ApplyResult::unavailable;
        }
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return bcn::skin_override::ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginSkinChange(actor->GetFormID());
        {
            // An unavailable desired profile is intentionally retained in the
            // Actor Registry, while the runtime selection cache stays empty so
            // equipment refreshes do not repeatedly retry a missing folder.
            std::scoped_lock lock(g_selectionLock);
            g_currentProfileIds[actor->GetFormID()] = {};
        }
        bcn::frame_tasks::Queue(actor->GetFormID(),
            [handle, generation, overrideVersion,
                unavailableProfileId = std::move(unavailableProfileId)]() mutable {
                if (overrideVersion == 1U) {
                    ClearLegacyNow(handle, generation, std::move(unavailableProfileId));
                } else {
                    ClearNow(handle, generation, std::move(unavailableProfileId));
                }
            }, 1, 204);
        return bcn::skin_override::ApplyResult::queued;
    }

    [[nodiscard]] std::optional<std::string> RuntimeProfileId(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_selectionLock);
        const auto found = g_currentProfileIds.find(actorFormID);
        if (found == g_currentProfileIds.end() || found->second.empty()) return std::nullopt;
        return found->second;
    }

    [[nodiscard]] std::optional<std::uint64_t> CurrentSkinGeneration(
        const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_generationLock);
        const auto found = g_applyGenerations.find(actorFormID);
        return found == g_applyGenerations.end() ? std::nullopt :
            std::optional<std::uint64_t>{ found->second };
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
                const auto currentProfile = RuntimeProfileId(actorFormID);
                if (!currentProfile || *currentProfile != profileId) return;
                const auto actor = actorHandle.get();
                if (!actor || !actor->Is3DLoaded() || actor->GetFormID() != actorFormID) return;
                const auto profile = bcn::SkinProfiles::Get().Find(profileId);
                auto* base = actor->GetActorBase();
                if (!profile || !base || !ProfileMatchesActor(actor.get(), *profile)) return;
                const auto face = FaceNode(actor.get(), base);
                if (!face) return;
                const auto layers = EffectiveFaceLayers(*profile, base, face->detailFilename);
                if (layers.empty()) return;
                const auto female = base->GetSex() == RE::SEX::kFemale;
                const auto overrideVersion = OverrideVersion();
                if (overrideVersion == 1U) {
                    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    const auto skinGeneration = CurrentSkinGeneration(actorFormID);
                    if (!vm || !skinGeneration) return;
                    auto batch = MakeLegacyBatch(actor.get(), *skinGeneration);
                    static_cast<void>(DispatchLegacyFaceApply(
                        *vm, actor.get(), female, *face, layers, batch));
                    CompleteLegacyBatch(batch);
                } else if (auto* overrides = OverrideInterfaceV2()) {
                    static_cast<void>(ApplyFacePart(
                        *overrides, actor.get(), female, *face, layers));
                }
            }, 8U, 109U);
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
                    SKSE::log::info(
                        "Body Change NG applied futanari skin '{}' ({}) to actor {:08X} through RaceMenu Override v1",
                        profile.name, bcn::FutanariSkinTypeLabel(profile.type),
                        settledActor->GetFormID());
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
                "Body Change NG restored the default futanari skin for actor {:08X} through RaceMenu Override v1",
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
        {
            std::scoped_lock lock(g_generationLock);
            g_applyGenerations.clear();
        }
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentProfileIds.clear();
        }
        {
            std::scoped_lock lock(g_legacyCleanupLock);
            g_legacyCleanupComplete.clear();
        }
        {
            std::scoped_lock lock(g_rsvFaceLock);
            g_rsvFaceGenerations.clear();
            g_rsvTransientFaces.clear();
        }
        {
            std::scoped_lock lock(g_futanariLock);
            g_futanariTypes.clear();
            g_futanariApplyGenerations.clear();
        }
    }

    ApplyResult QueueApply(RE::Actor* actor, std::string profileId)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        // RaceMenu SE exposes Override v1; AE exposes the public v2 wrapper.
        // Accept only those audited versions and require Papyrus for v1's
        // serialization-safe string path.
        const auto overrideVersion = OverrideVersion();
        if (overrideVersion == 0U ||
            (overrideVersion == 1U && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
            return ApplyResult::unavailable;
        }
        const auto profile = SkinProfiles::Get().Find(profileId);
        if (!profile) {
            SKSE::log::warn(
                "Body Change NG skin profile '{}' is unavailable; removing only stale BCNG texture overrides",
                profileId);
            static_cast<void>(QueueClearInternal(actor, std::move(profileId)));
            return ApplyResult::missingProfile;
        }
        auto* base = actor->GetActorBase();
        if (!base) return ApplyResult::invalidActor;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        if ((female && profile->sex != SkinSex::female) || (!female && profile->sex != SkinSex::male)) {
            return ApplyResult::incompatibleSex;
        }
        if (!SkinRaceMatchesActor(profile->race, ResolveActorSkinRace(actor))) {
            return ApplyResult::incompatibleRace;
        }
        if (!SkinMatchesActor(profile->bodyFamilies, body_family::ResolveActor(actor))) {
            return ApplyResult::incompatibleBodyFamily;
        }
        // Partial body/hands/feet packs do not need a face target. Require
        // live FaceGen geometry only when this profile supplies face layers.
        if (ProfileUsesFace(*profile, base) && !FaceNode(actor, base)) {
            return ApplyResult::faceGeometryUnavailable;
        }
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginSkinChange(actor->GetFormID());
        {
            // Preserve the desired selection even when a covered body part is
            // not repaintable until a later equipment event.
            std::scoped_lock lock(g_selectionLock);
            g_currentProfileIds[actor->GetFormID()] = profile->id;
        }
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, profile = *profile, generation, overrideVersion] {
            const auto resolved = handle.get();
            if (!resolved || !IsCurrentSkinChange(resolved->GetFormID(), generation)) return;
            if (profile.contentHash != SkinProfiles::Get().ContentHash(profile.id)) {
                [[maybe_unused]] const auto refreshed = QueueApply(resolved.get(), profile.id);
                return;
            }
            auto* currentBase = resolved->GetActorBase();
            if (!currentBase || !ProfileMatchesActor(resolved.get(), profile)) return;
            auto paths = EffectiveTexturePreparations(profile, resolved.get(), currentBase);
            const auto lease = bcn::frame_tasks::CurrentLease();
            const auto continueApply = [lease, handle, profile, generation, overrideVersion](const bool prepared) {
                if (!prepared) {
                    SKSE::log::warn(
                        "Body Change NG could not prepare every runtime texture alias for '{}' outside actor application; unavailable files will remain untouched",
                        profile.name);
                }
                static_cast<void>(bcn::frame_tasks::Continue(lease,
                    [handle, profile, generation, overrideVersion] {
                        const auto current = handle.get();
                        if (!current || !IsCurrentSkinChange(current->GetFormID(), generation)) return;
                        if (profile.contentHash != SkinProfiles::Get().ContentHash(profile.id)) {
                            [[maybe_unused]] const auto refreshed = QueueApply(current.get(), profile.id);
                            return;
                        }
                        if (overrideVersion == 1U) ApplyLegacyNow(handle, profile, generation);
                        else ApplyNow(handle, profile, generation);
                    }));
            };
            if (!bcn::runtime_assets::PrepareTexturePathsAsync(
                    static_cast<std::uint64_t>(resolved->GetFormID()) << 1U,
                    std::move(paths), continueApply,
                    bcn::async_work::FrameTaskQueue::InteractiveLease(lease))) {
                continueApply(false);
            }
        }, 1, 204);
        return ApplyResult::queued;
    }

    ApplyResult QueueClear(RE::Actor* actor)
    {
        return QueueClearInternal(actor, {});
    }

    std::optional<std::string> CurrentProfileId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        {
            std::scoped_lock lock(g_selectionLock);
            const auto found = g_currentProfileIds.find(actor->GetFormID());
            if (found != g_currentProfileIds.end()) return found->second.empty() ?
                std::nullopt : std::optional<std::string>(found->second);
        }
        // A partial profile can remain pending until its covered geometry is
        // loaded. Keep returning the desired selection so equipment and 3D
        // refresh events retry it instead of reviving an older complete skin.
        if (const auto selected = bcn::ActorRegistry::Get().SelectedSkinId(actor)) return selected;
        return bcn::ActorRegistry::Get().AppliedSkinId(actor);
    }

    std::optional<bcn::FutanariSkinType> CurrentFutanariType(
        RE::Actor* actor, const bool refresh)
    {
        if (!actor || !actor->Is3DLoaded()) return std::nullopt;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return std::nullopt;
        if (!refresh) {
            std::scoped_lock lock(g_futanariLock);
            const auto found = g_futanariTypes.find(actor->GetFormID());
            if (found != g_futanariTypes.end()) return found->second.type;
        }
        const auto detected = FindLoadedFutanariRoute(actor, false).type;
        {
            std::scoped_lock lock(g_futanariLock);
            g_futanariTypes[actor->GetFormID()] = { detected };
        }
        return detected;
    }

    ApplyResult QueueApplyFutanari(RE::Actor* actor, std::string profileId)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        auto* base = actor->GetActorBase();
        if (!base || base->GetSex() != RE::SEX::kFemale) return ApplyResult::incompatibleSex;
        const auto overrideVersion = OverrideVersion();
        if (overrideVersion == 0U ||
            (overrideVersion == 1U && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
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
            [handle, profile = *profile, generation, overrideVersion] {
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
                const auto continueApply = [lease, handle, profile, generation, overrideVersion](const bool prepared) {
                    if (!prepared) {
                        SKSE::log::warn(
                            "Body Change NG could not prepare every futanari texture for '{}'; unavailable channels remain unchanged",
                            profile.name);
                    }
                    static_cast<void>(bcn::frame_tasks::Continue(lease,
                        [handle, profile, generation, overrideVersion] {
                            const auto resolved = handle.get();
                            if (!resolved || !IsCurrentFutanariChange(
                                    resolved->GetFormID(), generation)) return;
                            if (overrideVersion == 1U) {
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
            }, 1U, 205U);
        return ApplyResult::queued;
    }

    ApplyResult QueueClearFutanari(RE::Actor* actor)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!actor) return ApplyResult::invalidActor;
        bcn::ActorRegistry::Get().ClearFutanariSkin(actor);
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        const auto overrideVersion = OverrideVersion();
        if (overrideVersion == 0U ||
            (overrideVersion == 1U && !RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
            return ApplyResult::unavailable;
        }
        if (!CurrentFutanariType(actor, true)) return ApplyResult::futanariGeometryUnavailable;
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginFutanariChange(actor->GetFormID());
        bcn::frame_tasks::Queue(actor->GetFormID(), [handle, generation, overrideVersion] {
            if (overrideVersion == 1U) ClearFutanariLegacyNow(handle, generation);
            else ClearFutanariV2Now(handle, generation);
        }, 1U, 205U);
        return ApplyResult::queued;
    }

    std::optional<std::string> CurrentFutanariProfileId(const RE::Actor* actor)
    {
        return bcn::ActorRegistry::Get().SelectedFutanariSkinId(actor);
    }

    void QueueReapplyCurrentFutanari(RE::Actor* actor)
    {
        if (const auto profile = CurrentFutanariProfileId(actor)) {
            [[maybe_unused]] const auto result = QueueApplyFutanari(actor, *profile);
        }
    }

    void InvalidateFutanariDetection(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0U) return;
        std::scoped_lock lock(g_futanariLock);
        g_futanariTypes.erase(actorFormID);
    }

    void NotifyNiNodeUpdated(RE::Actor* actor)
    {
        if (!actor || !actor->Is3DLoaded() || !bcn::frame_tasks::Active()) return;
        InvalidateFutanariDetection(actor->GetFormID());
        // Ordinary BCNG skins do not need RSV's delayed face race. Avoid a
        // queued 150-ms reconciliation on every unrelated NiNode rebuild.
        if (!HasRsvTransientFace(actor->GetFormID())) return;
        const auto profileId = RuntimeProfileId(actor->GetFormID());
        if (!profileId) return;
        // RSV's head effect intentionally waits 0.1 seconds before restoring
        // its serialized face keys. Apply BCNG immediately on selection, then
        // reconcile only this already-selected actor once after that boundary.
        // Channel coalescing collapses repeated NiNode events; there is no
        // polling, catalog scan, or all-NPC pass.
        const auto generation = BeginRsvFaceRefresh(actor->GetFormID());
        QueueSettledFaceRefresh(actor->GetHandle(), actor->GetFormID(), *profileId,
            generation, std::chrono::steady_clock::now() + std::chrono::milliseconds(150));
    }

    std::optional<bool> LiveSkinStateMatches(RE::Actor* actor, const std::string_view profileId,
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
                    const auto textureSet = material ? material->GetTextureSet() : nullptr;
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
                std::scoped_lock lock(g_selectionLock);
                g_currentProfileIds[actor->GetFormID()] = {};
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
            const std::vector<LoadedPartTarget>* loadedTargets = nullptr) {
            std::vector<LoadedPartView> views;
            if (loadedTargets) {
                for (const auto& target : *loadedTargets) {
                    views.insert(views.end(), target.views.begin(), target.views.end());
                }
            } else if (slot) {
                for (const auto& target : FindLoadedPartTargets(actor, *slot, selection, false)) {
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
                    const auto textureSet = material ? material->GetTextureSet() : nullptr;
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

        const auto bodyRoute = FindLoadedProfileBodyRoute(actor, *profile, false);
        inspectPart(EffectiveBodyLayers(*profile, base), bodyRoute.slot,
            bodyRoute.selection, "skin", &bodyRoute.targets);
        inspectPart(profile->cbbeGenitalAnal, RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::cbbeGenitalAnal);
        inspectPart(profile->unpGenitalAnal, RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            bcn::skin_geometry::BodySelection::unpGenitalAnal);
        inspectPart(EffectiveMaleGenitalLayers(*profile, actor, base), kSosMaleGenitalSlot,
            bcn::skin_geometry::BodySelection::maleGenitals);
        if (UsesBeastTail(*profile)) {
            inspectPart(EffectiveBodyLayers(*profile, base),
                RE::BGSBipedObjectForm::BipedObjectSlot::kTail);
        }
        if (UsesUbeBodySlot(*profile)) {
            const auto bodyLayers = EffectiveBodyLayers(*profile, base);
            inspectPart(bodyLayers, RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            inspectPart(bodyLayers, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        } else {
            inspectPart(EffectiveHandsLayers(*profile, base),
                RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            inspectPart(profile->feet, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        }
        if (scope == LiveCheckScope::fullProfile) {
            inspectPart(faceNode ? EffectiveFaceLayers(*profile, base, faceNode->detailFilename) :
                EffectiveFaceLayers(*profile, base, {}), std::nullopt,
                bcn::skin_geometry::BodySelection::all, "skin-face");
        }
        return comparableLayers != 0U && !mismatch;
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
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentProfileIds.erase(actorFormID);
        }
        {
            std::scoped_lock lock(g_generationLock);
            g_applyGenerations.erase(actorFormID);
        }
        {
            std::scoped_lock lock(g_futanariLock);
            g_futanariTypes.erase(actorFormID);
            g_futanariApplyGenerations.erase(actorFormID);
        }
        {
            std::scoped_lock lock(g_legacyCleanupLock);
            g_legacyCleanupComplete.erase(actorFormID);
        }
        static_cast<void>(ReleaseRsvTransientFace(actorFormID));
    }
}
