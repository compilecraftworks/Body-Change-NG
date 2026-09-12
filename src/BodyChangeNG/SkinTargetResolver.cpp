#include "BodyChangeNG/SkinTargetResolver.h"

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/NativeAddonSkinBackend.h"

#include <RE/A/Actor.h>
#include <RE/B/BGSListForm.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BipedAnim.h>
#include <RE/B/BipedObjects.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSTextureSet.h>
#include <RE/B/BSVisit.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <unordered_set>
#include <utility>

namespace
{
    using bcn::skin_target::LoadedPartTarget;
    using bcn::skin_target::LoadedPartView;

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
        std::string result{ separator == std::string_view::npos ?
            path : path.substr(separator + 1U) };
        return LowerAscii(std::move(result));
    }

    [[nodiscard]] std::string AddonModelPath(
        RE::TESObjectARMA* addon, const bool firstPerson)
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

    [[nodiscard]] LoadedPartTarget& FindOrAppendTarget(
        std::vector<LoadedPartTarget>& targets, RE::TESObjectARMO* armor,
        RE::TESObjectARMA* addon, const std::uint32_t slotMask)
    {
        const auto found = std::ranges::find_if(targets, [&](const LoadedPartTarget& target) {
            return target.armor == armor && target.addon == addon;
        });
        if (found != targets.end()) return *found;
        targets.push_back({ .armor = armor, .addon = addon, .slotMask = slotMask });
        return targets.back();
    }

    void AppendUnique(std::vector<std::string>& values, const std::string& value)
    {
        if (std::ranges::find(values, value) == values.end()) values.push_back(value);
    }

    void AppendView(LoadedPartTarget& target, const bool firstPerson,
        const bool actorSkinArmor, RE::NiAVObject* object,
        const std::vector<std::string>& nodes)
    {
        const auto duplicate = std::ranges::any_of(target.views, [&](const LoadedPartView& view) {
            return view.object == object && view.firstPerson == firstPerson;
        });
        if (!duplicate) {
            target.views.push_back({
                .firstPerson = firstPerson,
                .actorSkinArmor = actorSkinArmor,
                .object = object,
                .nodes = nodes
            });
        }
        for (const auto& node : nodes) {
            AppendUnique(target.immediateNodes, node);
            AppendUnique(target.persistentNodes, node);
        }
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
                    !addon->IsValidRace(actor->GetRace()) ||
                    !inspectedClones.insert(partClone).second) continue;

                std::vector<std::string> matchingNodes;
                RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                    if (!bcn::skin_target::IsSkinGeometry(geometry, false)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    const auto* rawName = geometry->name.c_str();
                    const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                    const auto texturePath = bcn::skin_target::GeometryDiffuseTexture(geometry);
                    if (!bcn::skin_geometry::IsBodyGeometryCandidate(name, texturePath) ||
                        !bcn::skin_geometry::Matches(name, selection, texturePath)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    AppendUnique(matchingNodes, name);
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (matchingNodes.empty()) continue;

                auto& target = FindOrAppendTarget(results, armor, addon,
                    armor->GetSlotMask().underlying() & addon->GetSlotMask().underlying());
                AppendView(target, firstPerson, armor == actor->GetSkin(),
                    partClone, matchingNodes);
            }
        }
    }

    [[nodiscard]] RE::TESObjectARMA* ActiveAddonRecord(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot)
    {
        if (!actor) return nullptr;
        const auto requestedMask = static_cast<std::uint32_t>(slot);
        if (requestedMask == 0U || !std::has_single_bit(requestedMask)) return nullptr;
        const auto objectIndex = static_cast<std::size_t>(std::countr_zero(requestedMask));
        if (objectIndex >= RE::BIPED_OBJECTS::kEditorTotal) return nullptr;
        if (const auto& biped = actor->GetBiped(false)) {
            if (auto* addon = biped->objects[objectIndex].addon) return addon;
        }
        if (auto* skin = actor->GetSkin()) {
            for (auto* candidate : skin->armorAddons) {
                if (candidate &&
                    (static_cast<std::uint32_t>(candidate->GetSlotMask().underlying()) &
                        requestedMask) != 0U) return candidate;
            }
        }
        return nullptr;
    }
}

namespace bcn::skin_target
{
    RE::BSTextureSet* StableTextureSet(RE::BSLightingShaderMaterialBase* material)
    {
        auto* textureSet = material ? material->textureSet.get() : nullptr;
        if (!textureSet) return nullptr;
        // RaceMenu may detach a material while applying an override. A cleared
        // vtable is a transitional object and must never receive a virtual call.
        if (*reinterpret_cast<const std::uintptr_t*>(textureSet) == 0U) return nullptr;
        return textureSet;
    }

    bool IsSkinGeometry(RE::BSGeometry* geometry, const bool actorSkinArmor)
    {
        if (!geometry) return false;
        if (actorSkinArmor) return true;
        auto* shader = geometry->lightingShaderProp_cast();
        auto* material = shader ?
            static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        return material && material->GetFeature() ==
            RE::BSShaderMaterial::Feature::kFaceGenRGBTint;
    }

    std::string_view GeometryDiffuseTexture(RE::BSGeometry* geometry)
    {
        if (!geometry) return {};
        auto* shader = geometry->lightingShaderProp_cast();
        auto* material = shader ?
            static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        const auto textureSet = StableTextureSet(material);
        if (!textureSet) return {};
        const auto* path = textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse);
        return path ? std::string_view{ path } : std::string_view{};
    }

    [[nodiscard]] std::string GenitalSourceDiffuse(RE::Actor* actor,
        RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSGeometry* geometry)
    {
        if (!actor || !geometry) return {};
        const auto* base = actor->GetActorBase();
        const auto channel = base && base->GetSex() == RE::SEX::kFemale ?
            native_addon::Channel::futanari : native_addon::Channel::maleGenitals;
        auto original = native_addon::SourceDiffuseTexture(actor->GetFormID(), channel,
            armor ? armor->GetFormID() : 0U, addon ? addon->GetFormID() : 0U, *geometry);
        return original.empty() ? std::string{ GeometryDiffuseTexture(geometry) } : original;
    }

    LoadedFutanariRoute FindLoadedFutanariRoute(RE::Actor* actor)
    {
        LoadedFutanariRoute result;
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base || base->GetSex() != RE::SEX::kFemale ||
            !actor->Is3DLoaded()) return result;

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
                if (!armor || !addon || !addon->IsValidRace(actor->GetRace())) continue;

                const auto slot52 = static_cast<std::uint32_t>(kSosMaleGenitalSlot);
                if ((armor->GetSlotMask().underlying() & slot52) == 0U ||
                    (addon->GetSlotMask().underlying() & slot52) == 0U) continue;

                const auto modelKind = futanari::ClassifyEvidence(
                    AddonModelPath(addon, firstPerson));
                if (modelKind != futanari::AddonKind::none) {
                    if (result.addonKind != futanari::AddonKind::none &&
                        result.addonKind != modelKind) {
                        SKSE::log::warn(
                            "Body Change NG found simultaneous TRX and ERF futanari equipment on actor {:08X}; refusing an ambiguous texture route",
                            actor->GetFormID());
                        return {};
                    }
                    result.addonKind = modelKind;
                }
                if (!partClone || !inspectedClones.insert(partClone).second) continue;

                std::vector<std::string> matchingNodes;
                auto targetKind = modelKind;
                RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto* rawName = geometry->name.c_str();
                    const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                    if (name.empty()) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto geometryKind = futanari::ClassifyEvidence(
                        {}, name, GenitalSourceDiffuse(actor, armor, addon, geometry));
                    if (geometryKind == futanari::AddonKind::none ||
                        (modelKind != futanari::AddonKind::none && geometryKind != modelKind)) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (targetKind == futanari::AddonKind::none) targetKind = geometryKind;
                    if (geometryKind == targetKind) AppendUnique(matchingNodes, name);
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (targetKind == futanari::AddonKind::none || matchingNodes.empty()) continue;
                if (result.addonKind != futanari::AddonKind::none &&
                    result.addonKind != targetKind) {
                    SKSE::log::warn(
                        "Body Change NG found simultaneous TRX and ERF futanari targets on actor {:08X}; refusing an ambiguous texture route",
                        actor->GetFormID());
                    return {};
                }
                result.addonKind = targetKind;
                auto& target = FindOrAppendTarget(result.targets, armor, addon,
                    armor->GetSlotMask().underlying() & addon->GetSlotMask().underlying());
                AppendView(target, firstPerson, armor == actor->GetSkin(),
                    partClone, matchingNodes);
            }
        }

        result.type = FutanariTypeFor(result.addonKind, body_family::ResolveActor(actor));
        if (!result.type) result.targets.clear();
        return result;
    }

    std::vector<LoadedPartTarget> FindLoadedPartTargets(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot,
        const skin_geometry::BodySelection selection,
        const bool allowExplicitLimbNode)
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
            if ((armorMask & requestedMask) == 0U ||
                (addonMask & requestedMask) == 0U) continue;

            const auto actorSkinArmor = armor == skinArmor;
            std::vector<std::string> matchingNodes;
            RE::BSVisit::TraverseScenegraphGeometries(partClone, [&](RE::BSGeometry* geometry) {
                const auto* rawName = geometry->name.c_str();
                const std::string geometryName = rawName && rawName[0] != '\0' ? rawName : "";
                const auto explicitLimb = allowExplicitLimbNode &&
                    skin_geometry::MatchesExplicitRequestedLimbNode(requestedMask,
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHands),
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet),
                        geometryName);
                const auto texturePath = explicitLimb ? std::string{} :
                    (selection == skin_geometry::BodySelection::maleGenitals ?
                        GenitalSourceDiffuse(actor, armor, addon, geometry) :
                        std::string{ GeometryDiffuseTexture(geometry) });
                if (!skin_geometry::MatchesRequestedPart(requestedMask,
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kBody),
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHands),
                        static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet),
                        geometryName, texturePath) ||
                    !skin_geometry::Matches(geometryName, selection, texturePath) ||
                    (!IsSkinGeometry(geometry, actorSkinArmor) &&
                        selection != skin_geometry::BodySelection::maleGenitals)) {
                    return RE::BSVisit::BSVisitControl::kContinue;
                }
                AppendUnique(matchingNodes, geometryName);
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            if (matchingNodes.empty()) continue;

            auto& target = FindOrAppendTarget(results, armor, addon, armorMask & addonMask);
            AppendView(target, firstPerson, actorSkinArmor, partClone, matchingNodes);
        }

        const auto requestedLimb = slot == RE::BGSBipedObjectForm::BipedObjectSlot::kHands ||
            slot == RE::BGSBipedObjectForm::BipedObjectSlot::kFeet;
        const auto requestedBody = slot == RE::BGSBipedObjectForm::BipedObjectSlot::kBody ||
            slot == kUbeBodySlot;
        const auto hasExactWornBody = requestedBody && std::ranges::any_of(results,
            [skinArmor](const LoadedPartTarget& target) { return target.armor != skinArmor; });
        if (requestedBody && !hasExactWornBody) {
            AppendCrossSlotBodyTargets(actor, selection, results);
        }
        if (requestedLimb) {
            std::erase_if(results,
                [](const LoadedPartTarget& target) { return target.immediateNodes.empty(); });

            for (const bool firstPerson : { false, true }) {
                if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
                const auto& biped = actor->GetBiped(firstPerson);
                if (!biped) continue;
                std::unordered_set<RE::NiAVObject*> inspectedClones;
                for (const auto& target : results) {
                    for (const auto& view : target.views) {
                        if (view.firstPerson == firstPerson && view.object) {
                            inspectedClones.insert(view.object);
                        }
                    }
                }
                for (std::size_t index{}; index < RE::BIPED_OBJECTS::kEditorTotal; ++index) {
                    const auto& object = biped->objects[index];
                    auto* armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
                    auto* addon = object.addon;
                    auto* partClone = object.partClone.get();
                    if (!armor || !addon || !partClone ||
                        !addon->IsValidRace(actor->GetRace()) ||
                        !inspectedClones.insert(partClone).second) continue;

                    std::vector<std::string> matchingNodes;
                    RE::BSVisit::TraverseScenegraphGeometries(partClone,
                        [&](RE::BSGeometry* geometry) {
                        const auto* rawName = geometry->name.c_str();
                        const std::string name = rawName && rawName[0] != '\0' ? rawName : "";
                        const auto explicitLimb = allowExplicitLimbNode &&
                            skin_geometry::MatchesExplicitRequestedLimbNode(requestedMask,
                                static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHands),
                                static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet),
                                name);
                        const auto texturePath = explicitLimb ? std::string_view{} :
                            GeometryDiffuseTexture(geometry);
                        const auto skinGeometry = IsSkinGeometry(geometry, armor == skinArmor);
                        if (!skin_geometry::IsSafeCrossSlotLimbCandidate(requestedMask,
                                static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHands),
                                static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet),
                                name, texturePath, skinGeometry) ||
                            !skin_geometry::Matches(name, selection, texturePath)) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        AppendUnique(matchingNodes, name);
                        return RE::BSVisit::BSVisitControl::kContinue;
                    });
                    if (matchingNodes.empty()) continue;

                    auto& target = FindOrAppendTarget(results, armor, addon,
                        armor->GetSlotMask().underlying() & addon->GetSlotMask().underlying());
                    AppendView(target, firstPerson, armor == skinArmor, partClone, matchingNodes);
                }
            }
        }

        return results;
    }

    std::optional<FaceNodeInfo> FaceNode(RE::Actor* actor, RE::TESNPC* base)
    {
        std::optional<FaceNodeInfo> result;
        int bestScore{};
        if (actor) {
            if (auto* root = actor->GetFaceNodeSkinned()) {
                RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
                    if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
                    auto* shader = geometry->lightingShaderProp_cast();
                    auto* material = shader ?
                        static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
                    if (!material) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto feature = material->GetFeature();
                    if (feature != RE::BSShaderMaterial::Feature::kFaceGen &&
                        feature != RE::BSShaderMaterial::Feature::kFaceGenRGBTint) {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    const auto* name = geometry->name.c_str();
                    if (!name || name[0] == '\0') return RE::BSVisit::BSVisitControl::kContinue;

                    const auto loweredName = LowerAscii(name);
                    const auto rejectedName = loweredName.contains("hair") ||
                        loweredName.contains("eye") || loweredName.contains("brow") ||
                        loweredName.contains("mouth") || loweredName.contains("teeth") ||
                        loweredName.contains("tongue") || loweredName.contains("body") ||
                        loweredName.contains("hand") || loweredName.contains("feet") ||
                        loweredName.contains("foot");
                    int score = feature == RE::BSShaderMaterial::Feature::kFaceGen ? 100 : 0;
                    FaceNodeInfo info{ .nodeName = name, .object = geometry };
                    if (const auto textureSet = StableTextureSet(material)) {
                        if (const auto* diffuse = textureSet->GetTexturePath(
                                RE::BSTextureSet::Textures::kDiffuse);
                            diffuse && diffuse[0] != '\0') {
                            const auto loweredDiffuse = LowerAscii(diffuse);
                            if (loweredDiffuse.contains("facegendata\\facetint") ||
                                loweredDiffuse.contains("femalehead") ||
                                loweredDiffuse.contains("malehead")) {
                                score = (std::max)(score, 90);
                            }
                        }
                        if (const auto* detail = textureSet->GetTexturePath(
                                RE::BSTextureSet::Textures::kDetailMap);
                            detail && detail[0] != '\0') {
                            info.detailFilename = LowerFilename(detail);
                            score = (std::max)(score, 80);
                        }
                    }
                    if (!rejectedName &&
                        (loweredName.contains("head") || loweredName.contains("face"))) {
                        score = (std::max)(score, 85);
                    }
                    if (!rejectedName && score > bestScore) {
                        bestScore = score;
                        result = std::move(info);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            }
        }
        static_cast<void>(base);
        return result;
    }

    std::string ActiveAddonModelPath(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot, const bool female)
    {
        auto* addon = ActiveAddonRecord(actor, slot);
        if (!addon) return {};
        const auto* rawPath = addon->bipedModels[female ? 1U : 0U].GetModel();
        auto path = rawPath ? std::string{ rawPath } : std::string{};
        std::ranges::replace(path, '/', '\\');
        return LowerAscii(std::move(path));
    }

    std::string ActiveAddonTextureEvidence(RE::Actor* actor,
        const RE::BGSBipedObjectForm::BipedObjectSlot slot, const bool female)
    {
        auto* addon = ActiveAddonRecord(actor, slot);
        const auto sexIndex = female ? 1U : 0U;
        if (!addon || sexIndex >= RE::SEXES::kTotal) return {};
        std::string result;
        const auto append = [&result](RE::BGSTextureSet* textureSet) {
            if (!textureSet) return;
            for (std::size_t index{};
                 index < RE::BSTextureSet::Textures::kUsedTotal; ++index) {
                const auto* path = textureSet->GetTexturePath(
                    static_cast<RE::BSTextureSet::Texture>(index));
                if (!path || path[0] == '\0') continue;
                if (!result.empty()) result.push_back('|');
                result.append(path);
            }
        };
        append(addon->skinTextures[sexIndex]);
        if (const auto* list = addon->skinTextureSwapLists[sexIndex]) {
            for (auto* form : list->forms) {
                append(form && form->GetFormType() == RE::FormType::TextureSet ?
                    static_cast<RE::BGSTextureSet*>(form) : nullptr);
            }
        }
        std::ranges::replace(result, '/', '\\');
        return LowerAscii(std::move(result));
    }
}
