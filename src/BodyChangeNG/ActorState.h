#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bcn
{
    struct ActorState final
    {
        std::uint32_t actorFormID{};
        std::uint32_t baseLocalFormID{};
        std::string basePlugin;

        std::string selectedBodyId;
        std::string selectedSkinId;
        bool manualBody{};
        bool manualSkin{};
        bool useDefaultBody{};
        bool useDefaultSkin{};

        std::string appliedBodyId;
        std::string appliedSkinId;
        bool appliedDefaultBody{};
        bool appliedDefaultSkin{};
        bool bodyApplied{};
        bool skinApplied{};
        std::uint64_t bodySignature{};
        std::uint64_t skinSignature{};
        std::uint64_t outfitSignature{};
    };

    [[nodiscard]] inline std::uint64_t StableStateSignature(const std::string_view channel,
        const std::string_view value, const bool useDefault, const std::uint32_t optionBits = 0U) noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto append = [&hash](const std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ULL; };
        for (const auto character : channel) append(static_cast<std::uint8_t>(character));
        append(0xFFU);
        for (const auto character : value) append(static_cast<std::uint8_t>(character));
        append(useDefault ? 1U : 0U);
        for (std::uint32_t shift{}; shift < 32U; shift += 8U) {
            append(static_cast<std::uint8_t>(optionBits >> shift));
        }
        return hash;
    }
}
