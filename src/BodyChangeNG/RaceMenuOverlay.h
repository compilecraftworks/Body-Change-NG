#pragma once

#include "BodyChangeNG/OverlayTypes.h"
#include "BodyChangeNG/OverlaySlotAccounting.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RE
{
    class Actor;
    class GFxMovieView;
}

namespace bcn::overlay
{
    struct Entry final
    {
        std::string id;
        std::string name;
        std::string texturePath;
        Area area{ Area::body };
        Layout layout{ Layout::legacy };
        Sex sex{ Sex::unisex };
        Source source{ Source::raceMenu };
    };

    enum class ApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidActor,
        actor3DUnavailable,
        missingEntry,
        noFreeSlot,
        ownershipConflict,
        unsupportedInterface,
        unsupportedFace,
        incompatibleActor,
        noTaskInterface
    };

    bool InitializeCatalogBridge();
    void ResolveInstalledEntryMetadata(Entry& a_entry);
    bool RequestCatalog(RE::Actor* a_target);
    bool RefreshCatalog(RE::Actor* a_target);
    void EndCatalogRequest();
    [[nodiscard]] std::vector<Entry> Snapshot(Area a_area);
    [[nodiscard]] std::vector<Entry> SnapshotForActor(Area a_area, RE::Actor* a_actor);
    [[nodiscard]] std::vector<Entry> SnapshotLegacy(Area a_area, bool a_female);
    [[nodiscard]] std::optional<Entry> Find(std::string_view a_id);
    [[nodiscard]] std::uint64_t CatalogRevision() noexcept;
    [[nodiscard]] bool CatalogRequested() noexcept;
    void ResetCatalogSessionState();
    void UpdateProviderCounts(RE::GFxMovieView* a_movie);

    [[nodiscard]] bool IsReady() noexcept;
    [[nodiscard]] std::uint32_t InterfaceVersion() noexcept;
    [[nodiscard]] SlotUsage CurrentSlotUsage(RE::Actor* a_actor, Area a_area);
    [[nodiscard]] std::optional<std::string> CurrentSelectionId(
        const RE::Actor* a_actor, Area a_area);
    [[nodiscard]] std::vector<std::string> CurrentSelectionIds(
        const RE::Actor* a_actor, Area a_area);
    [[nodiscard]] bool HasActivePreview(const RE::Actor* a_actor);
    [[nodiscard]] std::optional<std::uint32_t> CurrentColor(
        const RE::Actor* a_actor, Area a_area, std::string_view a_entryId);
    [[nodiscard]] ApplyResult QueueColor(RE::Actor* a_actor, Area a_area,
        std::string a_entryId, std::uint32_t a_color);
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, Area a_area,
        std::string a_entryId, ApplyMode a_mode,
        std::optional<std::uint32_t> a_color = std::nullopt);
    [[nodiscard]] ApplyResult QueueClear(RE::Actor* a_actor, Area a_area,
        ApplyMode a_mode);
    // Removes every BCNG-owned paint from the captured actor state while
    // leaving foreign RaceMenu/SlaveTats slots untouched. Unlike QueueClear,
    // this is safe to pair with an immediate persistent state reset.
    [[nodiscard]] ApplyResult QueueReset(RE::Actor* a_actor, Area a_area);
    [[nodiscard]] ApplyResult QueueRemove(RE::Actor* a_actor, Area a_area,
        std::string a_entryId);
    // Transfer live preview ownership to pending removal after recording Default.
    void DiscardPreviewsForReset(RE::Actor* a_actor);
    void QueueCancelPreviews(RE::Actor* a_actor = nullptr);
    void QueueReapplySaved(RE::Actor* a_actor);
    void ForgetActorState(std::uint32_t a_actorFormID);
    void ResetSessionState();
}
