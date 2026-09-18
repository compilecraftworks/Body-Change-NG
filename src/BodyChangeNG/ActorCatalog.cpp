#include "BodyChangeNG/ActorCatalog.h"
#include "BodyChangeNG/ActorSearchPolicy.h"
#include "BodyChangeNG/FrameTasks.h"

#include <RE/T/TES.h>

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <mutex>
#include <memory>

namespace
{
    std::mutex g_searchLock;
    bcn::ActorSearchResult g_search;
    std::uint64_t g_searchGeneration{};
    struct SearchJob
    {
        std::uint64_t generation{}, epoch{};
        std::string query;
        std::vector<RE::FormID> references;
        std::vector<bcn::ActorEntry> results;
        std::size_t next{};
        bool collected{};
    };
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
        // Appearance selection is independent of hostility/combat state.
        // Keep the live-target safety checks for both friendly and enemy NPCs.
        return actor && player && !actor->IsDisabled() && actor->Is3DLoaded() && !actor->IsDead() &&
            HasMeaningfulName(actor) &&
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
        handle = pickData->targetActor; // EXCLUSIVE_SKYRIM_FLAT only
        const auto reference = handle.get();
        return reference ? reference->As<RE::Actor>() : nullptr;
    }

    void SearchStep(const std::shared_ptr<SearchJob>& job)
    {
        {
            std::scoped_lock lock(g_searchLock);
            if (job->generation != g_searchGeneration || !bcn::frame_tasks::IsCurrent(job->epoch)) return;
        }
        if (!job->collected) {
            // Hold the form-map lock only while copying reference IDs. Naming,
            // lookup and keyword checks must run AFTER releasing that lock.
            const auto [forms, lock] = RE::TESForm::GetAllForms();
            const RE::BSReadLockGuard guard(lock);
            if (forms) for (const auto& [id, form] : *forms)
                if (form && form->Is(RE::FormType::ActorCharacter)) job->references.push_back(id);
            job->collected = true;
        }
        // Spread actor/name resolution across real game ticks; never scan all
        // forms each frame or for every typed character.
        const auto end = (std::min)(job->references.size(), job->next + 256U);
        for (; job->next < end; ++job->next) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(job->references[job->next]);
            if (!actor || actor->IsDisabled() || actor->IsDead() || !HasMeaningfulName(actor) ||
                (!actor->IsPlayerTeammate() && !actor->HasKeywordString("ActorTypeNPC"))) continue;
            const auto name = DisplayName(actor, "NPC");
            if (!bcn::actor_search::Matches(job->query, name, actor->GetFormID())) continue;
            const auto* base = actor->GetActorBase();
            job->results.push_back({actor->GetFormID(), name,
                base && base->GetSex() == RE::SEX::kFemale, actor == RE::PlayerCharacter::GetSingleton()});
        }
        if (job->next < job->references.size()) {
            if (bcn::frame_tasks::Queue(0U, [job] { SearchStep(job); }, 1U,
                    bcn::appearance::WorkChannel::actorSearch)) return;
            std::scoped_lock lock(g_searchLock);
            if (job->generation == g_searchGeneration) {
                g_search.running = false;
                g_search.failed = true;
            }
            return; // Do not present a partial scan as a complete result.
        }
        std::ranges::sort(job->results, {}, &bcn::ActorEntry::formID);
        std::scoped_lock lock(g_searchLock);
        if (job->generation == g_searchGeneration && bcn::frame_tasks::IsCurrent(job->epoch)) {
            g_search.entries = std::make_shared<const std::vector<bcn::ActorEntry>>(std::move(job->results));
            g_search.running = false;
        }
    }
}

namespace bcn
{
    void ActorCatalog::Search(std::string query)
    {
        query = actor_search::Normalize(query);
        CancelSearch();
        if (query.empty() || !frame_tasks::Active()) return;
        auto job = std::make_shared<SearchJob>();
        job->query = query;
        job->epoch = frame_tasks::Epoch();
        {
            std::scoped_lock lock(g_searchLock);
            job->generation = g_searchGeneration;
            g_search = {std::move(query), {}, true};
        }
        if (!frame_tasks::Queue(0U, [job] { SearchStep(job); }, 1U, appearance::WorkChannel::actorSearch)) {
            std::scoped_lock lock(g_searchLock);
            if (job->generation == g_searchGeneration) {
                g_search.running = false;
                g_search.failed = true;
            }
        }
    }

    void ActorCatalog::CancelSearch()
    {
        std::scoped_lock lock(g_searchLock);
        ++g_searchGeneration;
        g_search = {};
    }

    ActorSearchResult ActorCatalog::SearchSnapshot() const
    {
        std::scoped_lock lock(g_searchLock);
        return g_search;
    }

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
