#pragma once

#include "BodyChangeNG/ActorState.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bcn::overlay
{
    // Normal browsing has one temporary item. Distribution checkboxes own a
    // separate temporary stack; neither changes the committed vector.
    struct PreviewState final
    {
        std::vector<OverlayItemState> original;
        std::optional<OverlayItemState> live;
        bool liveDefault{};
        bool batchMode{};
        bool capacityLimited{};
        std::vector<OverlayItemState> batch;

        template <class Visit>
        void VisitLive(Visit&& visit) const
        {
            if (liveDefault) return;
            if (live) visit(*live);
            for (const auto& item : batch) visit(item);
        }

        [[nodiscard]] const OverlayItemState* FindLive(std::string_view id) const noexcept
        {
            if (liveDefault) return nullptr;
            if (live && live->selectedId == id) return &*live;
            const auto found = std::ranges::find(batch, id, &OverlayItemState::selectedId);
            return found == batch.end() ? nullptr : &*found;
        }

        // Batch color previews may borrow an already committed node. Restore
        // its committed color instead of removing that node on cancel/switch.
        // A reset/reassignment invalidates the loan; never resurrect old state.
        [[nodiscard]] const OverlayItemState* BorrowedSelection(
            const std::span<const OverlayItemState> committed) const noexcept
        {
            return live ? BorrowedSelection(*live, committed) : nullptr;
        }

        [[nodiscard]] const OverlayItemState* BorrowedSelection(const OverlayItemState& value,
            const std::span<const OverlayItemState> committed) const noexcept
        {
            if (liveDefault || value.ownedSlot == overlay::kNoOwnedSlot) return nullptr;
            const auto matches = [&](const OverlayItemState& item) {
                return item.selectedId == value.selectedId &&
                    item.texturePath == value.texturePath && item.ownedSlot == value.ownedSlot;
            };
            if (std::ranges::none_of(original, matches)) return nullptr;
            const auto found = std::ranges::find_if(committed, matches);
            return found == committed.end() ? nullptr : &*found;
        }
    };

    // Remove only unchecked items, retain checked ownership, then apply the
    // desired colors/new items. The callback sees slots reserved earlier in
    // this same batch, before deferred RaceMenu writes make them visible.
    template <class Remove, class Upsert>
    [[nodiscard]] std::vector<OverlayItemState> ReconcilePreviewItems(
        const std::span<const OverlayItemState> previous,
        const std::span<const OverlayItemState> desired, Remove&& remove, Upsert&& upsert)
    {
        std::vector<OverlayItemState> result;
        for (const auto& item : previous) {
            const auto retained = std::ranges::any_of(desired, [&](const auto& wanted) {
                return wanted.selectedId == item.selectedId && wanted.texturePath == item.texturePath;
            });
            if (retained) result.push_back(item);
            else remove(item);
        }
        for (const auto& wanted : desired) {
            auto found = std::ranges::find(result, wanted.selectedId, &OverlayItemState::selectedId);
            const auto index = static_cast<std::size_t>(found - result.begin());
            const auto applied = upsert(wanted, found == result.end() ? nullptr : &*found,
                std::span<const OverlayItemState>{ result });
            if (!applied) continue;
            if (index == result.size()) result.push_back(*applied);
            else result[index] = *applied;
        }
        return result;
    }
}
