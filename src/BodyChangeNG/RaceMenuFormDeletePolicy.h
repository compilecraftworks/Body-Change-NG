#pragma once
#include <cstdint>

namespace bcn::racemenu_form_delete
{
    // Verified from loaded SE 1.5.97 SkyrimScript::HandlePolicy:
    // GetHandleForObject +0xC8 forms, +0x33 aliases, +0xAD unique references,
    // +0xD7 active effects; HandleIsType corroborates all four encodings.
    // Only a direct Form handle's low word identifies the object being deleted.
    // The low word of an alias/effect/unique-reference handle identifies its
    // parent, NOT the deleted object. Never truncate before this check.
    // The SE/AE startup adapter additionally checks the loaded VM's own empty,
    // Actor and NPC handles before enabling this encoding. No product version
    // or guessed offset substitutes for that runtime check.
    [[nodiscard]] constexpr bool IsDirectFormHandle(std::uint64_t handle) noexcept
    {
        return (handle >> 32U) == 0x0000FFFFULL &&
               static_cast<std::uint32_t>(handle) != 0U;
    }

    [[nodiscard]] constexpr bool VerifyDirectFormSamples(std::uint64_t empty,
        std::uint32_t actorId,std::uint64_t actorHandle,
        std::uint32_t baseId,std::uint64_t baseHandle) noexcept
    {
        return empty==0x0000FFFF00000000ULL && actorId && baseId && actorId!=baseId &&
            actorHandle==(empty|actorId) && baseHandle==(empty|baseId);
    }
}
