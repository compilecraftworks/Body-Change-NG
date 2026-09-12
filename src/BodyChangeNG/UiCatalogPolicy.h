#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bcn::ui_catalog
{
    enum class Tab : std::uint8_t
    {
        body,
        skin,
        tint,
        futanari,
        overlay,
        count
    };

    constexpr std::array kCanonicalOrder{
        Tab::body, Tab::skin, Tab::tint, Tab::futanari, Tab::overlay
    };

    struct AvailableTabs final
    {
        std::array<Tab, static_cast<std::size_t>(Tab::count)> values{};
        std::size_t size{};
    };

    [[nodiscard]] constexpr AvailableTabs ResolveAvailableTabs(
        const bool playerSelected, const bool futanariAvailable) noexcept
    {
        AvailableTabs result;
        result.values[result.size++] = Tab::body;
        result.values[result.size++] = Tab::skin;
        if (playerSelected) result.values[result.size++] = Tab::tint;
        if (futanariAvailable) result.values[result.size++] = Tab::futanari;
        result.values[result.size++] = Tab::overlay;
        return result;
    }

    enum class ChoiceIntent : std::uint8_t
    {
        none,
        preview,
        confirm
    };

    // ImGui reports the second click of a double-click as both clicked and
    // double-clicked. Confirmation must therefore take precedence.
    [[nodiscard]] constexpr ChoiceIntent MouseIntent(
        const bool clicked, const bool doubleClicked) noexcept
    {
        if (doubleClicked) return ChoiceIntent::confirm;
        return clicked ? ChoiceIntent::preview : ChoiceIntent::none;
    }
}
