#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bcn
{
    enum class SkinSex : std::uint8_t
    {
        female,
        male
    };

    enum class SkinRace : std::uint8_t
    {
        humanoid,
        argonian,
        khajiit
    };

    // UV/material layouts are deliberately separate from BodySlide morph
    // presets. Two packs can use the same Bethesda filenames while targeting
    // incompatible UVs. Unknown is a real fail-closed state, not "all".
    enum class SkinUvLayout : std::uint8_t
    {
        unknown,
        femaleVanilla,
        cbbe,
        unp,
        ube,
        maleVanilla,
        himbo,
        sam,
        argonian,
        khajiit
    };

    enum class SkinCompatibilityStatus : std::uint8_t
    {
        compatible,
        incompatibleSex,
        incompatibleRace,
        unknownProfileLayout,
        unknownActorLayout,
        incompatibleLayout
    };

    struct SkinCompatibility final
    {
        SkinCompatibilityStatus status{ SkinCompatibilityStatus::unknownProfileLayout };

        [[nodiscard]] constexpr bool Compatible() const noexcept
        {
            return status == SkinCompatibilityStatus::compatible;
        }
    };

    enum class FeetLayerSource : std::uint8_t
    {
        none,
        explicitFeet,
        bodyAtlas
    };

    [[nodiscard]] constexpr body_family::Mask SkinLayoutFamilyMask(
        const SkinUvLayout layout) noexcept
    {
        using body_family::Bit;
        using body_family::Family;
        switch (layout) {
        case SkinUvLayout::femaleVanilla: return Bit(Family::femaleVanilla);
        case SkinUvLayout::cbbe: return Bit(Family::cbbe);
        case SkinUvLayout::unp: return Bit(Family::unp);
        case SkinUvLayout::ube: return Bit(Family::ube);
        case SkinUvLayout::maleVanilla: return Bit(Family::maleVanilla);
        case SkinUvLayout::himbo: return Bit(Family::himbo);
        case SkinUvLayout::sam: return Bit(Family::sam);
        default: return 0U;
        }
    }

    [[nodiscard]] constexpr SkinUvLayout SkinLayoutFromFamilyMask(
        const body_family::Mask families) noexcept
    {
        if (std::popcount(families) != 1) return SkinUvLayout::unknown;
        using body_family::Bit;
        using body_family::Family;
        if (families == Bit(Family::femaleVanilla)) return SkinUvLayout::femaleVanilla;
        if (families == Bit(Family::cbbe)) return SkinUvLayout::cbbe;
        if (families == Bit(Family::unp)) return SkinUvLayout::unp;
        if (families == Bit(Family::ube)) return SkinUvLayout::ube;
        if (families == Bit(Family::maleVanilla)) return SkinUvLayout::maleVanilla;
        if (families == Bit(Family::himbo)) return SkinUvLayout::himbo;
        if (families == Bit(Family::sam)) return SkinUvLayout::sam;
        return SkinUvLayout::unknown;
    }

    [[nodiscard]] constexpr SkinUvLayout BeastSkinLayout(const SkinRace race) noexcept
    {
        if (race == SkinRace::argonian) return SkinUvLayout::argonian;
        if (race == SkinRace::khajiit) return SkinUvLayout::khajiit;
        return SkinUvLayout::unknown;
    }

    [[nodiscard]] constexpr SkinCompatibility EvaluateSkinCompatibility(
        const SkinUvLayout profileLayout, const SkinSex profileSex,
        const SkinRace profileRace, const SkinSex actorSex,
        const SkinRace actorRace, const body_family::Mask actorFamilies) noexcept
    {
        if (profileSex != actorSex) {
            return { SkinCompatibilityStatus::incompatibleSex };
        }
        if (profileRace != actorRace) {
            return { SkinCompatibilityStatus::incompatibleRace };
        }
        if (profileLayout == SkinUvLayout::unknown) {
            return { SkinCompatibilityStatus::unknownProfileLayout };
        }
        if (profileRace != SkinRace::humanoid) {
            return { profileLayout == BeastSkinLayout(profileRace) ?
                SkinCompatibilityStatus::compatible :
                SkinCompatibilityStatus::incompatibleLayout };
        }
        const auto profileFamily = SkinLayoutFamilyMask(profileLayout);
        if (profileFamily == 0U) {
            return { SkinCompatibilityStatus::unknownProfileLayout };
        }
        if (std::popcount(actorFamilies) != 1) {
            return { SkinCompatibilityStatus::unknownActorLayout };
        }
        return { (profileFamily & actorFamilies) != 0U ?
            SkinCompatibilityStatus::compatible :
            SkinCompatibilityStatus::incompatibleLayout };
    }

    // Only a verified humanoid layout may inherit its body atlas for feet.
    // Beast and unknown layouts must supply an explicit feet atlas.
    [[nodiscard]] constexpr FeetLayerSource ResolveFeetLayerSource(
        const SkinUvLayout layout, const SkinRace race,
        const std::size_t bodyLayerCount, const std::size_t feetLayerCount) noexcept
    {
        if (feetLayerCount != 0U) return FeetLayerSource::explicitFeet;
        if (race != SkinRace::humanoid || layout == SkinUvLayout::unknown ||
            bodyLayerCount == 0U) {
            return FeetLayerSource::none;
        }
        return FeetLayerSource::bodyAtlas;
    }

    // RaceMenu's skin-slot API can visit several ArmorAddons. UBE explicitly
    // shares one body atlas across those surfaces; all other layouts require
    // exact Armor+ArmorAddon+node targets.
    [[nodiscard]] constexpr bool AllowsBroadSkinSlotFallback(
        const SkinUvLayout layout) noexcept
    {
        return layout == SkinUvLayout::ube;
    }

    [[nodiscard]] constexpr std::string_view SkinUvLayoutName(
        const SkinUvLayout layout) noexcept
    {
        switch (layout) {
        case SkinUvLayout::femaleVanilla: return "female-vanilla";
        case SkinUvLayout::cbbe: return "cbbe";
        case SkinUvLayout::unp: return "unp";
        case SkinUvLayout::ube: return "ube";
        case SkinUvLayout::maleVanilla: return "male-vanilla";
        case SkinUvLayout::himbo: return "himbo";
        case SkinUvLayout::sam: return "sam";
        case SkinUvLayout::argonian: return "argonian";
        case SkinUvLayout::khajiit: return "khajiit";
        default: return "unknown";
        }
    }
}
