#pragma once

#include "BodyChangeNG/RuntimeCompatibility.h"

#include <cstdint>
#include <string_view>

namespace bcn::racemenu_compat
{
    enum class BodyMorphAbi : std::uint8_t
    {
        unsupported,
        v4,
        v5
    };

    enum class OverlayStackAbi : std::uint8_t
    {
        unsupported,
        legacyV1,
        publicV2
    };

    enum class NodeOverrideAbi : std::uint8_t { papyrus, legacyV1, publicV2 };
    [[nodiscard]] constexpr NodeOverrideAbi ResolveNodeOverrideAbi(
        std::uint32_t version, runtime::GameBranch branch) noexcept
    {
        if (branch == runtime::GameBranch::unsupported || version == 0) return NodeOverrideAbi::papyrus;
        return version == 1 ? NodeOverrideAbi::legacyV1 : NodeOverrideAbi::publicV2;
    }

    // Product/file versions do not gate features. For a newer interface
    // revision use the highest known lower contract, assuming its existing
    // prefix is retained. Never call added/unknown tail methods. This is a
    // forward-compatibility policy, not proof of an unknown provider's ABI.
    [[nodiscard]] constexpr BodyMorphAbi ResolveBodyMorphAbi(
        const std::uint32_t interfaceVersion, const runtime::GameBranch branch) noexcept
    {
        if (branch == runtime::GameBranch::unsupported) return BodyMorphAbi::unsupported;
        if (interfaceVersion == 4U) return BodyMorphAbi::v4;
        if (interfaceVersion >= 5U) return BodyMorphAbi::v5;
        return BodyMorphAbi::unsupported;
    }

    [[nodiscard]] constexpr std::string_view BodyMorphAbiLabel(const BodyMorphAbi abi) noexcept
    {
        switch (abi) {
        case BodyMorphAbi::v4: return "BodyMorph-v4";
        case BodyMorphAbi::v5: return "BodyMorph-v5";
        default: return "unsupported";
        }
    }

    // Official RaceMenu releases expose these as a matched pair.  0.4.16 and
    // early AE builds return the concrete legacy Overlay/Override v1 objects;
    // releases with the public overlay programmer API return the wrapper v2
    // objects. Their vtables and value types are not interchangeable. Newer
    // revisions of either public wrapper fall back independently to v2; do
    // not cast a legacy v1 object to v2 just to match its companion interface.
    [[nodiscard]] constexpr OverlayStackAbi ResolveOverlayStackAbi(
        const std::uint32_t overlayVersion, const std::uint32_t overrideVersion,
        const runtime::GameBranch branch) noexcept
    {
        if (branch == runtime::GameBranch::unsupported) return OverlayStackAbi::unsupported;
        if (overlayVersion == 1U && overrideVersion == 1U) return OverlayStackAbi::legacyV1;
        if (overlayVersion >= 2U && overrideVersion >= 2U) return OverlayStackAbi::publicV2;
        return OverlayStackAbi::unsupported;
    }

    [[nodiscard]] constexpr bool UsesBodyMorphFallback(const std::uint32_t version) noexcept
    {
        return version > 5U;
    }

    [[nodiscard]] constexpr bool UsesOverlayFallback(
        const std::uint32_t overlayVersion, const std::uint32_t overrideVersion) noexcept
    {
        return overlayVersion >= 2U && overrideVersion >= 2U &&
            (overlayVersion > 2U || overrideVersion > 2U);
    }

    [[nodiscard]] constexpr std::string_view OverlayStackAbiLabel(
        const OverlayStackAbi abi) noexcept
    {
        switch (abi) {
        case OverlayStackAbi::legacyV1: return "Overlay/Override-v1";
        case OverlayStackAbi::publicV2: return "Overlay/Override-v2";
        default: return "unsupported";
        }
    }
}
