#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SkinProfiles.h"

#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinOverrides.h"

#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <ranges>
#include <unordered_map>

namespace
{
    constexpr std::uint32_t kCosaveID = 0x42434E47U;       // BCNG
    constexpr std::uint32_t kActorRecord = 0x41535452U;    // ASTR
    constexpr std::uint32_t kTintRecord = 0x54494E54U;     // TINT
    constexpr std::uint32_t kActorRecordVersion = 2U;
    constexpr std::uint32_t kLegacyActorRecordVersion = 1U;
    constexpr std::uint32_t kTintRecordVersion = 1U;
    constexpr std::uint32_t kMaxActors = 16384U;
    constexpr std::uint32_t kMaxStrings = 131072U;
    constexpr std::uint32_t kMaxTintLayers = 32U;
    constexpr std::uint32_t kMaxStringLength = 1024U;

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

    struct SerializedActorStateV1 final
    {
        std::uint32_t actorFormID{};
        std::uint32_t baseLocalFormID{};
        std::uint32_t basePluginIndex{};
        std::uint32_t selectedBodyIndex{};
        std::uint32_t selectedSkinIndex{};
        std::uint32_t appliedBodyIndex{};
        std::uint32_t appliedSkinIndex{};
        std::uint16_t flags{};
        std::uint16_t reserved{};
        std::uint64_t bodySignature{};
        std::uint64_t skinSignature{};
        std::uint64_t outfitSignature{};
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

    struct BaseIdentity final
    {
        std::string plugin;
        std::uint32_t localFormID{};
    };

    [[nodiscard]] BaseIdentity IdentityFor(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return {};
        const auto* file = base->GetFile(0);
        if (!file) return { {}, base->GetFormID() };
        return {
            std::string{ file->GetFilename() },
            base->GetFormID() & (file->IsLight() ? 0xFFFU : 0xFFFFFFU)
        };
    }

    [[nodiscard]] bool EqualIgnoreCase(const std::string_view left, const std::string_view right)
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index{}; index < left.size(); ++index) {
            const auto lower = [](const char value) {
                return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
            };
            if (lower(left[index]) != lower(right[index])) return false;
        }
        return true;
    }

    template <class T>
    [[nodiscard]] bool WriteValue(SKSE::SerializationInterface* output, const T& value)
    {
        return output && output->WriteRecordData(value);
    }

    template <class T>
    [[nodiscard]] bool ReadValue(SKSE::SerializationInterface* input, T& value)
    {
        return input && input->ReadRecordData(value) == sizeof(T);
    }

    [[nodiscard]] bool WriteString(SKSE::SerializationInterface* output, const std::string_view value)
    {
        const auto length = static_cast<std::uint32_t>((std::min)(value.size(),
            static_cast<std::size_t>(kMaxStringLength)));
        return WriteValue(output, length) &&
            (length == 0U || output->WriteRecordData(value.data(), length));
    }

    [[nodiscard]] bool ReadString(SKSE::SerializationInterface* input, std::string& value)
    {
        std::uint32_t length{};
        if (!ReadValue(input, length) || length > kMaxStringLength) return false;
        value.resize(length);
        return length == 0U || input->ReadRecordData(value.data(), length) == length;
    }

    void SaveState(SKSE::SerializationInterface* output)
    {
        auto states = bcn::ActorRegistry::Get().SnapshotAll();
        if (states.size() > kMaxActors) {
            SKSE::log::warn("Body Change NG actor registry exceeded {}; only the first entries will be saved",
                kMaxActors);
            states.resize(kMaxActors);
        }
        if (output && output->OpenRecord(kActorRecord, kActorRecordVersion)) {
            std::vector<std::string> strings{ std::string{} };
            std::unordered_map<std::string, std::uint32_t> indexByString{ { {}, 0U } };
            auto indexFor = [&](const std::string& value) {
                const auto found = indexByString.find(value);
                if (found != indexByString.end()) return found->second;
                const auto index = static_cast<std::uint32_t>(strings.size());
                strings.push_back(value);
                indexByString.emplace(value, index);
                return index;
            };
            std::vector<SerializedActorStateV2> serialized;
            serialized.reserve(states.size());
            for (const auto& state : states) {
                std::uint16_t flags{};
                if (state.body.selection.manual) flags |= kManualBody;
                if (state.skin.selection.manual) flags |= kManualSkin;
                if (state.body.selection.useDefault) flags |= kDefaultBody;
                if (state.skin.selection.useDefault) flags |= kDefaultSkin;
                if (state.body.application.appliedDefault) flags |= kAppliedDefaultBody;
                if (state.skin.application.appliedDefault) flags |= kAppliedDefaultSkin;
                if (state.body.application.applied) flags |= kBodyApplied;
                if (state.skin.application.applied) flags |= kSkinApplied;
                serialized.push_back(SerializedActorStateV2{
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
                });
            }
            const auto stringCount = static_cast<std::uint32_t>(strings.size());
            const auto actorCount = static_cast<std::uint32_t>(serialized.size());
            auto ok = WriteValue(output, stringCount);
            for (const auto& value : strings) ok = WriteString(output, value) && ok;
            ok = WriteValue(output, actorCount) && ok;
            for (const auto& state : serialized) ok = WriteValue(output, state) && ok;
            if (!ok) SKSE::log::error("Body Change NG could not write its actor registry cosave record");
        }

        if (output && output->OpenRecord(kTintRecord, kTintRecordVersion)) {
            const auto tint = bcn::player_tint::SnapshotPersistedState();
            auto ok = WriteString(output, tint.pack.value_or(std::string{}));
            const auto count = static_cast<std::uint32_t>((std::min)(tint.layers.size(),
                static_cast<std::size_t>(kMaxTintLayers)));
            ok = WriteValue(output, count) && ok;
            for (std::uint32_t index{}; index < count; ++index) {
                const auto& layer = tint.layers[index];
                const auto rawLayer = static_cast<std::uint8_t>(layer.layer);
                const auto restored = static_cast<std::uint8_t>(layer.restored);
                ok = WriteValue(output, rawLayer) && WriteValue(output, restored) &&
                    WriteValue(output, layer.color.red) && WriteValue(output, layer.color.green) &&
                    WriteValue(output, layer.color.blue) && WriteValue(output, layer.color.alpha) &&
                    WriteString(output, layer.assetID) && ok;
            }
            if (!ok) SKSE::log::error("Body Change NG could not write its player tint cosave record");
        }
    }

    void LoadActorRecord(SKSE::SerializationInterface* input, const std::uint32_t version)
    {
        std::uint32_t stringCount{};
        if (!ReadValue(input, stringCount) || stringCount == 0U || stringCount > kMaxStrings) return;
        std::vector<std::string> strings(stringCount);
        for (auto& value : strings) if (!ReadString(input, value)) return;
        std::uint32_t actorCount{};
        if (!ReadValue(input, actorCount) || actorCount > kMaxActors) return;
        std::vector<bcn::ActorState> loaded;
        loaded.reserve(actorCount);
        for (std::uint32_t index{}; index < actorCount; ++index) {
            SerializedActorStateV2 source;
            if (version == kLegacyActorRecordVersion) {
                SerializedActorStateV1 legacy;
                if (!ReadValue(input, legacy)) return;
                source = {
                    .actorFormID = legacy.actorFormID,
                    .baseLocalFormID = legacy.baseLocalFormID,
                    .basePluginIndex = legacy.basePluginIndex,
                    .selectedBodyIndex = legacy.selectedBodyIndex,
                    .selectedSkinIndex = legacy.selectedSkinIndex,
                    .selectedFutanariSkinIndex = 0U,
                    .appliedBodyIndex = legacy.appliedBodyIndex,
                    .appliedSkinIndex = legacy.appliedSkinIndex,
                    .flags = legacy.flags,
                    .reserved = legacy.reserved,
                    .bodySignature = legacy.bodySignature,
                    .skinSignature = legacy.skinSignature,
                    .outfitSignature = legacy.outfitSignature
                };
            } else if (!ReadValue(input, source)) {
                return;
            }
            if (!input->ResolveFormID(source.actorFormID, source.actorFormID)) continue;
            const auto validIndex = [&strings](const std::uint32_t value) { return value < strings.size(); };
            if (!validIndex(source.basePluginIndex) || !validIndex(source.selectedBodyIndex) ||
                !validIndex(source.selectedSkinIndex) || !validIndex(source.selectedFutanariSkinIndex) ||
                !validIndex(source.appliedBodyIndex) ||
                !validIndex(source.appliedSkinIndex)) continue;
            loaded.push_back(bcn::ActorState{
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
                .futanari = { .selectedSkinId = strings[source.selectedFutanariSkinIndex] }
            });
        }
        for (auto& state : loaded) bcn::ActorRegistry::Get().RestoreSerialized(std::move(state));
    }

    void LoadTintRecord(SKSE::SerializationInterface* input)
    {
        std::string pack;
        std::uint32_t count{};
        if (!ReadString(input, pack) || !ReadValue(input, count) || count > kMaxTintLayers) return;
        bcn::player_tint::PersistedState state;
        if (!pack.empty()) state.pack = std::move(pack);
        state.layers.reserve(count);
        for (std::uint32_t index{}; index < count; ++index) {
            std::uint8_t rawLayer{};
            std::uint8_t restored{};
            bcn::player_tint::PersistedLayerState layer;
            if (!ReadValue(input, rawLayer) || !ReadValue(input, restored) ||
                !ReadValue(input, layer.color.red) || !ReadValue(input, layer.color.green) ||
                !ReadValue(input, layer.color.blue) || !ReadValue(input, layer.color.alpha) ||
                !ReadString(input, layer.assetID)) return;
            if (rawLayer > static_cast<std::uint8_t>(bcn::player_tint::Layer::dirt)) continue;
            layer.layer = static_cast<bcn::player_tint::Layer>(rawLayer);
            layer.restored = restored != 0U;
            state.layers.push_back(std::move(layer));
        }
        bcn::player_tint::RestorePersistedState(std::move(state));
    }

    void LoadState(SKSE::SerializationInterface* input)
    {
        bcn::ActorRegistry::Get().Revert();
        bcn::player_tint::ResetPersistedState();
        std::uint32_t type{};
        std::uint32_t version{};
        std::uint32_t length{};
        while (input && input->GetNextRecordInfo(type, version, length)) {
            if (type == kActorRecord &&
                (version == kLegacyActorRecordVersion || version == kActorRecordVersion)) {
                LoadActorRecord(input, version);
            } else if (type == kTintRecord && version == kTintRecordVersion) {
                LoadTintRecord(input);
            } else {
                SKSE::log::warn("Body Change NG ignored cosave record {:08X} version {}", type, version);
            }
        }
        SKSE::log::info("Body Change NG loaded {} actor registry entries from the current save",
            bcn::ActorRegistry::Get().Size());
    }

    void RevertState(SKSE::SerializationInterface*)
    {
        bcn::frame_tasks::Reset(false);
        bcn::ActorRegistry::Get().Revert();
        bcn::player_tint::ResetPersistedState();
    }
}

namespace bcn
{
    ActorRegistry& ActorRegistry::Get()
    {
        static ActorRegistry registry;
        return registry;
    }

    void ActorRegistry::RegisterSerialization()
    {
        std::scoped_lock lock(lock_);
        if (serializationRegistered_) return;
        auto* serialization = SKSE::GetSerializationInterface();
        if (!serialization) {
            SKSE::log::error("Body Change NG could not obtain the SKSE serialization interface");
            return;
        }
        serialization->SetUniqueID(kCosaveID);
        serialization->SetSaveCallback(SaveState);
        serialization->SetLoadCallback(LoadState);
        serialization->SetRevertCallback(RevertState);
        serializationRegistered_ = true;
        SKSE::log::info("Body Change NG registered SKSE cosave callbacks with ID {:08X}", kCosaveID);
    }

    ActorState& ActorRegistry::EnsureLocked(RE::Actor* actor)
    {
        const auto actorFormID = actor ? actor->GetFormID() : 0U;
        const auto identity = IdentityFor(actor);
        auto [entry, inserted] = states_.try_emplace(actorFormID);
        if (inserted || entry->second.actorFormID == 0U ||
            entry->second.baseLocalFormID != identity.localFormID ||
            !EqualIgnoreCase(entry->second.basePlugin, identity.plugin)) {
            entry->second = ActorState{
                .actorFormID = actorFormID,
                .baseLocalFormID = identity.localFormID,
                .basePlugin = identity.plugin
            };
        }
        return entry->second;
    }

    const ActorState* ActorRegistry::FindValidatedLocked(const RE::Actor* actor) const
    {
        if (!actor || actor->GetFormID() == 0U) return nullptr;
        const auto found = states_.find(actor->GetFormID());
        if (found == states_.end()) return nullptr;
        const auto identity = IdentityFor(actor);
        if (found->second.baseLocalFormID != identity.localFormID ||
            !EqualIgnoreCase(found->second.basePlugin, identity.plugin)) {
            states_.erase(found);
            return nullptr;
        }
        return &states_.at(actor->GetFormID());
    }

    std::optional<ActorState> ActorRegistry::Snapshot(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state ? std::optional{ *state } : std::nullopt;
    }

    std::vector<ActorState> ActorRegistry::SnapshotAll() const
    {
        std::scoped_lock lock(lock_);
        std::vector<ActorState> result;
        result.reserve(states_.size());
        for (const auto& [formID, state] : states_) {
            if (formID != 0U) result.push_back(state);
        }
        return result;
    }

    std::optional<ManualActorSelection> ActorRegistry::ManualSelection(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        if (!state || (!state->body.selection.manual && !state->skin.selection.manual)) return std::nullopt;
        return ManualActorSelection{
            .bodyId = state->body.selection.selectedId,
            .skinId = state->skin.selection.selectedId,
            .hasBody = state->body.selection.manual,
            .hasSkin = state->skin.selection.manual,
            .useDefaultBody = state->body.selection.useDefault,
            .useDefaultSkin = state->skin.selection.useDefault
        };
    }

    std::optional<std::string> ActorRegistry::AppliedBodyId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && state->body.application.applied && !state->body.application.appliedDefault &&
            !state->body.application.appliedId.empty() ?
            std::optional{ state->body.application.appliedId } : std::nullopt;
    }

    std::optional<std::string> ActorRegistry::SelectedSkinId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && !state->skin.selection.useDefault && !state->skin.selection.selectedId.empty() ?
            std::optional{ state->skin.selection.selectedId } : std::nullopt;
    }

    std::optional<std::string> ActorRegistry::AppliedSkinId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && state->skin.application.applied && !state->skin.application.appliedDefault &&
            !state->skin.application.appliedId.empty() ?
            std::optional{ state->skin.application.appliedId } : std::nullopt;
    }

    std::optional<std::string> ActorRegistry::SelectedFutanariSkinId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && !state->futanari.selectedSkinId.empty() ?
            std::optional{ state->futanari.selectedSkinId } : std::nullopt;
    }

    void ActorRegistry::SetManualBody(RE::Actor* actor, std::string bodyId, const bool useDefault)
    {
        if (!actor || (!useDefault && bodyId.empty()) || bodyId.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.body.selection.selectedId = useDefault ? std::string{} : std::move(bodyId);
        state.body.selection.manual = true;
        state.body.selection.useDefault = useDefault;
    }

    void ActorRegistry::SetManualSkin(RE::Actor* actor, std::string skinId, const bool useDefault)
    {
        if (!actor || (!useDefault && skinId.empty()) || skinId.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.skin.selection.selectedId = useDefault ? std::string{} : std::move(skinId);
        state.skin.selection.manual = true;
        state.skin.selection.useDefault = useDefault;
    }

    void ActorRegistry::SetFutanariSkin(RE::Actor* actor, std::string skinId)
    {
        if (!actor || skinId.empty() || skinId.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        EnsureLocked(actor).futanari.selectedSkinId = std::move(skinId);
    }

    void ActorRegistry::ClearFutanariSkin(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        if (auto* state = const_cast<ActorState*>(FindValidatedLocked(actor))) {
            state->futanari.selectedSkinId.clear();
        }
    }

    bool ActorRegistry::RemoveManual(RE::Actor* actor)
    {
        if (!actor) return false;
        std::scoped_lock lock(lock_);
        auto* state = const_cast<ActorState*>(FindValidatedLocked(actor));
        if (!state || (!state->body.selection.manual && !state->skin.selection.manual)) return false;
        state->body.selection = {};
        state->skin.selection = {};
        return true;
    }

    bool ActorRegistry::RemoveManualBody(RE::Actor* actor)
    {
        if (!actor) return false;
        std::scoped_lock lock(lock_);
        auto* state = const_cast<ActorState*>(FindValidatedLocked(actor));
        if (!state || !state->body.selection.manual) return false;
        state->body.selection = {};
        return true;
    }

    void ActorRegistry::ClearManualSelections()
    {
        std::scoped_lock lock(lock_);
        for (auto& [formID, state] : states_) {
            state.body.selection = {};
            state.skin.selection = {};
        }
    }

    void ActorRegistry::ClearManualBodySelections()
    {
        std::scoped_lock lock(lock_);
        for (auto& [formID, state] : states_) {
            state.body.selection = {};
        }
    }

    bool ActorRegistry::HasManualSelection(const RE::Actor* actor) const
    {
        return ManualSelection(actor).has_value();
    }

    void ActorRegistry::SetRuleSelection(RE::Actor* actor, std::optional<std::string> bodyId,
        std::optional<std::string> skinId, const bool useDefaultBody)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        UpdateAutomaticSelection(state.body.selection, bodyId, useDefaultBody);
        UpdateAutomaticSelection(state.skin.selection, skinId);
    }

    std::uint64_t ActorRegistry::BodySignature(const std::string_view bodyId, const bool useDefault)
    {
        const auto options = useDefault ? 0U : Settings::Get().RandomizationOptions();
        return StableStateSignature("body", bodyId, useDefault,
            options, useDefault ? 0 : PresetCatalog::Get().ContentHash(bodyId));
    }

    std::uint64_t ActorRegistry::SkinSignature(const std::string_view skinId, const bool useDefault)
    {
        return StableStateSignature("skin", skinId, useDefault, 0U,
            useDefault ? 0 : SkinProfiles::Get().ContentHash(skinId));
    }

    bool ActorRegistry::NeedsBodyApply(RE::Actor* actor, const std::string_view bodyId,
        const bool useDefault)
    {
        const auto expectedSignature = BodySignature(bodyId, useDefault);
        {
            std::scoped_lock lock(lock_);
            const auto* state = FindValidatedLocked(actor);
            if (!state || !state->body.application.applied ||
                state->body.application.signature != expectedSignature) return true;
            if (state->body.application.verifiedThisSession) return false;
        }

        const auto liveMatches = racemenu::LiveBodyChangeStateMatches(actor, useDefault);
        std::scoped_lock lock(lock_);
        auto* state = const_cast<ActorState*>(FindValidatedLocked(actor));
        if (!state) return true;
        const auto decision = EvaluateRestoredApplication(state->body.application.applied,
            state->body.application.verifiedThisSession,
            state->body.application.signature == expectedSignature, liveMatches);
        if (decision == RestoredApplicationDecision::acceptLive) {
            state->body.application.verifiedThisSession = true;
            return false;
        }
        if (decision == RestoredApplicationDecision::skipVerified) return false;
        state->body.application.applied = false;
        state->body.application.verifiedThisSession = false;
        return true;
    }

    bool ActorRegistry::NeedsSkinApply(RE::Actor* actor, const std::string_view skinId,
        const bool useDefault)
    {
        const auto expectedSignature = SkinSignature(skinId, useDefault);
        {
            std::scoped_lock lock(lock_);
            const auto* state = FindValidatedLocked(actor);
            if (!state || !state->skin.application.applied ||
                state->skin.application.signature != expectedSignature) return true;
            if (state->skin.application.verifiedThisSession) return false;
        }

        const auto liveMatches = skin_override::LiveSkinStateMatches(actor, skinId, useDefault);
        std::scoped_lock lock(lock_);
        auto* state = const_cast<ActorState*>(FindValidatedLocked(actor));
        if (!state) return true;
        const auto decision = EvaluateRestoredApplication(state->skin.application.applied,
            state->skin.application.verifiedThisSession,
            state->skin.application.signature == expectedSignature, liveMatches);
        if (decision == RestoredApplicationDecision::acceptLive) {
            state->skin.application.verifiedThisSession = true;
            return false;
        }
        if (decision == RestoredApplicationDecision::skipVerified) return false;
        state->skin.application.applied = false;
        state->skin.application.verifiedThisSession = false;
        return true;
    }

    void ActorRegistry::MarkBodyApplied(RE::Actor* actor, std::string bodyId, const bool useDefault)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.body.application.appliedId = useDefault ? std::string{} : std::move(bodyId);
        state.body.application.appliedDefault = useDefault;
        state.body.application.applied = true;
        state.body.application.verifiedThisSession = true;
        state.body.application.signature = BodySignature(state.body.application.appliedId, useDefault);
    }

    void ActorRegistry::MarkSkinApplied(RE::Actor* actor, std::string skinId, const bool useDefault)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.skin.application.appliedId = useDefault ? std::string{} : std::move(skinId);
        state.skin.application.appliedDefault = useDefault;
        state.skin.application.applied = true;
        state.skin.application.verifiedThisSession = true;
        state.skin.application.signature = SkinSignature(state.skin.application.appliedId, useDefault);
    }

    void ActorRegistry::MarkOutfitApplied(RE::Actor* actor, const std::uint64_t signature)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        EnsureLocked(actor).body.outfitSignature = signature;
    }

    bool ActorRegistry::NeedsOutfitApply(RE::Actor* actor, const std::uint64_t signature) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return !state || state->body.outfitSignature != signature;
    }

    void ActorRegistry::InvalidateBody(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.body.application.applied = false;
        state.body.application.verifiedThisSession = false;
        state.body.application.signature = 0U;
    }

    void ActorRegistry::InvalidateSkin(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        state.skin.application.applied = false;
        state.skin.application.verifiedThisSession = false;
        state.skin.application.signature = 0U;
    }

    void ActorRegistry::InvalidateOutfit(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        EnsureLocked(actor).body.outfitSignature = 0U;
    }

    void ActorRegistry::InvalidateAllBodyResults()
    {
        std::scoped_lock lock(lock_);
        for (auto& [formID, state] : states_) {
            state.body.application.applied = false;
            state.body.application.verifiedThisSession = false;
            state.body.application.signature = 0U;
        }
    }

    void ActorRegistry::RestoreSerialized(ActorState state)
    {
        if (state.actorFormID == 0U) return;
        PrepareRestoredState(state);
        std::scoped_lock lock(lock_);
        states_.insert_or_assign(state.actorFormID, std::move(state));
    }

    void ActorRegistry::Revert()
    {
        std::scoped_lock lock(lock_);
        states_.clear();
        ++sessionGeneration_;
    }

    std::size_t ActorRegistry::Size() const
    {
        std::scoped_lock lock(lock_);
        return states_.size();
    }

    std::uint64_t ActorRegistry::SessionGeneration() const
    {
        std::scoped_lock lock(lock_);
        return sessionGeneration_;
    }
}
