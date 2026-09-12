#include "BodyChangeNG/SkinSessionState.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace
{
    using ActorId = bcn::skin_session::ActorId;

    struct FutanariCacheValue final
    {
        std::optional<bcn::FutanariSkinType> type;
    };

    std::mutex g_lock;
    std::unordered_map<ActorId, std::string> g_skinSelections;
    std::unordered_map<ActorId, std::uint64_t> g_skinGenerations;
    std::unordered_map<ActorId, std::uint64_t> g_futanariGenerations;
    std::unordered_map<ActorId, FutanariCacheValue> g_futanariTypes;
    std::unordered_map<ActorId, std::uint64_t> g_maleGenitalSignatures;
    std::unordered_map<ActorId, std::uint64_t> g_futanariSignatures;

    std::atomic_uint64_t g_nextSkinGeneration{ 1U };
    std::atomic_uint64_t g_nextFutanariGeneration{ 1U };
}

namespace bcn::skin_session
{
    std::uint64_t BeginSkinChange(const ActorId actorId)
    {
        const auto generation = g_nextSkinGeneration.fetch_add(1U, std::memory_order_relaxed);
        std::scoped_lock lock(g_lock);
        g_skinGenerations.insert_or_assign(actorId, generation);
        return generation;
    }

    bool IsCurrentSkinChange(const ActorId actorId, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_skinGenerations.find(actorId);
        return found != g_skinGenerations.end() && found->second == generation;
    }

    std::optional<std::uint64_t> CurrentSkinGeneration(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_skinGenerations.find(actorId);
        return found == g_skinGenerations.end() ? std::nullopt :
            std::optional<std::uint64_t>{ found->second };
    }

    std::uint64_t BeginFutanariChange(const ActorId actorId)
    {
        const auto generation = g_nextFutanariGeneration.fetch_add(1U, std::memory_order_relaxed);
        std::scoped_lock lock(g_lock);
        g_futanariGenerations.insert_or_assign(actorId, generation);
        return generation;
    }

    bool IsCurrentFutanariChange(const ActorId actorId, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_futanariGenerations.find(actorId);
        return found != g_futanariGenerations.end() && found->second == generation;
    }

    void TrackSkinSelection(const ActorId actorId, std::string profileId)
    {
        std::scoped_lock lock(g_lock);
        g_skinSelections.insert_or_assign(actorId, std::move(profileId));
    }

    std::optional<std::string> RuntimeProfileId(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_skinSelections.find(actorId);
        if (found == g_skinSelections.end() || found->second.empty()) return std::nullopt;
        return found->second;
    }

    bool HasTrackedSelection(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        return g_skinSelections.contains(actorId);
    }

    FutanariCacheResult CachedFutanariType(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_futanariTypes.find(actorId);
        return found == g_futanariTypes.end() ? FutanariCacheResult{} :
            FutanariCacheResult{ true, found->second.type };
    }

    void CacheFutanariType(
        const ActorId actorId, const std::optional<FutanariSkinType> type)
    {
        std::scoped_lock lock(g_lock);
        g_futanariTypes.insert_or_assign(actorId, FutanariCacheValue{ type });
    }

    void InvalidateFutanariType(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        g_futanariTypes.erase(actorId);
    }

    std::optional<std::uint64_t> ReconciledAddonSignature(
        const ActorId actorId, const AddonTextureChannel channel)
    {
        std::scoped_lock lock(g_lock);
        const auto& signatures = channel == AddonTextureChannel::maleGenitals ?
            g_maleGenitalSignatures : g_futanariSignatures;
        const auto found = signatures.find(actorId);
        return found == signatures.end() ? std::nullopt :
            std::optional<std::uint64_t>{ found->second };
    }

    void MarkAddonReconciled(const ActorId actorId, const AddonTextureChannel channel,
        const std::uint64_t signature)
    {
        if (actorId == 0U || signature == 0U) return;
        std::scoped_lock lock(g_lock);
        auto& signatures = channel == AddonTextureChannel::maleGenitals ?
            g_maleGenitalSignatures : g_futanariSignatures;
        signatures.insert_or_assign(actorId, signature);
    }

    void ClearAddonReconciled(const ActorId actorId, const AddonTextureChannel channel)
    {
        std::scoped_lock lock(g_lock);
        auto& signatures = channel == AddonTextureChannel::maleGenitals ?
            g_maleGenitalSignatures : g_futanariSignatures;
        signatures.erase(actorId);
    }

    void Reset()
    {
        std::scoped_lock lock(g_lock);
        g_skinSelections.clear();
        g_skinGenerations.clear();
        g_futanariGenerations.clear();
        g_futanariTypes.clear();
        g_maleGenitalSignatures.clear();
        g_futanariSignatures.clear();
    }

    void Forget(const ActorId actorId)
    {
        std::scoped_lock lock(g_lock);
        g_skinSelections.erase(actorId);
        g_skinGenerations.erase(actorId);
        g_futanariGenerations.erase(actorId);
        g_futanariTypes.erase(actorId);
        g_maleGenitalSignatures.erase(actorId);
        g_futanariSignatures.erase(actorId);
    }
}
