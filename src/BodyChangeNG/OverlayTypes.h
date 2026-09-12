#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bcn::overlay
{
    enum class Area : std::uint8_t
    {
        face,
        body,
        hands,
        feet,
        count
    };

    enum class ApplyMode : std::uint8_t
    {
        preview,
        manualCommit,
        automatic,
        restore
    };

    enum class Layout : std::uint8_t
    {
        legacy,
        ube
    };

    enum class Sex : std::uint8_t
    {
        male,
        female,
        unisex
    };

    enum class Source : std::uint8_t
    {
        raceMenu,
        slaveTats
    };

    inline constexpr std::uint8_t kNoOwnedSlot = 0xFFU;

    [[nodiscard]] constexpr std::size_t Index(const Area area) noexcept
    {
        return static_cast<std::size_t>(area);
    }

    [[nodiscard]] constexpr std::string_view StableName(const Area area) noexcept
    {
        switch (area) {
        case Area::face: return "face";
        case Area::body: return "body";
        case Area::hands: return "hands";
        case Area::feet: return "feet";
        default: return "invalid";
        }
    }

    inline constexpr std::array<Area, Index(Area::count)> kAreas{
        Area::face, Area::body, Area::hands, Area::feet
    };
}
