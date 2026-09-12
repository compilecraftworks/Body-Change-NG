#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <cstdint>
#include <array>
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
        body_family::Mask bodyFamilies{};
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

    struct OriginalBackup final
    {
        std::uint8_t type{};
        std::string texturePath;
        std::array<std::uint8_t, 3> color{};
        float alpha{};
    };

    struct PersistedState final
    {
        std::optional<std::string> pack;
        std::vector<PersistedLayerState> layers;
        std::vector<OriginalBackup> backups;
    };

    enum class ApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidAsset,
        incompatibleBodyFamily,
        unsupportedLayer,
        noTaskInterface,
        noOriginalBackup
    };

    class Catalog final
    {
    public:
        static Catalog& Get();

        // Searches BodySkin\\<pack>\\textures\\...\\tintmasks. Body-skin and
        // tint rows share one top-level pack folder but keep independent
        // scanners, IDs, application state, and generated cache namespaces.
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
    [[nodiscard]] constexpr bool TintMatchesActor(
        const body_family::Mask tintFamilies, const body_family::Mask actorFamily) noexcept
    {
        if (tintFamilies == 0U || actorFamily == 0U) return true;
        const auto ube = body_family::Bit(body_family::Family::ube);
        const auto legacyFemale = body_family::kFemaleFamilies & ~ube;
        const auto actorHasUbe = (actorFamily & ube) != 0U;
        const auto actorHasLegacyFemale = (actorFamily & legacyFemale) != 0U;
        if (actorHasUbe && !actorHasLegacyFemale) return (tintFamilies & ube) != 0U;
        if (actorHasLegacyFemale && !actorHasUbe) return (tintFamilies & legacyFemale) != 0U;
        if ((actorFamily & body_family::kMaleFamilies) != 0U) {
            return (tintFamilies & body_family::kMaleFamilies) != 0U;
        }
        // Missing or contradictory live layout evidence must not hide every
        // usable row. ResolveActor normally supplies a single verified family.
        return (tintFamilies & actorFamily) != 0U;
    }
    [[nodiscard]] constexpr bool TintAssetMatchesActor(const Sex tintSex,
        const body_family::Mask tintFamilies, const body_family::Mask actorFamily,
        const bool actorFemale) noexcept
    {
        const auto sexMatches = tintSex == Sex::unisex ||
            (actorFemale ? tintSex == Sex::female : tintSex == Sex::male);
        return sexMatches && TintMatchesActor(tintFamilies, actorFamily);
    }
    [[nodiscard]] std::string TintFamilyLabel(body_family::Mask a_families);
    // Returns the one usable asset for the player's current sex, active tint
    // layer, and race. Race-specific files for other races are never used as
    // fallbacks; a race-neutral file is used only when the pack provides one.
    [[nodiscard]] std::optional<Asset> BestAssetForPlayer(std::string_view a_pack, Layer a_layer);
    [[nodiscard]] std::optional<Color> CurrentColor(Layer a_layer);
    // Returns the RaceMenu-authored RGBA captured immediately before this mod
    // first changed the layer. UI restore controls use the same backup as the
    // queued world restore so their preview cannot remain stale.
    [[nodiscard]] std::optional<Color> OriginalColor(Layer a_layer);
    // Applies one best-matching DDS for every supported layer in a top-level
    // BodySkin pack. Race-specific filenames are preferred over generic ones,
    // while the character's existing color and opacity are preserved.
    [[nodiscard]] ApplyResult QueueApplyPack(std::string a_pack,
        std::vector<PersistedLayerState> a_layerDrafts = {}, bool a_commitPreview = false);
    // Returns the whole tint pack used as the current base. Per-layer detailed
    // edits and restores are tracked on top of this base for RaceMenu rebuilds.
    [[nodiscard]] std::optional<std::string> CurrentPack();
    // Rebuilds the current pack, followed by detailed per-layer edits/restores,
    // after RaceMenu has recreated the player's tint arrays.
    [[nodiscard]] ApplyResult QueueReapplyCurrent();
    // Restores every tint layer captured immediately before Body Change NG's
    // first change, i.e. the values authored in the current save/RaceMenu
    // character preset.
    [[nodiscard]] ApplyResult QueueRestoreAll(bool a_commitPreview = false);
    // Save serialization reads the committed baseline while the UI previews.
    void BeginPreview();
    // The current tint selection is save-specific.  SKSE serialization keeps
    // this small descriptor so RaceMenu rebuilds can restore the same pack and
    // detailed edits without storing texture data in the global settings file.
    [[nodiscard]] PersistedState SnapshotPersistedState();
    void RestorePersistedState(PersistedState a_state, bool a_restoreBackups = true);
    void ResetPersistedState();
}
