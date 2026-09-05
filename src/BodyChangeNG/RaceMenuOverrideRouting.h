#pragma once

#include <cstdint>
#include <string_view>

namespace bcn::racemenu_override
{
    enum class Route : std::uint8_t
    {
        unsupported,
        legacySeV0Papyrus,
        aeBackportV0Papyrus,
        officialV1Papyrus,
        officialV2Native
    };

    [[nodiscard]] constexpr Route ResolveRoute(
        const std::uint32_t interfaceVersion, const bool aeRuntime) noexcept
    {
        if (interfaceVersion == 0U) {
            return aeRuntime ? Route::aeBackportV0Papyrus : Route::legacySeV0Papyrus;
        }
        if (interfaceVersion == 1U) return Route::officialV1Papyrus;
        if (interfaceVersion == 2U) return Route::officialV2Native;
        return Route::unsupported;
    }

    [[nodiscard]] constexpr bool UsesPapyrus(const Route route) noexcept
    {
        return route == Route::legacySeV0Papyrus ||
            route == Route::aeBackportV0Papyrus ||
            route == Route::officialV1Papyrus;
    }

    [[nodiscard]] constexpr bool UsesNativeV2(const Route route) noexcept
    {
        return route == Route::officialV2Native;
    }

    [[nodiscard]] constexpr std::string_view RouteLabel(const Route route) noexcept
    {
        switch (route) {
        case Route::legacySeV0Papyrus: return "legacy-se-v0-papyrus";
        case Route::aeBackportV0Papyrus: return "ube-ae-backport-v0-papyrus";
        case Route::officialV1Papyrus: return "official-v1-papyrus";
        case Route::officialV2Native: return "official-v2-native";
        default: return "unsupported";
        }
    }
}
