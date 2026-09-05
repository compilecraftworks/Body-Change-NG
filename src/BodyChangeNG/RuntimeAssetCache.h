#pragma once

#include <filesystem>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace bcn::runtime_assets
{
    struct TexturePreparation final
    {
        std::string path;
        std::string nameSpace;
    };

    // Cached at explicit catalog registration; never reads DDS during NPC evaluation.
    std::uint64_t SourceContentHash(std::string_view path);
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
    // Resolves the stable cache alias without creating or changing a file.
    // Used by the one-time restored-state verifier to distinguish the exact
    // selected profile from a stale BCNG override left by another profile.
    [[nodiscard]] std::string ExpectedTexturePathFromGameRelative(std::string_view a_path,
        std::string_view a_namespace);
    // Accepts only a generated BCNG cache-relative path and performs a cheap
    // file metadata check. It never opens or hashes the DDS.
    [[nodiscard]] bool CachedTextureExists(std::string_view a_path);
    [[nodiscard]] std::string TexturePathFromGameRelative(std::string_view a_path,
        std::string_view a_namespace);
    // Prepares only the selected profile's effective actor layers on one
    // bounded worker. Completion runs on that worker and must hand any engine
    // access back to the game task queue.
    bool PrepareTexturePathsAsync(std::uint64_t a_key,
        std::vector<TexturePreparation> a_paths, std::function<void(bool)> a_completion,
        bool a_urgent = false);
    void InitializeTexturePreparation();
    void CancelTexturePreparations();
}
