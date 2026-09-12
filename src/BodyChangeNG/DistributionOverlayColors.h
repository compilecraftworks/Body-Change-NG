#pragma once

#include "BodyChangeNG/Distribution.h"
#include <nlohmann/json.hpp>
#include <limits>

namespace bcn
{
    // Read only selected IDs. Malformed optional colors never discard an otherwise valid rule.
    [[nodiscard]] inline decltype(DistributionRule::overlayColors) ReadDistributionOverlayColors(
        const nlohmann::json& source, const decltype(DistributionRule::overlayIds)& ids)
    {
        decltype(DistributionRule::overlayColors) result;
        if (!source.is_array()) return result;
        for (std::size_t area{}; area < result.size() && area < source.size(); ++area) {
            if (!source[area].is_object()) continue;
            for (const auto& id : ids[area]) {
                const auto value = source[area].find(id);
                if (value == source[area].end() || !value->is_number_integer()) continue;
                if (value->is_number_unsigned()) {
                    const auto color = value->get<std::uint64_t>();
                    if (color <= (std::numeric_limits<std::uint32_t>::max)()) result[area][id] = static_cast<std::uint32_t>(color);
                } else {
                    const auto color = value->get<std::int64_t>();
                    if (color >= 0 && color <= (std::numeric_limits<std::uint32_t>::max)()) result[area][id] = static_cast<std::uint32_t>(color);
                }
            }
        }
        return result;
    }
}
