#include "BodyChangerNG/ActorEvents.h"

#include "BodyChangerNG/ActorRegistry.h"
#include "BodyChangerNG/ActorWorkQueue.h"
#include "BodyChangerNG/Distribution.h"
#include "BodyChangerNG/OutfitRefit.h"
#include "BodyChangerNG/PlayerTint.h"
#include "BodyChangerNG/RaceMenuBodyMorph.h"
#include "BodyChangerNG/Settings.h"
#include "BodyChangerNG/SkinOverrides.h"

#include <RE/R/RaceSexMenu.h>
#include <RE/T/TESCellAttachDetachEvent.h>
#include <SKSE/Logger.h>

namespace bcn
{
    namespace
    {
        std::atomic_uint64_t g_raceMenuRestoreGeneration{};
        std::mutex g_equipmentLock;
        std::unordered_map<RE::FormID, std::uint64_t> g_equipmentGeneration;

        [[nodiscard]] std::uint64_t BeginEquipmentChange(const RE::FormID actorFormID)
        {
            std::scoped_lock lock(g_equipmentLock);
            return ++g_equipmentGeneration[actorFormID];
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

        void ReconcileEquipmentChange(const RE::ActorHandle& handle, const RE::FormID actorFormID,
            const std::uint32_t remainingHops,
            const std::uint64_t generation, const std::uint64_t session)
        {
            const auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                FinishEquipmentChange(actorFormID, generation);
                return;
            }
            tasks->AddTask([handle, actorFormID, remainingHops, generation, session] {
                if (!IsCurrentEquipmentChange(actorFormID, generation, session)) return;
                const auto actor = handle.get();
                if (!actor || actor->GetFormID() != actorFormID) {
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                if (remainingHops != 0U) {
                    ReconcileEquipmentChange(handle, actorFormID, remainingHops - 1U, generation, session);
                    return;
                }
                if (!actor->Is3DLoaded()) {
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                // The Biped clone is now settled. Rebuild only the outfit key
                // and repaint the already selected skin onto new embedded body
                // geometry; body distribution is deliberately not rerun.
                ActorRegistry::Get().InvalidateOutfit(actor.get());
                OutfitRefit::Get().ProcessActor(actor.get());
                if (const auto skin = skin_override::CurrentProfileId(actor.get())) {
                    [[maybe_unused]] const auto result = skin_override::QueueApply(actor.get(), *skin);
                }
                FinishEquipmentChange(actorFormID, generation);
            });
        }

        void ReapplyPlayerSelectionsAfterRaceMenu(const RE::ActorHandle& handle,
            const std::uint32_t remainingHops, const std::uint32_t remainingLoadRetries,
            const std::uint32_t remainingVerificationPasses, const std::uint64_t generation)
        {
            const auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) return;
            tasks->AddTask([handle, remainingHops, remainingLoadRetries, remainingVerificationPasses, generation] {
                if (g_raceMenuRestoreGeneration.load(std::memory_order_acquire) != generation) return;
                const auto actor = handle.get();
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!actor || !player || actor->GetFormID() != player->GetFormID()) return;
                if (remainingHops != 0U) {
                    ReapplyPlayerSelectionsAfterRaceMenu(handle, remainingHops - 1U, remainingLoadRetries,
                        remainingVerificationPasses, generation);
                    return;
                }
                auto* ui = RE::UI::GetSingleton();
                const auto raceMenuOpen = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
                if (!actor->Is3DLoaded() || raceMenuOpen) {
                    if (remainingLoadRetries != 0U) {
                        ReapplyPlayerSelectionsAfterRaceMenu(handle, 1U, remainingLoadRetries - 1U,
                            remainingVerificationPasses, generation);
                    } else {
                        SKSE::log::warn("Body Changer NG could not restore player selections after RaceMenu "
                                        "because its rebuilt state did not settle");
                    }
                    return;
                }

                // RaceMenu may recreate all three backing objects at close:
                // the body geometry, NiOverride texture targets and player
                // tint arrays. Reapply only the selections already owned by
                // Body Changer NG, after those replacements have settled.
                if (racemenu::CurrentPresetId(actor.get())) {
                    racemenu::QueueReapplyCurrent(actor.get());
                    // An equipped outfit uses a separate Body Changer NG
                    // morph key. RaceMenu may rebuild that visible geometry
                    // along with the naked body, so regenerate it after the
                    // committed body task and before repainting textures.
                    ActorRegistry::Get().InvalidateOutfit(actor.get());
                    OutfitRefit::Get().ProcessActor(actor.get());
                }
                if (const auto skin = skin_override::CurrentProfileId(actor.get())) {
                    [[maybe_unused]] const auto skinResult = skin_override::QueueApply(actor.get(), *skin);
                }
                [[maybe_unused]] const auto tintResult = player_tint::QueueReapplyCurrent();
                SKSE::log::info("Body Changer NG queued player body, skin and tint restoration after RaceMenu close "
                                "generation={} verification-passes-left={}",
                    generation, remainingVerificationPasses);
                if (remainingVerificationPasses != 0U) {
                    // RaceMenu and third-party overlays can finish one more
                    // deferred rebuild after the close event. A generation-
                    // guarded second pass verifies the final objects without
                    // allowing an older menu session to overwrite new input.
                    ReapplyPlayerSelectionsAfterRaceMenu(handle, 3U, 4U,
                        remainingVerificationPasses - 1U, generation);
                }
            });
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
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
            }
            registered_ = true;
            SKSE::log::info("Body Changer NG registered the NPC initialization event sink");
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
            // TESEquipEvent is emitted before the replacement BipedAnim clone
            // is always available. Consecutive equipment events are coalesced;
            // only the newest settled outfit is corrected and repainted.
            const auto generation = BeginEquipmentChange(actor->GetFormID());
            ReconcileEquipmentChange(actor->GetHandle(), actor->GetFormID(), 2U, generation,
                ActorRegistry::Get().SessionGeneration());
        }
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
            return RE::BSEventNotifyControl::kContinue;
        }
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            // Character generation can keep replacing these objects for more
            // than the close-event frame. Two task turns avoid racing the last
            // RaceMenu rebuild without introducing a timer or frame loop.
            const auto generation = g_raceMenuRestoreGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
            ReapplyPlayerSelectionsAfterRaceMenu(player->GetHandle(), 3U, 8U, 2U, generation);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
