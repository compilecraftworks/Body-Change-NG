#pragma once

#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinLayout.h"

#include <cstdint>
#include <string_view>

namespace bcn::native_skin
{
    // BOD2 masks are a serialized game-data contract. Keeping the handful of
    // native skin masks local makes this routing policy independently testable
    // and prevents it from depending on a runtime class layout.
    namespace slot_mask
    {
        inline constexpr std::uint32_t body = 1U << 2U;
        inline constexpr std::uint32_t hands = 1U << 3U;
        inline constexpr std::uint32_t forearms = 1U << 4U;
        inline constexpr std::uint32_t feet = 1U << 7U;
        inline constexpr std::uint32_t calves = 1U << 8U;
        inline constexpr std::uint32_t tail = 1U << 10U;
        inline constexpr std::uint32_t ubeBody = 1U << 23U;
    }

    enum class TextureRole : std::uint8_t
    {
        unmanaged,
        body,
        hands,
        feet,
        cbbeGenitalAnal,
        unpGenitalAnal
    };

    using TextureRoleMask = std::uint8_t;

    [[nodiscard]] constexpr TextureRoleMask RoleBit(const TextureRole role) noexcept
    {
        return role == TextureRole::unmanaged ? 0U :
            static_cast<TextureRoleMask>(1U << (static_cast<std::uint8_t>(role) - 1U));
    }

    [[nodiscard]] constexpr TextureRoleMask RequiredRoleMask(
        const bool broadSharedAtlas, const bool body, const bool hands,
        const bool feet, const bool cbbeGenitalAnal,
        const bool unpGenitalAnal) noexcept
    {
        TextureRoleMask result{};
        if (body || (broadSharedAtlas && (hands || feet))) {
            result |= RoleBit(TextureRole::body);
        }
        if (!broadSharedAtlas && hands) result |= RoleBit(TextureRole::hands);
        if (!broadSharedAtlas && feet) result |= RoleBit(TextureRole::feet);
        if (cbbeGenitalAnal) result |= RoleBit(TextureRole::cbbeGenitalAnal);
        if (unpGenitalAnal) result |= RoleBit(TextureRole::unpGenitalAnal);
        return result;
    }

    [[nodiscard]] constexpr bool CoversRequiredRoles(
        const TextureRoleMask available, const TextureRoleMask required) noexcept
    {
        return (available & required) == required;
    }

    [[nodiscard]] constexpr TextureRole ResolveTextureRole(
        const std::uint32_t slotMask, const std::string_view originalDiffuse,
        const SkinUvLayout layout) noexcept
    {
        if (skin_geometry::IsCBBEGenitalAnalTexture(originalDiffuse)) {
            return TextureRole::cbbeGenitalAnal;
        }
        if (skin_geometry::IsUNPGenitalAnalTexture(originalDiffuse)) {
            return TextureRole::unpGenitalAnal;
        }
        const auto has = [slotMask](const std::uint32_t mask) {
            return (slotMask & mask) != 0U;
        };
        // UBE 2.0's real NakedTorso mask is 0x00800174: slot 53 plus the
        // conventional torso/forearm/calf partitions. Slot 53 is the explicit
        // body-layout discriminator, so it must win even when the source ARMA
        // has no NAM1 TXST and consequently no diffuse path to inspect.
        if (layout == SkinUvLayout::ube && has(slot_mask::ubeBody)) {
            return TextureRole::body;
        }
        const auto body = has(slot_mask::body) || has(slot_mask::tail) ||
            (layout == SkinUvLayout::ube && has(slot_mask::ubeBody));
        const auto hands = has(slot_mask::hands) || has(slot_mask::forearms);
        const auto feet = has(slot_mask::feet) || has(slot_mask::calves);
        const auto groups = static_cast<unsigned>(body) + static_cast<unsigned>(hands) +
            static_cast<unsigned>(feet);
        // A single-part ARMA remains authoritative even when its feet reuse a
        // conventional body atlas. Source paths become the discriminator only
        // when one ARMA aggregates several biped parts.
        if (groups == 1U) {
            if (hands) return TextureRole::hands;
            if (feet) return TextureRole::feet;
            if (body) return TextureRole::body;
        }
        if (groups > 1U) {
            if (skin_geometry::ContainsIgnoreAsciiCase(originalDiffuse, "hands")) {
                return TextureRole::hands;
            }
            if (skin_geometry::ContainsIgnoreAsciiCase(originalDiffuse, "feet") ||
                skin_geometry::ContainsIgnoreAsciiCase(originalDiffuse, "foot")) {
                return TextureRole::feet;
            }
            if (skin_geometry::ContainsIgnoreAsciiCase(originalDiffuse, "body")) {
                return TextureRole::body;
            }
        }
        return TextureRole::unmanaged;
    }

    // UBE 2.0 ships its NakedTorso/Hands/Feet ARMAs without NAM0/NAM1. Their
    // canonical NIFs instead share one !UBE\Body atlas. A per-ActorBase native
    // skin therefore needs a synthesized TXST on those three cloned ARMAs.
    // Restrict synthesis to the verified naked-model paths: an unknown custom
    // UBE skin armor must fail closed rather than inherit guessed channels.
    [[nodiscard]] constexpr bool IsCanonicalUbeNakedModel(
        const TextureRole role, const std::string_view modelPath) noexcept
    {
        if (role == TextureRole::body) {
            return skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube\\body\\") ||
                skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube/body/");
        }
        if (role == TextureRole::hands) {
            return skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube\\hands\\") ||
                skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube/hands/");
        }
        if (role == TextureRole::feet) {
            return skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube\\feet\\") ||
                skin_geometry::ContainsIgnoreAsciiCase(modelPath, "!ube/feet/");
        }
        return false;
    }

    [[nodiscard]] constexpr bool ShouldSynthesizeUbeTextureSet(
        const SkinUvLayout layout, const TextureRole role,
        const std::string_view modelPath, const bool hasDirectTexture,
        const bool hasSwapList) noexcept
    {
        return layout == SkinUvLayout::ube && !hasDirectTexture && !hasSwapList &&
            IsCanonicalUbeNakedModel(role, modelPath);
    }

    [[nodiscard]] constexpr std::string_view UbeBaselineTexturePath(
        const std::size_t shaderTextureIndex) noexcept
    {
        switch (shaderTextureIndex) {
        case 0U: return "!UBE\\Body\\femalebody_1_d.dds";
        case 1U: return "!UBE\\Body\\femalebody_1_n.dds";
        case 3U: return "!UBE\\Body\\femalebody_1_sk.dds";
        default: return {};
        }
    }
}
