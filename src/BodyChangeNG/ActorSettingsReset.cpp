#include "BodyChangeNG/ActorSettingsReset.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/SkinApplication.h"

#include <unordered_set>

namespace
{
    [[nodiscard]] bcn::actor_settings_reset::Result QueueActorReset(
        RE::Actor* actor, const bool includeTint)
    {
        if (!actor || !actor->GetActorBase() || !bcn::frame_tasks::Active() ||
            !SKSE::GetTaskInterface() || bcn::runtime::ResolveGameBranch(REL::Module::get().version()) ==
                bcn::runtime::GameBranch::unsupported) return {};

        bcn::ActorRegistry::Get().ResetSelectionsToDefaults(actor);
        bcn::overlay::DiscardPreviewsForReset(actor);
        if (bcn::racemenu::IsReady()) {
            bcn::racemenu::QueueClearBodyChangeMorphs(actor);
        }
        (void)bcn::skin_application::QueueClear(actor);

        if (const auto* base = actor->GetActorBase();
            base && base->GetSex() == RE::SEX::kFemale) {
            (void)bcn::skin_application::QueueClearFutanari(actor,
                bcn::skin_application::FutanariSelectionMode::restore);
        }

        // Remove exact owned nodes; keep pending ownership if an interface is unavailable.
        for (const auto area : bcn::overlay::kAreas) {
            (void)bcn::overlay::QueueReset(actor, area);
        }
        if (includeTint && actor->IsPlayerRef()) {
            (void)bcn::player_tint::QueueRestoreAll(true);
        }
        // Accepted is persisted Default intent, not a claim that unloaded 3D changed.
        return { .actors = 1U, .accepted = true };
    }
}

namespace bcn::actor_settings_reset
{
    Result QueueActor(RE::Actor* actor)
    {
        return QueueActorReset(actor, true);
    }

    Result QueueAll()
    {
        if (!frame_tasks::Active() || !SKSE::GetTaskInterface() ||
            runtime::ResolveGameBranch(REL::Module::get().version()) ==
                runtime::GameBranch::unsupported) return {};
        std::unordered_set<RE::FormID> actorIds;
        const auto saved = ActorRegistry::Get().SnapshotAll();
        actorIds.reserve(saved.size() + 1U);
        for (const auto& state : saved) {
            if (state.actorFormID != 0U) actorIds.insert(state.actorFormID);
        }
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            actorIds.insert(player->GetFormID());
        }

        Result result;
        for (const auto actorId : actorIds) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
            if (!actor) continue;
            if (!actor->IsPlayerRef() && !ActorRegistry::Get().Snapshot(actor)) continue;
            const auto queued = QueueActorReset(actor, true);
            result.actors += queued.actors;
            result.accepted = result.accepted || queued.accepted;
        }

        // RaceMenu may still contain an owned morph key from an older save
        // schema even when that actor no longer has an ASTR registry record.
        result.accepted = racemenu::QueueClearAllBodyChangeMorphs({ actorIds.begin(), actorIds.end() }) || result.accepted;
        ActorRegistry::Get().ResetAllSelectionsToDefaults();
        result.accepted = result.accepted || !saved.empty();
        return result;
    }
}
