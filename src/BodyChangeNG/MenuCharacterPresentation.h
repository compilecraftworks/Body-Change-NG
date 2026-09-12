#pragma once

#include "BodyChangeNG/OverlayTypes.h"
#include "BodyChangeNG/Settings.h"

#include <optional>

namespace RE
{
    class Actor;
}

namespace bcn::menu_character
{
    // Temporary third-person menu presentation. It never changes an actor's
    // world position and restores every camera/actor value it owns on close.
    class Presentation final
    {
    public:
        static Presentation& Get();

        void Apply(CharacterPosition a_side, RE::Actor* a_actor);
        void Restore();
        void SetTintFocus(bool a_tintTab);
        void SetOverlayFocus(std::optional<overlay::Area> a_area);
        void UpdateRotationInteraction();

    private:
        Presentation() = default;

        struct State;
        State* state_{};
    };
}
