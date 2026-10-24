#include "BodyChangeNG/TransformationSkin.h"
#include "BodyChangeNG/Settings.h"

#include <array>
#include <algorithm>

namespace bcn::transformation_skin
{
    Form Classify(const RE::TESRace* race)
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!race || !data) return Form::ordinary;
        const auto* vampireLord = data->LookupForm<RE::TESRace>(0x283A, "Dawnguard.esm");
        const auto* werewolf = data->LookupForm<RE::TESRace>(0xCDD84, "Skyrim.esm");
        // Armor inheritance also covers custom humanoid transformation races.
        // Never use the Vampire keyword: ordinary vampires have it as well.
        std::array<const RE::TESRace*, 16> seen{};
        std::size_t count{};
        while (race && count < seen.size()) {
            if (race == vampireLord) return Form::vampireLord;
            if (race == werewolf) return Form::werewolf;
            if (std::find(seen.begin(), seen.begin() + count, race) != seen.begin() + count) break;
            seen[count++] = race;
            race = race->armorParentRace;
        }
        return Form::ordinary;
    }

    bool Suspended(const RE::Actor* actor)
    {
        if (!actor || actor->GetFormID() != 0x14) return false;
        const auto form = Classify(actor->GetRace());
        if (form == Form::ordinary) return false;
        const auto settings = Settings::Get().Snapshot();
        return Suspend(form, settings.keepSkinDuringVampireLord, settings.keepSkinDuringWerewolf);
    }
}
