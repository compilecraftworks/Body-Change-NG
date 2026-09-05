#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bcn::skin_geometry
{
    enum class BodySelection
    {
        all,
        regular,
        cbbeGenitalAnal,
        unpGenitalAnal,
        maleGenitals
    };

    enum class LimbSelection
    {
        hands,
        feet
    };

    // Hands and feet can live in a multi-slot naked Skin Armor clone instead
    // of the exact biped-array entry.  An exact entry is useful only after at
    // least one skin geometry has been identified; a sleeve/glove object with
    // zero matching skin nodes must not suppress the bounded fallback scan.
    [[nodiscard]] constexpr bool NeedsFixedBipedFallback(
        const bool requestedHandsOrFeet, const std::size_t usableExactTargets) noexcept
    {
        return requestedHandsOrFeet && usableExactTargets == 0U;
    }

    // UBE normally exposes its naked body on slot 53, but some valid UBE
    // skin-armour/equipment states expose the live body geometry through the
    // ordinary slot 32 instead. Only fall back when the UBE-specific target is
    // genuinely absent; an apply failure on an existing target must not spill
    // the profile onto a second body object.
    [[nodiscard]] constexpr bool NeedsStandardBodyFallback(
        const bool usesUbeBodySlot, const std::size_t usableUbeTargets) noexcept
    {
        return usesUbeBodySlot && usableUbeTargets == 0U;
    }

    [[nodiscard]] constexpr char LowerAscii(const char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    [[nodiscard]] constexpr bool EqualsIgnoreAsciiCase(
        const std::string_view left, const std::string_view right) noexcept
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (LowerAscii(left[index]) != LowerAscii(right[index])) return false;
        }
        return true;
    }

    [[nodiscard]] constexpr bool ContainsIgnoreAsciiCase(
        const std::string_view value, const std::string_view token) noexcept
    {
        if (token.empty()) return true;
        if (token.size() > value.size()) return false;
        for (std::size_t start{}; start + token.size() <= value.size(); ++start) {
            bool match{ true };
            for (std::size_t index{}; index < token.size(); ++index) {
                if (LowerAscii(value[start + index]) != LowerAscii(token[index])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool MatchesLimb(
        const LimbSelection selection, const std::string_view nodeName,
        const std::string_view texturePath = {}) noexcept
    {
        if (selection == LimbSelection::hands) {
            return ContainsIgnoreAsciiCase(nodeName, "hand") ||
                ContainsIgnoreAsciiCase(texturePath, "hands");
        }
        return ContainsIgnoreAsciiCase(nodeName, "feet") ||
            ContainsIgnoreAsciiCase(nodeName, "foot") ||
            ContainsIgnoreAsciiCase(texturePath, "feet");
    }

    [[nodiscard]] constexpr bool IsHandsOrFeetSlotPart(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        return MatchesLimb(LimbSelection::hands, nodeName, texturePath) ||
            MatchesLimb(LimbSelection::feet, nodeName, texturePath);
    }

    [[nodiscard]] constexpr bool MatchesRequestedPart(
        const std::uint32_t requestedSlotMask, const std::uint32_t bodySlotMask,
        const std::uint32_t handsSlotMask, const std::uint32_t feetSlotMask,
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        if (requestedSlotMask == handsSlotMask) {
            return MatchesLimb(LimbSelection::hands, nodeName, texturePath);
        }
        if (requestedSlotMask == feetSlotMask) {
            return MatchesLimb(LimbSelection::feet, nodeName, texturePath);
        }
        // A naked Skin Armor may expose one multi-slot clone from each biped
        // entry. Keep its hand/foot geometries out of a slot-32 body request.
        if (requestedSlotMask == bodySlotMask) {
            return !IsHandsOrFeetSlotPart(nodeName, texturePath);
        }
        return true;
    }

    [[nodiscard]] constexpr bool IsCBBEGenitalAnalTexture(
        const std::string_view texturePath) noexcept
    {
        return ContainsIgnoreAsciiCase(texturePath, "femalebody_etc_v2_1");
    }

    [[nodiscard]] constexpr bool IsUNPGenitalAnalTexture(
        const std::string_view texturePath) noexcept
    {
        return ContainsIgnoreAsciiCase(texturePath, "bakaunp\\vaginalanalcanal2") ||
            ContainsIgnoreAsciiCase(texturePath, "bakaunp/vaginalanalcanal2");
    }

    [[nodiscard]] constexpr bool IsCBBEGenitalAnal(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        // The current material path is authoritative when it still exposes a
        // known atlas. Node names remain the fallback after RaceMenu replaces
        // that path with Body Change NG's hashed runtime-cache alias.
        if (IsUNPGenitalAnalTexture(texturePath)) return false;
        if (IsCBBEGenitalAnalTexture(texturePath)) return true;
        return EqualsIgnoreAsciiCase(nodeName, "3BA_Vagina") ||
            EqualsIgnoreAsciiCase(nodeName, "3BBB_Vagina") ||
            EqualsIgnoreAsciiCase(nodeName, "3BA_Anus") ||
            EqualsIgnoreAsciiCase(nodeName, "3BBB_Anus");
    }

    [[nodiscard]] constexpr bool IsUNPGenitalAnal(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        if (IsCBBEGenitalAnalTexture(texturePath)) return false;
        if (IsUNPGenitalAnalTexture(texturePath)) return true;
        // BHUNP BodySlide outputs observed in the base body and outfits use a
        // shared VaginalAnalCanal2 atlas on these three separate geometries.
        return EqualsIgnoreAsciiCase(nodeName, "BaseShapeVagina") ||
            EqualsIgnoreAsciiCase(nodeName, "BaseShapeAnus") ||
            EqualsIgnoreAsciiCase(nodeName, "BaseShapeCanal");
    }

    [[nodiscard]] constexpr bool IsGenitalAnal(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        return IsCBBEGenitalAnal(nodeName, texturePath) ||
            IsUNPGenitalAnal(nodeName, texturePath);
    }

    [[nodiscard]] constexpr bool IsMaleGenital(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        // SOS addon NIFs consistently expose malegenitals_* material paths.
        // Keep a narrow node fallback for an already overridden/cache path.
        return ContainsIgnoreAsciiCase(texturePath, "malegenitals_") ||
            ContainsIgnoreAsciiCase(nodeName, "malegenital") ||
            EqualsIgnoreAsciiCase(nodeName, "schlong");
    }

    // A revealing outfit can keep its visible body copy on any biped slot,
    // rather than on the naked body's conventional slot 32 (or UBE slot 53).
    // Cross-slot discovery therefore needs stronger evidence than a generic
    // BaseShape name: accept an explicit body node/path while rejecting the
    // independent hand, foot, genital and anal atlases.
    [[nodiscard]] constexpr bool IsBodyGeometryCandidate(
        const std::string_view nodeName, const std::string_view texturePath = {}) noexcept
    {
        if (IsHandsOrFeetSlotPart(nodeName, texturePath) ||
            IsGenitalAnal(nodeName, texturePath) || IsMaleGenital(nodeName, texturePath)) {
            return false;
        }
        return ContainsIgnoreAsciiCase(texturePath, "body") ||
            ContainsIgnoreAsciiCase(nodeName, "body") ||
            ContainsIgnoreAsciiCase(nodeName, "torso") ||
            ContainsIgnoreAsciiCase(nodeName, "3ba") ||
            ContainsIgnoreAsciiCase(nodeName, "3bbb");
    }

    [[nodiscard]] constexpr bool Matches(
        const std::string_view nodeName, const BodySelection selection,
        const std::string_view texturePath = {}) noexcept
    {
        switch (selection) {
        case BodySelection::regular: return !IsGenitalAnal(nodeName, texturePath);
        case BodySelection::cbbeGenitalAnal: return IsCBBEGenitalAnal(nodeName, texturePath);
        case BodySelection::unpGenitalAnal: return IsUNPGenitalAnal(nodeName, texturePath);
        case BodySelection::maleGenitals: return IsMaleGenital(nodeName, texturePath);
        default: return true;
        }
    }
}
