#pragma once

#include "BodyChangeNG/NativeAddonPolicy.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinTargetResolver.h"

namespace bcn::native_addon
{
    struct MutationResult final
    {
        std::size_t geometries{};
        std::size_t textureLayers{};
        std::size_t targets{};
        bool changed{};
        [[nodiscard]] explicit operator bool() const noexcept
        { return geometries != 0 && geometries == targets && textureLayers != 0; }
    };

    void Install();
    [[nodiscard]] bool Available() noexcept;
    // Publishes value-only selection data. Caller requests normal native 3D
    // refresh after publication. No live shader/material write takes place.
    [[nodiscard]] MutationResult Apply(std::uint32_t actor, Channel channel,
        const std::vector<skin_target::LoadedPartTarget>& targets,
        const std::vector<SkinTextureLayer>& layers, std::string_view cacheNamespace);
    bool Clear(std::uint32_t actor);
    void Reset();
    void Forget(std::uint32_t actor);
    [[nodiscard]] bool HasSelection(std::uint32_t actor, Channel channel);
    [[nodiscard]] std::string SourceDiffuseTexture(std::uint32_t actor, Channel channel,
        std::uint32_t armor, std::uint32_t addon, const RE::BSGeometry& geometry);
}
