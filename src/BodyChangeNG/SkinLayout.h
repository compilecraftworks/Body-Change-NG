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

    // Catalog classification is intentionally independent of the installed
    // BodySlide family. Every conventional humanoid pack is one Legacy layout;
    // only the explicit !UBE namespace is UBE. The actor's runtime BodyFamily
    // chooses the concrete material route at apply time.
    enum class SkinLayout : std::uint8_t
    {
        unknown,
        legacy,
        ube,
        argonian,
        khajiit
    };

    // Concrete runtime layout used only by material/geometry routing. It is
    // derived from SkinLayout + the actor's BodyFamily and is never used to
    // classify or identify a catalog entry.
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

    [[nodiscard]] constexpr body_family::Mask LegacyHumanoidFamilies() noexcept
    {
        return (body_family::kFemaleFamilies &
                ~body_family::Bit(body_family::Family::ube)) |
            body_family::kMaleFamilies;
    }

    [[nodiscard]] constexpr body_family::Mask SkinLayoutFamilyMask(
        const SkinLayout layout) noexcept
    {
        switch (layout) {
        case SkinLayout::legacy: return LegacyHumanoidFamilies();
        case SkinLayout::ube:
            return body_family::Bit(body_family::Family::ube);
        default: return 0U;
        }
    }

    [[nodiscard]] constexpr SkinLayout SkinLayoutFromFamilyMask(
        const body_family::Mask families) noexcept
    {
        using body_family::Bit;
        using body_family::Family;
        if (families != 0U && (families & ~LegacyHumanoidFamilies()) == 0U) {
            return SkinLayout::legacy;
        }
        if (families == Bit(Family::ube)) return SkinLayout::ube;
        return SkinLayout::unknown;
    }

    [[nodiscard]] constexpr SkinLayout BeastSkinLayout(const SkinRace race) noexcept
    {
        if (race == SkinRace::argonian) return SkinLayout::argonian;
        if (race == SkinRace::khajiit) return SkinLayout::khajiit;
        return SkinLayout::unknown;
    }

    [[nodiscard]] constexpr SkinUvLayout ResolveRuntimeSkinUvLayout(
        const SkinLayout layout, const body_family::Mask actorFamilies) noexcept
    {
        using body_family::Bit;
        using body_family::Family;
        if (layout == SkinLayout::argonian) return SkinUvLayout::argonian;
        if (layout == SkinLayout::khajiit) return SkinUvLayout::khajiit;
        if (std::popcount(actorFamilies) != 1) return SkinUvLayout::unknown;
        switch (layout) {
        case SkinLayout::legacy:
            if (actorFamilies == Bit(Family::femaleVanilla)) return SkinUvLayout::femaleVanilla;
            if (actorFamilies == Bit(Family::cbbe)) return SkinUvLayout::cbbe;
            if (actorFamilies == Bit(Family::unp)) return SkinUvLayout::unp;
            if (actorFamilies == Bit(Family::maleVanilla)) return SkinUvLayout::maleVanilla;
            if (actorFamilies == Bit(Family::himbo)) return SkinUvLayout::himbo;
            if (actorFamilies == Bit(Family::sam)) return SkinUvLayout::sam;
            break;
        case SkinLayout::ube:
            if (actorFamilies == Bit(Family::ube)) return SkinUvLayout::ube;
            break;
        default:
            break;
        }
        return SkinUvLayout::unknown;
    }

    [[nodiscard]] constexpr SkinCompatibility EvaluateSkinCompatibility(
        const SkinLayout profileLayout, const SkinSex profileSex,
        const SkinRace profileRace, const SkinSex actorSex,
        const SkinRace actorRace, const body_family::Mask actorFamilies) noexcept
    {
        if (profileSex != actorSex) {
            return { SkinCompatibilityStatus::incompatibleSex };
        }
        if (profileRace != actorRace) {
            return { SkinCompatibilityStatus::incompatibleRace };
        }
        if (profileLayout == SkinLayout::unknown) {
            return { SkinCompatibilityStatus::unknownProfileLayout };
        }
        if (profileRace != SkinRace::humanoid) {
            return { profileLayout == BeastSkinLayout(profileRace) ?
                SkinCompatibilityStatus::compatible :
                SkinCompatibilityStatus::incompatibleLayout };
        }
        if (std::popcount(actorFamilies) != 1) {
            return { SkinCompatibilityStatus::unknownActorLayout };
        }
        return { ResolveRuntimeSkinUvLayout(profileLayout, actorFamilies) !=
                SkinUvLayout::unknown ?
            SkinCompatibilityStatus::compatible :
            SkinCompatibilityStatus::incompatibleLayout };
    }

    // Only a verified humanoid layout may inherit its body atlas for feet.
    // Beast and unknown layouts must supply an explicit feet atlas.
    [[nodiscard]] constexpr FeetLayerSource ResolveFeetLayerSource(
        const SkinLayout layout, const SkinRace race,
        const std::size_t bodyLayerCount, const std::size_t feetLayerCount) noexcept
    {
        if (feetLayerCount != 0U) return FeetLayerSource::explicitFeet;
        if (race != SkinRace::humanoid || layout == SkinLayout::unknown ||
            bodyLayerCount == 0U) {
            return FeetLayerSource::none;
        }
        return FeetLayerSource::bodyAtlas;
    }

    [[nodiscard]] constexpr bool AllowsBroadSkinSlotFallback(
        const SkinLayout layout) noexcept
    {
        return layout == SkinLayout::ube;
    }

    enum class LimbSkinSlotRoute : std::uint8_t
    {
        none,
        broadLive,
        persistentOnly,
        persistentAndExact
    };

    [[nodiscard]] constexpr LimbSkinSlotRoute ResolveLimbSkinSlotRoute(
        const SkinLayout layout, const std::size_t exactTargetCount) noexcept
    {
        if (layout == SkinLayout::unknown) return LimbSkinSlotRoute::none;
        if (AllowsBroadSkinSlotFallback(layout)) return LimbSkinSlotRoute::broadLive;
        return exactTargetCount == 0U ?
            LimbSkinSlotRoute::persistentOnly : LimbSkinSlotRoute::persistentAndExact;
    }

    [[nodiscard]] constexpr std::string_view SkinLayoutName(
        const SkinLayout layout) noexcept
    {
        switch (layout) {
        case SkinLayout::legacy: return "legacy";
        case SkinLayout::ube: return "ube";
        case SkinLayout::argonian: return "argonian";
        case SkinLayout::khajiit: return "khajiit";
        default: return "unknown";
        }
    }
}
