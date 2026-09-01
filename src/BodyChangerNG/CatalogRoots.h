#pragma once

#include <filesystem>
#include <vector>

namespace bcn::catalog_roots
{
    // MO2's USVFS can retain a launch-time merged directory enumeration even
    // though files added to an already mounted physical provider are readable.
    // Return those provider roots first and the logical Data root last, so the
    // current virtual winner replaces duplicate physical entries.
    [[nodiscard]] std::vector<std::filesystem::path> Discover(
        const std::filesystem::path& logicalRoot);
}
