#pragma once

#include "BodyChangeNG/OverlayTypes.h"

#include <algorithm>
#include <utility>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bcn
{
    struct FeatureSelectionState final
    {
        std::string selectedId;
        bool manual{};
        bool useDefault{};
    };

    struct FeatureApplicationState final
    {
        std::string appliedId;
        bool appliedDefault{};
        bool applied{};
        // Serialized completion metadata is only a hint until the owning
        // feature verifies its own live backend in the current session.
        bool verifiedThisSession{};
        std::uint64_t signature{};
    };

    struct BodyFeatureState final
    {
        FeatureSelectionState selection;
        FeatureApplicationState application;
        // Outfit correction is a RaceMenu morph layer, not skin state. Keep
        // its invalidation boundary with the body feature that owns it.
        std::uint64_t outfitSignature{};
    };

    struct SkinFeatureState final
    {
        FeatureSelectionState selection;
        FeatureApplicationState application;
    };

    struct FutanariFeatureState final
    {
        // This is an optional, reference-scoped addon texture choice. It must
        // not be cleared when either the body or base-skin feature resets.
        std::string selectedSkinId;
        // Direct UI choices own this channel until the user explicitly
        // releases them. NPC distribution may update only non-manual state.
        bool manual{};
        // An explicit Default choice is different from an actor that BCNG has
        // never managed. It must survive save/load and block a rule from
        // silently selecting another futanari skin.
        bool useDefault{};
    };

    struct OverlayItemState final
    {
        std::string selectedId;
        // Keep the resolved packed RaceMenu texture string in the co-save so
        // a selected overlay can be restored before the Scaleform catalog has
        // answered in the new game session.
        std::string texturePath;
        std::uint8_t ownedSlot{ overlay::kNoOwnedSlot };
        std::uint32_t color{ 0xFFFFFFFFU }; // AARRGGBB, independent per actor/overlay
    };

    struct OverlayAreaState final
    {
        std::vector<OverlayItemState> items;
        // A direct edit locks only this anatomical area against automatic
        // distribution. Each selected paint remains independently removable.
        bool manual{};
        bool useDefault{};
        // Session-only transaction fence. Default + items denotes owned nodes
        // awaiting removal, never selected paints. The value-only items survive
        // saving/unloading until the exact native registrations are removed.
        std::uint64_t resetRevision{};
    };

    inline bool CompleteOverlayTransaction(OverlayAreaState& area, OverlayItemState item,
        overlay::ApplyMode mode, std::uint64_t resetRevision)
    {
        if (area.resetRevision != resetRevision) {
            if (area.useDefault) {
                const auto found = std::ranges::find(area.items, item.ownedSlot,
                    &OverlayItemState::ownedSlot);
                if (found == area.items.end()) area.items.push_back(std::move(item));
                else *found = std::move(item);
            }
            return false; // A late completion cannot undo an explicit reset.
        }
        if (mode == overlay::ApplyMode::preview) return true;
        if (mode == overlay::ApplyMode::automatic && !area.manual) {
            area.items = { std::move(item) };
            area.useDefault = false;
        } else if (mode == overlay::ApplyMode::manualCommit) {
            area.manual = true;
            area.useDefault = false;
            const auto found = std::ranges::find(area.items, item.selectedId,
                &OverlayItemState::selectedId);
            if (found == area.items.end()) area.items.push_back(std::move(item));
            else *found = std::move(item);
        } else if (!area.useDefault) {
            const auto found = std::ranges::find(area.items, item.selectedId,
                &OverlayItemState::selectedId);
            if (found != area.items.end()) *found = std::move(item);
        }
        return true;
    }

    struct OverlayFeatureState final
    {
        std::array<OverlayAreaState, overlay::Index(overlay::Area::count)> areas{};
    };

    struct ActorState final
    {
        std::uint32_t actorFormID{};
        std::uint32_t baseLocalFormID{};
        std::string basePlugin;

        BodyFeatureState body;
        SkinFeatureState skin;
        OverlayFeatureState overlay;
        FutanariFeatureState futanari;
    };

    // A settings reset is an explicit, persistent Default choice for every
    // actor-owned appearance channel.  Keeping the manual Default sentinel is
    // intentional: an old automatic distribution result must not reappear on
    // the next reconcile immediately after the user reset the actor.
    inline void ResetActorSelectionsToDefaults(ActorState& state)
    {
        state.body.selection = { .manual = true, .useDefault = true };
        state.body.application = {};
        state.body.outfitSignature = 0U;
        state.skin.selection = { .manual = true, .useDefault = true };
        state.skin.application = {};
        state.futanari = { .manual = true, .useDefault = true };
        for (auto& area : state.overlay.areas) {
            ++area.resetRevision;
            area.manual = true;
            area.useDefault = true;
        }
    }

    // Automatic distribution has three distinct outcomes for each feature:
    // preserve the previous choice (no value), select a new asset (value), or
    // explicitly select Default.  Collapsing preserve into an empty string
    // loses the serialized choice and makes an unchanged rule fail to restore
    // after the next save load.
    inline void UpdateAutomaticSelection(FeatureSelectionState& selection,
        const std::optional<std::string>& selectedId, const bool useDefault = false)
    {
        if (selection.manual) return;
        if (selectedId) {
            selection.selectedId = *selectedId;
            selection.useDefault = false;
        } else if (useDefault) {
            selection.selectedId.clear();
            selection.useDefault = true;
        }
    }

    enum class RestoredApplicationDecision : std::uint8_t
    {
        apply,
        acceptLive,
        skipVerified
    };

    [[nodiscard]] constexpr RestoredApplicationDecision EvaluateRestoredApplication(
        const bool applied, const bool verifiedThisSession, const bool signatureMatches,
        const std::optional<bool> liveStateMatches) noexcept
    {
        if (!applied || !signatureMatches) return RestoredApplicationDecision::apply;
        if (verifiedThisSession) return RestoredApplicationDecision::skipVerified;
        return liveStateMatches.value_or(false) ? RestoredApplicationDecision::acceptLive :
            RestoredApplicationDecision::apply;
    }

    inline void PrepareRestoredState(ActorState& state) noexcept
    {
        state.body.application.verifiedThisSession = false;
        state.skin.application.verifiedThisSession = false;
    }

    [[nodiscard]] inline std::uint64_t StableStateSignature(const std::string_view channel,
        const std::string_view value, const bool useDefault, const std::uint32_t optionBits = 0U,
        const std::uint64_t contentHash = 0U) noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto append = [&hash](const std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ULL; };
        for (const auto character : channel) append(static_cast<std::uint8_t>(character));
        append(0xFFU);
        for (const auto character : value) append(static_cast<std::uint8_t>(character));
        append(useDefault ? 1U : 0U);
        for (std::uint32_t shift{}; shift < 32U; shift += 8U) {
            append(static_cast<std::uint8_t>(optionBits >> shift));
        }
        if (contentHash != 0) {
            append(0xFEU);
            for (std::uint32_t shift{}; shift < 64U; shift += 8U) append(static_cast<std::uint8_t>(contentHash >> shift));
        }
        return hash;
    }
}
