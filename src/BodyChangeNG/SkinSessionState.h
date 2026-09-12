#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace bcn
{
    enum class FutanariSkinType : std::uint8_t;
}

namespace bcn::skin_session
{
    using ActorId = std::uint32_t;

    enum class AddonTextureChannel : std::uint8_t
    {
        maleGenitals,
        futanari
    };

    struct FutanariCacheResult final
    {
        bool cached{};
        std::optional<FutanariSkinType> type;
    };

    [[nodiscard]] std::uint64_t BeginSkinChange(ActorId actorId);
    [[nodiscard]] bool IsCurrentSkinChange(ActorId actorId, std::uint64_t generation);
    [[nodiscard]] std::optional<std::uint64_t> CurrentSkinGeneration(ActorId actorId);

    [[nodiscard]] std::uint64_t BeginFutanariChange(ActorId actorId);
    [[nodiscard]] bool IsCurrentFutanariChange(ActorId actorId, std::uint64_t generation);

    // An empty value is a tracked Default selection. It is intentionally
    // different from an actor that has not entered Body Change NG this session.
    void TrackSkinSelection(ActorId actorId, std::string profileId);
    [[nodiscard]] std::optional<std::string> RuntimeProfileId(ActorId actorId);
    [[nodiscard]] bool HasTrackedSelection(ActorId actorId);

    [[nodiscard]] FutanariCacheResult CachedFutanariType(ActorId actorId);
    void CacheFutanariType(ActorId actorId, std::optional<FutanariSkinType> type);
    void InvalidateFutanariType(ActorId actorId);

    // Equipment events are broad: OStim and many outfit systems emit them for
    // unrelated armor. Remember the exact slot-52 clone most recently
    // reconciled, whether it was painted or restored to Default, so identical
    // events do not create duplicate material work.
    [[nodiscard]] std::optional<std::uint64_t> ReconciledAddonSignature(
        ActorId actorId, AddonTextureChannel channel);
    [[nodiscard]] constexpr bool NeedsAddonReconcile(
        const std::optional<std::uint64_t> reconciledSignature,
        const std::uint64_t currentSignature,
        const bool currentCloneHasBaseline = true) noexcept
    {
        return currentSignature != 0U &&
            (reconciledSignature != currentSignature || !currentCloneHasBaseline);
    }
    void MarkAddonReconciled(ActorId actorId, AddonTextureChannel channel,
        std::uint64_t signature);
    void ClearAddonReconciled(ActorId actorId, AddonTextureChannel channel);

    void Reset();
    void Forget(ActorId actorId);
}
