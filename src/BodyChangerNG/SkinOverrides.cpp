#include "BodyChangerNG/SkinOverrides.h"

#include "BodyChangerNG/RaceMenuBodyMorph.h"
#include "BodyChangerNG/SkinProfiles.h"
#include "BodyChangerNG/RuntimeAssetCache.h"
#include "BodyChangerNG/SkinOverrideOwnership.h"

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
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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
    std::unordered_map<RE::FormID, std::string> g_currentProfileIds;
    std::unordered_map<RE::FormID, std::uint64_t> g_applyGenerations;
    std::unordered_set<RE::FormID> g_legacyCleanupComplete;
    std::atomic<skee_override::IPluginInterface*> g_overrideInterface{};
    std::atomic_uint32_t g_overrideVersion{};

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
            SKSE::log::error("Body Changer NG rejected unsupported RaceMenu Override interface version {}", version);
            return nullptr;
        }
        g_overrideVersion.store(version, std::memory_order_release);
        g_overrideInterface.store(candidate, std::memory_order_release);
        SKSE::log::info("Body Changer NG received RaceMenu Override interface version {} path={}", version,
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
        return ++g_applyGenerations[actorFormID];
    }

    [[nodiscard]] bool IsCurrentSkinChange(const RE::FormID actorFormID, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_generationLock);
        const auto found = g_applyGenerations.find(actorFormID);
        return found != g_applyGenerations.end() && found->second == generation;
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
        default: return "unknown";
        }
    }

    struct LoadedPartView final
    {
        bool firstPerson{};
        RE::NiAVObject* object{};
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

    [[nodiscard]] std::vector<LoadedPartTarget> FindLoadedPartTargets(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot)
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

            auto target = std::ranges::find_if(results, [&](const LoadedPartTarget& candidate) {
                return candidate.armor == armor && candidate.addon == addon;
            });
            if (target == results.end()) {
                results.push_back({ .armor = armor, .addon = addon, .slotMask = armorMask & addonMask });
                target = std::prev(results.end());
            }
            target->views.push_back({ .firstPerson = firstPerson, .object = partClone });
            const auto actorSkinArmor = armor == skinArmor;
            RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                if (!IsSkinGeometry(geometry, actorSkinArmor)) {
                    return RE::BSVisit::BSVisitControl::kContinue;
                }
                const auto* rawName = geometry->name.c_str();
                const std::string geometryName = rawName && rawName[0] != '\0' ? rawName : "";
                if (std::ranges::find(target->immediateNodes, geometryName) == target->immediateNodes.end()) {
                    target->immediateNodes.push_back(geometryName);
                    target->persistentNodes.push_back(geometryName);
                }
                return RE::BSVisit::BSVisitControl::kContinue;
            });
        }
        for (const auto& target : results) {
            SKSE::log::info(
                "SkinAudit target actor={:08X} part={} armor={:08X} addon={:08X} addon-mask={:08X} source={} views={} skin-geometries={}",
                actor->GetFormID(), SkinPartName(slot), target.armor->GetFormID(), target.addon->GetFormID(), target.slotMask,
                target.armor == skinArmor ? "skin-armor" : "worn-armor", target.views.size(),
                target.immediateNodes.size());
        }
        return results;
    }

    void QueueSettledSkinAudit(RE::ActorHandle actorHandle, std::uint64_t generation,
        std::uint32_t remainingTaskHops);

    class NodeUpdateCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        NodeUpdateCallback(RE::ActorHandle actorHandle, const std::uint64_t generation) :
            actorHandle_(std::move(actorHandle)), generation_(generation) {}

        void operator()(RE::BSScript::Variable) override
        {
            // Actor.QueueNiNodeUpdate is asynchronous with respect to the
            // scene graph used by RaceMenu BodyMorph.  Start the post-rebuild
            // barrier only after Papyrus reports that the call completed.
            QueueSettledSkinAudit(actorHandle_, generation_, 2U);
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        RE::ActorHandle actorHandle_;
        std::uint64_t generation_{};
    };

    [[nodiscard]] bool QueueNiNodeUpdate(RE::BSScript::Internal::VirtualMachine& vm, RE::Actor* actor,
        RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        if (!actor) return false;
        auto* policy = vm.GetObjectHandlePolicy();
        if (!policy) return false;
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(actor->GetFormType()), actor);
        if (handle == policy->EmptyHandle()) return false;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
            new NodeUpdateCallback(std::move(actorHandle), generation));
        return vm.DispatchMethodCall(handle, "Actor", "QueueNiNodeUpdate",
            RE::MakeFunctionArguments(), callback);
    }

    [[nodiscard]] bool ApplyPart(skee_override::IOverrideInterfaceV2& overrides, RE::Actor* actor,
                                 const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
                                 const std::vector<bcn::SkinTextureLayer>& layers)
    {
        if (!actor) return false;
        const auto targets = FindLoadedPartTargets(actor, slot);
        if (targets.empty()) return false;

        std::vector<std::pair<std::uint8_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, "skin");
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
                            actor->GetFormID(), SkinPartName(slot), target.armor->GetFormID(),
                            target.addon->GetFormID(), node, static_cast<std::uint32_t>(textureIndex), current.Value());
                        continue;
                    }
                    skee_override::StringVariant value{ path };
                    overrides.AddArmorOverride(actor, female, target.armor, target.addon, node.c_str(),
                        static_cast<std::uint16_t>(kShaderTextureProperty), textureIndex, value);
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part={} armor={:08X} addon={:08X} node='{}' index={}({}) action={} value='{}'",
                        actor->GetFormID(), SkinPartName(slot), target.armor->GetFormID(),
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
            actor->GetFormID(), SkinPartName(slot), submitted, appliedTargets);
        return submitted != 0U && appliedTargets != 0U;
    }

    [[nodiscard]] bool ClearArmorAddonPart(skee_override::IOverrideInterfaceV2& overrides,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot)
    {
        const auto targets = FindLoadedPartTargets(actor, slot);
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
        return stored;
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
                relevant = relevant || loweredPath.contains("bodychangerng\\cache\\skin\\") ||
                    loweredPath.contains("bodychangerng\\cache\\skin-face\\") ||
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
        tasks->AddTask([actorHandle, generation, remainingTaskHops] {
            const auto actor = actorHandle.get();
            if (!actor || !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
            if (remainingTaskHops != 0U) {
                QueueSettledSkinAudit(actorHandle, generation, remainingTaskHops - 1U);
                return;
            }
            if (const auto profileID = bcn::skin_override::CurrentProfileId(actor.get())) {
                if (const auto profile = bcn::SkinProfiles::Get().Find(*profileID)) {
                    AuditLiveSkinProfile(actor.get(), *profile);
                }
            }
            SKSE::log::info(
                "SkinAudit settled actor={:08X} generation={} body-reapply=false",
                actor->GetFormID(), generation);
        });
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
                    if (lowered.contains("bodychangerng\\cache\\skin-face\\")) {
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
                "Body Changer NG removed {} legacy face-texture override node(s) from non-face geometry on actor {:08X}",
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

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> EffectiveFaceLayers(const bcn::SkinProfile& profile,
        const bool vampire, const std::string_view currentDetailFilename)
    {
        // Vampire maps in BodyChange can be deliberately partial. Start with
        // the normal face's maps (including the selected detail map) and let a
        // supplied vampire map replace only its matching texture slot.
        auto layers = profile.face;
        if (vampire) {
            for (const auto& vampireLayer : profile.vampireFace) {
                const auto existing = std::ranges::find(layers, vampireLayer.shaderTextureIndex,
                                                         &bcn::SkinTextureLayer::shaderTextureIndex);
                if (existing != layers.end()) {
                    *existing = vampireLayer;
                } else {
                    layers.push_back(vampireLayer);
                }
            }
        }
        if (std::ranges::find(layers, kFaceDetailTextureIndex,
                &bcn::SkinTextureLayer::shaderTextureIndex) == layers.end()) {
            if (const auto detail = MatchingFaceDetail(profile, currentDetailFilename)) layers.push_back(*detail);
        }
        return layers;
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
        const auto faceLayers = faceNode ?
            EffectiveFaceLayers(profile, IsVampireRace(base), faceNode->detailFilename) : profile.face;
        const auto verifyPart = [&](const std::string_view part,
            const std::vector<bcn::SkinTextureLayer>& layers,
            const std::optional<RE::BGSBipedObjectForm::BipedObjectSlot> slot) {
            std::vector<LoadedPartView> views;
            if (slot) {
                for (const auto& target : FindLoadedPartTargets(actor, *slot)) {
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
                        auto* shader = geometry->lightingShaderProp_cast();
                        auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                        const auto textureSet = material ? material->GetTextureSet() : nullptr;
                        if (!textureSet) return RE::BSVisit::BSVisitControl::kContinue;
                        const auto* actual = textureSet->GetTexturePath(
                            static_cast<RE::BSTextureSet::Texture>(layer.shaderTextureIndex));
                        if (!actual || NormalizedTexturePath(actual) != normalizedExpected) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        const auto* rawName = geometry->name.c_str();
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
        verifyPart("body", profile.body, RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
        verifyPart("hands", profile.hands, RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
        verifyPart("feet", profile.feet, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
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
        std::atomic_uint32_t pending{ 1U };  // submission sentinel
        std::atomic_uint32_t accepted{};
        std::function<void(std::uint32_t)> completion;
    };

    void CompleteLegacyBatch(const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        if (!batch || batch->pending.fetch_sub(1U, std::memory_order_acq_rel) != 1U) return;
        const auto accepted = batch->accepted.load(std::memory_order_acquire);
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto completion = batch->completion;
            tasks->AddTask([completion, accepted] {
                if (completion) completion(accepted);
            });
        }
    }

    class LegacyOverrideCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        explicit LegacyOverrideCallback(std::shared_ptr<LegacyOverrideBatch> batch) : batch_(std::move(batch)) {}
        void operator()(RE::BSScript::Variable) override { CompleteLegacyBatch(batch_); }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::shared_ptr<LegacyOverrideBatch> batch_;
    };

    [[nodiscard]] bool DispatchLegacy(RE::BSScript::Internal::VirtualMachine& vm, const char* function,
        RE::BSScript::IFunctionArguments* arguments, const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        batch->pending.fetch_add(1U, std::memory_order_relaxed);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new LegacyOverrideCallback(batch));
        const auto dispatched = vm.DispatchStaticCall("NiOverride", function, arguments, callback);
        if (dispatched) batch->accepted.fetch_add(1U, std::memory_order_release);
        else CompleteLegacyBatch(batch);
        return dispatched;
    }

    class LegacyOwnershipQueryCallback final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        LegacyOwnershipQueryCallback(std::shared_ptr<LegacyOverrideBatch> batch,
            std::function<void(std::string)> completion) :
            batch_(std::move(batch)), completion_(std::move(completion)) {}

        void operator()(RE::BSScript::Variable result) override
        {
            std::string current;
            if (result.IsString()) current = result.GetString();
            if (completion_) completion_(std::move(current));
            CompleteLegacyBatch(batch_);
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::shared_ptr<LegacyOverrideBatch> batch_;
        std::function<void(std::string)> completion_;
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
            CompleteLegacyBatch(batch);
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

        // Clean the broad skin-slot keys written by Body Changer NG v0.2.15
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

    [[nodiscard]] bool DispatchLegacyPartApply(RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
        std::vector<std::pair<std::uint32_t, std::string>> resolvedLayers;
        resolvedLayers.reserve(layers.size());
        for (const auto& layer : layers) {
            auto path = bcn::runtime_assets::TexturePathFromGameRelative(layer.path, "skin");
            if (!path.empty()) resolvedLayers.emplace_back(layer.shaderTextureIndex, std::move(path));
        }
        if (resolvedLayers.empty()) return false;

        std::size_t submitted{};
        for (const auto& target : FindLoadedPartTargets(actor, slot)) {
            for (const auto& node : target.persistentNodes) {
                for (const auto& [textureIndex, path] : resolvedLayers) {
                    auto* armor = target.armor;
                    auto* addon = target.addon;
                    DispatchLegacyOwnershipQuery(vm, "GetOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                        static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                        std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex)), batch,
                        [&vm, actor, female, armor, addon, node, textureIndex, path, batch](std::string current) {
                            const auto exists = !current.empty();
                            if (!bcn::skin_override::ownership::MayReplace(exists, current)) {
                                SKSE::log::warn(
                                    "SkinOverride persistent-register skipped actor={:08X} armor={:08X} addon={:08X} node='{}' index={} reason=foreign-owner mode=RaceMenu-v1 current='{}'",
                                    actor->GetFormID(), armor->GetFormID(), addon->GetFormID(), node,
                                    textureIndex, current);
                                return;
                            }
                            static_cast<void>(DispatchLegacy(vm, "AddOverrideString", RE::MakeFunctionArguments(
                                static_cast<RE::TESObjectREFR*>(actor), bool{ female },
                                static_cast<RE::TESObjectARMO*>(armor), static_cast<RE::TESObjectARMA*>(addon),
                                std::string{ node }, static_cast<std::uint32_t>(kShaderTextureProperty),
                                static_cast<std::uint32_t>(textureIndex), std::string{ path }, true), batch));
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
            actor->GetFormID(), SkinPartName(slot), submitted);
        return submitted != 0U;
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

    [[nodiscard]] bool DispatchLegacyFaceApply(RE::BSScript::Internal::VirtualMachine& vm,
        RE::Actor* actor, const bool female, const FaceNodeInfo& face,
        const std::vector<bcn::SkinTextureLayer>& layers,
        const std::shared_ptr<LegacyOverrideBatch>& batch)
    {
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
                [&vm, actor, female, node = face.nodeName, textureIndex, path, batch](std::string current) {
                    const auto exists = !current.empty();
                    if (!bcn::skin_override::ownership::MayReplace(exists, current)) {
                        SKSE::log::warn(
                            "SkinOverride persistent-register skipped actor={:08X} part=face node='{}' index={} reason=foreign-owner mode=RaceMenu-v1 current='{}'",
                            actor->GetFormID(), node, textureIndex, current);
                        return;
                    }
                    static_cast<void>(DispatchLegacy(vm, "AddNodeOverrideString", RE::MakeFunctionArguments(
                        static_cast<RE::TESObjectREFR*>(actor), bool{ female }, std::string{ node },
                        static_cast<std::uint32_t>(kShaderTextureProperty),
                        static_cast<std::uint32_t>(textureIndex), std::string{ path }, true), batch));
                    SKSE::log::info(
                        "SkinOverride persistent-register actor={:08X} part=face node='{}' index={}({}) action={} value='{}' mode=RaceMenu-v1",
                        actor->GetFormID(), node, textureIndex, TextureIndexName(textureIndex),
                        exists ? "replace-owned" : "add", path);
                });
            ++submitted;
        }
        return submitted != 0U;
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
        const auto faceNode = FaceNode(actor.get(), base);
        if (!faceNode) {
            SKSE::log::warn(
                "Body Changer NG skipped skin '{}' on actor {:08X}: no live FaceGen geometry was found; partial application would create a neck seam",
                profile.name, actor->GetFormID());
            return;
        }
        const auto faceLayers = EffectiveFaceLayers(profile, IsVampireRace(base), faceNode->detailFilename);
        const auto clearDetail = std::ranges::find(faceLayers, kFaceDetailTextureIndex,
            &bcn::SkinTextureLayer::shaderTextureIndex) != faceLayers.end();

        auto clearBatch = std::make_shared<LegacyOverrideBatch>();
        clearBatch->completion = [actorHandle, profile, generation](const std::uint32_t) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentActor->Is3DLoaded() || !currentVM ||
                !IsCurrentSkinChange(currentActor->GetFormID(), generation)) return;
            auto* currentBase = currentActor->GetActorBase();
            const auto currentFace = FaceNode(currentActor.get(), currentBase);
            if (!currentBase || !currentFace) return;
            const auto currentFemale = currentBase->GetSex() == RE::SEX::kFemale;
            const auto currentFaceLayers = EffectiveFaceLayers(
                profile, IsVampireRace(currentBase), currentFace->detailFilename);

            auto applyBatch = std::make_shared<LegacyOverrideBatch>();
            applyBatch->completion = [actorHandle, profile, generation](const std::uint32_t accepted) {
                const auto settledActor = actorHandle.get();
                if (!settledActor || !IsCurrentSkinChange(settledActor->GetFormID(), generation)) return;
                if (accepted == 0U) {
                    SKSE::log::warn("Body Changer NG could not dispatch RaceMenu v1 skin profile '{}'", profile.name);
                    return;
                }
                {
                    std::scoped_lock lock(g_selectionLock);
                    g_currentProfileIds[settledActor->GetFormID()] = profile.id;
                }
                SKSE::log::info(
                    "Body Changer NG applied texture skin profile '{}' to actor {:08X} through RaceMenu Override v1",
                    profile.name, settledActor->GetFormID());
                QueueSettledSkinAudit(actorHandle, generation, 3U);
            };

            bool submitted{};
            submitted = DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                RE::BGSBipedObjectForm::BipedObjectSlot::kBody, profile.body, applyBatch) || submitted;
            submitted = DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                RE::BGSBipedObjectForm::BipedObjectSlot::kHands, profile.hands, applyBatch) || submitted;
            submitted = DispatchLegacyPartApply(*currentVM, currentActor.get(), currentFemale,
                RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, profile.feet, applyBatch) || submitted;
            submitted = DispatchLegacyFaceApply(*currentVM, currentActor.get(), currentFemale,
                *currentFace, currentFaceLayers, applyBatch) || submitted;
            if (!submitted) {
                SKSE::log::warn("Body Changer NG found no RaceMenu v1 skin targets for '{}'", profile.name);
            }
            CompleteLegacyBatch(applyBatch);
        };

        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, clearBatch);
        DispatchLegacyFaceClear(*vm, actor.get(), female, faceNode->nodeName, clearDetail, clearBatch);
        CompleteLegacyBatch(clearBatch);
    }

    void ClearLegacyNow(RE::ActorHandle actorHandle, const std::uint64_t generation)
    {
        const auto actor = actorHandle.get();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !actor->Is3DLoaded() || !vm ||
            !IsCurrentSkinChange(actor->GetFormID(), generation)) return;
        auto* base = actor->GetActorBase();
        if (!base) return;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto faceNode = FaceNode(actor.get(), base);

        auto clearBatch = std::make_shared<LegacyOverrideBatch>();
        clearBatch->completion = [actorHandle, generation](const std::uint32_t accepted) {
            const auto currentActor = actorHandle.get();
            auto* currentVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!currentActor || !currentVM ||
                !IsCurrentSkinChange(currentActor->GetFormID(), generation)) return;
            if (accepted == 0U) {
                SKSE::log::warn("Body Changer NG could not dispatch RaceMenu v1 skin cleanup for actor {:08X}",
                    currentActor->GetFormID());
                return;
            }
            if (!QueueNiNodeUpdate(*currentVM, currentActor.get(), actorHandle, generation)) {
                QueueSettledSkinAudit(actorHandle, generation, 2U);
            }
            SKSE::log::info("Body Changer NG removed its RaceMenu v1 skin texture overrides for actor {:08X}",
                currentActor->GetFormID());
        };
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands, clearBatch);
        DispatchLegacyPartClear(*vm, actor.get(), female,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, clearBatch);
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
        auto* overrides = OverrideInterfaceV2();
        if (!overrides) return;

        // Never change only the body. A missing face node would leave a visible
        // neck seam, especially on follower and custom FaceGen NPCs.
        const auto faceNode = FaceNode(actor.get(), base);
        if (!faceNode) {
            SKSE::log::warn(
                "Body Changer NG skipped skin '{}' on actor {:08X}: no live FaceGen geometry was found; partial application would create a neck seam",
                profile.name, actor->GetFormID());
            return;
        }
        const auto faceLayers = EffectiveFaceLayers(profile, IsVampireRace(base), faceNode->detailFilename);
        // Store exact Armor + ArmorAddon + geometry keys, then repaint only
        // that loaded addon clone. Broad AddSkinOverrideString/skin-slot
        // registration is intentionally never used for new entries.
        [[maybe_unused]] const auto cleanedLegacy =
            ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female);
        if (ClaimLegacyCleanup(actor->GetFormID())) {
            [[maybe_unused]] const auto clearedLegacySkinBody = ClearTexturePart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
            [[maybe_unused]] const auto clearedLegacySkinHands = ClearTexturePart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            [[maybe_unused]] const auto clearedLegacySkinFeet = ClearTexturePart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
            [[maybe_unused]] const auto clearedLegacyBody = ClearArmorAddonPart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
            [[maybe_unused]] const auto clearedLegacyHands = ClearArmorAddonPart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands);
            [[maybe_unused]] const auto clearedLegacyFeet = ClearArmorAddonPart(
                *overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        }
        bool applied{};
        applied = ApplyPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody, profile.body) || applied;
        applied = ApplyPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands, profile.hands) || applied;
        applied = ApplyPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, profile.feet) || applied;
        applied = ApplyFacePart(*overrides, actor.get(), female, *faceNode, faceLayers) || applied;
        if (applied) {
            {
                std::scoped_lock lock(g_selectionLock);
                g_currentProfileIds[actor->GetFormID()] = profile.id;
            }
            SKSE::log::info("Body Changer NG applied texture skin profile '{}' to actor {:08X} synchronously",
                profile.name, actor->GetFormID());
            QueueSettledSkinAudit(actorHandle, generation, 0U);
        } else {
            SKSE::log::warn("Body Changer NG could not apply the RaceMenu texture profile '{}'", profile.name);
        }
    }

    void ClearNow(RE::ActorHandle actorHandle, const std::uint64_t generation)
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
        bool cleared{};
        cleared = ClearLegacyMisdirectedFaceNodes(*overrides, actor.get(), female) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || cleared;
        cleared = ClearTexturePart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kBody) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kHands) || cleared;
        cleared = ClearArmorAddonPart(*overrides, actor.get(), female, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) || cleared;
        if (faceNode) cleared = ClearFaceTextures(*overrides, actor.get(), female, faceNode->nodeName, true) || cleared;
        if (!cleared) {
            SKSE::log::warn("Body Changer NG could not clear its RaceMenu skin texture overrides for actor {:08X}", actor->GetFormID());
            return;
        }
        if (!QueueNiNodeUpdate(*vm, actor.get(), actorHandle, generation)) {
            QueueSettledSkinAudit(actorHandle, generation, 2U);
        }
        SKSE::log::info("Body Changer NG removed its RaceMenu skin texture overrides for actor {:08X}",
            actor->GetFormID());
    }
}

namespace bcn::skin_override
{
    ApplyResult QueueApply(RE::Actor* actor, std::string profileId)
    {
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
        if (!profile) return ApplyResult::missingProfile;
        const auto* base = actor->GetActorBase();
        if (!base) return ApplyResult::invalidActor;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        if ((female && profile->sex != SkinSex::female) || (!female && profile->sex != SkinSex::male)) {
            return ApplyResult::incompatibleSex;
        }
        // Body/hands/feet without the matching live face would create the neck
        // seam the user is explicitly trying to avoid. Reject synchronously so
        // the UI never reports a partial skin as successfully applied.
        if (!FaceNode(actor, const_cast<RE::TESNPC*>(base))) return ApplyResult::faceGeometryUnavailable;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginSkinChange(actor->GetFormID());
        tasks->AddTask([handle, profile = *profile, generation, overrideVersion] {
            if (overrideVersion == 1U) ApplyLegacyNow(handle, profile, generation);
            else ApplyNow(handle, profile, generation);
        });
        return ApplyResult::queued;
    }

    ApplyResult QueueClear(RE::Actor* actor)
    {
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        const auto overrideVersion = OverrideVersion();
        if (overrideVersion == 0U || !RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
            return ApplyResult::unavailable;
        }
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return ApplyResult::noTaskInterface;
        const auto handle = actor->GetHandle();
        const auto generation = BeginSkinChange(actor->GetFormID());
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentProfileIds.erase(actor->GetFormID());
        }
        tasks->AddTask([handle, generation, overrideVersion] {
            if (overrideVersion == 1U) ClearLegacyNow(handle, generation);
            else ClearNow(handle, generation);
        });
        return ApplyResult::queued;
    }

    std::optional<std::string> CurrentProfileId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        std::scoped_lock lock(g_selectionLock);
        const auto found = g_currentProfileIds.find(actor->GetFormID());
        return found == g_currentProfileIds.end() ? std::nullopt : std::optional{ found->second };
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
        std::scoped_lock lock(g_selectionLock);
        g_currentProfileIds.erase(actorFormID);
    }
}
