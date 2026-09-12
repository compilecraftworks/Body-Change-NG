#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace bcn::racemenu_form_delete
{
    struct CallbackPlan final {
        std::uint32_t entry{}, object{}, armorErase{}, nodeErase{};
        bool operator==(const CallbackPlan&) const = default;
    };

    // Recognize the COMPLETE erroneous function, not a product version or a
    // short signature. The two MOV EDXs prove narrowing BEFORE either erase.
    // Original uint64-handle callbacks (REX.W MOV RDX) must not match.
    // LEA/CALL displacements are decoded, never reused across DLL versions.
    [[nodiscard]] inline std::optional<CallbackPlan> DecodeNarrowingCallback(
        std::span<const std::uint8_t> code, std::uint32_t rva, std::uint32_t imageSize)
    {
        constexpr std::array<std::uint8_t,42> shape{
            0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x8b,0xd1,
            0x48,0x8d,0x0d,0,0,0,0,0xe8,0,0,0,0,
            0x8b,0xd3,0x48,0x8d,0x0d,0,0,0,0,0x48,0x83,0xc4,0x20,
            0x5b,0xe9,0,0,0,0};
        if (code.size()!=shape.size() || rva>imageSize || code.size()>imageSize-rva) return std::nullopt;
        for (std::size_t i{}; i<shape.size(); ++i) {
            if ((i>=14 && i<18) || (i>=19 && i<23) || (i>=28 && i<32) || i>=38) continue;
            if (code[i]!=shape[i]) return std::nullopt;
        }
        const auto target = [&](std::size_t displacement) -> std::optional<std::uint32_t> {
            std::int32_t value{};
            std::memcpy(&value,code.data()+displacement,4);
            const auto absolute=static_cast<std::int64_t>(rva)+displacement+4+value;
            if (absolute<=0 || absolute>=imageSize) return std::nullopt;
            return static_cast<std::uint32_t>(absolute);
        };
        const auto object=target(14), objectAgain=target(28), armor=target(19), node=target(38);
        if (!object || object!=objectAgain || !armor || !node || armor==node ||
            *armor==rva || *node==rva) return std::nullopt;
        return CallbackPlan{rva,*object,*armor,*node};
    }
}
