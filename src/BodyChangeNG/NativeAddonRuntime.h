#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace bcn::native_addon
{
    struct CodeFingerprint final
    {
        std::uint32_t rva, size;
        std::uint64_t hash;
    };

    [[nodiscard]] constexpr std::uint64_t CodeHash(std::span<const std::uint8_t> bytes) noexcept
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto value : bytes) hash = (hash ^ value) * 1099511628211ULL;
        return hash;
    }

    // Read-only captures in build/audits/addon-txst/se-* (2026-09-11).
    // Exact SE 1.5.97 instruction ranges, not an address guess for other games.
    // Do not reuse this table for AE.
    inline constexpr std::array seOwnershipCode{
        CodeFingerprint{ 0x1CCD50, 0x271, 0x78B64F3F0A1E2011ULL },
        CodeFingerprint{ 0x12CF480, 0x11C, 0x7B9E7EEB458A298FULL },
        CodeFingerprint{ 0x2D1980, 0xD, 0xD04B6A6715F40A4BULL },
        CodeFingerprint{ 0xC61A30, 0x11, 0xBEA553A07FE8A303ULL },
        CodeFingerprint{ 0x2D2074, 0x9, 0xA53FB52550E5E21FULL },
        CodeFingerprint{ 0x2D11A0, 0xE9, 0x724F87E0505C0437ULL },
        CodeFingerprint{ 0x2D2230, 0x5C, 0x51DE6E4C8FEA4BE9ULL }
    };

    // Independently captured from TAKEALOOK 1.6.1170, PID 19600, main menu.
    inline constexpr std::array aeOwnershipCode{
        CodeFingerprint{ 0x219510, 0x273, 0x18659184E87C81DDULL },
        CodeFingerprint{ 0x14B7920, 0x11E, 0x84AE1945E2C4D1FFULL },
        CodeFingerprint{ 0x3270A0, 0xE, 0xC7B49EF6D28A4CFCULL },
        CodeFingerprint{ 0xD27520, 0x11, 0xBEA553A07FE8A303ULL },
        CodeFingerprint{ 0x327784, 0x9, 0xA694B30FC376BDDFULL },
        CodeFingerprint{ 0x3267F0, 0x118, 0xA3EB9E82AC84D2E5ULL },
        CodeFingerprint{ 0x326680, 0x5F, 0xE7A8A37D3219C88EULL }
    };
}
