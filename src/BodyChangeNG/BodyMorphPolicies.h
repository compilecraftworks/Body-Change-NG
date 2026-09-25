#pragma once

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"

#include <cstdint>
#include <array>

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

    // UBE uses a separate additive recipe, never translated 3BA sliders.
    [[nodiscard]] constexpr bool SupportsOutfitCorrection(const FemaleFamily family) noexcept
    {
        return family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp ||
            family == FemaleFamily::ube;
    }

    // Player and NPC use the same eligibility. This selects only the original
    // recipe; UBE has independent anatomy recipes and must never enter it.
    [[nodiscard]] constexpr bool UsesConventionalAnatomyRecipe(const FemaleFamily family) noexcept
    {
        return family == FemaleFamily::cbbe3ba || family == FemaleFamily::bhunpUnp;
    }

    struct UbeOutfitDelta
    {
        const char* name;
        float lowPercent;
        float highPercent;
        bool nipple;
    };

    // Fit to Thicc v1: authored Pushup minus Nude, in BodySlide percent.
    // These are offsets, not targets or values to subtract from the actor.
    // Source/provenance and the complete comparison are in the morph audit.
    // NipplesShowUp is omitted in Nude: its reference UBE OSP default is 100.
    // That fact belongs only to this fixed recipe, NOT the runtime XML parser.
    inline constexpr std::array ubeOutfitDeltas{
        UbeOutfitDelta{ "Big_SaggyBreasts", 0.F, -5.F, false },
        UbeOutfitDelta{ "BreastCenterGapLowerHeight n|p", -15.F, 0.F, false },
        UbeOutfitDelta{ "BreastCenterGapWidth n|p", -80.F, -80.F, false },
        UbeOutfitDelta{ "BreastsCupSag n|p", -40.F, -20.F, false },
        UbeOutfitDelta{ "BreastsPositionWidth n|p", -30.F, -40.F, false },
        UbeOutfitDelta{ "BreastsRotate_Y", -10.F, 5.F, false },
        UbeOutfitDelta{ "BreastsTBD", 5.F, 5.F, false },
        UbeOutfitDelta{ "BreastUpperCurve n|p", 0.F, -10.F, false },
        UbeOutfitDelta{ "NEW_NippleCircularCrease_Uv_fix", -20.F, -20.F, true },
        UbeOutfitDelta{ "NippleCircularCrease", -20.F, -20.F, true },
        UbeOutfitDelta{ "NippleDispertionSides n|p", 0.F, 10.F, true },
        UbeOutfitDelta{ "NippleHeight n|p", 20.F, 30.F, true },
        UbeOutfitDelta{ "NipplesPerkiness", -10.F, -30.F, true },
        UbeOutfitDelta{ "NipplesShowUp", -100.F, -100.F, true }
    };

    // OBody NG's authored policy mixes target corrections and additive offsets.
    // Only targets subtract the preset key; offsets must not become targets.
    // readOwned must read BCNG's preset key, never RaceMenu's aggregate.
    template <class ReadOwned, class Write>
    void GenerateOutfitMorphs(const FemaleFamily family, const float weight,
        const bool nipples, ReadOwned&& readOwned, Write&& write)
    {
        if (family == FemaleFamily::ube) {
            for (const auto& delta : ubeOutfitDeltas) {
                if (delta.nipple && !nipples) continue;
                const auto low = delta.lowPercent / 100.0F;
                const auto high = delta.highPercent / 100.0F;
                write(delta.name, low + (high - low) * weight);
            }
            return;
        }
        const auto derive = [&](const char* name, const float target) {
            write(name, racemenu::OutfitTargetCorrection(target, readOwned(name)));
        };
        const auto fixed = [&](const char* name, const float low, const float high) {
            write(name, low + (high - low) * weight);
        };
        if (SupportsOutfitCorrection(family)) {
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
        }
    }
}
