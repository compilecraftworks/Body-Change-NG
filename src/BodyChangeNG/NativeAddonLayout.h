#pragma once
#include "BodyChangeNG/RuntimeCompatibility.h"

namespace bcn::native_addon
{
    struct RuntimeLayout final {
        bool ae{};
        std::uint64_t visitor{}, materialLoader{}, textureLoader{};
        std::uint64_t deleteThis{}, textureDestructor{}, constructor{}, factory{};
        std::uint64_t directCaller{}, recursiveCaller{};
    };

    // Addresses are resolved from the library for the ACTUAL loaded game.
    // Admission does not bypass the startup code/ownership/callsite proof.
    [[nodiscard]] constexpr std::optional<RuntimeLayout> ResolveRuntimeLayout(
        const REL::Version version) noexcept
    {
        switch (runtime::ResolveGameBranch(version)) {
        case runtime::GameBranch::se:
            return RuntimeLayout{ false, 15561, 100010, 20904, 69177, 20914, 20895, 20920, 15535, 15546 };
        case runtime::GameBranch::ae:
            return RuntimeLayout{ true, 15739, 106717, 21360, 70538, 21371, 21349, 21341, 15712, 15722 };
        default: return {};
        }
    }
}
