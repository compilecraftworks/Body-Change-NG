#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace bcn::faction_editor_ids
{
    using IDs = std::unordered_map<std::uint32_t, std::string>;
    struct Result
    {
        IDs ids;
        bool valid{};
        std::uint64_t bytesRead{};
    };
    // Reads only TES4 metadata and requested, originally defined FACT records.
    // Master overrides with coinciding low IDs must never name another form.
    // 'wanted' IDs are plugin-local (never load-order-prefixed runtime IDs).
    [[nodiscard]] Result Read(std::istream& stream,
        const std::unordered_set<std::uint32_t>& wanted,
        const std::function<bool()>& current = {});
    [[nodiscard]] Result ReadFile(const std::filesystem::path& dataDirectory,
        std::string_view plugin, const std::unordered_set<std::uint32_t>& wanted,
        const std::function<bool()>& current = {});
}
