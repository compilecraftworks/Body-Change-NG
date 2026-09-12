#pragma once

#include "BodyChangeNG/FutanariRouting.h"
#include "BodyChangeNG/SkinProfiles.h"

#include <cstdint>
#include <optional>

namespace RE
{
    class Actor;
}

namespace bcn::futanari_support
{
    enum class Provider : std::uint8_t
    {
        none = 0U,
        sos = 1U << 0U,
        tng = 1U << 1U
    };

    struct Availability final
    {
        std::uint8_t providers{};
        bool trx{};
        bool erf{};
        bool ube{};

        [[nodiscard]] constexpr bool Any() const noexcept
        {
            return providers != 0U && (trx || erf || ube);
        }

        [[nodiscard]] constexpr bool Has(const Provider provider) const noexcept
        {
            return (providers & static_cast<std::uint8_t>(provider)) != 0U;
        }
    };

    // Scans loaded forms, not folders or hard-coded plugin names. Ordinary
    // male SOS/TNG addons never enable the optional futanari feature.
    void RefreshInstalledAddons();
    [[nodiscard]] Availability InstalledAddons();
    [[nodiscard]] bool Available();

    // Returns a type only when a female actor is actually registered to a
    // loaded female SOS/TNG addon. Covering armor and a temporarily detached
    // slot-52 geometry do not erase provider registration.
    [[nodiscard]] std::optional<FutanariSkinType> RegisteredType(RE::Actor* actor);
    [[nodiscard]] bool Registered(RE::Actor* actor);
}
