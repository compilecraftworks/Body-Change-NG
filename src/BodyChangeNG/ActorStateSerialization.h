#pragma once

#include "BodyChangeNG/ActorState.h"

#include <cstddef>
#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace bcn::actor_serialization
{
    enum StateFlags : std::uint16_t
    {
        kManualBody = 1U << 0U,
        kManualSkin = 1U << 1U,
        kDefaultBody = 1U << 2U,
        kDefaultSkin = 1U << 3U,
        kAppliedDefaultBody = 1U << 4U,
        kAppliedDefaultSkin = 1U << 5U,
        kBodyApplied = 1U << 6U,
        kSkinApplied = 1U << 7U
    };

    struct SerializedActorStateV2 final
    {
        std::uint32_t actorFormID{};
        std::uint32_t baseLocalFormID{};
        std::uint32_t basePluginIndex{};
        std::uint32_t selectedBodyIndex{};
        std::uint32_t selectedSkinIndex{};
        std::uint32_t selectedFutanariSkinIndex{};
        std::uint32_t appliedBodyIndex{};
        std::uint32_t appliedSkinIndex{};
        std::uint16_t flags{};
        std::uint16_t reserved{};
        std::uint64_t bodySignature{};
        std::uint64_t skinSignature{};
        std::uint64_t outfitSignature{};
    };

    // ASTR version 2 on-disk layout: do not reorder fields or change packing.
    static_assert(sizeof(SerializedActorStateV2) == 64);
    static_assert(offsetof(SerializedActorStateV2, bodySignature) == 40);
    static_assert(std::is_trivially_copyable_v<SerializedActorStateV2>);

    enum OverlayStateFlags : std::uint8_t
    {
        kOverlayManual = 1U << 0U,
        kOverlayDefault = 1U << 1U
    };

    enum FutanariStateFlags : std::uint16_t
    {
        kFutanariManual = 1U << 0U,
        kFutanariDefault = 1U << 1U
    };

    struct SerializedActorStateV3 final
    {
        // The v2 prefix is copied field-for-field rather than embedded so its
        // on-disk representation remains explicit and reviewable.
        std::uint32_t actorFormID{};
        std::uint32_t baseLocalFormID{};
        std::uint32_t basePluginIndex{};
        std::uint32_t selectedBodyIndex{};
        std::uint32_t selectedSkinIndex{};
        std::uint32_t selectedFutanariSkinIndex{};
        std::uint32_t appliedBodyIndex{};
        std::uint32_t appliedSkinIndex{};
        std::uint16_t flags{};
        std::uint16_t reserved{};
        std::uint64_t bodySignature{};
        std::uint64_t skinSignature{};
        std::uint64_t outfitSignature{};
        std::array<std::uint32_t, overlay::Index(overlay::Area::count)> overlayIdIndices{};
        std::array<std::uint32_t, overlay::Index(overlay::Area::count)> overlayPathIndices{};
        std::array<std::uint8_t, overlay::Index(overlay::Area::count)> overlaySlots{};
        std::array<std::uint8_t, overlay::Index(overlay::Area::count)> overlayFlags{};
    };

    // ASTR v3 appends overlay state after the byte-identical 64-byte v2
    // prefix. Current BCNG reads all three historical versions explicitly;
    // older builds safely reject the newer record version.
    static_assert(sizeof(SerializedActorStateV3) == 104);
    static_assert(offsetof(SerializedActorStateV3, overlayIdIndices) == 64);
    static_assert(std::is_trivially_copyable_v<SerializedActorStateV3>);

    struct SerializedActorStateV4 final
    {
        SerializedActorStateV3 state;
        std::array<std::uint32_t, overlay::Index(overlay::Area::count)> overlayColors{};
    };
    static_assert(sizeof(SerializedActorStateV4) == 120);
    static_assert(offsetof(SerializedActorStateV4, overlayColors) == 104);
    static_assert(std::is_trivially_copyable_v<SerializedActorStateV4>);

    struct SerializedActorStateV5 final
    {
        SerializedActorStateV2 state;
        std::array<std::uint16_t, overlay::Index(overlay::Area::count)> overlayCounts{};
        std::array<std::uint8_t, overlay::Index(overlay::Area::count)> overlayFlags{};
        std::uint32_t reserved{};
    };
    static_assert(sizeof(SerializedActorStateV5) == 80);
    static_assert(std::is_trivially_copyable_v<SerializedActorStateV5>);

    struct SerializedOverlayItemV5 final
    {
        std::uint32_t idIndex{};
        std::uint32_t pathIndex{};
        std::uint32_t color{ 0xFFFFFFFFU };
        std::uint8_t slot{ overlay::kNoOwnedSlot };
        std::array<std::uint8_t, 3> reserved{};
    };
    static_assert(sizeof(SerializedOverlayItemV5) == 16);
    static_assert(std::is_trivially_copyable_v<SerializedOverlayItemV5>);

    template <class IndexFor>
    [[nodiscard]] SerializedActorStateV3 Encode(const ActorState& state, IndexFor&& indexFor)
    {
        std::uint16_t flags{};
        if (state.body.selection.manual) flags |= kManualBody;
        if (state.skin.selection.manual) flags |= kManualSkin;
        if (state.body.selection.useDefault) flags |= kDefaultBody;
        if (state.skin.selection.useDefault) flags |= kDefaultSkin;
        if (state.body.application.appliedDefault) flags |= kAppliedDefaultBody;
        if (state.skin.application.appliedDefault) flags |= kAppliedDefaultSkin;
        if (state.body.application.applied) flags |= kBodyApplied;
        if (state.skin.application.applied) flags |= kSkinApplied;
        SerializedActorStateV3 result{
            .actorFormID = state.actorFormID,
            .baseLocalFormID = state.baseLocalFormID,
            .basePluginIndex = indexFor(state.basePlugin),
            .selectedBodyIndex = indexFor(state.body.selection.selectedId),
            .selectedSkinIndex = indexFor(state.skin.selection.selectedId),
            .selectedFutanariSkinIndex = indexFor(state.futanari.selectedSkinId),
            .appliedBodyIndex = indexFor(state.body.application.appliedId),
            .appliedSkinIndex = indexFor(state.skin.application.appliedId),
            .flags = flags,
            .bodySignature = state.body.application.signature,
            .skinSignature = state.skin.application.signature,
            .outfitSignature = state.body.outfitSignature
        };
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            const auto& selected = state.overlay.areas[index];
            const auto* item = selected.items.empty() ? nullptr : std::addressof(selected.items.front());
            result.overlayIdIndices[index] = indexFor(item ? item->selectedId : std::string{});
            result.overlayPathIndices[index] = indexFor(item ? item->texturePath : std::string{});
            result.overlaySlots[index] = item ? item->ownedSlot : overlay::kNoOwnedSlot;
            result.overlayFlags[index] = static_cast<std::uint8_t>(
                (selected.manual ? kOverlayManual : 0U) |
                (selected.useDefault ? kOverlayDefault : 0U));
        }
        return result;
    }

    // Form-ID remapping remains the SKSE caller's responsibility.
    [[nodiscard]] inline std::optional<ActorState> Decode(const SerializedActorStateV2& source,
        std::span<const std::string> strings)
    {
        const auto validIndex = [&strings](const std::uint32_t value) { return value < strings.size(); };
        if (!validIndex(source.basePluginIndex) || !validIndex(source.selectedBodyIndex) ||
            !validIndex(source.selectedSkinIndex) || !validIndex(source.selectedFutanariSkinIndex) ||
            !validIndex(source.appliedBodyIndex) ||
            !validIndex(source.appliedSkinIndex)) return std::nullopt;
        return ActorState{
            .actorFormID = source.actorFormID,
            .baseLocalFormID = source.baseLocalFormID,
            .basePlugin = strings[source.basePluginIndex],
            .body = {
                .selection = {
                    .selectedId = strings[source.selectedBodyIndex],
                    .manual = (source.flags & kManualBody) != 0U,
                    .useDefault = (source.flags & kDefaultBody) != 0U
                },
                .application = {
                    .appliedId = strings[source.appliedBodyIndex],
                    .appliedDefault = (source.flags & kAppliedDefaultBody) != 0U,
                    .applied = (source.flags & kBodyApplied) != 0U,
                    .signature = source.bodySignature
                },
                .outfitSignature = source.outfitSignature
            },
            .skin = {
                .selection = {
                    .selectedId = strings[source.selectedSkinIndex],
                    .manual = (source.flags & kManualSkin) != 0U,
                    .useDefault = (source.flags & kDefaultSkin) != 0U
                },
                .application = {
                    .appliedId = strings[source.appliedSkinIndex],
                    .appliedDefault = (source.flags & kAppliedDefaultSkin) != 0U,
                    .applied = (source.flags & kSkinApplied) != 0U,
                    .signature = source.skinSignature
                }
            },
            // ASTR v2-v5 predate futanari provenance. Every non-empty value
            // in those versions came from the direct catalog, so migrate it
            // as a manual choice rather than letting a new distribution rule
            // overwrite an existing save.
            .futanari = {
                .selectedSkinId = strings[source.selectedFutanariSkinIndex],
                .manual = !strings[source.selectedFutanariSkinIndex].empty()
            }
        };
    }

    template <class IndexFor>
    [[nodiscard]] SerializedActorStateV4 EncodeV4(const ActorState& state, IndexFor&& indexFor)
    {
        SerializedActorStateV4 result{ .state = Encode(state, indexFor) };
        for (const auto area : overlay::kAreas) {
            const auto& items = state.overlay.areas[overlay::Index(area)].items;
            result.overlayColors[overlay::Index(area)] = items.empty() ? 0xFFFFFFFFU : items.front().color;
        }
        return result;
    }

    [[nodiscard]] inline std::optional<ActorState> Decode(const SerializedActorStateV3& source,
        std::span<const std::string> strings)
    {
        const SerializedActorStateV2 legacyPrefix{
            .actorFormID = source.actorFormID,
            .baseLocalFormID = source.baseLocalFormID,
            .basePluginIndex = source.basePluginIndex,
            .selectedBodyIndex = source.selectedBodyIndex,
            .selectedSkinIndex = source.selectedSkinIndex,
            .selectedFutanariSkinIndex = source.selectedFutanariSkinIndex,
            .appliedBodyIndex = source.appliedBodyIndex,
            .appliedSkinIndex = source.appliedSkinIndex,
            .flags = source.flags,
            .reserved = source.reserved,
            .bodySignature = source.bodySignature,
            .skinSignature = source.skinSignature,
            .outfitSignature = source.outfitSignature
        };
        auto decoded = Decode(legacyPrefix, strings);
        if (!decoded) return std::nullopt;
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            if (source.overlayIdIndices[index] >= strings.size() ||
                source.overlayPathIndices[index] >= strings.size()) return std::nullopt;
            auto& selected = decoded->overlay.areas[index];
            selected.manual = (source.overlayFlags[index] & kOverlayManual) != 0U;
            selected.useDefault = (source.overlayFlags[index] & kOverlayDefault) != 0U;
            if (selected.useDefault) {
                selected.items.clear();
            } else if (strings[source.overlayIdIndices[index]].empty() ||
                strings[source.overlayPathIndices[index]].empty()) {
                selected = {};
            } else {
                selected.items.push_back({
                    .selectedId = strings[source.overlayIdIndices[index]],
                    .texturePath = strings[source.overlayPathIndices[index]],
                    .ownedSlot = source.overlaySlots[index]
                });
            }
        }
        return decoded;
    }

    [[nodiscard]] inline std::optional<ActorState> Decode(const SerializedActorStateV4& source,
        std::span<const std::string> strings)
    {
        auto decoded = Decode(source.state, strings);
        if (!decoded) return std::nullopt;
        for (const auto area : overlay::kAreas) {
            auto& items = decoded->overlay.areas[overlay::Index(area)].items;
            if (!items.empty()) items.front().color = source.overlayColors[overlay::Index(area)];
        }
        return decoded;
    }

    template <class IndexFor>
    [[nodiscard]] SerializedActorStateV5 EncodeV5(const ActorState& source, IndexFor&& indexFor,
        std::vector<SerializedOverlayItemV5>& items)
    {
        const auto legacy = Encode(source, indexFor);
        SerializedActorStateV5 result{
            .state = {
                .actorFormID = legacy.actorFormID,
                .baseLocalFormID = legacy.baseLocalFormID,
                .basePluginIndex = legacy.basePluginIndex,
                .selectedBodyIndex = legacy.selectedBodyIndex,
                .selectedSkinIndex = legacy.selectedSkinIndex,
                .selectedFutanariSkinIndex = legacy.selectedFutanariSkinIndex,
                .appliedBodyIndex = legacy.appliedBodyIndex,
                .appliedSkinIndex = legacy.appliedSkinIndex,
                .flags = legacy.flags,
                .reserved = legacy.reserved,
                .bodySignature = legacy.bodySignature,
                .skinSignature = legacy.skinSignature,
                .outfitSignature = legacy.outfitSignature
            }
        };
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            const auto& selected = source.overlay.areas[index];
            const auto count = (std::min)(selected.items.size(),
                static_cast<std::size_t>((std::numeric_limits<std::uint16_t>::max)()));
            result.overlayCounts[index] = static_cast<std::uint16_t>(count);
            result.overlayFlags[index] = static_cast<std::uint8_t>(
                (selected.manual ? kOverlayManual : 0U) |
                (selected.useDefault ? kOverlayDefault : 0U));
            for (std::size_t itemIndex{}; itemIndex < count; ++itemIndex) {
                const auto& item = selected.items[itemIndex];
                items.push_back({ .idIndex = indexFor(item.selectedId),
                    .pathIndex = indexFor(item.texturePath), .color = item.color,
                    .slot = item.ownedSlot });
            }
        }
        return result;
    }

    [[nodiscard]] inline std::optional<ActorState> Decode(const SerializedActorStateV5& source,
        std::span<const SerializedOverlayItemV5> overlayItems, std::span<const std::string> strings)
    {
        auto decoded = Decode(source.state, strings);
        if (!decoded) return std::nullopt;
        std::size_t cursor{};
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            auto& selected = decoded->overlay.areas[index];
            selected.manual = (source.overlayFlags[index] & kOverlayManual) != 0U;
            selected.useDefault = (source.overlayFlags[index] & kOverlayDefault) != 0U;
            selected.items.clear();
            const auto count = static_cast<std::size_t>(source.overlayCounts[index]);
            if (count > overlayItems.size() - cursor) return std::nullopt;
            selected.items.reserve(count);
            for (std::size_t itemIndex{}; itemIndex < count; ++itemIndex) {
                const auto& item = overlayItems[cursor++];
                if (item.idIndex >= strings.size() || item.pathIndex >= strings.size() ||
                    strings[item.idIndex].empty() || strings[item.pathIndex].empty()) return std::nullopt;
                selected.items.push_back({ .selectedId = strings[item.idIndex],
                    .texturePath = strings[item.pathIndex], .ownedSlot = item.slot,
                    .color = item.color });
            }
            // Default items are pending exact-node removals, not selections.
        }
        return decoded;
    }

    // ASTR v6 deliberately reuses the byte-stable v5 layout. The formerly
    // reserved 16-bit field in its v2 prefix now stores futanari provenance;
    // older readers reject record version 6 before interpreting those bits.
    template <class IndexFor>
    [[nodiscard]] SerializedActorStateV5 EncodeV6(const ActorState& source,
        IndexFor&& indexFor, std::vector<SerializedOverlayItemV5>& items)
    {
        auto result = EncodeV5(source, std::forward<IndexFor>(indexFor), items);
        result.state.reserved = static_cast<std::uint16_t>(
            (source.futanari.manual ? kFutanariManual : 0U) |
            (source.futanari.useDefault ? kFutanariDefault : 0U));
        return result;
    }

    [[nodiscard]] inline std::optional<ActorState> DecodeV6(
        const SerializedActorStateV5& source,
        std::span<const SerializedOverlayItemV5> overlayItems,
        std::span<const std::string> strings)
    {
        auto decoded = Decode(source, overlayItems, strings);
        if (!decoded) return std::nullopt;
        decoded->futanari.manual = (source.state.reserved & kFutanariManual) != 0U;
        decoded->futanari.useDefault = (source.state.reserved & kFutanariDefault) != 0U;
        if (decoded->futanari.useDefault) decoded->futanari.selectedSkinId.clear();
        return decoded;
    }
}
