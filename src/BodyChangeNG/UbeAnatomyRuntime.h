#pragma once

#include <cstdint>

namespace RE { class Actor; }

namespace bcn::ube_anatomy
{
    [[nodiscard]] std::uint8_t SupportedExtras(RE::Actor* actor);
    void ClearSupportCache();
}
