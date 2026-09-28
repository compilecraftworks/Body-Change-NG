#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bcn::futanari
{
    // Slot 52 has two independent owners: ordinary male BodySkin profiles use
    // it for SOS, while female actors use it for the optional Futanari tab.
    // Keeping the ownership decision shared makes every RaceMenu ABI route
    // preserve the same boundary.
    [[nodiscard]] constexpr bool BodySkinOwnsSosSlot(const bool female) noexcept
    {
        return !female;
    }

    enum class AddonKind
    {
        none,
        ube,
        trx,
        erf,
        ubeTrx
    };

    [[nodiscard]] constexpr bool IsUbeAddon(const AddonKind kind) noexcept
    { return kind == AddonKind::ube || kind == AddonKind::ubeTrx; }

    inline constexpr std::uint32_t kSlot52 = 1U << 22U;
    inline constexpr std::uint32_t kSlot54 = 1U << 24U;

    // UBE SOS/TRX: ARMO 52+54, ARMA 53+54. Only their shared genital slot
    // 54 is admitted; slot 53 also carries the ordinary UBE body. Other addon
    // families keep the original shared-slot-52 requirement.
    [[nodiscard]] constexpr std::uint32_t GenitalSlots(const AddonKind kind,
        const std::uint32_t armorMask, const std::uint32_t addonMask) noexcept
    {
        if ((armorMask & kSlot52) == 0U) return 0U;
        return armorMask & addonMask & (kSlot52 | (IsUbeAddon(kind) ? kSlot54 : 0U));
    }

    [[nodiscard]] constexpr char LowerAscii(const char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    [[nodiscard]] constexpr bool EqualsIgnoreAsciiCase(
        const std::string_view left, const std::string_view right) noexcept
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index{}; index < left.size(); ++index) {
            if (LowerAscii(left[index]) != LowerAscii(right[index])) return false;
        }
        return true;
    }

    [[nodiscard]] constexpr bool ContainsIgnoreAsciiCase(
        const std::string_view value, const std::string_view token,
        const bool path = false) noexcept
    {
        if (token.empty()) return true;
        if (token.size() > value.size()) return false;
        for (std::size_t start{}; start + token.size() <= value.size(); ++start) {
            bool match{ true };
            for (std::size_t index{}; index < token.size(); ++index) {
                const auto normalize = [path](const char c) {
                    return path && c == '/' ? '\\' : LowerAscii(c);
                };
                if (normalize(value[start + index]) != normalize(token[index])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    // Called only after associating this faction with a verified SOS addon
    // from the same plugin. Native UBE deliberately has no "futa" in its ID.
    [[nodiscard]] constexpr bool IsAddonFaction(const AddonKind kind,
        const std::string_view editorID, const std::string_view name,
        const std::string_view signals) noexcept
    {
        if (kind == AddonKind::ube) {
            return EqualsIgnoreAsciiCase(editorID, "SOS_Addon_UBE_Faction") ||
                EqualsIgnoreAsciiCase(name, "SOS UBE");
        }
        if (kind == AddonKind::ubeTrx &&
            (EqualsIgnoreAsciiCase(editorID, "SOS_Addon_TRX_FutaU_Faction") ||
                EqualsIgnoreAsciiCase(name, "TRX Futanari TRX Addon UBE"))) return true;
        return kind != AddonKind::none && ContainsIgnoreAsciiCase(signals, "faction") &&
            ContainsIgnoreAsciiCase(signals, "futa");
    }

    // ArmorAddon model paths are the primary evidence because a BCNG texture
    // override replaces the live material path. Exact node/material names are
    // conservative fallbacks for already rebuilt or unusual addon clones.
    [[nodiscard]] constexpr AddonKind ClassifyEvidence(
        const std::string_view modelPath, const std::string_view nodeName = {},
        const std::string_view texturePath = {}) noexcept
    {
        // Prefer the addon model whenever it names a concrete family. The
        // live diffuse path is replaced with BCNG's private cache after a
        // selection, so it can only be a fallback and must not override a
        // model path that already identifies TRX or ERF.
        if (ContainsIgnoreAsciiCase(modelPath, "!ube\\sos_addon\\ube_penis", true)) {
            return AddonKind::ube;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "[trx] futa addon ube", true)) {
            return AddonKind::ubeTrx;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "[trx] futa addon", true)) {
            return AddonKind::trx;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "erf_futanari", true)) {
            return AddonKind::erf;
        }
        if (ContainsIgnoreAsciiCase(texturePath, "[trx] futa addon ube", true)) {
            return AddonKind::ubeTrx;
        }
        if (ContainsIgnoreAsciiCase(texturePath, "[trx] futa addon", true) ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE_Shlong") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE_Schlong")) return AddonKind::trx;
        if (ContainsIgnoreAsciiCase(texturePath, "erf_futanari", true) ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE Schlong")) return AddonKind::erf;
        // A BCNG cache path is not UV/provider evidence: TRX/ERF use that
        // namespace too. The live adapter supplies the captured source path.
        if (EqualsIgnoreAsciiCase(nodeName, "Penis") &&
            ContainsIgnoreAsciiCase(texturePath, "!ube\\body\\malebody_1", true)) {
            return AddonKind::ube;
        }
        return AddonKind::none;
    }
}
