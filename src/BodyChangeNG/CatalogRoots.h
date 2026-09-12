#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace bcn::catalog_roots
{
    // Resolves a logical MO2/USVFS path to the physical winning provider when
    // Windows exposes one.  Consumers may use the provider folder name as
    // catalog metadata evidence; failure simply means that only the logical
    // resource path is available (for example for a BSA member).
    [[nodiscard]] std::optional<std::filesystem::path> ResolveProviderPath(
        const std::filesystem::path& logicalPath);

    // MO2's USVFS can retain a launch-time merged directory enumeration even
    // though files added to an already mounted physical provider are readable.
    // Return those provider roots first and the logical Data root last, so the
    // current virtual winner replaces duplicate physical entries.
    [[nodiscard]] std::vector<std::filesystem::path> Discover(
        const std::filesystem::path& logicalRoot);
}
