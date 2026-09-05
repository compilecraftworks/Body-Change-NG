#pragma once

#include <cstddef>
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
        erf
    };

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
        if (ContainsIgnoreAsciiCase(modelPath, "!ube\\sos_addon\\ube_penis")) {
            return AddonKind::ube;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "[trx] futa addon")) {
            return AddonKind::trx;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "erf_futanari")) {
            return AddonKind::erf;
        }
        if (ContainsIgnoreAsciiCase(texturePath, "[trx] futa addon") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE_Shlong") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE_Schlong")) return AddonKind::trx;
        if (ContainsIgnoreAsciiCase(texturePath, "erf_futanari") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE Schlong")) return AddonKind::erf;
        if (EqualsIgnoreAsciiCase(nodeName, "Penis") &&
            (ContainsIgnoreAsciiCase(texturePath, "!ube\\body\\malebody_1") ||
                ContainsIgnoreAsciiCase(texturePath, "bodychangeng\\cache\\futanari\\") ||
                ContainsIgnoreAsciiCase(texturePath, "bodychangerng\\cache\\futanari\\"))) {
            return AddonKind::ube;
        }
        return AddonKind::none;
    }
}
