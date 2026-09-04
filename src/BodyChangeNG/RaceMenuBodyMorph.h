#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace RE
{
    class Actor;
}

namespace bcn::racemenu
{
    [[nodiscard]] constexpr float AbsolutePresetCorrection(
        const float desiredPresetValue, const float currentAggregate,
        const float preservedOutfitValue = 0.0F) noexcept
    {
        return desiredPresetValue + preservedOutfitValue - currentAggregate;
    }

    [[nodiscard]] constexpr float OutfitTargetCorrection(
        const float target, const float aggregateWithoutOutfit) noexcept
    {
        return target - aggregateWithoutOutfit;
    }

    enum class ApplyMode : std::uint8_t
    {
        preview,
        commit,
        outfit
    };

    enum class UpdatePolicy : std::uint8_t
    {
        synchronous,
        deferred
    };

    enum class ApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        invalidActor,
        actor3DUnavailable,
        missingPreset,
        emptyPreset,
        incompatibleSex,
        incompatibleBodyFamily,
        noTaskInterface
    };

    void Initialize();
    void ResetSessionState();
    [[nodiscard]] bool IsReady() noexcept;
    [[nodiscard]] std::uint32_t Version() noexcept;
    // Returns another interface from the same verified RaceMenu exchange map.
    // The caller owns only the typed view; RaceMenu retains the object.
    [[nodiscard]] void* QueryInterface(const char* a_name) noexcept;
    [[nodiscard]] std::optional<std::string> CurrentPresetId(const RE::Actor* a_actor);
    void ForgetActorState(std::uint32_t a_actorFormID);
    [[nodiscard]] ApplyResult QueueApply(RE::Actor* a_actor, std::string a_presetId, ApplyMode a_mode,
        std::uint64_t a_outfitSignature = 0U,
        UpdatePolicy a_updatePolicy = UpdatePolicy::synchronous);
    // RaceMenu recreates the player's 3D when character generation closes.
    // Reapply the already selected preset to that fresh geometry.
    void QueueReapplyCurrent(RE::Actor* a_actor);
    [[nodiscard]] ApplyResult QueueApplyOutfit(RE::Actor* a_actor, std::string a_refitPresetId,
        std::uint64_t a_outfitSignature);
    // Reverts the transient single-click preview on the exact actor that owns
    // it. Safe to call repeatedly when switching actors, tabs, or closing UI.
    void QueueCancelPreview();
    void QueueApplyProceduralOutfit(RE::Actor* a_actor, std::uint64_t a_outfitSignature);
    void QueueClearOutfit(RE::Actor* a_actor, std::uint64_t a_outfitSignature);
    void QueueClearBodyChangeMorphs(RE::Actor* a_actor);
    [[nodiscard]] bool QueueClearAllBodyChangeMorphs();
}
