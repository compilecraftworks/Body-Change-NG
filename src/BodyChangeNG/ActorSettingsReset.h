#pragma once

#include <cstddef>

namespace RE
{
    class Actor;
}

namespace bcn::actor_settings_reset
{
    struct Result final
    {
        std::size_t actors{};
        bool accepted{};
    };

    [[nodiscard]] Result QueueActor(RE::Actor* a_actor);
    [[nodiscard]] Result QueueAll();
}
