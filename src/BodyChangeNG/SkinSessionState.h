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

    [[nodiscard]] std::uint64_t BeginFaceRefresh(ActorId actorId);
    [[nodiscard]] bool IsCurrentFaceRefresh(ActorId actorId, std::uint64_t generation);
    void MarkTransientFace(ActorId actorId);
    [[nodiscard]] bool ReleaseTransientFace(ActorId actorId);
    [[nodiscard]] bool HasTransientFace(ActorId actorId);

    [[nodiscard]] bool ClaimLegacyCleanup(ActorId actorId);

    // An empty value is a tracked Default selection. It is intentionally
    // different from an actor that has not entered Body Change NG this session.
    void TrackSkinSelection(ActorId actorId, std::string profileId);
    [[nodiscard]] std::optional<std::string> RuntimeProfileId(ActorId actorId);
    [[nodiscard]] bool HasTrackedSelection(ActorId actorId);

    [[nodiscard]] FutanariCacheResult CachedFutanariType(ActorId actorId);
    void CacheFutanariType(ActorId actorId, std::optional<FutanariSkinType> type);
    void InvalidateFutanariType(ActorId actorId);

    void Reset();
    void Forget(ActorId actorId);
}
