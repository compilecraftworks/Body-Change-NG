#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bcn::native_skin
{
    enum class BodyTextureState { unobserved, waiting, ready, missing };

    // A foreign shader override is not evidence that BCNG should reclaim it.
    // Require a selected cache path still present in the live shader. Paths
    // alone cannot establish that the actual required resource loaded.
    [[nodiscard]] constexpr bool MissingOwnedBodyResource(bool cachedSelection,
        bool shaderPathMatches, bool texturePresent, bool rendererPresent) noexcept
    {
        return cachedSelection && shaderPathMatches && (!texturePresent || !rendererPresent);
    }

    struct BodyTextureRecoveryBudget final
    {
        std::uint64_t generation{};
        unsigned attempts{};
        [[nodiscard]] constexpr bool Take(std::uint64_t current) noexcept
        {
            if (generation != current) { generation = current; attempts = 0; }
            if (attempts >= 2U) return false;
            ++attempts;
            return true;
        }
    };

    // Flat BOD2 slots 32/33/37/53 precede all runtime-inserted weapon indices.
    // Only current private ARMA parts, no buffered objects or retained pointers.
    template<class Biped, class OwnsAddon, class Observe>
    void VisitOwnedBodyParts(Biped& biped, OwnsAddon&& ownsAddon, Observe&& observe)
    {
        constexpr std::array<std::size_t, 4> slots{2U, 3U, 7U, 23U};
        using Part = decltype(biped.objects[0].partClone.get());
        std::array<Part, slots.size()> seen{};
        std::size_t count{};
        for (const auto slot : slots) {
            auto& object = biped.objects[slot];
            auto* part = object.partClone.get();
            if (!part || !object.addon || !ownsAddon(object.addon)) continue;
            bool duplicate{};
            for (std::size_t i{}; i < count; ++i) duplicate |= seen[i] == part;
            if (duplicate) continue;
            seen[count++] = part;
            observe(object, part);
        }
    }
}
