#include "BodyChangeNG/FacePreviewTransaction.h"
#include "BodyChangeNG/FacePreviewSaveGate.h"
#include "BodyChangeNG/FaceSkinNodeAccess.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include <RE/B/BGSLoadFormData.h>
#include <RE/B/BGSLoadGameSubBuffer.h>
#include <RE/B/BGSSaveLoadGame.h>
#include <RE/B/BGSSaveLoadManager.h>
#include <RE/R/RaceSexMenu.h>
#include <mutex>

#ifndef EXCLUSIVE_SKYRIM_FLAT
#error Face preview transactions target SE/AE flat only.
#endif

namespace
{
    using namespace bcn::face_skin;
    struct Entry { SavedKey original; std::string written; bool active{}; };
    struct Transaction
    {
        RE::ActorHandle handle;
        std::uint32_t actor{}, base{};
        bool female{};
        std::string node;
        NodeAccess api;
        std::array<Entry, kChannels.size()> entries;
    };
    std::mutex g_lock;
    Transaction g_transaction;
    bcn::face_preview::SaveGate g_save;

    bool RestoreLocked()
    {
        if (!g_transaction.actor) return true;
        auto actor = g_transaction.handle.get();
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base || base->GetFormID() != g_transaction.base) {
            // The deleted reference must not be replaced by another actor.
            g_transaction = {};
            return true;
        }
        bool ok = true;
        for (std::size_t i{}; i < kChannels.size(); ++i) {
            auto& entry = g_transaction.entries[i];
            if (!entry.active) continue;
            SavedKey current;
            if (!g_transaction.api.ReadSaved(actor.get(), g_transaction.female, g_transaction.node, kChannels[i], current)) {
                ok = false;
                continue;
            }
            // Do not replace a newer independent provider's value.
            if (current.present && Owns(current.path, entry.written) &&
                !g_transaction.api.RestoreSaved(actor.get(), g_transaction.female,
                    g_transaction.node, kChannels[i], entry.original)) {
                ok = false;
                continue;
            }
            entry = {};
        }
        if (ok) g_transaction = {};
        return ok;
    }
}

namespace bcn::face_preview
{
    bool Publish(const face_skin::NodeAccess& api, RE::Actor* actor, bool female,
        const std::string& node, unsigned channel, const std::string& path)
    {
        // NPC commit intentionally remains live-only. Do not introduce saved
        // NPC overrides just because its preview uses the same face backend.
        if (!actor || !actor->IsPlayerRef() || !api) return true;
        ObserveSaveCompletion();
        const auto found = std::ranges::find(kChannels, channel);
        if (found == kChannels.end() || path.empty()) return false;
        std::scoped_lock lock(g_lock);
        if (g_save.suspended || !frame_tasks::HasPreview(actor->GetFormID())) return true;
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) return true;
        auto* base = actor->GetActorBase();
        if (!base) return false;
        if (g_transaction.actor && (g_transaction.actor != actor->GetFormID() ||
            g_transaction.base != base->GetFormID() || g_transaction.node != node || g_transaction.female != female)) {
            if (!RestoreLocked()) return false;
        }
        if (!g_transaction.actor) {
            g_transaction.handle = actor->GetHandle();
            g_transaction.actor = actor->GetFormID();
            g_transaction.base = base->GetFormID();
            g_transaction.female = female;
            g_transaction.node = node;
            g_transaction.api = api;
        }
        auto& entry = g_transaction.entries[static_cast<std::size_t>(found - kChannels.begin())];
        if (entry.active && Owns(entry.written, path)) return true;
        if (entry.active) {
            // A normal batch restores the whole journal first. Keep this
            // boundary safe even if a caller publishes a different DDS twice.
            SavedKey current;
            if (!api.ReadSaved(actor, female, node, channel, current)) return false;
            if (!current.present || !Owns(current.path, entry.written)) entry.original = std::move(current);
        }
        if (!entry.active) {
            if (!api.ReadSaved(actor, female, node, channel, entry.original)) return false;
            entry.active = true;
        }
        const auto previous = entry.written;
        entry.written = path; // ownership is recorded before a registry mutation
        if (api.SaveCurrent(actor, female, node, channel, path)) return true;
        // SaveCurrent validates the live value BEFORE it mutates the registry.
        // On a validation failure retain ownership of the prior temporary key.
        entry.written = previous;
        return false;
    }

    bool Restore(std::uint32_t actor)
    {
        std::scoped_lock lock(g_lock);
        return actor && g_transaction.actor != actor ? true : RestoreLocked();
    }
    void OwnerChanged()
    {
        std::scoped_lock lock(g_lock);
        // Read current ownership after obtaining this lock; a notification
        // from an earlier UI change must not undo a newer preview's keys.
        if (g_transaction.actor && !frame_tasks::HasPreview(g_transaction.actor) && !RestoreLocked())
            SKSE::log::error("BCNG could not restore temporary face keys after preview ownership changed");
    }
    void BeforeSave()
    {
        // SKSE dispatches kSaveGame BEFORE any plugin's save callback. This
        // lock also excludes publication by an already-running face batch.
        std::scoped_lock lock(g_lock);
        g_save.Begin();
        if (!RestoreLocked()) SKSE::log::error("BCNG could not restore temporary face keys before save");
    }
    void OnSerialization()
    {
        std::scoped_lock lock(g_lock);
        g_save.Serialized();
    }
    void ObserveSaveCompletion()
    {
        std::scoped_lock lock(g_lock);
        if (!g_save.suspended || !g_save.serialized) return;
        if (runtime::ResolveGameBranch(REL::Module::get().version()) == runtime::GameBranch::unsupported) return;
        const auto* state = RE::BGSSaveLoadGame::GetSingleton();
        const auto* manager = RE::BGSSaveLoadManager::GetSingleton();
        // Accessor selects the thread at 0x2B0 before AE 1130, 0x2F8 afterwards.
        // Do not treat our callback as the END of SKSE serialization: RaceMenu
        // may still be next. Wait for the engine's save/load worker to be idle.
        if (state && manager) g_save.ObserveIdle(state->GetSaveGameSaving(),
            state->GetSaveGameLoading(), manager->GetRuntimeData().thread.isBusy);
    }
    void Reset()
    {
        std::scoped_lock lock(g_lock);
        RestoreLocked();
        g_save = {};
    }
}
