#include "BodyChangerNG/ActorCatalog.h"

#include <RE/T/TES.h>

#include <algorithm>
#include <ranges>
#include <unordered_set>

namespace
{
    // Match Skyrim Fitting System's workbench discovery boundary.  ProcessLists
    // also contains loaded/detached temporary actors outside the local scene;
    // listing those can leave a stale FFxxxxxx selection whose camera target
    // no longer exists.  A local reference walk keeps the selector and camera
    // pointed at the same live actor.
    constexpr float kNearbyActorRadius = 4096.0F;
    constexpr std::size_t kMaximumNearbyActors = 32;

    [[nodiscard]] bool HasMeaningfulName(RE::Actor* actor)
    {
        if (!actor) return false;
        if (const auto* name = actor->GetDisplayFullName(); name && name[0] != '\0') return true;
        if (const auto* base = actor->GetActorBase()) {
            if (const auto* name = base->GetName(); name && name[0] != '\0') return true;
        }
        return false;
    }

    [[nodiscard]] bool IsSelectableActor(RE::Actor* actor, RE::Actor* player)
    {
        return actor && player && !actor->IsDisabled() && actor->Is3DLoaded() && !actor->IsDead() &&
            HasMeaningfulName(actor) && !actor->IsHostileToActor(player) &&
            (actor->IsPlayerTeammate() || actor->HasKeywordString("ActorTypeNPC"));
    }

    [[nodiscard]] std::string DisplayName(RE::Actor* actor, const char* fallback)
    {
        if (actor) {
            if (const auto* name = actor->GetDisplayFullName(); name && name[0] != '\0') return name;
            if (const auto* base = actor->GetActorBase()) {
                if (const auto* name = base->GetName(); name && name[0] != '\0') return name;
            }
        }
        return fallback;
    }

    [[nodiscard]] RE::Actor* ResolveCrosshairActor()
    {
        const auto* pickData = RE::CrosshairPickData::GetSingleton();
        if (!pickData) return nullptr;
        RE::ObjectRefHandle handle;
#if defined(EXCLUSIVE_SKYRIM_FLAT)
        handle = pickData->targetActor;
#else
        handle = pickData->targetActor[RE::VR_DEVICE::kHeadset];
#endif
        const auto reference = handle.get();
        return reference ? reference->As<RE::Actor>() : nullptr;
    }
}

namespace bcn
{
    ActorCatalog& ActorCatalog::Get()
    {
        static ActorCatalog catalog;
        return catalog;
    }

    void ActorCatalog::Refresh(const bool includeCrosshairActor)
    {
        entries_.clear();
        crosshairActorFormID_ = 0;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* tes = RE::TES::GetSingleton();
        if (!player || !tes) return;

        const auto playerBase = player->GetActorBase();
        entries_.push_back({
            .formID = player->GetFormID(),
            .name = DisplayName(player, "Player"),
            .female = playerBase && playerBase->GetSex() == RE::SEX::kFemale,
            .player = true
        });

        struct Candidate final
        {
            ActorEntry entry;
            float distanceSquared{};
        };
        std::vector<Candidate> candidates;
        std::unordered_set<RE::FormID> seen{ player->GetFormID() };
        if (includeCrosshairActor) {
            if (auto* crosshair = ResolveCrosshairActor(); IsSelectableActor(crosshair, player) &&
                seen.insert(crosshair->GetFormID()).second) {
                const auto* base = crosshair->GetActorBase();
                entries_.push_back({
                    .formID = crosshair->GetFormID(),
                    .name = DisplayName(crosshair, "NPC"),
                    .female = base && base->GetSex() == RE::SEX::kFemale
                });
                crosshairActorFormID_ = crosshair->GetFormID();
            }
        }
        const auto playerPosition = player->GetPosition();
        tes->ForEachReferenceInRange(player, kNearbyActorRadius, [&](RE::TESObjectREFR* reference) {
            auto* actor = reference ? reference->As<RE::Actor>() : nullptr;
            if (!IsSelectableActor(actor, player) || !seen.insert(actor->GetFormID()).second) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto actorBase = actor->GetActorBase();
            const auto position = actor->GetPosition();
            const auto dx = position.x - playerPosition.x;
            const auto dy = position.y - playerPosition.y;
            const auto dz = position.z - playerPosition.z;
            candidates.push_back({
                .entry = {
                    .formID = actor->GetFormID(),
                    .name = DisplayName(actor, "NPC"),
                    .female = actorBase && actorBase->GetSex() == RE::SEX::kFemale
                },
                .distanceSquared = dx * dx + dy * dy + dz * dz
            });
            return RE::BSContainer::ForEachResult::kContinue;
        });
        std::ranges::sort(candidates, {}, &Candidate::distanceSquared);
        if (candidates.size() > kMaximumNearbyActors) candidates.resize(kMaximumNearbyActors);
        entries_.reserve(entries_.size() + candidates.size());
        for (auto& candidate : candidates) entries_.push_back(std::move(candidate.entry));
    }

    std::vector<ActorEntry> ActorCatalog::Snapshot() const
    {
        return entries_;
    }

    RE::Actor* ActorCatalog::Resolve(const std::uint32_t formID) const
    {
        return formID == 0 ? nullptr : RE::TESForm::LookupByID<RE::Actor>(formID);
    }

    std::uint32_t ActorCatalog::CrosshairActorFormID() const noexcept
    {
        return crosshairActorFormID_;
    }
}
