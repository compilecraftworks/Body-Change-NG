#pragma once

#include <RE/M/MenuOpenCloseEvent.h>

namespace bcn
{
    class ActorEvents final : public RE::BSTEventSink<RE::TESInitScriptEvent>,
                              public RE::BSTEventSink<RE::TESCellAttachDetachEvent>,
                              public RE::BSTEventSink<RE::TESEquipEvent>,
                              public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static ActorEvents& Get();

        void Register();
        // Invalidates deferred callbacks and releases per-actor coalescing
        // state whenever a new game/save session becomes active.
        void ResetSessionState();

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESInitScriptEvent* a_event,
            RE::BSTEventSource<RE::TESInitScriptEvent>* a_source) override;

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCellAttachDetachEvent* a_event,
            RE::BSTEventSource<RE::TESCellAttachDetachEvent>* a_source) override;

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEquipEvent* a_event,
            RE::BSTEventSource<RE::TESEquipEvent>* a_source) override;

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:
        bool registered_{};
    };
}
