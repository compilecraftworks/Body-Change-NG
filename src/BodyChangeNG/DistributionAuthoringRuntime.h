#pragma once
#include "BodyChangeNG/DistributionAuthoring.h"
#include <memory>

namespace bcn::distribution_authoring
{
    enum class Kind { preset, skin, futa, face, body, hands, feet };
    [[nodiscard]] std::shared_ptr<const AssetIndex> Catalog(Kind kind);
    // Call only at DataLoaded, on the game thread. No form walks in UI saving or matching.
    void RefreshTargets(bool includeNPCs);
    [[nodiscard]] bool ResolveNamedTarget(DistributionRule& rule);
    [[nodiscard]] std::vector<DistributionRule> ReadableRules(std::vector<DistributionRule> rules);
}
