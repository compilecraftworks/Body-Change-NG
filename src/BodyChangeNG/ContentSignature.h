#pragma once
#include <bit>
#include <cstdint>
#include <string_view>
namespace bcn
{
    struct ContentSignature {
        std::uint64_t value{14695981039346656037ULL};
        void Byte(unsigned char v) { value = (value ^ v) * 1099511628211ULL; }
        void Number(std::uint64_t v) { for (unsigned n{}; n < 8; ++n) { Byte(static_cast<unsigned char>(v)); v >>= 8; } }
        void Text(std::string_view v) { Number(v.size()); for (unsigned char c : v) Byte(c); }
        void Float(float v) { Number(std::bit_cast<std::uint32_t>(v)); }
    };
}
