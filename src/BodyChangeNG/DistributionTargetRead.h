#pragma once

#include <type_traits>

// Included after the engine headers. Kept separate so the same read path can
// be exercised with hostile component/vtable stand-ins outside the game.
namespace bcn::distribution_target
{
    template <class T>
    [[nodiscard]] T* Validate(RE::TESForm* candidate)
    {
        if (!candidate) return nullptr;
        // TESForm::As<T>() trusts the stored form-type byte. It cannot detect a
        // torn-down object whose base vtable no longer implements TESForm.
        auto* typed = skyrim_cast<T*>(static_cast<RE::BaseFormComponent*>(candidate));
        if (!typed || typed->GetFormType() != T::FORMTYPE || typed->IsDeleted()) return nullptr;
        const auto id = typed->GetFormID();
        if (!id || RE::TESForm::LookupByID(id) != candidate) return nullptr;
        return typed;
    }

    template <class T>
    [[nodiscard]] const char* Name(T* form)
    {
        if constexpr (std::is_base_of_v<RE::TESFullName, T>) {
            auto* component = static_cast<RE::TESFullName*>(form);
            auto* live = skyrim_cast<RE::TESFullName*>(static_cast<RE::BaseFormComponent*>(component));
            // A valid form can still have a torn-down name component. Preserve
            // the option using its editor ID / plugin:ID instead of calling a
            // nonexistent GetFullName virtual slot on BaseFormComponent.
            if (live == component) return live->fullName.c_str();
        }
        return "";
    }

    template <class T, class Consume>
    bool Read(RE::TESForm* candidate, Consume&& consume)
    {
        auto* form = Validate<T>(candidate);
        if (!form) return false;
        const auto* name = Name(form);
        // Preserve editor-ID provider hooks, but only on the validated live
        // concrete form, never on an array entry trusted by its type byte alone.
        consume(form, name, form->GetFormEditorID());
        return true;
    }
}
