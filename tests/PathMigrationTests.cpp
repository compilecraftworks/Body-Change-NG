#include "BodyChangeNG/PathMigration.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "BodyChangeNGPathMigrationTests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto preferred = root / "BodyChangeNG" / "settings.json";
    const auto legacy = root / "BodyChangerNG" / "settings.json";

    auto resolved = bcn::path_migration::ResolveFile(preferred, legacy);
    if (!Require(resolved.path == preferred && !resolved.legacy,
            "missing files did not resolve to the preferred path")) return 1;

    std::filesystem::create_directories(legacy.parent_path());
    std::ofstream(legacy) << "legacy";
    resolved = bcn::path_migration::ResolveFile(preferred, legacy);
    if (!Require(resolved.path == legacy && resolved.legacy,
            "legacy file was not selected when the preferred path was absent")) return 1;

    std::filesystem::create_directories(preferred.parent_path());
    std::ofstream(preferred) << "preferred";
    resolved = bcn::path_migration::ResolveFile(preferred, legacy);
    if (!Require(resolved.path == preferred && !resolved.legacy,
            "preferred file did not win over the legacy fallback")) return 1;

    std::filesystem::remove_all(root);
    return 0;
}
