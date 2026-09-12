#pragma once

#include <cstdint>

namespace bcn::appearance
{
    enum class Feature : std::uint8_t
    {
        bodyMorph,
        baseSkin,
        faceTint,
        maleGenitalAddon,
        futanariAddon,
        outfitMorph
    };

    enum class Event : std::uint8_t
    {
        equipmentChanged,
        actor3DAttached,
        niNodeUpdated,
        raceMenuClosed,
        saveLoaded
    };

    [[nodiscard]] constexpr bool NeedsReconcile(
        const Feature feature, const Event event) noexcept
    {
        switch (event) {
        case Event::equipmentChanged:
            return feature == Feature::maleGenitalAddon ||
                feature == Feature::futanariAddon || feature == Feature::outfitMorph;
        case Event::actor3DAttached:
            return feature == Feature::maleGenitalAddon ||
                feature == Feature::futanariAddon || feature == Feature::outfitMorph;
        case Event::niNodeUpdated:
            return feature == Feature::maleGenitalAddon ||
                feature == Feature::futanariAddon;
        case Event::raceMenuClosed:
            return feature == Feature::bodyMorph || feature == Feature::baseSkin ||
                feature == Feature::faceTint ||
                feature == Feature::maleGenitalAddon || feature == Feature::futanariAddon;
        case Event::saveLoaded:
            return true;
        }
        return false;
    }
}
