#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/ActorWorkQueue.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void Require(const bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
}

int main()
{
    try {
        using bcn::StableStateSignature;
        const auto body = StableStateSignature("body", "preset-a", false, 0U);
        Require(body == StableStateSignature("body", "preset-a", false, 0U),
            "identical body state was not stable");
        Require(body != StableStateSignature("body", "preset-b", false, 0U),
            "different preset IDs collided");
        Require(body != StableStateSignature("skin", "preset-a", false, 0U),
            "channel identity was not included");
        Require(body != StableStateSignature("body", "preset-a", true, 0U),
            "default and selected state collided");
        Require(body != StableStateSignature("body", "preset-a", false, 1U),
            "randomization options were not included");
        Require(bcn::UsesQueuedAutomaticPath(false) && bcn::UsesQueuedAutomaticPath(true),
            "an automatic actor mode bypassed the coalescing queue");
        Require(bcn::AutomaticDrainDelayHops(false) == 0U,
            "normal mode unexpectedly delayed the first queued actor");
        Require(bcn::AutomaticDrainDelayHops(true) == 1U,
            "performance mode did not add its conservative scheduling hop");

        bcn::ActorState state{
            .actorFormID = 0x1234U,
            .baseLocalFormID = 0x5678U,
            .basePlugin = "Example.esp",
            .selectedBodyId = "preset-a",
            .selectedSkinId = "skin-a",
            .manualBody = true,
            .manualSkin = true,
            .bodySignature = body,
            .skinSignature = StableStateSignature("skin", "skin-a", false)
        };
        Require(state.manualBody && state.manualSkin && state.selectedBodyId != state.selectedSkinId,
            "body and skin channels did not remain independent");
        std::cout << "ActorStateTests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ActorStateTests failed: " << error.what() << '\n';
        return 1;
    }
}
