#include "BodyChangeNG/ActorEvents.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinOverrides.h"

#include <RE/R/RaceSexMenu.h>
#include <RE/T/TESCellAttachDetachEvent.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <SKSE/Logger.h>

namespace bcn
{
    namespace
    {
        std::atomic_uint64_t g_raceMenuRestoreGeneration{};
        std::mutex g_equipmentLock;
        std::unordered_map<RE::FormID, std::uint64_t> g_equipmentGeneration;
        std::uint64_t g_nextEquipmentGeneration{};

        [[nodiscard]] std::uint64_t BeginEquipmentChange(const RE::FormID actorFormID)
        {
            std::scoped_lock lock(g_equipmentLock);
            return g_equipmentGeneration[actorFormID] = ++g_nextEquipmentGeneration;
        }

        [[nodiscard]] bool IsCurrentEquipmentChange(const RE::FormID actorFormID,
            const std::uint64_t generation, const std::uint64_t session)
        {
            if (ActorRegistry::Get().SessionGeneration() != session) return false;
            std::scoped_lock lock(g_equipmentLock);
            const auto found = g_equipmentGeneration.find(actorFormID);
            return found != g_equipmentGeneration.end() && found->second == generation;
        }

        void FinishEquipmentChange(const RE::FormID actorFormID,
            const std::uint64_t generation)
        {
            std::scoped_lock lock(g_equipmentLock);
            const auto found = g_equipmentGeneration.find(actorFormID);
            if (found != g_equipmentGeneration.end() && found->second == generation) {
                g_equipmentGeneration.erase(found);
            }
        }

        void VerifyEquipmentSkin(const RE::ActorHandle& handle, const RE::FormID actorFormID,
            const std::uint64_t generation, const std::uint64_t session,
            const unsigned remainingRepairs, const unsigned remainingLoadRetries = 60)
        {
            if (!SKSE::GetTaskInterface()) {
                FinishEquipmentChange(actorFormID, generation);
                return;
            }
            frame_tasks::Queue(actorFormID,
                [handle, actorFormID, generation, session, remainingRepairs, remainingLoadRetries] {
                    if (!IsCurrentEquipmentChange(actorFormID, generation, session)) return;
                    const auto actor = handle.get();
                    if (!actor || actor->GetFormID() != actorFormID) {
                        FinishEquipmentChange(actorFormID, generation);
                        return;
                    }
                    if (!actor->Is3DLoaded()) {
                        if (remainingLoadRetries != 0U) {
                            VerifyEquipmentSkin(handle, actorFormID, generation, session,
                                remainingRepairs, remainingLoadRetries - 1U);
                        } else {
                            FinishEquipmentChange(actorFormID, generation);
                        }
                        return;
                    }

                    const auto skin = skin_override::CurrentProfileId(actor.get());
                    if (!skin) {
                        FinishEquipmentChange(actorFormID, generation);
                        return;
                    }
                    const auto liveMatches = skin_override::LiveSkinStateMatches(
                        actor.get(), *skin, false, skin_override::LiveCheckScope::equipmentParts);
                    if (liveMatches.value_or(false)) {
                        FinishEquipmentChange(actorFormID, generation);
                        return;
                    }
                    if (remainingRepairs == 0U) {
                        SKSE::log::warn(
                            "Body Change NG could not verify skin '{}' after equipment rebuild for actor {:08X}; the desired selection remains pending",
                            *skin, actorFormID);
                        FinishEquipmentChange(actorFormID, generation);
                        return;
                    }

                    const auto result = skin_override::QueueApply(actor.get(), *skin);
                    SKSE::log::debug(
                        "Body Change NG repaired skin after equipment rebuild actor={:08X} remaining-passes={} result={}",
                        actorFormID, remainingRepairs, static_cast<std::uint32_t>(result));
                    VerifyEquipmentSkin(handle, actorFormID, generation, session,
                        remainingRepairs - 1U, remainingLoadRetries);
                }, 4U, 104U, true);
        }

        void ReconcileEquipmentChange(const RE::ActorHandle& handle, const RE::FormID actorFormID,
            const std::uint32_t remainingHops,
            const std::uint64_t generation, const std::uint64_t session,
            const bool verifyFinalSkin, const unsigned retries = 60)
        {
            const auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                FinishEquipmentChange(actorFormID, generation);
                return;
            }
            frame_tasks::Queue(actorFormID,
                [handle, actorFormID, generation, session, verifyFinalSkin, retries] {
                if (!IsCurrentEquipmentChange(actorFormID, generation, session)) return;
                const auto actor = handle.get();
                if (!actor || actor->GetFormID() != actorFormID) {
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                if (!actor->Is3DLoaded()) {
                    if (retries) {
                        ReconcileEquipmentChange(handle, actorFormID, 2, generation, session,
                            verifyFinalSkin, retries - 1);
                        return;
                    }
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                // The Biped clone has had time to rebuild. Rebuild only the outfit key
                // and repaint the already selected skin onto new embedded body
                // geometry; body distribution is deliberately not rerun.
                ActorRegistry::Get().InvalidateOutfit(actor.get());
                OutfitRefit::Get().ProcessActor(actor.get());
                skin_override::InvalidateFutanariDetection(actorFormID);
                skin_override::QueueReapplyCurrentFutanari(actor.get());
                if (const auto skin = skin_override::CurrentProfileId(actor.get())) {
                    [[maybe_unused]] const auto result = skin_override::QueueApply(actor.get(), *skin);
                    if (verifyFinalSkin) {
                        // Corpse looting can replace the naked Biped clone after
                        // TESEquipEvent. Verify that final clone rather than
                        // assuming this first repaint survives.
                        VerifyEquipmentSkin(handle, actorFormID, generation, session, 2U);
                        return;
                    }
                }
                FinishEquipmentChange(actorFormID, generation);
            }, std::max(1U, remainingHops), 102, true);
        }

        void ReapplyPlayerSelectionsAfterRaceMenu(const RE::ActorHandle& handle,
            const std::uint32_t remainingHops, const std::uint32_t remainingLoadRetries,
            const std::uint32_t remainingVerificationPasses, const std::uint64_t generation)
        {
            const auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) return;
            frame_tasks::Queue(0, [handle, remainingLoadRetries, remainingVerificationPasses, generation] {
                if (g_raceMenuRestoreGeneration.load(std::memory_order_acquire) != generation) return;
                const auto actor = handle.get();
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!actor || !player || actor->GetFormID() != player->GetFormID()) return;
                auto* ui = RE::UI::GetSingleton();
                const auto raceMenuOpen = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
                if (!actor->Is3DLoaded() || raceMenuOpen || frame_tasks::HasActorWork(actor->GetFormID())) {
                    if (remainingLoadRetries != 0U) {
                        ReapplyPlayerSelectionsAfterRaceMenu(handle, 2U, remainingLoadRetries - 1U,
                            remainingVerificationPasses, generation);
                    } else {
                        SKSE::log::warn("Body Change NG could not restore player selections after RaceMenu "
                                        "because its rebuilt state did not settle");
                    }
                    return;
                }

                // RaceMenu may recreate all three backing objects at close:
                // the body geometry, NiOverride texture targets and player
                // tint arrays. Reapply only the selections already owned by
                // Body Change NG, after those replacements have settled.
                if (racemenu::CurrentPresetId(actor.get())) {
                    racemenu::QueueReapplyCurrent(actor.get());
                }
                if (const auto skin = skin_override::CurrentProfileId(actor.get())) {
                    [[maybe_unused]] const auto skinResult = skin_override::QueueApply(actor.get(), *skin);
                }
                skin_override::InvalidateFutanariDetection(actor->GetFormID());
                skin_override::QueueReapplyCurrentFutanari(actor.get());
                [[maybe_unused]] const auto tintResult = player_tint::QueueReapplyCurrent();
                SKSE::log::info("Body Change NG queued player body, skin and tint restoration after RaceMenu close "
                                "generation={} verification-passes-left={}",
                    generation, remainingVerificationPasses);
                if (remainingVerificationPasses != 0U) {
                    // RaceMenu and third-party overlays can finish one more
                    // deferred rebuild after the close event. A generation-
                    // guarded second pass verifies the final objects without
                    // allowing an older menu session to overwrite new input.
                    ReapplyPlayerSelectionsAfterRaceMenu(handle, 3U, 120U,
                        remainingVerificationPasses - 1U, generation);
                }
            }, std::max(1U, remainingHops), 103, true);
        }
    }

    ActorEvents& ActorEvents::Get()
    {
        static ActorEvents events;
        return events;
    }

    void ActorEvents::Register()
    {
        if (registered_) return;
        if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
            events->AddEventSink<RE::TESInitScriptEvent>(this);
            events->AddEventSink<RE::TESCellAttachDetachEvent>(this);
            events->AddEventSink<RE::TESEquipEvent>(this);
            events->AddEventSink<RE::TESContainerChangedEvent>(this);
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
            }
            if (auto* nodeUpdates = SKSE::GetNiNodeUpdateEventSource()) {
                nodeUpdates->AddEventSink(this);
            }
            registered_ = true;
            SKSE::log::info("Body Change NG registered the NPC initialization event sink");
        }
    }

    void ActorEvents::ResetSessionState()
    {
        g_raceMenuRestoreGeneration.fetch_add(1U, std::memory_order_acq_rel);
        std::scoped_lock lock(g_equipmentLock);
        g_equipmentGeneration.clear();
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const RE::TESCellAttachDetachEvent* event,
        RE::BSTEventSource<RE::TESCellAttachDetachEvent>*)
    {
        if (!event || !event->reference) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (auto* actor = event->reference->As<RE::Actor>()) {
            if (event->attached) {
                [[maybe_unused]] const auto requested =
                    ActorWorkQueue::Get().Request(actor, ActorWorkReason::cellAttached);
            } else if (actor != RE::PlayerCharacter::GetSingleton()) {
                ActorWorkQueue::Get().NotifyDetached(actor->GetFormID());
                {
                    std::scoped_lock lock(g_equipmentLock);
                    g_equipmentGeneration.erase(actor->GetFormID());
                }
                body_family::ForgetActorState(actor->GetFormID());
                racemenu::ForgetActorState(actor->GetFormID());
                skin_override::ForgetActorState(actor->GetFormID());
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const RE::TESInitScriptEvent* event,
        RE::BSTEventSource<RE::TESInitScriptEvent>*)
    {
        if (!event || !event->objectInitialized) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (auto* actor = event->objectInitialized->As<RE::Actor>()) {
            [[maybe_unused]] const auto requested =
                ActorWorkQueue::Get().Request(actor, ActorWorkReason::initialized);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const RE::TESEquipEvent* event,
        RE::BSTEventSource<RE::TESEquipEvent>*)
    {
        if (!event || !event->actor || event->baseObject == 0) return RE::BSEventNotifyControl::kContinue;
        const auto* form = RE::TESForm::LookupByID(event->baseObject);
        if (!form || (form->GetFormType() != RE::FormType::Armor && form->GetFormType() != RE::FormType::Armature)) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (auto* actor = event->actor->As<RE::Actor>()) {
            if (!frame_tasks::Active()) return RE::BSEventNotifyControl::kContinue;
            skin_override::InvalidateFutanariDetection(actor->GetFormID());
            const auto hasSkin = skin_override::CurrentProfileId(actor).has_value();
            const auto hasFutanariSkin = skin_override::CurrentFutanariProfileId(actor).has_value();
            const auto needsOutfit = Settings::Get().OutfitCorrectionEnabled() ||
                racemenu::HasOutfitCorrection(actor);
            if (!hasSkin && !hasFutanariSkin && !needsOutfit) {
                return RE::BSEventNotifyControl::kContinue;
            }
            // TESEquipEvent is emitted before the replacement BipedAnim clone
            // is always available. Consecutive equipment events are coalesced;
            // only the newest settled outfit is corrected and repainted.
            const auto generation = BeginEquipmentChange(actor->GetFormID());
            ReconcileEquipmentChange(actor->GetHandle(), actor->GetFormID(), 2U, generation,
                ActorRegistry::Get().SessionGeneration(), actor->IsDead());
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const RE::TESContainerChangedEvent* event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*)
    {
        if (!event || event->oldContainer == 0 || event->oldContainer == event->newContainer ||
            event->baseObj == 0 || !frame_tasks::Active()) {
            return RE::BSEventNotifyControl::kContinue;
        }
        const auto* item = RE::TESForm::LookupByID(event->baseObj);
        if (!item || item->GetFormType() != RE::FormType::Armor) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto* oldContainer = RE::TESForm::LookupByID(event->oldContainer);
        auto* actor = oldContainer ? oldContainer->As<RE::Actor>() : nullptr;
        if (!actor || !actor->IsDead() ||
            (!skin_override::CurrentProfileId(actor) &&
                !skin_override::CurrentFutanariProfileId(actor))) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Removing equipped armor through a corpse container does not reliably
        // emit TESEquipEvent for that corpse. Coalesce every removed item and
        // reconcile only the final naked Biped state.
        const auto generation = BeginEquipmentChange(actor->GetFormID());
        ReconcileEquipmentChange(actor->GetHandle(), actor->GetFormID(), 3U, generation,
            ActorRegistry::Get().SessionGeneration(), true);
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!event || event->menuName != RE::RaceSexMenu::MENU_NAME) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (event->opening) {
            // Invalidate a deferred close restoration from an older RaceMenu
            // session before the new editor begins rebuilding the player.
            g_raceMenuRestoreGeneration.fetch_add(1U, std::memory_order_acq_rel);
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                frame_tasks::CancelActor(player->GetFormID());
                std::scoped_lock lock(g_equipmentLock);
                g_equipmentGeneration.erase(player->GetFormID());
            }
            return RE::BSEventNotifyControl::kContinue;
        }
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            // Each retry waits for external input updates, not SKSE FIFO hops.
            const auto generation = g_raceMenuRestoreGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
            ReapplyPlayerSelectionsAfterRaceMenu(player->GetHandle(), 3U, 120U, 2U, generation);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ActorEvents::ProcessEvent(
        const SKSE::NiNodeUpdateEvent* event,
        RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
    {
        if (event && event->reference) {
            if (auto* actor = event->reference->As<RE::Actor>()) {
                skin_override::NotifyNiNodeUpdated(actor);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
