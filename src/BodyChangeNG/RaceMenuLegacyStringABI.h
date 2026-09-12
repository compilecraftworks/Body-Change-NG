#pragma once

#include <type_traits>

namespace bcn::racemenu_compat
{
    // Old SKSE BSFixedString crosses the DLL boundary as a trivial pointer-
    // sized value. CommonLib BSFixedString has a destructor and is passed
    // INDIRECTLY by MSVC x64, despite having the same sizeof. Keep its owning
    // temporary alive at the call site and pass only this trivial borrowed ABI.
    struct LegacyNodeName final
    {
        const char* data{};

        template <class OwningString>
        LegacyNodeName(const OwningString& value) noexcept : data(value.c_str()) {}
    };
    static_assert(sizeof(LegacyNodeName) == sizeof(const char*));
    static_assert(std::is_trivially_copyable_v<LegacyNodeName>);
    static_assert(std::is_trivially_destructible_v<LegacyNodeName>);
    static_assert(std::is_standard_layout_v<LegacyNodeName>);
}
