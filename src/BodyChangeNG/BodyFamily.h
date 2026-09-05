#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace RE
{
    class Actor;
}

namespace bcn::body_family
{
    using Mask = std::uint32_t;

    enum class Sex : std::uint8_t
    {
        male,
        female
    };

    enum class Family : Mask
    {
        femaleVanilla = 1U << 0U,
        cbbe = 1U << 1U,
        unp = 1U << 2U,
        ube = 1U << 3U,
        maleVanilla = 1U << 4U,
        himbo = 1U << 5U,
        sam = 1U << 6U
    };

    enum class SkinTextureLayout : std::uint8_t
    {
        unknown,
        standard,
        ube
    };

    [[nodiscard]] constexpr Mask Bit(const Family family) noexcept
    {
        return static_cast<Mask>(family);
    }

    inline constexpr Mask kFemaleFamilies = Bit(Family::femaleVanilla) | Bit(Family::cbbe) |
        Bit(Family::unp) | Bit(Family::ube);
    inline constexpr Mask kMaleFamilies = Bit(Family::maleVanilla) | Bit(Family::himbo) | Bit(Family::sam);

    [[nodiscard]] constexpr Mask SexFamilies(const Sex sex) noexcept
    {
        return sex == Sex::female ? kFemaleFamilies : kMaleFamilies;
    }

    [[nodiscard]] constexpr Mask VanillaFamily(const Sex sex) noexcept
    {
        return sex == Sex::female ? Bit(Family::femaleVanilla) : Bit(Family::maleVanilla);
    }

    [[nodiscard]] constexpr Mask NonVanillaFamilies(const Sex sex) noexcept
    {
        return SexFamilies(sex) & ~VanillaFamily(sex);
    }

    [[nodiscard]] Mask DetectText(std::string_view text, Sex sex);
    [[nodiscard]] SkinTextureLayout DetectSkinTextureLayout(std::string_view path, Sex sex);
    // Resolves only live texture-layout evidence. A loaded head wins over
    // exposed body geometry embedded in an outfit; contradictory evidence is
    // left unknown instead of guessing.
    [[nodiscard]] constexpr SkinTextureLayout ResolveLoadedSkinTextureLayout(
        const bool standardTexture, const bool ubeTexture,
        const bool standardHeadTexture, const bool ubeHeadTexture) noexcept
    {
        if (standardHeadTexture != ubeHeadTexture) {
            return ubeHeadTexture ? SkinTextureLayout::ube : SkinTextureLayout::standard;
        }
        if (standardHeadTexture && ubeHeadTexture) return SkinTextureLayout::unknown;
        if (standardTexture != ubeTexture) {
            return ubeTexture ? SkinTextureLayout::ube : SkinTextureLayout::standard;
        }
        return SkinTextureLayout::unknown;
    }
    // Resolves live geometry/texture evidence against the installed body
    // frameworks. UBE's private texture namespace is authoritative; the
    // standard female texture tree deliberately excludes UBE before using an
    // installed-framework fallback. Ambiguous evidence remains unknown so the
    // catalog can preserve its safe show-all fallback.
    [[nodiscard]] Mask ResolveSkinTextureFamily(Mask explicitFamilies, Mask installedFamilies,
        SkinTextureLayout layout, Sex sex);
    struct PresetClassification final
    {
        Mask families{};
        bool male{};
        bool conflict{};
        bool usedMetadataFallback{};
    };

    // BodySlide's Preset/@set is authoritative.  The preset/file names are
    // deliberately only a fallback because authors frequently put unrelated
    // body-family names in descriptive preset names.
    [[nodiscard]] PresetClassification ClassifyPreset(
        std::string_view bodySet, std::string_view presetName, std::string_view sourcePath,
        std::string_view groupNames = {});
    [[nodiscard]] std::string PresetFamilyLabel(const PresetClassification& classification);
    [[nodiscard]] Mask PresetMask(std::string_view family, bool male);
    [[nodiscard]] Mask ResolveActor(RE::Actor* actor);
    void ForgetActorState(std::uint32_t actorFormID);
    void ResetRuntimeCaches();

    [[nodiscard]] constexpr bool Matches(const Mask presetFamilies, const Mask actorFamily) noexcept
    {
        // Missing or conflicting evidence must never hide a usable preset.
        if (actorFamily == 0U) return true;
        auto compatible = actorFamily;
        if ((actorFamily & kFemaleFamilies) != 0U) compatible |= Bit(Family::femaleVanilla);
        if ((actorFamily & kMaleFamilies) != 0U) compatible |= Bit(Family::maleVanilla);
        return (presetFamilies & compatible) != 0U;
    }
}
