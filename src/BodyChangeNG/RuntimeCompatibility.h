#pragma once

#include <REL/Version.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bcn::runtime
{
    enum class GameBranch : std::uint8_t
    {
        unsupported,
        se,
        ae
    };

    struct PlayerTintLayout final
    {
        std::size_t baseOffset{};
        std::size_t overlayOffset{};
    };

    // This is the single admission table for runtime-sensitive BCNG code.
    // CommonLib relocations remain version-aware, but any raw member layout or
    // external ABI route must first resolve through this exact table. An
    // unlisted patch is intentionally unsupported until it has been audited.
    // 1.6.678 is Epic, not GOG: upstream SKSE explicitly does not support it.
    // A version constant in upstream headers is not a supported ABI/loader.
    [[nodiscard]] constexpr GameBranch ResolveGameBranch(const REL::Version version) noexcept
    {
        if (version == REL::Version{ 1, 5, 97, 0 }) return GameBranch::se;
        if (version.major() != 1 || version.build() != 0) return GameBranch::unsupported;
        if (version.minor() != 6) return GameBranch::unsupported;
        switch (version.patch()) {
        case 317:
        case 318:
        case 323:
        case 342:
        case 353:
        case 629:
        case 640:
        case 659:
        case 1130:
        case 1170:
        case 1179:
            return GameBranch::ae;
        default:
            return GameBranch::unsupported;
        }
    }

    [[nodiscard]] constexpr std::string_view GameBranchLabel(const GameBranch branch) noexcept
    {
        switch (branch) {
        case GameBranch::se: return "SE";
        case GameBranch::ae: return "AE";
        default: return "unsupported";
        }
    }

    // PlayerCharacter tint arrays are raw in-object members. Keep every known
    // layout boundary explicit; do not infer a future patch from >= checks.
    [[nodiscard]] constexpr std::optional<PlayerTintLayout> ResolvePlayerTintLayout(
        const REL::Version version) noexcept
    {
        if (version == REL::Version{ 1, 5, 97, 0 }) return PlayerTintLayout{ 0xB10, 0xB28 };
        if (version.major() != 1 || version.build() != 0) return std::nullopt;
        if (version.minor() != 6) return std::nullopt;
        switch (version.patch()) {
        case 317:
        case 318:
        case 323:
        case 342:
        case 353:
            return PlayerTintLayout{ 0xB10, 0xB28 };
        case 629:
        case 640:
        case 659:
        case 1130:
        case 1170:
        case 1179:
            return PlayerTintLayout{ 0xB18, 0xB30 };
        default:
            return std::nullopt;
        }
    }
}
