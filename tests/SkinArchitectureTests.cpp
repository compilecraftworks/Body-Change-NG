#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/AppearanceEventPolicy.h"
#include "BodyChangeNG/NativeSkinRouting.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinLayout.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace NativeSlot = bcn::native_skin::slot_mask;

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
}

int main()
{
    using bcn::SkinCompatibilityStatus;
    using bcn::SkinLayout;
    using bcn::SkinRace;
    using bcn::SkinSex;
    using bcn::SkinUvLayout;
    using bcn::appearance::WorkChannel;
    using bcn::body_family::Bit;
    using bcn::body_family::Family;

    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::cbbe)).Compatible(),
            "Legacy was rejected for a CBBE actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::unp)).Compatible(),
            "Legacy was rejected for a UNP actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::ube)).status == SkinCompatibilityStatus::incompatibleLayout,
            "Legacy leaked into a UBE actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::unknown, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo)).status == SkinCompatibilityStatus::unknownProfileLayout,
            "an ambiguous skin profile did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo) | Bit(Family::sam)).status ==
                SkinCompatibilityStatus::unknownActorLayout,
            "an actor with conflicting layout evidence did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo)).Compatible() &&
            bcn::ResolveRuntimeSkinUvLayout(SkinLayout::legacy,
                Bit(Family::himbo)) == SkinUvLayout::himbo &&
            bcn::ResolveRuntimeSkinUvLayout(SkinLayout::legacy,
                Bit(Family::sam)) == SkinUvLayout::sam,
            "Legacy male classification lost exact runtime BodyFamily routing")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::argonian, SkinSex::female,
            SkinRace::argonian, SkinSex::female, SkinRace::argonian, 0U).Compatible(),
            "an exact Argonian layout incorrectly depended on a humanoid body family")) return 1;

    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::legacy, SkinRace::humanoid,
            4U, 0U) == bcn::FeetLayerSource::bodyAtlas,
            "a verified humanoid layout lost its shared feet atlas")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::argonian, SkinRace::argonian,
            4U, 0U) == bcn::FeetLayerSource::none,
            "Argonian feet incorrectly inherited the body DDS")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::khajiit, SkinRace::khajiit,
            4U, 1U) == bcn::FeetLayerSource::explicitFeet,
            "an explicit Khajiit feet atlas was not authoritative")) return 1;

    if (!Require(bcn::AllowsBroadSkinSlotFallback(SkinLayout::ube) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinLayout::legacy) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinLayout::unknown),
            "broad skin-slot routing escaped the UBE shared-atlas boundary")) return 1;
    using NativeRole = bcn::native_skin::TextureRole;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::body, "actors\\character\\female\\femalebody_1.dds",
            SkinUvLayout::cbbe) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::hands, "femalehands_1.dds",
                SkinUvLayout::cbbe) == NativeRole::hands &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::feet, "femalebody_1.dds",
                SkinUvLayout::unp) == NativeRole::feet,
            "native ARMA slot routing crossed body, hand, or foot TXST roles")) return 1;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::body,
            "textures\\actors\\character\\female\\femalebody_etc_v2_1.dds",
            SkinUvLayout::cbbe) == NativeRole::cbbeGenitalAnal &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::body,
                "textures\\BakaUNP\\VaginalAnalCanal2.dds",
                SkinUvLayout::unp) == NativeRole::unpGenitalAnal,
            "native skin texture-swap lists lost the 3BA or BHUNP auxiliary atlas")) return 1;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::ubeBody, "!UBE\\Body\\femalebody_1_d.dds",
            SkinUvLayout::ube) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                0x00800174U, {}, SkinUvLayout::ube) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::ubeBody, "unknown.dds",
                SkinUvLayout::cbbe) == NativeRole::unmanaged,
            "UBE slot 53 escaped its explicit layout boundary")) return 1;
    if (!Require(bcn::native_skin::ShouldSynthesizeUbeTextureSet(
            SkinUvLayout::ube, NativeRole::body,
            "!UBE\\Body\\femalebody_tangent_1.nif", false, false) &&
            bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::hands,
                "!UBE/Hands/femalehands_tangent_1.nif", false, false) &&
            bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::feet,
                "!UBE\\Feet\\femalefeet_tangent_1.nif", false, false) &&
            !bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::body,
                "Custom\\Body\\body.nif", false, false) &&
            !bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::body,
                "!UBE\\Body\\femalebody_tangent_1.nif", true, false) &&
            bcn::native_skin::UbeBaselineTexturePath(0U) ==
                "!UBE\\Body\\femalebody_1_d.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(1U) ==
                "!UBE\\Body\\femalebody_1_n.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(3U) ==
                "!UBE\\Body\\femalebody_1_sk.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(2U).empty(),
            "UBE's missing NAM1 synthesis escaped the verified naked graph or guessed an undeclared channel")) return 1;
    constexpr auto multiPartMask = NativeSlot::body | NativeSlot::hands | NativeSlot::feet;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            multiPartMask, "femalehands_1.dds", SkinUvLayout::cbbe) == NativeRole::hands &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "femalefeet_1.dds", SkinUvLayout::cbbe) == NativeRole::feet &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "femalebody_1.dds", SkinUvLayout::cbbe) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "renamed.dds", SkinUvLayout::cbbe) == NativeRole::unmanaged,
            "a multi-slot ARMA ignored exact source TXST evidence or guessed an ambiguous role")) return 1;
    const auto conventionalRequired = bcn::native_skin::RequiredRoleMask(
        false, true, true, true, false, false);
    const auto conventionalAvailable = bcn::native_skin::RoleBit(NativeRole::body) |
        bcn::native_skin::RoleBit(NativeRole::hands) |
        bcn::native_skin::RoleBit(NativeRole::feet);
    const auto missingHands = conventionalAvailable &
        static_cast<bcn::native_skin::TextureRoleMask>(~bcn::native_skin::RoleBit(NativeRole::hands));
    const auto ubeRequired = bcn::native_skin::RequiredRoleMask(
        true, true, true, true, false, false);
    const auto bodyOnlyRequired = bcn::native_skin::RequiredRoleMask(
        false, true, false, false, false, false);
    if (!Require(bcn::native_skin::CoversRequiredRoles(
            conventionalAvailable, conventionalRequired) &&
            !bcn::native_skin::CoversRequiredRoles(missingHands, conventionalRequired) &&
            bcn::native_skin::CoversRequiredRoles(
                bcn::native_skin::RoleBit(NativeRole::body), bodyOnlyRequired) &&
            bcn::native_skin::CoversRequiredRoles(
                bcn::native_skin::RoleBit(NativeRole::body), ubeRequired),
            "native TXST coverage required an undeclared partial-pack part, accepted a missing declared limb, or rejected UBE's shared atlas")) return 1;
    using GraphAction = bcn::native_skin::GraphAction;
    if (!Require(bcn::native_skin::ResolveGraphAction(
            true, true, true, true, false) == GraphAction::reuse &&
            bcn::native_skin::ResolveGraphAction(
                true, false, true, false, false) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, false, false, true) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, true, false, false, true) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, false, false, false) == GraphAction::rejectForeignOwner,
            "native skin ownership failed to isolate RSV/unowned-source rebasing from arbitrary providers")) return 1;
    using SharedBaseAction = bcn::native_skin::SharedBaseAction;
    if (!Require(bcn::native_skin::ResolveSharedBaseAction(
            0U, 0x20U, {}, "skin-a") == SharedBaseAction::keepOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, {}, "skin-a") == SharedBaseAction::transferOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", "skin-a") == SharedBaseAction::keepOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", {}) == SharedBaseAction::rejectConflict &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", "skin-b") == SharedBaseAction::rejectConflict,
            "ActorBase ownership could not transfer at Default or admitted contradictory active requests")) return 1;

    using bcn::appearance::Event;
    using bcn::appearance::Feature;
    if (!Require(!bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::equipmentChanged) &&
            !bcn::appearance::NeedsReconcile(Feature::faceTint, Event::equipmentChanged) &&
            !bcn::appearance::NeedsReconcile(Feature::rsvFaceBridge, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::maleGenitalAddon, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::outfitMorph, Event::equipmentChanged),
            "equipment events regained ownership of native base skin or lost external-addon handling")) return 1;
    if (!Require(!bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::rsvFaceBridge, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::maleGenitalAddon, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::outfitMorph, Event::actor3DAttached),
            "3D attach handling lost an external addon or started repainting native base skin")) return 1;
    if (!Require(bcn::appearance::NeedsReconcile(Feature::rsvFaceBridge, Event::niNodeUpdated) &&
            !bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::niNodeUpdated) &&
            !bcn::appearance::NeedsReconcile(Feature::faceTint, Event::niNodeUpdated) &&
            !bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::niNodeUpdated),
            "the RSV face bridge leaked into unrelated appearance features")) return 1;

    constexpr std::array channels{
        WorkChannel::actorReconcile,
        WorkChannel::initialDistribution,
        WorkChannel::equipmentReconcile,
        WorkChannel::raceMenuRestore,
        WorkChannel::equipmentVerify,
        WorkChannel::rsvFaceReconcile,
        WorkChannel::legacySkinWatchdog,
        WorkChannel::bodyPreview,
        WorkChannel::bodyCommit,
        WorkChannel::outfitRefit,
        WorkChannel::bodyPreviewCleanup,
        WorkChannel::skinApply,
        WorkChannel::maleGenitalSkinApply,
        WorkChannel::futanariSkinApply,
        WorkChannel::tintApply,
        WorkChannel::legacyBodySkinCleanup
    };
    for (std::size_t left{}; left < channels.size(); ++left) {
        for (std::size_t right = left + 1U; right < channels.size(); ++right) {
            if (!Require(bcn::appearance::ChannelValue(channels[left]) !=
                    bcn::appearance::ChannelValue(channels[right]),
                    "two appearance operations share a replacement channel")) return 1;
        }
    }
    if (!Require(WorkChannel::tintApply != WorkChannel::futanariSkinApply &&
            bcn::appearance::IsInteractiveChannel(WorkChannel::tintApply) &&
            !bcn::appearance::IsInteractiveChannel(WorkChannel::legacyBodySkinCleanup) &&
            !bcn::appearance::IsInteractiveChannel(WorkChannel::equipmentVerify),
            "appearance channel semantics regressed")) return 1;

    bcn::SkinProfile cbbeProfile;
    cbbeProfile.layout = SkinLayout::legacy;
    cbbeProfile.race = SkinRace::humanoid;
    cbbeProfile.body = { { 0U, "base-body.dds" }, { 1U, "base-body_n.dds" } };
    cbbeProfile.hands = { { 0U, "base-hands.dds" } };
    cbbeProfile.face = { { 0U, "base-face.dds" } };
    cbbeProfile.elderBody = { { 1U, "elder-body_n.dds" } };
    cbbeProfile.cbbeGenitalAnal = { { 0U, "cbbe-genital.dds" } };
    cbbeProfile.unpGenitalAnal = { { 0U, "unp-genital.dds" } };
    const auto elderPlan = bcn::skin_plan::Build(cbbeProfile, {
        .elder = true,
        .bodyFamily = Bit(Family::cbbe)
    });
    if (!Require(elderPlan.body.size() == 2U &&
            elderPlan.body[1].path == "elder-body_n.dds" &&
            elderPlan.hands.size() == 1U && elderPlan.face.size() == 1U &&
            elderPlan.feet.size() == 2U && elderPlan.feet[1].path == "elder-body_n.dds" &&
            elderPlan.runtimeUvLayout == SkinUvLayout::cbbe &&
            elderPlan.cbbeGenitalAnal.size() == 1U && elderPlan.unpGenitalAnal.size() == 1U &&
            bcn::skin_plan::RoutesGenitalAnalAtlas(elderPlan, SkinUvLayout::cbbe) &&
            !bcn::skin_plan::RoutesGenitalAnalAtlas(elderPlan, SkinUvLayout::unp),
            "the equipment-independent plan lost a body, hand, foot, or face layer")) return 1;

    const auto unpPlan = bcn::skin_plan::Build(cbbeProfile, {
        .bodyFamily = Bit(Family::unp)
    });
    if (!Require(unpPlan.runtimeUvLayout == SkinUvLayout::unp &&
            unpPlan.cbbeGenitalAnal.size() == 1U && unpPlan.unpGenitalAnal.size() == 1U &&
            !bcn::skin_plan::RoutesGenitalAnalAtlas(unpPlan, SkinUvLayout::cbbe) &&
            bcn::skin_plan::RoutesGenitalAnalAtlas(unpPlan, SkinUvLayout::unp),
            "Legacy genital/anal assets were incorrectly used as catalog classification")) return 1;

    bcn::SkinProfile beastProfile;
    beastProfile.layout = SkinLayout::argonian;
    beastProfile.race = SkinRace::argonian;
    beastProfile.body = { { 0U, "argonian-body.dds" } };
    const auto beastPlan = bcn::skin_plan::Build(beastProfile, {});
    if (!Require(beastPlan.beastTail && beastPlan.feet.empty(),
            "the planner leaked an Argonian body DDS into feet")) return 1;

    bcn::SkinProfile ubeProfile;
    ubeProfile.layout = SkinLayout::ube;
    ubeProfile.race = SkinRace::humanoid;
    ubeProfile.body = { { 0U, "ube-body.dds" } };
    ubeProfile.hands = { { 0U, "wrong-hands.dds" } };
    const auto ubePlan = bcn::skin_plan::Build(ubeProfile, {
        .bodyFamily = Bit(Family::ube)
    });
    if (!Require(ubePlan.broadSharedAtlas && ubePlan.hands.size() == 1U &&
            ubePlan.hands.front().path == "ube-body.dds" &&
            ubePlan.feet.size() == 1U && ubePlan.feet.front().path == "ube-body.dds",
            "UBE did not remain one explicit shared-atlas plan")) return 1;

    cbbeProfile.vampireFace = { { 0U, "vampire-face.dds" } };
    cbbeProfile.faceDetails = {
        { 3U, "femalehead_frek.dds" }, { 3U, "femalehead_rough.dds" }
    };
    const auto facePlan = bcn::skin_plan::Build(cbbeProfile, {
        .vampire = true,
        .faceDetailFilename = "textures\\actors\\character\\femalehead_rough.dds",
        .bodyFamily = Bit(Family::cbbe)
    });
    if (!Require(facePlan.requiresFaceGeometry && facePlan.face.size() == 2U &&
            facePlan.face[0].path == "vampire-face.dds" &&
            facePlan.face[1].path == "femalehead_rough.dds",
            "face specificity or detail matching escaped the planner")) return 1;

    std::cout << "Skin architecture tests passed\n";
    return 0;
}
