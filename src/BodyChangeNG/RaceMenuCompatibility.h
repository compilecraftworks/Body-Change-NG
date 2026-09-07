#pragma once

#include "BodyChangeNG/RuntimeCompatibility.h"

#include <cstdint>
#include <string_view>

namespace bcn::racemenu_compat
{
    enum class BodyMorphAbi : std::uint8_t
    {
        unsupported,
        v4,
        v5
    };

    // RaceMenu BodyMorph v4 and v5 expose the common prefix BCNG uses. Keep
    // both versions explicit and reject an unknown game branch or future ABI.
    [[nodiscard]] constexpr BodyMorphAbi ResolveBodyMorphAbi(
        const std::uint32_t interfaceVersion, const runtime::GameBranch branch) noexcept
    {
        if (branch == runtime::GameBranch::unsupported) return BodyMorphAbi::unsupported;
        if (interfaceVersion == 4U) return BodyMorphAbi::v4;
        if (interfaceVersion == 5U) return BodyMorphAbi::v5;
        return BodyMorphAbi::unsupported;
    }

    [[nodiscard]] constexpr std::string_view BodyMorphAbiLabel(const BodyMorphAbi abi) noexcept
    {
        switch (abi) {
        case BodyMorphAbi::v4: return "BodyMorph-v4";
        case BodyMorphAbi::v5: return "BodyMorph-v5";
        default: return "unsupported";
        }
    }
}
