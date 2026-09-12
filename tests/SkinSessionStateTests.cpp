#include "BodyChangeNG/SkinSessionState.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void Require(const bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try {
        using namespace bcn::skin_session;
        constexpr ActorId actorA{ 0x14U };
        constexpr ActorId actorB{ 0x1234U };

        Reset();
        const auto first = BeginSkinChange(actorA);
        const auto second = BeginSkinChange(actorA);
        Require(first != second, "skin generations were reused");
        Require(!IsCurrentSkinChange(actorA, first) && IsCurrentSkinChange(actorA, second),
            "an obsolete skin task remained current");
        Require(!IsCurrentSkinChange(actorB, second), "skin generations leaked between actors");
        Require(CurrentSkinGeneration(actorA) == second, "current skin generation was lost");

        Require(!HasTrackedSelection(actorA), "reset retained a skin selection");
        TrackSkinSelection(actorA, {});
        Require(HasTrackedSelection(actorA) && !RuntimeProfileId(actorA),
            "tracked Default became indistinguishable from untracked state");
        TrackSkinSelection(actorA, "cbbe-demo");
        Require(RuntimeProfileId(actorA) == "cbbe-demo", "profile selection was not retained");

        Require(!CachedFutanariType(actorA).cached, "empty futanari cache reported a hit");
        CacheFutanariType(actorA, std::nullopt);
        const auto cachedNone = CachedFutanariType(actorA);
        Require(cachedNone.cached && !cachedNone.type,
            "cached no-geometry result became an uncached result");
        constexpr auto erf = static_cast<bcn::FutanariSkinType>(2U);
        CacheFutanariType(actorA, erf);
        Require(CachedFutanariType(actorA).type == erf,
            "detected futanari family was not retained");
        InvalidateFutanariType(actorA);
        Require(!CachedFutanariType(actorA).cached, "futanari invalidation failed");

        using Channel = AddonTextureChannel;
        Require(!ReconciledAddonSignature(actorA, Channel::maleGenitals) &&
                !ReconciledAddonSignature(actorA, Channel::futanari),
            "an empty session exposed a genital addon signature");
        Require(!NeedsAddonReconcile(std::nullopt, 0U) &&
                NeedsAddonReconcile(std::nullopt, 0x1234U) &&
                !NeedsAddonReconcile(0x1234U, 0x1234U) &&
                NeedsAddonReconcile(0x1234U, 0x5678U),
            "genital reapply filtering did not distinguish missing, unchanged, and replaced addons");
        MarkAddonReconciled(actorA, Channel::maleGenitals, 0x1234U);
        MarkAddonReconciled(actorA, Channel::futanari, 0x5678U);
        Require(ReconciledAddonSignature(actorA, Channel::maleGenitals) == 0x1234U &&
                ReconciledAddonSignature(actorA, Channel::futanari) == 0x5678U,
            "independent genital addon identities crossed channels");
        ClearAddonReconciled(actorA, Channel::maleGenitals);
        Require(!ReconciledAddonSignature(actorA, Channel::maleGenitals) &&
                ReconciledAddonSignature(actorA, Channel::futanari) == 0x5678U,
            "clearing one genital addon identity changed the other channel");

        static_cast<void>(BeginFutanariChange(actorA));
        Forget(actorA);
        Require(!HasTrackedSelection(actorA) && !CurrentSkinGeneration(actorA) &&
                !ReconciledAddonSignature(actorA, Channel::futanari),
            "actor teardown left session state behind");

        Reset();
        Require(!HasTrackedSelection(actorA) && !CachedFutanariType(actorA).cached &&
                !ReconciledAddonSignature(actorA, Channel::maleGenitals),
            "session reset left cached state behind");
        std::cout << "Skin session state tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
