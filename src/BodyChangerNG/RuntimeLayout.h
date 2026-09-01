#pragma once

#include <REL/Version.h>

#include <cstdint>
#include <optional>

namespace bcn::runtime
{
    struct RendererHookLayout
    {
        std::uint64_t relocationID{};
        std::uintptr_t callOffset{};
    };

    struct InputPollHookLayout
    {
        std::uint64_t relocationID{};
        std::uintptr_t callOffset{};
        const char* name{};
    };

    // Direct ImGui rendering needs a verified point immediately after Skyrim
    // creates the D3D11 renderer.  Never guess a future layout: an absent
    // entry leaves the UI disabled instead of installing a wrong trampoline.
    [[nodiscard]] constexpr std::optional<RendererHookLayout> ResolveRendererHook(
        const REL::Version version) noexcept
    {
        if (version.major() != 1 || version.build() != 0) return std::nullopt;
        if (version.minor() == 5 && version.patch() == 97) {
            return RendererHookLayout{ 75595, 0x50 };
        }
        if (version.minor() != 6) return std::nullopt;
        switch (version.patch()) {
        case 317:
        case 318:
        case 323:
        case 342:
        case 353:
        case 629:
        case 640:
        case 659:
        case 678:
        case 1130:
        case 1170:
        case 1179:
            return RendererHookLayout{ 77226, 0x2BC };
        default:
            return std::nullopt;
        }
    }

    // Verified BSInputDeviceManager::PollInputDevices call sites. The input
    // filter is deliberately unavailable on an unlisted runtime rather than
    // guessing a call site and risking a global input lock or CTD.
    [[nodiscard]] constexpr std::optional<InputPollHookLayout> ResolveInputPollHook(
        const REL::Version version) noexcept
    {
        if (version == REL::Version{ 1, 5, 97, 0 }) {
            return InputPollHookLayout{ 67315, 0x7B, "Skyrim SE 1.5.97" };
        }
        if (version.major() != 1 || version.minor() != 6 || version.build() != 0) {
            return std::nullopt;
        }
        switch (version.patch()) {
        case 317:
        case 318:
        case 323:
        case 342:
        case 353:
        case 629:
        case 640:
        case 659:
        case 678:
        case 1130:
        case 1170:
        case 1179:
            return InputPollHookLayout{ 68617, 0x7B, "Skyrim AE 1.6.x" };
        default:
            return std::nullopt;
        }
    }
}
