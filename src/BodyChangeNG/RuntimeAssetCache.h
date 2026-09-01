#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace bcn::runtime_assets
{
    // Drops only one catalog's mappings before a live rescan. Sources owned by
    // the other catalogs remain valid while this prefix is rebuilt.
    void ClearGameRelativeSources(std::string_view a_prefix);
    // Catalog scans may discover a file through its physical MO2 provider
    // before the merged virtual directory notices it. Remember that source so
    // a subsequent game-relative skin override can still be materialized.
    void RegisterGameRelativeSource(std::string_view a_path,
        const std::filesystem::path& a_source);
    // Bethesda texture paths are narrow strings and do not reliably resolve
    // arbitrary Unicode folder names on every Windows system locale. Create a
    // stable ASCII resource alias under Data\textures on first use. NTFS hard
    // links are attempted first; copying is only a fallback.
    [[nodiscard]] std::string TexturePath(const std::filesystem::path& a_source,
        std::string_view a_namespace);
    [[nodiscard]] std::string TexturePathFromGameRelative(std::string_view a_path,
        std::string_view a_namespace);
}
