#pragma once

#include <string_view>

namespace bcn::skin_geometry
{
    enum class BodySelection
    {
        all,
        regular,
        vagina
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

    [[nodiscard]] constexpr bool IsVagina(const std::string_view nodeName) noexcept
    {
        // CBBE 3BA output uses 3BA_Vagina. Some BodySlide source meshes retain
        // the older 3BBB_Vagina name. The adjacent 3BA/3BBB_Anus geometry is
        // deliberately not included: femalebody_etc_v2_1 belongs only here.
        return EqualsIgnoreAsciiCase(nodeName, "3BA_Vagina") ||
            EqualsIgnoreAsciiCase(nodeName, "3BBB_Vagina");
    }

    [[nodiscard]] constexpr bool Matches(
        const std::string_view nodeName, const BodySelection selection) noexcept
    {
        switch (selection) {
        case BodySelection::regular: return !IsVagina(nodeName);
        case BodySelection::vagina: return IsVagina(nodeName);
        default: return true;
        }
    }
}
