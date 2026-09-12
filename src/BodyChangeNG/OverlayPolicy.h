#pragma once

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/OverlayTypes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace bcn::overlay
{
    inline constexpr std::uint16_t kTextureKey = 9U;
    inline constexpr std::uint16_t kTintKey = 7U;
    inline constexpr std::uint16_t kAlphaKey = 8U;
    inline constexpr std::uint8_t kScalarIndex = 0xFFU;
    inline constexpr std::uint32_t kWhiteTint = 0x00FFFFFFU;

    [[nodiscard]] constexpr bool IsAsciiAlphaNumeric(const char value) noexcept
    {
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9');
    }

    [[nodiscard]] inline bool ContainsAsciiToken(
        const std::string_view value, const std::string_view token)
    {
        if (token.empty() || token.size() > value.size()) return false;
        for (std::size_t start{}; start + token.size() <= value.size(); ++start) {
            bool equal{ true };
            for (std::size_t index{}; index < token.size(); ++index) {
                const auto left = static_cast<unsigned char>(value[start + index]);
                const auto right = static_cast<unsigned char>(token[index]);
                if (std::tolower(left) != std::tolower(right)) {
                    equal = false;
                    break;
                }
            }
            if (!equal) continue;
            const auto leftBoundary = start == 0U || !IsAsciiAlphaNumeric(value[start - 1U]);
            const auto end = start + token.size();
            const auto rightBoundary = end == value.size() || !IsAsciiAlphaNumeric(value[end]);
            if (leftBoundary && rightBoundary) return true;
        }
        return false;
    }

    [[nodiscard]] inline Layout ClassifyLayout(const std::string_view name,
        const std::string_view texturePath, const std::string_view providerPath = {})
    {
        return ContainsAsciiToken(name, "ube") || ContainsAsciiToken(texturePath, "ube") ||
                ContainsAsciiToken(providerPath, "ube") ?
            Layout::ube : Layout::legacy;
    }

    [[nodiscard]] inline Sex ClassifySex(const std::string_view name,
        const std::string_view texturePath, const std::string_view providerPath = {})
    {
        const auto female = ContainsAsciiToken(name, "female") ||
            ContainsAsciiToken(texturePath, "female") || ContainsAsciiToken(providerPath, "female");
        const auto male = ContainsAsciiToken(name, "male") ||
            ContainsAsciiToken(texturePath, "male") || ContainsAsciiToken(providerPath, "male");
        if (female != male) return female ? Sex::female : Sex::male;
        if (ClassifyLayout(name, texturePath, providerPath) == Layout::ube) return Sex::female;
        return Sex::unisex;
    }

    [[nodiscard]] constexpr bool SexMatches(
        const Sex entrySex, const bool actorFemale) noexcept
    {
        return entrySex == Sex::unisex ||
            (actorFemale ? entrySex == Sex::female : entrySex == Sex::male);
    }

    [[nodiscard]] constexpr bool LayoutMatchesActor(
        const Layout layout, const body_family::Mask actorFamily) noexcept
    {
        using body_family::Bit;
        using body_family::Family;
        const auto ube = Bit(Family::ube);
        const auto legacy = (body_family::kFemaleFamilies & ~ube) |
            body_family::kMaleFamilies;
        const auto hasUbe = (actorFamily & ube) != 0U;
        const auto hasLegacy = (actorFamily & legacy) != 0U;
        if (hasUbe && !hasLegacy) return layout == Layout::ube;
        if (hasLegacy && !hasUbe) return layout == Layout::legacy;
        // Missing or contradictory actor evidence must not hide both lists.
        return true;
    }

    [[nodiscard]] constexpr bool EntryMatchesActor(const Layout layout,
        const Sex entrySex, const body_family::Mask actorFamily,
        const bool actorFemale) noexcept
    {
        return SexMatches(entrySex, actorFemale) && LayoutMatchesActor(layout, actorFamily);
    }

    [[nodiscard]] inline std::string NormalizeTexturePath(std::string_view path);

    [[nodiscard]] inline std::string StableId(const Area area,
        const std::string_view texturePath)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto append = [&hash](const std::string_view value) {
            for (const auto character : value) {
                hash = (hash ^ static_cast<std::uint8_t>(character)) * 1099511628211ULL;
            }
            hash = (hash ^ 0xFFU) * 1099511628211ULL;
        };
        append(StableName(area));
        append(NormalizeTexturePath(texturePath));
        return std::format("overlay-{}-{:016x}", StableName(area), hash);
    }

    [[nodiscard]] constexpr std::uint32_t LegacyOverlayDefaultCount(
        const Area area) noexcept
    {
        return area == Area::count ? 0U : 3U;
    }

    [[nodiscard]] constexpr std::string_view LegacyOverlayNodePrefix(
        const Area area) noexcept
    {
        switch (area) {
        case Area::face: return "Face";
        case Area::hands: return "Hands";
        case Area::feet: return "Feet";
        default: return "Body";
        }
    }

    struct TextureLayer final
    {
        std::uint8_t index{};
        std::string path;
    };

    [[nodiscard]] inline std::string NormalizeTexturePath(std::string_view path)
    {
        while (!path.empty() && std::isspace(static_cast<unsigned char>(path.front()))) {
            path.remove_prefix(1U);
        }
        while (!path.empty() && std::isspace(static_cast<unsigned char>(path.back()))) {
            path.remove_suffix(1U);
        }
        std::string result{ path };
        std::ranges::transform(result, result.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        std::ranges::replace(result, '/', '\\');
        while (result.starts_with(".\\")) result.erase(0U, 2U);
        return result;
    }

    [[nodiscard]] inline bool IsRaceMenuDefaultTexture(const std::string_view path)
    {
        const auto normalized = NormalizeTexturePath(path);
        return normalized.empty() || normalized == "ignore" ||
            normalized.ends_with("\\overlays\\default.dds") ||
            normalized == "textures\\actors\\character\\overlays\\default.dds";
    }

    // Material/source texture names may include Textures/ (or Data/Textures/)
    // while a catalog/override stores a texture-root-relative name. Compare
    // their resource identity without changing the provider's write format.
    [[nodiscard]] inline std::string TextureIdentity(const std::string_view path)
    {
        auto result = NormalizeTexturePath(path);
        if (result.starts_with("data\\textures\\")) result.erase(0U, 14U);
        else if (result.starts_with("textures\\")) result.erase(0U, 9U);
        return result;
    }

    [[nodiscard]] inline bool CanClaimNode(const bool hasStoredOverrides,
        const std::string_view liveDiffuse)
    {
        // An invisible but reserved paint (alpha 0) still belongs to its mod.
        return !hasStoredOverrides && IsRaceMenuDefaultTexture(liveDiffuse);
    }

    // RaceMenu v1 may consume a registered paint key after materializing the
    // node.  BCNG still owns that exact actor/area/slot when the registry
    // points at it and the live material contains the complete expected
    // texture set.  Requiring both forms of evidence turns a valid live node
    // into a permanent ownership conflict and prevents later color changes.
    [[nodiscard]] constexpr bool OwnsRegisteredOrLive(
        const bool storedMatch, const bool liveMatch) noexcept
    {
        return storedMatch || liveMatch;
    }

    [[nodiscard]] constexpr bool IsFaceSourceFeature(const std::uint32_t feature,
        const bool isPaintNode) noexcept
    {
        // Native FaceGen (4) and RGB-tinted custom heads (5); an existing paint
        // must not be mistaken for the face geometry that supplies its mesh.
        return !isPaintNode && (feature == 4U || feature == 5U);
    }

    [[nodiscard]] inline std::vector<TextureLayer> TextureLayers(const std::string_view packed)
    {
        std::vector<TextureLayer> result;
        std::size_t begin{};
        for (std::uint8_t index{}; index < 8U && begin <= packed.size(); ++index) {
            const auto separator = packed.find('|', begin);
            const auto end = separator == std::string_view::npos ? packed.size() : separator;
            const auto value = NormalizeTexturePath(packed.substr(begin, end - begin));
            if (!value.empty() && value != "ignore") result.push_back({ index, value });
            if (separator == std::string_view::npos) break;
            begin = separator + 1U;
        }
        return result;
    }

    [[nodiscard]] inline std::vector<std::uint8_t> ObsoleteTextureIndices(
        const std::string_view previous, const std::string_view next)
    {
        const auto desired = TextureLayers(next);
        std::vector<std::uint8_t> result;
        for (const auto& old : TextureLayers(previous)) {
            if (std::ranges::none_of(desired, [&old](const TextureLayer& layer) {
                    return layer.index == old.index;
                })) result.push_back(old.index);
        }
        return result;
    }

    // RaceMenu's registration API replaces an existing key itself. Retain the
    // registration until the new values exist; only obsolete companion maps
    // are removed. This is not a missing-key recovery operation.
    template <class WriteTexture, class WriteScalars, class RemoveTexture>
    void ReplaceRegisteredPaint(const std::string_view previous, const std::string_view next,
        WriteTexture&& writeTexture, WriteScalars&& writeScalars, RemoveTexture&& removeTexture)
    {
        for (const auto& layer : TextureLayers(next)) writeTexture(layer);
        writeScalars();
        for (const auto index : ObsoleteTextureIndices(previous, next)) removeTexture(index);
    }

    template <class ReadTexture>
    [[nodiscard]] bool StoredPathsMatch(const std::string_view packed, ReadTexture&& read)
    {
        const auto layers = TextureLayers(packed);
        if (layers.empty()) return false;
        std::array<std::optional<std::string>, 8U> expected;
        for (const auto& layer : layers) expected[layer.index] = TextureIdentity(layer.path);
        for (std::uint8_t index{}; index < expected.size(); ++index) {
            const auto current = read(index);
            if (expected[index]) {
                if (!current || TextureIdentity(*current) != *expected[index]) return false;
            } else if (current && !IsRaceMenuDefaultTexture(*current)) {
                return false; // Foreign companion maps still block replacement.
            }
        }
        return true;
    }

    [[nodiscard]] constexpr std::uint32_t BipedMask(const Area area) noexcept
    {
        switch (area) {
        case Area::body: return 1U << 2U;
        case Area::hands: return 1U << 3U;
        case Area::feet: return 1U << 7U;
        default: return 0U;
        }
    }
}
