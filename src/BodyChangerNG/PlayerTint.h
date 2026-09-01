#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace bcn::player_tint
{
    enum class Layer : std::uint8_t
    {
        frekles,
        lips,
        cheeks,
        eyeliner,
        upperEyeSocket,
        lowerEyeSocket,
        skinTone,
        warPaint,
        frownLines,
        lowerCheeks,
        nose,
        chin,
        neck,
        forehead,
        dirt
    };

    enum class Sex : std::uint8_t
    {
        female,
        male,
        unisex
    };

    struct Asset final
    {
        std::string id;
        std::string pack;
        std::string name;
        std::string path;
        Layer layer{};
        Sex sex{ Sex::unisex };
        std::filesystem::path source;
    };

    struct Color final
    {
        float red{ 1.0F };
        float green{ 1.0F };
        float blue{ 1.0F };
        float alpha{ 1.0F };
    };

    struct PersistedLayerState final
    {
        Layer layer{};
        bool restored{};
        std::string assetID;
        Color color{};
    };

    struct PersistedState final
    {
        std::optional<std::string> pack;
        std::vector<PersistedLayerState> layers;
    };

    enum class ApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidAsset,
        unsupportedLayer,
        noTaskInterface,
        noOriginalBackup
    };

    class Catalog final
    {
    public:
        static Catalog& Get();

        // Searches the virtual game path TintMask\\<pack>\\textures\\...\\tintmasks. The
        // tint file itself is never copied or generated at runtime.
        void Refresh();
        [[nodiscard]] static std::vector<Asset> ScanDirectory(const std::filesystem::path& a_root);
        [[nodiscard]] std::vector<Asset> Snapshot() const;
        [[nodiscard]] std::optional<Asset> Find(std::string_view a_id) const;
        [[nodiscard]] static std::filesystem::path RootPath();

    private:
        mutable std::mutex lock_;
        std::vector<Asset> assets_;
    };

    [[nodiscard]] std::string_view LayerName(Layer a_layer);
    // Returns the one usable asset for the player's current sex, active tint
    // layer, and race. Race-specific files for other races are never used as
    // fallbacks; a race-neutral file is used only when the pack provides one.
    [[nodiscard]] std::optional<Asset> BestAssetForPlayer(std::string_view a_pack, Layer a_layer);
    [[nodiscard]] std::optional<Color> CurrentColor(Layer a_layer);
    // Returns the RaceMenu-authored RGBA captured immediately before this mod
    // first changed the layer. UI restore controls use the same backup as the
    // queued world restore so their preview cannot remain stale.
    [[nodiscard]] std::optional<Color> OriginalColor(Layer a_layer);
    [[nodiscard]] ApplyResult QueueApply(std::string a_assetID, Color a_color);
    // Applies one best-matching DDS for every supported layer in a top-level
    // TintMask pack. Race-specific filenames are preferred over generic ones,
    // while the character's existing color and opacity are preserved.
    [[nodiscard]] ApplyResult QueueApplyPack(std::string a_pack);
    // Returns the whole tint pack used as the current base. Per-layer detailed
    // edits and restores are tracked on top of this base for RaceMenu rebuilds.
    [[nodiscard]] std::optional<std::string> CurrentPack();
    // Rebuilds the current pack, followed by detailed per-layer edits/restores,
    // after RaceMenu has recreated the player's tint arrays.
    [[nodiscard]] ApplyResult QueueReapplyCurrent();
    [[nodiscard]] ApplyResult QueueRestore(Layer a_layer, std::string a_basePack = {});
    // Restores every tint layer captured immediately before Body Changer NG's
    // first change, i.e. the values authored in the current save/RaceMenu
    // character preset.
    [[nodiscard]] ApplyResult QueueRestoreAll();
    // The current tint selection is save-specific.  SKSE serialization keeps
    // this small descriptor so RaceMenu rebuilds can restore the same pack and
    // detailed edits without storing texture data in the global settings file.
    [[nodiscard]] PersistedState SnapshotPersistedState();
    void RestorePersistedState(PersistedState a_state);
    void ResetPersistedState();
}
