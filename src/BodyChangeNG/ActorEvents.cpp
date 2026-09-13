#include "BodyChangeNG/ActorEvents.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AppearanceEventPolicy.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariSupport.h"
#include "BodyChangeNG/FaceSkinOverrides.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/RenderedOutfit.h"
#include "BodyChangeNG/NativeSkinBackend.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinApplication.h"

#include <RE/R/RaceSexMenu.h>
#include <RE/T/TESCellAttachDetachEvent.h>
#include <RE/T/TESContainerChangedEvent.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <SKSE/Logger.h>

namespace bcn
{
    namespace
    {
        std::atomic_uint64_t g_raceMenuRestoreGeneration{};
        std::mutex g_equipmentLock;
        std::unordered_map<RE::FormID, std::uint64_t> g_equipmentGeneration;
        std::uint64_t g_nextEquipmentGeneration{};

        [[nodiscard]] bool UsesGenitalSlot(const RE::TESForm* form)
        {
            if (!form) return false;
            constexpr auto slot = static_cast<std::uint32_t>(
                RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);
            if (form->GetFormType() == RE::FormType::Armor) {
                const auto* armor = static_cast<const RE::TESObjectARMO*>(form);
                return (static_cast<std::uint32_t>(armor->GetSlotMask().underlying()) & slot) != 0U;
            }
            if (form->GetFormType() == RE::FormType::Armature) {
                const auto* addon = static_cast<const RE::TESObjectARMA*>(form);
                return (static_cast<std::uint32_t>(addon->GetSlotMask().underlying()) & slot) != 0U;
            }
            return false;
        }

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

        void ReconcileEquipmentChange(const RE::ActorHandle& handle, const RE::FormID actorFormID,
            const std::uint32_t remainingHops,
            const std::uint64_t generation, const std::uint64_t session,
            const unsigned retries = 60)
        {
            const auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                FinishEquipmentChange(actorFormID, generation);
                return;
            }
            frame_tasks::Queue(actorFormID,
                [handle, actorFormID, generation, session, retries] {
                if (!IsCurrentEquipmentChange(actorFormID, generation, session)) return;
                const auto actor = handle.get();
                if (!actor || actor->GetFormID() != actorFormID) {
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                if (frame_tasks::HasPreview(actorFormID)) {
                    ReconcileEquipmentChange(handle, actorFormID, 1, generation, session, retries);
                    return;
                }
                if (!actor->Is3DLoaded()) {
                    if (retries) {
                        ReconcileEquipmentChange(handle, actorFormID, 2, generation, session,
                            retries - 1);
                        return;
                    }
                    FinishEquipmentChange(actorFormID, generation);
                    return;
                }
                // Native BodySkin lives on ActorBase -> Skin Armor -> ARMA ->
                // TXST and therefore survives equipment replacement without a
                // repaint. Equipment events own only outfit morph correction
                // and optional external genital addons.
                ActorRegistry::Get().InvalidateOutfit(actor.get());
                OutfitRefit::Get().ProcessActor(actor.get());
                skin_application::QueueReapplyCurrentMaleGenitals(actor.get(), true);
                skin_application::InvalidateFutanariDetection(actorFormID);
                skin_application::QueueReapplyCurrentFutanari(actor.get(), true);
                FinishEquipmentChange(actorFormID, generation);
            }, std::max(1U, remainingHops),
                appearance::WorkChannel::equipmentReconcile, true);
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

                // RaceMenu may recreate body geometry and player tint arrays
                // at close. The native Skin Armor graph remains authoritative
                // for every new body clone; QueueApply only verifies or
                // reattaches the already-owned graph after the rebuild settles.
                const auto saved = ActorRegistry::Get().Snapshot(actor.get());
                if (saved && saved->body.selection.manual && saved->body.selection.useDefault) {
                    racemenu::QueueClearBodyChangeMorphs(actor.get());
                } else if (racemenu::CurrentPresetId(actor.get())) {
                    racemenu::QueueReapplyCurrent(actor.get());
                }
                if (saved && saved->skin.selection.manual && saved->skin.selection.useDefault) {
                    [[maybe_unused]] const auto skinResult = skin_application::QueueClear(actor.get());
                } else if (const auto skin = skin_application::CurrentProfileId(actor.get())) {
                    [[maybe_unused]] const auto skinResult = skin_application::QueueApply(actor.get(), *skin);
                }
                skin_application::InvalidateFutanariDetection(actor->GetFormID());
                skin_application::QueueReapplyCurrentFutanari(actor.get());
                overlay::QueueReapplySaved(actor.get());
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
            }, std::max(1U, remainingHops),
                appearance::WorkChannel::raceMenuRestore, true);
        }
    }

    ActorEvents& ActorEvents::Get()
    {
        static ActorEvents events;
        return events;
    }

    void ActorEvents::QueuePlayerLoadRestoration()
    {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            const auto generation = g_raceMenuRestoreGeneration.load(std::memory_order_acquire);
            // Use the existing skin/body restoration queue and its bounded
            // readiness checks, not a second polling service for addons.
            ReapplyPlayerSelectionsAfterRaceMenu(player->GetHandle(), 2U, 120U, 0U, generation);
        }
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
                if (actor == RE::PlayerCharacter::GetSingleton()) {
                    QueuePlayerLoadRestoration();
                }
                [[maybe_unused]] const auto requested =
                    ActorWorkQueue::Get().Request(actor, ActorWorkReason::cellAttached);
            } else if (actor != RE::PlayerCharacter::GetSingleton()) {
                ActorWorkQueue::Get().NotifyDetached(actor->GetFormID());
                {
                    std::scoped_lock lock(g_equipmentLock);
                    g_equipmentGeneration.erase(actor->GetFormID());
                }
                body_family::ForgetActorState(actor->GetFormID());
                rendered_outfit::Forget(actor->GetFormID());
                racemenu::ForgetActorState(actor->GetFormID());
                overlay::ForgetActorState(actor->GetFormID());
                skin_application::ForgetActorState(actor->GetFormID());
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
            skin_application::InvalidateFutanariDetection(actor->GetFormID());
            // A saved Default may still need its private live-material
            // baseline restored when an addon is re-equipped.
            const auto tracked = ActorRegistry::Get().Snapshot(actor).has_value();
            const auto* base = actor->GetActorBase();
            const auto female = base && base->GetSex() == RE::SEX::kFemale;
            const auto genitalSlotChanged = UsesGenitalSlot(form);
            const auto hasFutanariSkin = genitalSlotChanged && female &&
                (tracked || futanari_support::Available());
            const auto hasMaleGenitalSkin = genitalSlotChanged && !female &&
                (tracked || skin_application::HasCurrentMaleGenitalSkin(actor));
            const auto needsOutfit = Settings::Get().OutfitCorrectionEnabled() ||
                racemenu::HasOutfitCorrection(actor);
            const auto needsFutanariReconcile = hasFutanariSkin && appearance::NeedsReconcile(
                appearance::Feature::futanariAddon, appearance::Event::equipmentChanged);
            const auto needsMaleGenitalReconcile = hasMaleGenitalSkin && appearance::NeedsReconcile(
                appearance::Feature::maleGenitalAddon, appearance::Event::equipmentChanged);
            const auto needsOutfitReconcile = needsOutfit && appearance::NeedsReconcile(
                appearance::Feature::outfitMorph, appearance::Event::equipmentChanged);
            if (!needsMaleGenitalReconcile && !needsFutanariReconcile &&
                !needsOutfitReconcile) {
                return RE::BSEventNotifyControl::kContinue;
            }
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
        const auto needsOutfit = actor && (Settings::Get().OutfitCorrectionEnabled() ||
            racemenu::HasOutfitCorrection(actor));
        const auto needsMaleGenital = actor && skin_application::HasCurrentMaleGenitalSkin(actor);
        if (!actor || !actor->IsDead() ||
            (!needsOutfit && !needsMaleGenital &&
                !skin_application::CurrentFutanariProfileId(actor))) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Removing equipped armor through a corpse container does not reliably
        // emit TESEquipEvent for that corpse. Coalesce every removed item and
        // reconcile only the final naked Biped state.
        const auto generation = BeginEquipmentChange(actor->GetFormID());
        ReconcileEquipmentChange(actor->GetHandle(), actor->GetFormID(), 3U, generation,
            ActorRegistry::Get().SessionGeneration());
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
        // Biped skin consumes native forms during rebuild. Face skin uses
        // NiOverride's public DDS API after rebuild. Slot-52 addons keep their
        // independent native TXST reconciliation; no general skin repaint.
        if (event && event->reference) {
            if (auto* actor = event->reference->As<RE::Actor>();
                actor && frame_tasks::Active()) {
                // Notify before registry filtering: an addon-only rebuild can
                // have a barrier without a general body-skin selection.
                face_skin::OnNiNodeUpdate(actor);
                const auto* base = actor->GetActorBase();
                const auto mayRegisterFutanari = base && base->GetSex() == RE::SEX::kFemale &&
                    futanari_support::Available();
                if (!ActorRegistry::Get().Snapshot(actor) &&
                    !skin_application::HasTrackedSelection(actor) && !mayRegisterFutanari) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto handle = actor->GetHandle();
                frame_tasks::Queue(actor->GetFormID(), [handle] {
                    const auto resolved = handle.get();
                    auto* actor = resolved.get();
                    if (!actor || !actor->Is3DLoaded()) return;
                    if (appearance::NeedsReconcile(
                            appearance::Feature::maleGenitalAddon,
                            appearance::Event::niNodeUpdated)) {
                        skin_application::QueueReapplyCurrentMaleGenitals(actor, true);
                    }
                    if (appearance::NeedsReconcile(
                            appearance::Feature::futanariAddon,
                            appearance::Event::niNodeUpdated)) {
                        skin_application::QueueReapplyCurrentFutanari(actor, true);
                    }
                }, 2U, appearance::WorkChannel::equipmentVerify);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
}
