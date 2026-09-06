#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinLayout.h"

#include <array>
#include <cstdint>
#include <iostream>

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
    using bcn::SkinRace;
    using bcn::SkinSex;
    using bcn::SkinUvLayout;
    using bcn::LimbSkinSlotRoute;
    using bcn::appearance::WorkChannel;
    using bcn::body_family::Bit;
    using bcn::body_family::Family;

    if (!Require(bcn::EvaluateSkinCompatibility(SkinUvLayout::cbbe, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::cbbe)).Compatible(),
            "CBBE was rejected for an exact CBBE actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinUvLayout::cbbe, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::unp)).status == SkinCompatibilityStatus::incompatibleLayout,
            "CBBE leaked into a UNP actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinUvLayout::unknown, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo)).status == SkinCompatibilityStatus::unknownProfileLayout,
            "an ambiguous skin profile did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinUvLayout::himbo, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo) | Bit(Family::sam)).status ==
                SkinCompatibilityStatus::unknownActorLayout,
            "an actor with conflicting layout evidence did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinUvLayout::argonian, SkinSex::female,
            SkinRace::argonian, SkinSex::female, SkinRace::argonian, 0U).Compatible(),
            "an exact Argonian layout incorrectly depended on a humanoid body family")) return 1;

    if (!Require(bcn::ResolveFeetLayerSource(SkinUvLayout::cbbe, SkinRace::humanoid,
            4U, 0U) == bcn::FeetLayerSource::bodyAtlas,
            "a verified humanoid layout lost its shared feet atlas")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinUvLayout::argonian, SkinRace::argonian,
            4U, 0U) == bcn::FeetLayerSource::none,
            "Argonian feet incorrectly inherited the body DDS")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinUvLayout::khajiit, SkinRace::khajiit,
            4U, 1U) == bcn::FeetLayerSource::explicitFeet,
            "an explicit Khajiit feet atlas was not authoritative")) return 1;

    if (!Require(bcn::AllowsBroadSkinSlotFallback(SkinUvLayout::ube) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinUvLayout::cbbe) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinUvLayout::unp) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinUvLayout::unknown),
            "broad skin-slot routing escaped the UBE shared-atlas boundary")) return 1;
    if (!Require(bcn::ResolveLimbSkinSlotRoute(SkinUvLayout::cbbe, 0U) ==
                LimbSkinSlotRoute::persistentOnly &&
            bcn::ResolveLimbSkinSlotRoute(SkinUvLayout::unp, 1U) ==
                LimbSkinSlotRoute::persistentAndExact &&
            bcn::ResolveLimbSkinSlotRoute(SkinUvLayout::ube, 0U) ==
                LimbSkinSlotRoute::broadLive &&
            bcn::ResolveLimbSkinSlotRoute(SkinUvLayout::unknown, 0U) ==
                LimbSkinSlotRoute::none,
            "equipment-independent limb persistence escaped its layout boundary")) return 1;

    constexpr std::array channels{
        WorkChannel::actorReconcile,
        WorkChannel::initialDistribution,
        WorkChannel::equipmentReconcile,
        WorkChannel::raceMenuRestore,
        WorkChannel::equipmentVerify,
        WorkChannel::distributedSkinApply,
        WorkChannel::rsvFaceReconcile,
        WorkChannel::legacySkinWatchdog,
        WorkChannel::bodyPreview,
        WorkChannel::bodyCommit,
        WorkChannel::outfitRefit,
        WorkChannel::bodyPreviewCleanup,
        WorkChannel::skinApply,
        WorkChannel::futanariSkinApply,
        WorkChannel::tintApply
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
            !bcn::appearance::IsInteractiveChannel(WorkChannel::equipmentVerify),
            "appearance channel semantics regressed")) return 1;

    bcn::SkinProfile cbbeProfile;
    cbbeProfile.uvLayout = SkinUvLayout::cbbe;
    cbbeProfile.race = SkinRace::humanoid;
    cbbeProfile.body = { { 0U, "base-body.dds" }, { 1U, "base-body_n.dds" } };
    cbbeProfile.hands = { { 0U, "base-hands.dds" } };
    cbbeProfile.face = { { 0U, "base-face.dds" } };
    cbbeProfile.elderBody = { { 1U, "elder-body_n.dds" } };
    const auto elderPlan = bcn::skin_plan::Build(cbbeProfile, { .elder = true });
    if (!Require(elderPlan.body.size() == 2U &&
            elderPlan.body[1].path == "elder-body_n.dds" &&
            elderPlan.hands.size() == 1U && elderPlan.face.size() == 1U &&
            elderPlan.feet.size() == 2U && elderPlan.feet[1].path == "elder-body_n.dds",
            "the equipment-independent plan lost a body, hand, foot, or face layer")) return 1;

    bcn::SkinProfile beastProfile;
    beastProfile.uvLayout = SkinUvLayout::argonian;
    beastProfile.race = SkinRace::argonian;
    beastProfile.body = { { 0U, "argonian-body.dds" } };
    const auto beastPlan = bcn::skin_plan::Build(beastProfile, {});
    if (!Require(beastPlan.beastTail && beastPlan.feet.empty(),
            "the planner leaked an Argonian body DDS into feet")) return 1;

    bcn::SkinProfile ubeProfile;
    ubeProfile.uvLayout = SkinUvLayout::ube;
    ubeProfile.race = SkinRace::humanoid;
    ubeProfile.body = { { 0U, "ube-body.dds" } };
    ubeProfile.hands = { { 0U, "wrong-hands.dds" } };
    const auto ubePlan = bcn::skin_plan::Build(ubeProfile, {});
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
        .faceDetailFilename = "textures\\actors\\character\\femalehead_rough.dds"
    });
    if (!Require(facePlan.requiresFaceGeometry && facePlan.face.size() == 2U &&
            facePlan.face[0].path == "vampire-face.dds" &&
            facePlan.face[1].path == "femalehead_rough.dds",
            "face specificity or detail matching escaped the planner")) return 1;

    std::cout << "Skin architecture tests passed\n";
    return 0;
}
