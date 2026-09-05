#pragma once

#include <cstddef>
#include <string_view>

namespace bcn::futanari
{
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
        if (ContainsIgnoreAsciiCase(modelPath, "!ube\\sos_addon\\ube_penis") ||
            (EqualsIgnoreAsciiCase(nodeName, "Penis") &&
                ContainsIgnoreAsciiCase(texturePath, "!ube\\body\\malebody_1"))) {
            return AddonKind::ube;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "[trx] futa addon") ||
            ContainsIgnoreAsciiCase(texturePath, "[trx] futa addon") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE_Shlong")) {
            return AddonKind::trx;
        }
        if (ContainsIgnoreAsciiCase(modelPath, "erf_futanari") ||
            ContainsIgnoreAsciiCase(texturePath, "erf_futanari") ||
            EqualsIgnoreAsciiCase(nodeName, "CBBE Schlong")) {
            return AddonKind::erf;
        }
        return AddonKind::none;
    }
}
