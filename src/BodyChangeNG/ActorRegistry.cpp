#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorStateSerialization.h"
#include "BodyChangeNG/PlayerTintSerialization.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/RenderedOutfit.h"
#include "BodyChangeNG/FaceSkinOverrides.h"
#include "BodyChangeNG/FaceSkinSerialization.h"
#include "BodyChangeNG/NativeAddonSkinBackend.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SkinProfiles.h"

#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinApplication.h"

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
    constexpr std::uint32_t kFaceRecord = 0x46434E49U;     // FCNI, independent of ASTR
    constexpr std::uint32_t kActorRecordVersion = 6U;
    constexpr std::uint32_t kMultipleOverlayActorRecordVersion = 5U;
    constexpr std::uint32_t kOverlayActorRecordVersion = 3U;
    constexpr std::uint32_t kLegacyActorRecordVersion = 1U;
    constexpr std::uint32_t kPreviousActorRecordVersion = 2U;
    constexpr std::uint32_t kTintRecordVersion = bcn::player_tint::kStateVersion;
    constexpr std::uint32_t kMaxActors = 16384U;
    constexpr std::uint32_t kMaxStrings = 131072U;
    constexpr std::uint32_t kMaxOverlayItemsPerActor = 256U;
    constexpr std::uint32_t kMaxStringLength = 1024U;

    using bcn::actor_serialization::SerializedActorStateV2;
    using bcn::actor_serialization::SerializedActorStateV3;
    using bcn::actor_serialization::SerializedActorStateV4;
    using bcn::actor_serialization::SerializedActorStateV5;
    using bcn::actor_serialization::SerializedOverlayItemV5;

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
            struct EncodedActor final
            {
                SerializedActorStateV5 state;
                std::vector<SerializedOverlayItemV5> overlays;
            };
            std::vector<EncodedActor> serialized;
            serialized.reserve(states.size());
            for (const auto& state : states) {
                EncodedActor encoded;
                encoded.state = bcn::actor_serialization::EncodeV6(state, indexFor, encoded.overlays);
                serialized.push_back(std::move(encoded));
            }
            const auto stringCount = static_cast<std::uint32_t>(strings.size());
            const auto actorCount = static_cast<std::uint32_t>(serialized.size());
            auto ok = WriteValue(output, stringCount);
            for (const auto& value : strings) ok = WriteString(output, value) && ok;
            ok = WriteValue(output, actorCount) && ok;
            for (const auto& actor : serialized) {
                ok = WriteValue(output, actor.state) && ok;
                for (const auto& overlay : actor.overlays) ok = WriteValue(output, overlay) && ok;
            }
            if (!ok) SKSE::log::error("Body Change NG could not write its actor registry cosave record");
        }

        if (output && output->OpenRecord(kFaceRecord, 1U)) {
            const auto ok = bcn::face_skin::WriteBaselines(bcn::face_skin::SnapshotBaselines(),
                [output](const auto& value) { return WriteValue(output, value); },
                [output](const std::string& value) { return value.size() <= kMaxStringLength && WriteString(output, value); });
            if (!ok) SKSE::log::error("BCNG could not save face NiOverride restoration baselines");
        }
        if (output && output->OpenRecord(kTintRecord, kTintRecordVersion)) {
            const auto ok = bcn::player_tint::WriteState(bcn::player_tint::SnapshotPersistedState(),
                [output](const auto& value) { return WriteValue(output, value); },
                [output](const std::string& value) { return value.size() <= kMaxStringLength && WriteString(output, value); });
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
            std::optional<bcn::ActorState> decoded;
            if (version == kLegacyActorRecordVersion) {
                SerializedActorStateV1 legacy;
                if (!ReadValue(input, legacy)) return;
                SerializedActorStateV2 source{
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
                if (!input->ResolveFormID(source.actorFormID, source.actorFormID)) continue;
                decoded = bcn::actor_serialization::Decode(source, strings);
            } else if (version == kPreviousActorRecordVersion) {
                SerializedActorStateV2 source;
                if (!ReadValue(input, source)) return;
                if (!input->ResolveFormID(source.actorFormID, source.actorFormID)) continue;
                decoded = bcn::actor_serialization::Decode(source, strings);
            } else if (version == kOverlayActorRecordVersion) {
                SerializedActorStateV3 source;
                if (!ReadValue(input, source)) return;
                if (!input->ResolveFormID(source.actorFormID, source.actorFormID)) continue;
                decoded = bcn::actor_serialization::Decode(source, strings);
            } else if (version == 4U) {
                SerializedActorStateV4 source;
                if (!ReadValue(input, source)) return;
                if (!input->ResolveFormID(source.state.actorFormID, source.state.actorFormID)) continue;
                decoded = bcn::actor_serialization::Decode(source, strings);
            } else if (version == kMultipleOverlayActorRecordVersion ||
                version == kActorRecordVersion) {
                SerializedActorStateV5 source;
                if (!ReadValue(input, source)) return;
                std::size_t itemCount{};
                for (const auto count : source.overlayCounts) itemCount += count;
                if (itemCount > kMaxOverlayItemsPerActor) return;
                std::vector<SerializedOverlayItemV5> overlayItems(itemCount);
                for (auto& item : overlayItems) if (!ReadValue(input, item)) return;
                if (!input->ResolveFormID(source.state.actorFormID, source.state.actorFormID)) continue;
                decoded = version == kActorRecordVersion ?
                    bcn::actor_serialization::DecodeV6(source, overlayItems, strings) :
                    bcn::actor_serialization::Decode(source, overlayItems, strings);
            } else {
                return;
            }
            if (!decoded) continue;
            loaded.push_back(std::move(*decoded));
        }
        for (auto& state : loaded) bcn::ActorRegistry::Get().RestoreSerialized(std::move(state));
    }

    void LoadTintRecord(SKSE::SerializationInterface* input, const std::uint32_t version)
    {
        auto state = bcn::player_tint::ReadState(version,
            [input](auto& value) { return ReadValue(input, value); },
            [input](std::string& value) { return ReadString(input, value); });
        if (state) bcn::player_tint::RestorePersistedState(std::move(*state));
        else SKSE::log::warn("BCNG rejected a malformed player tint record");
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
                (version == kLegacyActorRecordVersion ||
                    version == kPreviousActorRecordVersion || version == kOverlayActorRecordVersion ||
                    version == 4U || version == kMultipleOverlayActorRecordVersion ||
                    version == kActorRecordVersion)) {
                LoadActorRecord(input, version);
            } else if (type == kFaceRecord && version == 1U) {
                auto rows = bcn::face_skin::ReadBaselines(
                    [input](auto& value) { return ReadValue(input, value); },
                    [input](std::string& value) { return ReadString(input, value); });
                if (rows) {
                    std::erase_if(*rows, [input](auto& row) {
                        return !input->ResolveFormID(row.actor, row.actor) ||
                            !input->ResolveFormID(row.base, row.base);
                    });
                    bcn::face_skin::RestoreBaselines(std::move(*rows));
                } else SKSE::log::error("BCNG rejected malformed face baseline record");
            } else if (type == kTintRecord && (version == 1U || version == kTintRecordVersion)) {
                LoadTintRecord(input, version);
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
        bcn::rendered_outfit::Reset();
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
        if (!state || (!state->body.selection.manual && !state->skin.selection.manual &&
                !state->futanari.manual)) return std::nullopt;
        return ManualActorSelection{
            .bodyId = state->body.selection.selectedId,
            .skinId = state->skin.selection.selectedId,
            .futanariSkinId = state->futanari.selectedSkinId,
            .hasBody = state->body.selection.manual,
            .hasSkin = state->skin.selection.manual,
            .hasFutanari = state->futanari.manual,
            .useDefaultBody = state->body.selection.useDefault,
            .useDefaultSkin = state->skin.selection.useDefault,
            .useDefaultFutanari = state->futanari.useDefault
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

    std::optional<std::string> ActorRegistry::SelectedBodyId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && !state->body.selection.useDefault && !state->body.selection.selectedId.empty() ?
            std::optional{ state->body.selection.selectedId } : std::nullopt;
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

    std::vector<OverlayItemState> ActorRegistry::SelectedOverlays(
        const RE::Actor* actor, const overlay::Area area) const
    {
        if (area == overlay::Area::count) return {};
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        if (!state) return {};
        const auto& selected = state->overlay.areas[overlay::Index(area)];
        return selected.useDefault ? std::vector<OverlayItemState>{} : selected.items;
    }

    std::optional<OverlayItemState> ActorRegistry::SelectedOverlay(
        const RE::Actor* actor, const overlay::Area area, const std::string_view overlayId) const
    {
        if (area == overlay::Area::count || overlayId.empty()) return std::nullopt;
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        if (!state) return std::nullopt;
        const auto& selected = state->overlay.areas[overlay::Index(area)];
        if (selected.useDefault) return std::nullopt;
        const auto found = std::ranges::find(selected.items, overlayId, &OverlayItemState::selectedId);
        return found == selected.items.end() ? std::nullopt : std::optional{ *found };
    }

    bool ActorRegistry::OverlayAreaIsManual(const RE::Actor* actor, const overlay::Area area) const
    {
        if (area == overlay::Area::count) return false;
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && state->overlay.areas[overlay::Index(area)].manual;
    }

    std::optional<std::string> ActorRegistry::SelectedFutanariSkinId(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && !state->futanari.useDefault && !state->futanari.selectedSkinId.empty() ?
            std::optional{ state->futanari.selectedSkinId } : std::nullopt;
    }

    bool ActorRegistry::HasFutanariSelection(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && (state->futanari.manual || state->futanari.useDefault ||
            !state->futanari.selectedSkinId.empty());
    }

    bool ActorRegistry::FutanariSelectionIsManual(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && state->futanari.manual;
    }

    bool ActorRegistry::FutanariUsesDefault(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && state->futanari.useDefault;
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

    void ActorRegistry::AddManualOverlay(RE::Actor* actor, const overlay::Area area,
        std::string overlayId, std::string texturePath, const std::uint8_t ownedSlot,
        const bool useDefault)
    {
        if (!actor || area == overlay::Area::count ||
            (!useDefault && (overlayId.empty() || texturePath.empty())) ||
            overlayId.size() > kMaxStringLength || texturePath.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& selected = EnsureLocked(actor).overlay.areas[overlay::Index(area)];
        selected.manual = true;
        selected.useDefault = useDefault;
        if (useDefault) {
            selected.items.clear();
            return;
        }
        const auto found = std::ranges::find(selected.items, overlayId, &OverlayItemState::selectedId);
        OverlayItemState item{ .selectedId = std::move(overlayId),
            .texturePath = std::move(texturePath), .ownedSlot = ownedSlot };
        if (found == selected.items.end()) selected.items.push_back(std::move(item));
        else {
            item.color = found->color;
            *found = std::move(item);
        }
    }

    void ActorRegistry::RemoveManualOverlay(RE::Actor* actor, const overlay::Area area,
        const std::string_view overlayId)
    {
        if (!actor || area == overlay::Area::count || overlayId.empty()) return;
        std::scoped_lock lock(lock_);
        auto& selected = EnsureLocked(actor).overlay.areas[overlay::Index(area)];
        std::erase_if(selected.items, [&](const auto& item) { return item.selectedId == overlayId; });
        selected.manual = true;
        selected.useDefault = selected.items.empty();
    }

    bool ActorRegistry::CompleteOverlayApply(RE::Actor* actor, overlay::Area area,
        OverlayItemState item, overlay::ApplyMode mode, std::uint64_t resetRevision)
    {
        if (!actor || area == overlay::Area::count) return false;
        std::scoped_lock lock(lock_);
        return CompleteOverlayTransaction(EnsureLocked(actor).overlay.areas[overlay::Index(area)],
            std::move(item), mode, resetRevision);
    }

    void ActorRegistry::CompleteOverlayReset(RE::Actor* actor, overlay::Area area,
        std::uint64_t resetRevision)
    {
        if (!actor || area == overlay::Area::count) return;
        std::scoped_lock lock(lock_);
        auto* state = const_cast<ActorState*>(FindValidatedLocked(actor));
        if (!state) return;
        auto& selected = state->overlay.areas[overlay::Index(area)];
        if (selected.useDefault && selected.resetRevision == resetRevision) selected.items.clear();
    }

    void ActorRegistry::ClearManualOverlays(RE::Actor* actor, const overlay::Area area)
    {
        if (!actor || area == overlay::Area::count) return;
        std::scoped_lock lock(lock_);
        auto& selected = EnsureLocked(actor).overlay.areas[overlay::Index(area)];
        selected.items.clear();
        selected.manual = true;
        selected.useDefault = true;
    }

    void ActorRegistry::SetAutomaticOverlaySelection(RE::Actor* actor, const overlay::Area area,
        std::optional<std::string> overlayId)
    {
        if (!actor || area == overlay::Area::count ||
            (overlayId && (overlayId->empty() || overlayId->size() > kMaxStringLength))) return;
        std::scoped_lock lock(lock_);
        auto& selected = EnsureLocked(actor).overlay.areas[overlay::Index(area)];
        if (selected.manual || !overlayId) return;
        const auto previous = selected.items.empty() ? OverlayItemState{} : selected.items.front();
        selected.items = { OverlayItemState{ .selectedId = std::move(*overlayId),
            .texturePath = previous.texturePath, .ownedSlot = previous.ownedSlot,
            .color = previous.color } };
        selected.useDefault = false;
    }

    void ActorRegistry::MarkOverlayResolved(RE::Actor* actor, const overlay::Area area,
        const std::string_view overlayId, std::string texturePath, const std::uint8_t ownedSlot)
    {
        if (!actor || area == overlay::Area::count || texturePath.empty() ||
            texturePath.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& selected = EnsureLocked(actor).overlay.areas[overlay::Index(area)];
        if (selected.useDefault) return;
        const auto found = std::ranges::find(selected.items, overlayId, &OverlayItemState::selectedId);
        if (found == selected.items.end()) return;
        found->texturePath = std::move(texturePath);
        found->ownedSlot = ownedSlot;
    }

    void ActorRegistry::SetManualFutanariSkin(
        RE::Actor* actor, std::string skinId, const bool useDefault)
    {
        if (!actor || (!useDefault && skinId.empty()) || skinId.size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& selection = EnsureLocked(actor).futanari;
        selection.selectedSkinId = useDefault ? std::string{} : std::move(skinId);
        selection.manual = true;
        selection.useDefault = useDefault;
    }

    void ActorRegistry::SetAutomaticFutanariSkin(
        RE::Actor* actor, std::optional<std::string> skinId)
    {
        if (!actor || !skinId || skinId->empty() || skinId->size() > kMaxStringLength) return;
        std::scoped_lock lock(lock_);
        auto& selection = EnsureLocked(actor).futanari;
        static_cast<void>(UpdateAutomaticFutanariSelection(selection, skinId));
    }

    void ActorRegistry::SetOverlayColor(RE::Actor* actor, const overlay::Area area,
        const std::string_view overlayId, const std::uint32_t color)
    {
        if (!actor || area == overlay::Area::count) return;
        std::scoped_lock lock(lock_);
        auto& items = EnsureLocked(actor).overlay.areas[overlay::Index(area)].items;
        const auto found = std::ranges::find(items, overlayId, &OverlayItemState::selectedId);
        if (found != items.end()) found->color = color;
    }

    void ActorRegistry::ResetSelectionsToDefaults(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        ResetActorSelectionsToDefaults(EnsureLocked(actor));
    }

    void ActorRegistry::ResetAllSelectionsToDefaults()
    {
        std::scoped_lock lock(lock_);
        for (auto& [formID, state] : states_) {
            static_cast<void>(formID);
            ResetActorSelectionsToDefaults(state);
        }
    }

    bool ActorRegistry::HasManualSelection(const RE::Actor* actor) const
    {
        std::scoped_lock lock(lock_);
        const auto* state = FindValidatedLocked(actor);
        return state && (state->body.selection.manual || state->skin.selection.manual ||
            state->futanari.manual ||
            std::ranges::any_of(state->overlay.areas,
                [](const auto& area) { return area.manual; }));
    }

    void ActorRegistry::SetRuleSelection(RE::Actor* actor, std::optional<std::string> bodyId,
        std::optional<std::string> skinId, const bool useDefaultBody,
        std::optional<std::string> futanariSkinId)
    {
        if (!actor) return;
        std::scoped_lock lock(lock_);
        auto& state = EnsureLocked(actor);
        UpdateAutomaticSelection(state.body.selection, bodyId, useDefaultBody);
        UpdateAutomaticSelection(state.skin.selection, skinId);
        if (futanariSkinId && futanariSkinId->size() <= kMaxStringLength) {
            static_cast<void>(UpdateAutomaticFutanariSelection(state.futanari, futanariSkinId));
        }
    }

    std::uint64_t ActorRegistry::BodySignature(const std::string_view bodyId, const bool useDefault)
    {
        const auto options = useDefault ? 0U : Settings::Get().BodyApplicationOptions();
        return StableStateSignature("body-keyed-v2", bodyId, useDefault,
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

        const auto liveMatches = skin_application::LiveSkinStateMatches(actor, skinId, useDefault);
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

    void ActorRegistry::RestoreSerialized(ActorState state)
    {
        if (state.actorFormID == 0U) return;
        PrepareRestoredState(state);
        std::scoped_lock lock(lock_);
        states_.insert_or_assign(state.actorFormID, std::move(state));
    }

    void ActorRegistry::Revert()
    {
        face_skin::Reset();
        native_addon::Reset();
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
