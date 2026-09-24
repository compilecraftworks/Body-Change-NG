#pragma once

#include "BodyChangeNG/SkinGeometryRouting.h"
#include <string_view>

namespace bcn::face_skin
{
    // Only feed geometries from Actor::GetFaceNodeSkinned(), never the whole
    // actor (body/hands/feet can also use RGBTint). Views live for this scan.
    struct NodeSelection
    {
        std::string_view name;
        unsigned rank{}, matches{};

        void Consider(std::string_view candidate, bool facegen, bool rgbTint,
            std::string_view preferred) noexcept
        {
            if (candidate.empty() || (!facegen && !rgbTint)) return;
            unsigned candidateRank{};
            if (!preferred.empty() && candidate == preferred) candidateRank = 2U;
            else {
                for (const auto excluded : { "hair", "eye", "brow", "mouth", "teeth",
                         "tongue", "body", "hand", "feet", "foot" }) {
                    if (skin_geometry::ContainsIgnoreAsciiCase(candidate, excluded)) return;
                }
                if (!facegen && !skin_geometry::ContainsIgnoreAsciiCase(candidate, "head") &&
                    !skin_geometry::ContainsIgnoreAsciiCase(candidate, "face")) return;
                candidateRank = 1U;
            }
            if (candidateRank > rank) { rank = candidateRank; name = candidate; matches = 1U; }
            else if (candidateRank == rank) ++matches;
        }

        [[nodiscard]] std::string_view Result() const noexcept
        { return matches == 1U ? name : std::string_view{}; }
    };
}
