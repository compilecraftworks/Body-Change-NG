#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace bcn::overlay
{
    enum class SlotAvailability : std::uint8_t { ready, actorUnavailable, providerUnavailable, disabled };
    struct SlotUsage final {
        std::uint32_t capacity{};
        std::uint32_t applied{};
        std::uint32_t total{};
        std::uint32_t foreignReserved{};
        SlotAvailability availability{SlotAvailability::providerUnavailable};
    };
    struct SlotAccounting final {
        std::uint32_t foreignReserved{};
        constexpr void Observe(bool reserved, bool committedOwns, bool previewOwns) noexcept
        {
            if (reserved && !committedOwns && !previewOwns) ++foreignReserved;
        }
        [[nodiscard]] constexpr SlotUsage Result(std::uint32_t total,
            std::size_t confirmedSelections) const noexcept
        {
            const auto available = total - (std::min)(total, foreignReserved);
            return { available, static_cast<std::uint32_t>(
                (std::min<std::size_t>)(available, confirmedSelections)), total,
                (std::min)(total, foreignReserved),
                total ? SlotAvailability::ready : SlotAvailability::disabled };
        }
    };
}
