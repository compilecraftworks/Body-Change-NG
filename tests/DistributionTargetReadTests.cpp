#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

// Only the engine RTTI/registry/string containers are stand-ins. The header
// below is the production validation/read algorithm, not a duplicated policy.
namespace RE {
    struct BaseFormComponent { virtual ~BaseFormComponent() = default; bool live{true}; };
    struct TESForm : BaseFormComponent {
        inline static std::unordered_map<std::uint32_t, TESForm*> registry;
        std::uint32_t id{1}; int type{}; bool deleted{}; int editorCalls{};
        std::string editor{"EditorID"};
        static TESForm* LookupByID(std::uint32_t id) {
            auto it = registry.find(id); return it == registry.end() ? nullptr : it->second;
        }
        auto GetFormID() const { return id; }
        auto GetFormType() const { return type; }
        bool IsDeleted() const { return deleted; }
        virtual const char* GetFormEditorID() { ++editorCalls; return editor.c_str(); }
        const char* GetName() { throw std::runtime_error("unsafe GetName called"); }
    };
    struct TESFullName : BaseFormComponent {
        std::string fullName{"Visible name"};
        virtual const char* GetFullName() { throw std::runtime_error("unsafe name virtual called"); }
    };
    struct TESFaction : TESForm, TESFullName { static constexpr int FORMTYPE=11; };
    struct TESRace : TESForm, TESFullName { static constexpr int FORMTYPE=14; };
    struct TESClass : TESForm, TESFullName { static constexpr int FORMTYPE=10; };
    struct BGSKeyword : TESForm { static constexpr int FORMTYPE=4; };
}
template<class To> To skyrim_cast(RE::BaseFormComponent* from) {
    return from && from->live ? dynamic_cast<To>(from) : nullptr;
}
#include "BodyChangeNG/DistributionTargetRead.h"

void Check(bool condition) { if (!condition) throw std::runtime_error("target read regression"); }

template<class T> void TestType() {
    T form;
    form.type=T::FORMTYPE;
    RE::TESForm::registry={{form.id,&form}};
    int consumed{};
    auto consume = [&](T* actual, const char* name, const char* editor) {
        Check(actual==&form && std::string(editor)=="EditorID");
        if constexpr (std::is_base_of_v<RE::TESFullName,T>) Check(std::string(name)=="Visible name");
        else Check(std::string(name).empty());
        ++consumed;
    };
    Check(bcn::distribution_target::Read<T>(&form,consume));
    Check(consumed==1 && form.editorCalls==1);
    Check(!bcn::distribution_target::Read<T>(nullptr,consume));
    form.deleted=true;
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    form.deleted=false; form.type=999;
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    form.type=T::FORMTYPE;
    static_cast<RE::TESForm&>(form).live=false;
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    static_cast<RE::TESForm&>(form).live=true;
    RE::TESForm::registry.clear();
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    T replacement;
    RE::TESForm::registry={{form.id,&replacement}};
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    form.id=0; RE::TESForm::registry={{0,&form}};
    Check(!bcn::distribution_target::Read<T>(&form,consume));
    Check(consumed==1 && form.editorCalls==1);
    form.id=1; RE::TESForm::registry={{1,&form}};
    if constexpr (std::is_base_of_v<RE::TESFullName,T>) {
        static_cast<RE::TESFullName&>(form).live=false;
        Check(bcn::distribution_target::Read<T>(&form,[&](T*,const char* name,const char* editor) {
            Check(std::string(name).empty() && std::string(editor)=="EditorID");
        }));
        static_cast<RE::TESFullName&>(form).live=true;
        form.fullName.clear(); form.editor.clear();
        Check(bcn::distribution_target::Read<T>(&form,[](T*,const char* name,const char* editor) {
            Check(std::string(name).empty() && std::string(editor).empty());
        }));
    }
}

int main() {
    TestType<RE::TESFaction>(); TestType<RE::TESRace>();
    TestType<RE::TESClass>(); TestType<RE::BGSKeyword>();
    // Mirrors the report: stored type/identity cannot make a BaseFormComponent
    // implement TESForm. No form fields or virtuals may be used before RTTI.
    RE::BaseFormComponent tornDown;
    bool invalidConsumed{};
    Check(!bcn::distribution_target::Read<RE::TESFaction>(reinterpret_cast<RE::TESForm*>(&tornDown),
        [&](auto*,auto*,auto*) { invalidConsumed=true; }));
    RE::TESRace wrongType; wrongType.type=RE::TESFaction::FORMTYPE;
    Check(!bcn::distribution_target::Read<RE::TESFaction>(&wrongType,
        [&](auto*,auto*,auto*) { invalidConsumed=true; }));
    Check(!invalidConsumed);
    struct HookedFaction : RE::TESFaction {
        const char* GetFormEditorID() override { return "HookProvidedEditorID"; }
    } hooked;
    hooked.type=RE::TESFaction::FORMTYPE;
    RE::TESForm::registry={{hooked.id,&hooked}};
    Check(bcn::distribution_target::Read<RE::TESFaction>(&hooked,[](auto*,auto*,const char* editor) {
        Check(std::string(editor)=="HookProvidedEditorID");
    }));
    std::cout << "Distribution target reads passed (4 categories, invalid base/component, stale registry, editor-ID hook)\n";
}
