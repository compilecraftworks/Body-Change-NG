#pragma once

#include "BodyChangeNG/ActorState.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RE
{
    class Actor;
}

namespace bcn
{
    struct ManualActorSelection final
    {
        std::string bodyId;
        std::string skinId;
        std::string futanariSkinId;
        bool hasBody{};
        bool hasSkin{};
        bool hasFutanari{};
        bool useDefaultBody{};
        bool useDefaultSkin{};
        bool useDefaultFutanari{};
    };

    class ActorRegistry final
    {
    public:
        static ActorRegistry& Get();

        // Installs one unique SKSE cosave owner for actor results and the
        // player's tint selection. It is safe to call more than once.
        void RegisterSerialization();

        [[nodiscard]] std::optional<ActorState> Snapshot(const RE::Actor* a_actor) const;
        [[nodiscard]] std::vector<ActorState> SnapshotAll() const;
        [[nodiscard]] std::optional<ManualActorSelection> ManualSelection(const RE::Actor* a_actor) const;
        [[nodiscard]] std::optional<std::string> SelectedBodyId(const RE::Actor* a_actor) const;
        [[nodiscard]] std::optional<std::string> SelectedSkinId(const RE::Actor* a_actor) const;
        [[nodiscard]] std::vector<OverlayItemState> SelectedOverlays(
            const RE::Actor* a_actor, overlay::Area a_area) const;
        [[nodiscard]] std::optional<OverlayItemState> SelectedOverlay(
            const RE::Actor* a_actor, overlay::Area a_area, std::string_view a_overlayId) const;
        [[nodiscard]] bool OverlayAreaIsManual(const RE::Actor* a_actor, overlay::Area a_area) const;
        [[nodiscard]] std::optional<std::string> SelectedFutanariSkinId(const RE::Actor* a_actor) const;
        [[nodiscard]] bool HasFutanariSelection(const RE::Actor* a_actor) const;
        [[nodiscard]] bool FutanariSelectionIsManual(const RE::Actor* a_actor) const;
        [[nodiscard]] bool FutanariUsesDefault(const RE::Actor* a_actor) const;
        [[nodiscard]] std::optional<std::string> AppliedBodyId(const RE::Actor* a_actor) const;
        [[nodiscard]] std::optional<std::string> AppliedSkinId(const RE::Actor* a_actor) const;

        void SetManualBody(RE::Actor* a_actor, std::string a_bodyId, bool a_useDefault);
        void SetManualSkin(RE::Actor* a_actor, std::string a_skinId, bool a_useDefault);
        void AddManualOverlay(RE::Actor* a_actor, overlay::Area a_area, std::string a_overlayId,
            std::string a_texturePath, std::uint8_t a_ownedSlot, bool a_useDefault);
        void RemoveManualOverlay(RE::Actor* a_actor, overlay::Area a_area, std::string_view a_overlayId);
        [[nodiscard]] bool CompleteOverlayApply(RE::Actor* a_actor, overlay::Area a_area,
            OverlayItemState a_item, overlay::ApplyMode a_mode, std::uint64_t a_resetRevision);
        void CompleteOverlayReset(RE::Actor* a_actor, overlay::Area a_area, std::uint64_t a_resetRevision);
        void ClearManualOverlays(RE::Actor* a_actor, overlay::Area a_area);
        void SetAutomaticOverlaySelection(RE::Actor* a_actor, overlay::Area a_area,
            std::optional<std::string> a_overlayId);
        void MarkOverlayResolved(RE::Actor* a_actor, overlay::Area a_area,
            std::string_view a_overlayId, std::string a_texturePath, std::uint8_t a_ownedSlot);
        void SetOverlayColor(RE::Actor* a_actor, overlay::Area a_area,
            std::string_view a_overlayId, std::uint32_t a_color);
        void SetManualFutanariSkin(RE::Actor* a_actor, std::string a_skinId, bool a_useDefault);
        void SetAutomaticFutanariSkin(RE::Actor* a_actor, std::optional<std::string> a_skinId);
        // Replaces both direct and previously resolved automatic selections
        // with explicit Default sentinels. Live backends are reset separately
        // after first capturing any resources they currently own.
        void ResetSelectionsToDefaults(RE::Actor* a_actor);
        void ResetAllSelectionsToDefaults();
        [[nodiscard]] bool HasManualSelection(const RE::Actor* a_actor) const;

        // Rule results are stored per save, but never overwrite a direct
        // selection. Stable rule hashing remains the fallback if no record is
        // present or the rule pools change.
        void SetRuleSelection(RE::Actor* a_actor, std::optional<std::string> a_bodyId,
            std::optional<std::string> a_skinId, bool a_useDefaultBody = false,
            std::optional<std::string> a_futanariSkinId = std::nullopt);
        [[nodiscard]] bool NeedsBodyApply(RE::Actor* a_actor, std::string_view a_bodyId,
            bool a_useDefault);
        [[nodiscard]] bool NeedsSkinApply(RE::Actor* a_actor, std::string_view a_skinId,
            bool a_useDefault);

        // Called only by the successful game-thread end of each existing
        // backend. A queued request is never serialized as completed.
        void MarkBodyApplied(RE::Actor* a_actor, std::string a_bodyId, bool a_useDefault);
        void MarkSkinApplied(RE::Actor* a_actor, std::string a_skinId, bool a_useDefault);
        void MarkOutfitApplied(RE::Actor* a_actor, std::uint64_t a_signature);
        [[nodiscard]] bool NeedsOutfitApply(RE::Actor* a_actor, std::uint64_t a_signature) const;
        void InvalidateBody(RE::Actor* a_actor);
        void InvalidateSkin(RE::Actor* a_actor);
        void InvalidateOutfit(RE::Actor* a_actor);

        // Serialization entry point. Records are still base-identity validated
        // before any caller can use them for a live actor.
        void RestoreSerialized(ActorState a_state);

        void Revert();
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::uint64_t SessionGeneration() const;

    private:
        [[nodiscard]] ActorState& EnsureLocked(RE::Actor* a_actor);
        [[nodiscard]] const ActorState* FindValidatedLocked(const RE::Actor* a_actor) const;
        [[nodiscard]] static std::uint64_t BodySignature(std::string_view a_bodyId, bool a_useDefault);
        [[nodiscard]] static std::uint64_t SkinSignature(std::string_view a_skinId, bool a_useDefault);

        mutable std::mutex lock_;
        mutable std::unordered_map<std::uint32_t, ActorState> states_;
        std::uint64_t sessionGeneration_{ 1U };
        bool serializationRegistered_{};
    };
}
