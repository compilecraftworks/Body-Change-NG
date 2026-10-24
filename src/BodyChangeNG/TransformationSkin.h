#pragma once

#include <cstdint>

namespace RE { class Actor; class TESRace; }

namespace bcn::transformation_skin
{
    enum class Form : std::uint8_t { ordinary, vampireLord, werewolf };

    [[nodiscard]] constexpr bool Suspend(Form form, bool keepVampireLord, bool keepWerewolf)
    {
        return (form == Form::vampireLord && !keepVampireLord) ||
            (form == Form::werewolf && !keepWerewolf);
    }

    [[nodiscard]] Form Classify(const RE::TESRace* race);
    // This policy is player-only. Ordinary vampires and NPC distribution are
    // deliberately unaffected; these options concern transformation races.
    [[nodiscard]] bool Suspended(const RE::Actor* actor);
}
