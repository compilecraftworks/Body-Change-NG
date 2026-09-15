#pragma once

#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinLayout.h"

#include <cstdint>
#include <optional>
#include <span>
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

    // Partial skin packs are valid. This is the subset that can be applied to
    // the current native graph without borrowing a texture from another body
    // part or guessing an absent target.
    [[nodiscard]] constexpr TextureRoleMask ApplicableRoleMask(
        const TextureRoleMask available, const TextureRoleMask requested) noexcept
    {
        return available & requested;
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

    [[nodiscard]] constexpr TextureRole ResolveEmbeddedOrdinaryRole(const std::uint32_t slots,
        const std::string_view node, const std::string_view diffuse, const SkinUvLayout layout) noexcept
    {
        // Separate SOS/TNG or foreign genital/anal atlases can share a body
        // ARMA. Being SkinTint does not make them ordinary body skin.
        if (skin_geometry::IsMaleGenital(node, diffuse) ||
            skin_geometry::IsKnownGenitalAnalNode(node)) return TextureRole::unmanaged;
        return ResolveTextureRole(slots, diffuse, layout);
    }

    [[nodiscard]] constexpr bool IsOrdinarySkinRole(const TextureRole role) noexcept
    {
        return role == TextureRole::body || role == TextureRole::hands || role == TextureRole::feet;
    }

    // A missing native Skin TXST can use the actual NIF baseline, independent
    // of its folder name and of the first selected pack. Existing NAM/FLST
    // providers remain authoritative. Do not inspect a genital-only addon.
    [[nodiscard]] constexpr bool NeedsEmbeddedSkinBaseline(const std::uint32_t slots,
        const SkinUvLayout layout, const bool hasDirectTexture, const bool hasSwapList) noexcept
    {
        const auto supported = slot_mask::body | slot_mask::hands | slot_mask::forearms |
            slot_mask::feet | slot_mask::calves | slot_mask::tail |
            (layout == SkinUvLayout::ube ? slot_mask::ubeBody : 0U);
        return !hasDirectTexture && !hasSwapList && (slots & supported) != 0U;
    }

    // Targets contain only verified skin-material observations (including an
    // unmanaged witness when a skin shape cannot be routed). A native NAM1
    // applies to the addon as a whole: never collapse differing skin atlases,
    // roles, normal conventions, or 1st/3rd-person providers into one guess.
    template <class Target>
    [[nodiscard]] std::optional<std::size_t> SharedEmbeddedSkinBaseline(
        const std::span<const Target> targets) noexcept
    {
        if (targets.empty()) return std::nullopt;
        const auto& first = targets.front();
        if (!IsOrdinarySkinRole(first.role) || first.paths[0].empty()) return std::nullopt;
        for (const auto& target : targets) {
            if (target.role != first.role || target.paths != first.paths ||
                target.modelSpaceNormals != first.modelSpaceNormals ||
                target.provider != first.provider) return std::nullopt;
        }
        return 0U;
    }
}
