#include "BodyChangeNG/RaceMenuFormDeleteGuard.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/RaceMenuFormDeletePolicy.h"
#include <SKSE/Logger.h>
#include <Windows.h>

#ifndef EXCLUSIVE_SKYRIM_FLAT
#error FormDelete compatibility is SE/AE-flat only.
#endif
namespace bcn::racemenu_form_delete
{
    void InstallInGame()
    {
        // DataLoaded, before saves: use the loaded VM's public policy, not
        // hardcoded VM vtable addresses. These are non-persisting queries.
        // No Actor/VM pointers are retained after this startup check.
        const auto version=REL::Module::get().version();
        const bool supported=runtime::ResolveGameBranch(version)!=runtime::GameBranch::unsupported;
        auto* vm=supported ? RE::BSScript::Internal::VirtualMachine::GetSingleton() : nullptr;
        auto* policy=vm ? vm->GetObjectHandlePolicy() : nullptr;
        auto* player=supported ? RE::PlayerCharacter::GetSingleton() : nullptr;
        auto* base=player ? player->GetActorBase() : nullptr;
        bool verified{};
        if (policy && player && base) {
            verified=VerifyDirectFormSamples(policy->EmptyHandle(),player->GetFormID(),
                policy->GetHandleForObject(static_cast<RE::VMTypeID>(player->GetFormType()),player),
                base->GetFormID(),policy->GetHandleForObject(static_cast<RE::VMTypeID>(base->GetFormType()),base));
        }
        const auto installed=Install(GetModuleHandleW(L"skee64.dll"),verified);
        SKSE::log::warn("BCNG FormDelete guard {}: {}",installed,Status());
    }
}
