#include "BodyChangeNG/SkinCatalog.h"

#include "BodyChangeNG/ActorRegistry.h"

#include <SKSE/Logger.h>

#include <algorithm>
#include <ranges>

namespace
{
    constexpr auto kSkinListEditorID = "BodyChangeSkinList";
    constexpr auto kFaceListEditorID = "BodyChangeFaceTextureSetList";
    constexpr auto kHeadListEditorID = "BodyChangeHeadPartList";

    class DiscardPapyrusResult final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        void operator()(RE::BSScript::Variable) override {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    [[nodiscard]] const RE::BGSListForm* FindList(const char* editorID)
    {
        const auto* form = RE::TESForm::LookupByEditorID(editorID);
        return form && form->GetFormType() == RE::FormType::FormList ? static_cast<const RE::BGSListForm*>(form) : nullptr;
    }

    [[nodiscard]] std::string NameFor(const RE::TESObjectARMO& armor)
    {
        if (const auto* name = armor.GetName(); name && name[0] != '\0') return name;
        if (const auto* editorID = armor.GetFormEditorID(); editorID && editorID[0] != '\0') return editorID;
        return std::format("Skin {:08X}", armor.GetFormID());
    }

    [[nodiscard]] std::uint32_t FormIDAt(const RE::BGSListForm* list, const std::size_t index, const RE::FormType expectedType)
    {
        if (!list || index >= list->forms.size()) return 0;
        const auto* form = list->forms[static_cast<std::uint32_t>(index)];
        return form && form->GetFormType() == expectedType ? form->GetFormID() : 0;
    }

    [[nodiscard]] RE::BGSTextureSet* FindTextureSet(const std::uint32_t formID)
    {
        auto* form = RE::TESForm::LookupByID(formID);
        return form && form->GetFormType() == RE::FormType::TextureSet ? static_cast<RE::BGSTextureSet*>(form) : nullptr;
    }

    [[nodiscard]] RE::BGSHeadPart* FindHeadPart(const std::uint32_t formID)
    {
        auto* form = RE::TESForm::LookupByID(formID);
        return form && form->GetFormType() == RE::FormType::HeadPart ? static_cast<RE::BGSHeadPart*>(form) : nullptr;
    }

    void QueuePlayerRefresh(RE::Actor* actor)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !actor) return;
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) return;
        const auto handle = policy->GetHandleForObject(static_cast<RE::VMTypeID>(actor->GetFormType()), actor);
        if (handle == policy->EmptyHandle()) return;

        // These are the same public Actor Papyrus calls used by BodyChange.
        // This uses the same owned, no-result callback pattern as SFS instead
        // of dropping an Awaitable before the VM has accepted the work.
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nodeCallback(new DiscardPapyrusResult());
        [[maybe_unused]] const auto nodeQueued = vm->DispatchMethodCall(
            handle, "Actor", "QueueNiNodeUpdate", RE::MakeFunctionArguments(), nodeCallback);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> headCallback(new DiscardPapyrusResult());
        [[maybe_unused]] const auto headQueued = vm->DispatchMethodCall(
            handle, "Actor", "RegenerateHead", RE::MakeFunctionArguments(), headCallback);
    }

    void ApplyToPlayerNow(RE::ActorHandle playerHandle, const bcn::SkinEntry entry)
    {
        const auto player = playerHandle.get();
        if (!player || player.get() != RE::PlayerCharacter::GetSingleton()) return;
        auto* base = player->GetActorBase();
        const auto* skin = RE::TESForm::LookupByID<RE::TESObjectARMO>(entry.skinFormID);
        if (!base || !skin) return;

        // BGSSkinForm has no setter in the game ABI. Keep this direct member
        // access isolated to the player-only BodyChange compatibility adapter;
        // touching an NPC base here would change every actor sharing that base.
        base->skin = const_cast<RE::TESObjectARMO*>(skin);
        if (auto* face = FindTextureSet(entry.faceTextureSetFormID)) {
            base->SetFaceTexture(face);
        }
        if (auto* headPart = FindHeadPart(entry.headPartFormID)) {
            base->ChangeHeadPart(headPart);
        }
        QueuePlayerRefresh(player.get());
        SKSE::log::info("Body Change NG applied BodyChange skin '{}' to the player", entry.name);
    }
}

namespace bcn
{
    SkinCatalog& SkinCatalog::Get()
    {
        static SkinCatalog catalog;
        return catalog;
    }

    void SkinCatalog::Refresh()
    {
        std::vector<SkinEntry> loaded;
        const auto* skins = FindList(kSkinListEditorID);
        const auto* faces = FindList(kFaceListEditorID);
        const auto* heads = FindList(kHeadListEditorID);
        if (skins) {
            loaded.reserve(skins->forms.size());
            for (std::uint32_t index{}; index < skins->forms.size(); ++index) {
                const auto* skin = skins->forms[index] ? skins->forms[index]->As<RE::TESObjectARMO>() : nullptr;
                if (!skin) continue;
                loaded.push_back({
                    .id = "bodychange:" + std::to_string(skin->GetFormID()),
                    .name = NameFor(*skin),
                    .skinFormID = skin->GetFormID(),
                    .faceTextureSetFormID = FormIDAt(faces, index, RE::FormType::TextureSet),
                    .headPartFormID = FormIDAt(heads, index, RE::FormType::HeadPart)
                });
            }
        }
        std::scoped_lock lock(lock_);
        entries_ = std::move(loaded);
        SKSE::log::info("Body Change NG found {} optional BodyChange player skin presets", entries_.size());
    }

    std::vector<SkinEntry> SkinCatalog::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return entries_;
    }

    SkinApplyResult SkinCatalog::QueueApplyToPlayer(std::string entryId) const
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return SkinApplyResult::unavailable;
        SkinEntry entry;
        {
            std::scoped_lock lock(lock_);
            const auto found = std::ranges::find(entries_, entryId, &SkinEntry::id);
            if (found == entries_.end()) return SkinApplyResult::missingEntry;
            entry = *found;
        }
        if (!RE::TESForm::LookupByID<RE::TESObjectARMO>(entry.skinFormID)) return SkinApplyResult::missingForm;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return SkinApplyResult::noTaskInterface;
        const auto playerHandle = player->GetHandle();
        const auto session = ActorRegistry::Get().SessionGeneration();
        tasks->AddTask([playerHandle, entry = std::move(entry), session] mutable {
            if (ActorRegistry::Get().SessionGeneration() == session) {
                ApplyToPlayerNow(playerHandle, std::move(entry));
            }
        });
        return SkinApplyResult::queued;
    }
}
