#pragma once

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
