#pragma once

#include <cstdint>
#include <optional>
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
        // Runtime-only proof. Serialized completion metadata is only a hint
        // until the new session verifies RaceMenu's live state once.
        bool bodyVerifiedThisSession{};
        bool skinVerifiedThisSession{};
        std::uint64_t bodySignature{};
        std::uint64_t skinSignature{};
        std::uint64_t outfitSignature{};
    };

    enum class RestoredApplicationDecision : std::uint8_t
    {
        apply,
        acceptLive,
        skipVerified
    };

    [[nodiscard]] constexpr RestoredApplicationDecision EvaluateRestoredApplication(
        const bool applied, const bool verifiedThisSession, const bool signatureMatches,
        const std::optional<bool> liveStateMatches) noexcept
    {
        if (!applied || !signatureMatches) return RestoredApplicationDecision::apply;
        if (verifiedThisSession) return RestoredApplicationDecision::skipVerified;
        return liveStateMatches.value_or(false) ? RestoredApplicationDecision::acceptLive :
            RestoredApplicationDecision::apply;
    }

    inline void PrepareRestoredState(ActorState& state) noexcept
    {
        state.bodyVerifiedThisSession = false;
        state.skinVerifiedThisSession = false;
    }

    [[nodiscard]] inline std::uint64_t StableStateSignature(const std::string_view channel,
        const std::string_view value, const bool useDefault, const std::uint32_t optionBits = 0U,
        const std::uint64_t contentHash = 0U) noexcept
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
        if (contentHash != 0) {
            append(0xFEU);
            for (std::uint32_t shift{}; shift < 64U; shift += 8U) append(static_cast<std::uint8_t>(contentHash >> shift));
        }
        return hash;
    }
}
