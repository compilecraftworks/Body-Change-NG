#pragma once

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"

#include <cstdint>

namespace bcn::body_morph_policy
{
    enum class FemaleFamily : std::uint8_t
    {
        none,
        cbbe3ba,
        bhunpUnp,
        ube
    };

    // Runtime actor evidence wins. Preset metadata is a safe fallback only
    // when it identifies exactly one supported family. An ambiguous
    // CBBE+UBE result must never receive both families' anatomical sliders.
    [[nodiscard]] constexpr FemaleFamily ResolveFemaleFamily(
        const body_family::Mask actorFamily,
        const body_family::Mask presetFamily = 0U) noexcept
    {
        const auto supported = body_family::Bit(body_family::Family::cbbe) |
            body_family::Bit(body_family::Family::unp) |
            body_family::Bit(body_family::Family::ube);
        auto candidates = actorFamily & supported;
        if (candidates == 0U) candidates = presetFamily & supported;
        if (candidates == body_family::Bit(body_family::Family::cbbe)) {
            return FemaleFamily::cbbe3ba;
        }
        if (candidates == body_family::Bit(body_family::Family::unp)) {
            return FemaleFamily::bhunpUnp;
        }
        if (candidates == body_family::Bit(body_family::Family::ube)) {
            return FemaleFamily::ube;
        }
        return FemaleFamily::none;
    }

    // CBBE/3BA and BHUNP/UNP use different verified slider dialects; the
    // caller selects the matching dialect. UBE uses materially different
    // anatomy and non-zero body defaults, so guessing targets can visibly
    // damage its shape.
    [[nodiscard]] constexpr bool SupportsOutfitCorrection(const FemaleFamily family) noexcept
    {
        return family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp;
    }

    // Anatomy randomization is an NPC-only feature. Both supported families
    // have an explicit slider dialect; UBE and ambiguous actors fail closed.
    [[nodiscard]] constexpr bool SupportsNpcAnatomyRandomization(
        const FemaleFamily family, const bool player) noexcept
    {
        return !player &&
            (family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp);
    }

    // OBody NG's authored policy mixes target corrections and additive offsets.
    // Only targets subtract the preset key; offsets must not become targets.
    // readOwned must read BCNG's preset key, never RaceMenu's aggregate.
    template <class ReadOwned, class Write>
    void GenerateOutfitMorphs(const FemaleFamily family, const float weight,
        const bool nipples, ReadOwned&& readOwned, Write&& write)
    {
        const auto derive = [&](const char* name, const float target) {
            write(name, racemenu::OutfitTargetCorrection(target, readOwned(name)));
        };
        const auto fixed = [&](const char* name, const float low, const float high) {
            write(name, low + (high - low) * weight);
        };
        if (family == FemaleFamily::cbbe3ba) {
            derive("BreastSideShape", 0.0F);
            derive("BreastUnderDepth", 0.0F);
            derive("BreastCleavage", 1.0F);
            fixed("BreastGravity2", -0.1F, -0.05F);
            fixed("BreastTopSlope", -0.2F, -0.35F);
            fixed("BreastsTogether", 0.3F, 0.35F);
            fixed("Breasts", -0.05F, -0.05F);
            fixed("BreastHeight", 0.15F, 0.15F);
            derive("ButtDimples", 0.0F);
            derive("ButtUnderFold", 0.0F);
            fixed("AppleCheeks", -0.05F, -0.05F);
            fixed("Butt", -0.05F, -0.05F);
            derive("Clavicle_v2", 0.0F);
            derive("NavelEven", 1.0F);
            derive("HipCarved", 0.0F);
            if (nipples) {
                derive("NippleDip", 0.0F);
                derive("NippleTip", 0.0F);
                derive("NipplePuffy_v2", 0.0F);
                derive("AreolaSize", -0.3F);
                derive("NipBGone", 1.0F);
                fixed("NippleDistance", 0.05F, 0.08F);
                fixed("NippleDown", 0.0F, -0.1F);
                derive("NipplePerkManga", -0.25F);
            }
        } else if (family == FemaleFamily::bhunpUnp) {
            // Retain the verified BHUNP/UNP dialect, not CBBE's v2 names.
            derive("BreastSideShape", 0.0F);
            derive("BreastUnderDepth", 0.0F);
            derive("BreastCleavage", 1.0F);
            fixed("BreastGravity", -0.1F, -0.05F);
            fixed("Breasts", -0.05F, -0.05F);
            fixed("BreastHeight", 0.15F, 0.15F);
            derive("ButtDimples", 0.0F);
            derive("ButtUnderFold", 0.0F);
            fixed("AppleCheeks", -0.05F, -0.05F);
            fixed("Butt", -0.05F, -0.05F);
            derive("Clavicle", 0.0F);
            derive("NavelEven", 1.0F);
            derive("HipCarved", 0.0F);
            if (nipples) {
                derive("NippleTip", 0.0F);
                derive("NippleErection", 0.0F);
                derive("NippleInverted", 0.0F);
                derive("NipplePuffyAreola", 0.0F);
                derive("NippleAreola", -0.3F);
                fixed("NippleDistance", 0.05F, 0.08F);
                fixed("NippleDown", 0.0F, -0.1F);
                derive("NipplePerkManga", -0.25F);
            }
        }
    }
}
