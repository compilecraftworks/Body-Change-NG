#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace bcn::racemenu_preset_migration
{
    struct HeadPartTarget
    {
        std::string plugin;
        std::string formIdentifier;
        std::uint32_t runtimeFormID{};
    };

    struct TransformResult
    {
        std::size_t replaced{};
        std::size_t removedDuplicates{};
        bool skippedUBE{};

        [[nodiscard]] bool Changed() const noexcept
        {
            return replaced != 0U || removedDuplicates != 0U;
        }
    };

    [[nodiscard]] bool HasUbeRaceDependency(const nlohmann::json& preset);
    [[nodiscard]] TransformResult TransformLegacyBodyChangeHeadParts(
        nlohmann::json& preset, const HeadPartTarget& target);
}
