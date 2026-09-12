#pragma once

#include "BodyChangeNG/FutanariRouting.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinProfiles.h"

#include <RE/B/BGSBipedObjectForm.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RE
{
    class Actor;
    class BSGeometry;
    class BSLightingShaderMaterialBase;
    class BSTextureSet;
    class NiAVObject;
    class TESNPC;
    class TESObjectARMA;
    class TESObjectARMO;
}

namespace bcn::skin_target
{
    inline constexpr auto kUbeBodySlot =
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight;
    inline constexpr auto kSosMaleGenitalSlot =
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary;

    struct LoadedPartView final
    {
        bool firstPerson{};
        bool actorSkinArmor{};
        RE::NiAVObject* object{};
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
        futanari::AddonKind addonKind{ futanari::AddonKind::none };
        std::optional<FutanariSkinType> type;
        std::vector<LoadedPartTarget> targets;
    };

    struct FaceNodeInfo final
    {
        std::string nodeName;
        std::string detailFilename;
        RE::NiAVObject* object{};
    };

    [[nodiscard]] RE::BSTextureSet* StableTextureSet(
        RE::BSLightingShaderMaterialBase* material);
    [[nodiscard]] bool IsSkinGeometry(RE::BSGeometry* geometry, bool actorSkinArmor);
    [[nodiscard]] std::string_view GeometryDiffuseTexture(RE::BSGeometry* geometry);
    [[nodiscard]] std::vector<LoadedPartTarget> FindLoadedPartTargets(
        RE::Actor* actor, RE::BGSBipedObjectForm::BipedObjectSlot slot,
        skin_geometry::BodySelection selection = skin_geometry::BodySelection::all,
        bool allowExplicitLimbNode = false);
    [[nodiscard]] LoadedFutanariRoute FindLoadedFutanariRoute(
        RE::Actor* actor);
    [[nodiscard]] std::optional<FaceNodeInfo> FaceNode(RE::Actor* actor, RE::TESNPC* base);
    [[nodiscard]] std::string ActiveAddonModelPath(
        RE::Actor* actor, RE::BGSBipedObjectForm::BipedObjectSlot slot, bool female);
    [[nodiscard]] std::string ActiveAddonTextureEvidence(
        RE::Actor* actor, RE::BGSBipedObjectForm::BipedObjectSlot slot, bool female);
}
