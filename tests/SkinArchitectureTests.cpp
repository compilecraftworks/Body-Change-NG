#include "BodyChangeNG/AppearanceWork.h"
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

    std::cout << "Skin architecture tests passed\n";
    return 0;
}
