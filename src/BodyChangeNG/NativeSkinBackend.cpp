#include "BodyChangeNG/NativeSkinBackend.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/NativeSkinRouting.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinOverrideOwnership.h"
#include "BodyChangeNG/SkinProfiles.h"

#include <RE/B/BGSListForm.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/C/ConcreteFormFactory.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    using TextureRole = bcn::native_skin::TextureRole;
    using TextureLayers = std::vector<bcn::SkinTextureLayer>;
    constexpr std::size_t kTextureCount = RE::BSTextureSet::Textures::kUsedTotal;

    struct TextureBinding final
    {
        RE::BGSTextureSet* textureSet{};
        TextureRole role{ TextureRole::unmanaged };
        std::uint32_t slotMask{};
        std::array<std::string, kTextureCount> originalPaths;
    };

    struct ArmorGraph final
    {
        RE::TESObjectARMO* armor{};
        std::vector<TextureBinding> textures;
    };

    struct BaseInstance final
    {
        RE::TESNPC* base{};
        RE::FormID ownerActor{};
        RE::TESObjectARMO* originalSkin{};
        RE::TESObjectARMO* originalFarSkin{};
        RE::BGSTextureSet* originalFace{};
        ArmorGraph skin;
        ArmorGraph farSkin;
        TextureBinding face;
        std::string desiredProfileId;
        std::string appliedProfileId;
        std::uint64_t appliedContentHash{};
        std::uint64_t generation{};
        bool tracked{};
        bool appliedDefault{};
        bool skinAttached{};
        bool farSkinAttached{};
        bool faceAttached{};
    };

    std::mutex g_lock;
    std::unordered_map<RE::FormID, BaseInstance> g_instances;
    std::atomic_uint64_t g_nextGeneration{ 1U };

    template <class T>
    [[nodiscard]] T* DuplicateForm(T* source)
    {
        if (!source) return nullptr;
        auto* duplicate = source->CreateDuplicateForm(false, nullptr);
        return duplicate && duplicate->GetFormType() == T::FORMTYPE ?
            static_cast<T*>(duplicate) : nullptr;
    }

    [[nodiscard]] RE::BGSTextureSet* CurrentFaceTexture(RE::TESNPC* base)
    {
        if (!base) return nullptr;
        if (base->headRelatedData && base->headRelatedData->faceDetails) {
            return base->headRelatedData->faceDetails;
        }
        if (auto* root = base->GetRootFaceNPC(); root && root != base &&
            root->headRelatedData && root->headRelatedData->faceDetails) {
            return root->headRelatedData->faceDetails;
        }
        auto* race = base->GetRace();
        const auto sex = static_cast<std::size_t>(base->GetSex());
        if (!race || sex >= RE::SEXES::kTotal || !race->faceRelatedData[sex]) return nullptr;
        return race->faceRelatedData[sex]->defaultFaceDetailsTextureSet;
    }

    [[nodiscard]] std::string TexturePath(
        RE::BGSTextureSet* textureSet, const std::size_t index)
    {
        if (!textureSet || index >= kTextureCount) return {};
        const auto* path = textureSet->GetTexturePath(
            static_cast<RE::BSTextureSet::Texture>(index));
        return path ? std::string{ path } : std::string{};
    }

    [[nodiscard]] std::optional<TextureBinding> CloneTexture(
        RE::BGSTextureSet* source, const std::uint32_t slotMask,
        const bcn::SkinUvLayout layout)
    {
        auto* clone = DuplicateForm(source);
        if (!clone) return std::nullopt;
        TextureBinding result;
        result.textureSet = clone;
        result.slotMask = slotMask;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            result.originalPaths[index] = TexturePath(source, index);
        }
        result.role = bcn::native_skin::ResolveTextureRole(
            slotMask, result.originalPaths[RE::BSTextureSet::Textures::kDiffuse], layout);
        return result;
    }

    [[nodiscard]] std::optional<TextureBinding> CreateUbeTexture(
        const std::uint32_t slotMask, const bcn::SkinUvLayout layout)
    {
        auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
        auto* textureSet = factory ? factory->Create() : nullptr;
        if (!textureSet) return std::nullopt;
        TextureBinding result;
        result.textureSet = textureSet;
        result.slotMask = slotMask;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            result.originalPaths[index] = bcn::native_skin::UbeBaselineTexturePath(index);
            textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(index),
                result.originalPaths[index].c_str());
        }
        result.role = bcn::native_skin::ResolveTextureRole(
            slotMask, result.originalPaths[RE::BSTextureSet::Textures::kDiffuse], layout);
        return result;
    }

    [[nodiscard]] const RE::BSTArray<RE::TESObjectARMA*>* EffectiveArmorAddons(
        RE::TESObjectARMO* armor)
    {
        for (std::uint32_t depth{}; armor && depth < 16U; ++depth) {
            if (!armor->armorAddons.empty()) return &armor->armorAddons;
            if (armor->templateArmor == armor) break;
            armor = armor->templateArmor;
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<ArmorGraph> CloneArmorGraph(
        RE::TESObjectARMO* source, const RE::SEX sex, const bcn::SkinUvLayout layout,
        const bool synthesizeMissingUbeTexture)
    {
        if (!source) return ArmorGraph{};
        auto* armor = DuplicateForm(source);
        const auto* sourceAddons = EffectiveArmorAddons(source);
        if (!armor || !sourceAddons) return std::nullopt;

        ArmorGraph graph{ .armor = armor };
        armor->armorAddons.clear();
        armor->armorAddons.reserve(sourceAddons->size());
        // The clone owns a fully materialized addon list. Keeping TNAM would
        // let the engine walk back into the provider's source armor and would
        // defeat both TXST isolation and ownership-aware restoration.
        armor->templateArmor = nullptr;
        const auto sexIndex = static_cast<std::uint32_t>(sex);
        for (auto* sourceAddon : *sourceAddons) {
            auto* addon = DuplicateForm(sourceAddon);
            if (!addon || sexIndex >= RE::SEXES::kTotal) return std::nullopt;
            const auto slotMask = static_cast<std::uint32_t>(
                sourceAddon->GetSlotMask().underlying());
            auto* sourceTexture = sourceAddon->skinTextures[sexIndex];
            auto* sourceList = sourceAddon->skinTextureSwapLists[sexIndex];

            if (sourceTexture) {
                auto binding = CloneTexture(sourceTexture, slotMask, layout);
                if (!binding) return std::nullopt;
                addon->skinTextures[sexIndex] = binding->textureSet;
                graph.textures.push_back(std::move(*binding));
            }

            if (sourceList) {
                auto* list = DuplicateForm(sourceList);
                if (!list) return std::nullopt;
                for (RE::BSTArray<RE::TESForm*>::size_type index{};
                     index < list->forms.size(); ++index) {
                    auto* listedTexture = sourceList->forms[index] &&
                        sourceList->forms[index]->GetFormType() == RE::FormType::TextureSet ?
                        static_cast<RE::BGSTextureSet*>(sourceList->forms[index]) : nullptr;
                    if (!listedTexture) continue;
                    auto binding = CloneTexture(listedTexture, slotMask, layout);
                    if (!binding) return std::nullopt;
                    list->forms[index] = binding->textureSet;
                    graph.textures.push_back(std::move(*binding));
                }
                addon->skinTextureSwapLists[sexIndex] = list;
            }

            const auto roleWithoutTexture = bcn::native_skin::ResolveTextureRole(
                slotMask, {}, layout);
            const auto* modelPath = sourceAddon->bipedModels[sexIndex].GetModel();
            if (synthesizeMissingUbeTexture &&
                bcn::native_skin::ShouldSynthesizeUbeTextureSet(layout,
                    roleWithoutTexture, modelPath ? std::string_view{ modelPath } :
                        std::string_view{}, sourceTexture != nullptr, sourceList != nullptr)) {
                auto binding = CreateUbeTexture(slotMask, layout);
                if (!binding) return std::nullopt;
                addon->skinTextures[sexIndex] = binding->textureSet;
                graph.textures.push_back(std::move(*binding));
            }
            armor->armorAddons.push_back(addon);
        }
        return graph;
    }

    [[nodiscard]] bool TextureSetUsesRsv(RE::BGSTextureSet* textureSet)
    {
        if (!textureSet) return false;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            if (bcn::skin_override::ownership::IsRacialSkinVarianceTexturePath(
                    TexturePath(textureSet, index))) return true;
        }
        return false;
    }

    [[nodiscard]] bool ArmorUsesRsv(
        RE::TESObjectARMO* armor, const RE::SEX sex)
    {
        const auto* addons = EffectiveArmorAddons(armor);
        const auto sexIndex = static_cast<std::uint32_t>(sex);
        if (!addons || sexIndex >= RE::SEXES::kTotal) return false;
        for (auto* addon : *addons) {
            if (!addon) continue;
            if (TextureSetUsesRsv(addon->skinTextures[sexIndex])) return true;
            const auto* list = addon->skinTextureSwapLists[sexIndex];
            if (!list) continue;
            for (auto* form : list->forms) {
                auto* textureSet = form && form->GetFormType() == RE::FormType::TextureSet ?
                    static_cast<RE::BGSTextureSet*>(form) : nullptr;
                if (TextureSetUsesRsv(textureSet)) return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsVampireRace(RE::TESNPC* base)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        if (!editorID) return false;
        std::string value{ editorID };
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value.contains("vampire");
    }

    [[nodiscard]] bcn::skin_plan::ApplicationPlan BuildPlan(
        const bcn::SkinProfile& profile, RE::TESNPC* base,
        RE::BGSTextureSet* faceTexture, const bcn::body_family::Mask actorFamily)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        return bcn::skin_plan::Build(profile, {
            .elder = bcn::IsElderActor(base),
            .vampire = IsVampireRace(base),
            .humanoidRace = bcn::HumanoidSkinRaceFromEditorID(
                editorID ? std::string_view{ editorID } : std::string_view{}),
            .faceDetailFilename = faceTexture ? TexturePath(
                faceTexture, RE::BSTextureSet::Textures::kDetailMap) : std::string{},
            .bodyFamily = actorFamily
        });
    }

    [[nodiscard]] const TextureLayers& LayersFor(
        const bcn::skin_plan::ApplicationPlan& plan, const TextureRole role)
    {
        static const TextureLayers empty;
        switch (role) {
        case TextureRole::body: return plan.body;
        case TextureRole::hands: return plan.hands;
        case TextureRole::feet: return plan.feet;
        case TextureRole::cbbeGenitalAnal:
            return bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::cbbe) ?
                plan.cbbeGenitalAnal : empty;
        case TextureRole::unpGenitalAnal:
            return bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::unp) ?
                plan.unpGenitalAnal : empty;
        default: return empty;
        }
    }

    [[nodiscard]] bool ApplyLayers(TextureBinding& binding,
        const TextureLayers& layers, const std::string_view nameSpace)
    {
        if (!binding.textureSet) return false;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            binding.textureSet->SetTexturePath(
                static_cast<RE::BSTextureSet::Texture>(index),
                binding.originalPaths[index].c_str());
        }
        for (const auto& layer : layers) {
            if (layer.shaderTextureIndex >= kTextureCount) {
                SKSE::log::warn("Body Change NG ignored TXST-incompatible shader texture index {}",
                    layer.shaderTextureIndex);
                continue;
            }
            const auto cached = bcn::runtime_assets::ExpectedTexturePathFromGameRelative(
                layer.path, nameSpace);
            if (cached.empty() || !bcn::runtime_assets::CachedTextureExists(cached)) return false;
            binding.textureSet->SetTexturePath(
                static_cast<RE::BSTextureSet::Texture>(layer.shaderTextureIndex), cached.c_str());
        }
        return true;
    }

    [[nodiscard]] bool ApplyGraph(
        ArmorGraph& graph, const bcn::skin_plan::ApplicationPlan& plan)
    {
        for (auto& binding : graph.textures) {
            binding.role = bcn::native_skin::ResolveTextureRole(binding.slotMask,
                binding.originalPaths[RE::BSTextureSet::Textures::kDiffuse], plan.runtimeUvLayout);
            if (!ApplyLayers(binding, LayersFor(plan, binding.role), "skin")) return false;
        }
        return true;
    }

    [[nodiscard]] bcn::native_skin::TextureRoleMask AvailableRoles(
        const ArmorGraph& graph, const bcn::SkinUvLayout layout)
    {
        bcn::native_skin::TextureRoleMask result{};
        for (const auto& binding : graph.textures) {
            result |= bcn::native_skin::RoleBit(
                bcn::native_skin::ResolveTextureRole(binding.slotMask,
                    binding.originalPaths[RE::BSTextureSet::Textures::kDiffuse], layout));
        }
        return result;
    }

    [[nodiscard]] bool GraphSupportsPlan(
        const BaseInstance& instance, const bcn::skin_plan::ApplicationPlan& plan)
    {
        const auto required = bcn::native_skin::RequiredRoleMask(
            plan.broadSharedAtlas, !plan.body.empty(), !plan.hands.empty(),
            !plan.feet.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::cbbe &&
                !plan.cbbeGenitalAnal.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::unp &&
                !plan.unpGenitalAnal.empty());
        const auto available = AvailableRoles(instance.skin, plan.runtimeUvLayout);
        return bcn::native_skin::CoversRequiredRoles(available, required) &&
            (plan.face.empty() || instance.face.textureSet != nullptr);
    }

    [[nodiscard]] bool OwnsCurrentPointers(const BaseInstance& instance)
    {
        if (!instance.base) return false;
        if (instance.skinAttached && instance.skin.armor &&
            instance.base->skin != instance.skin.armor) return false;
        if (instance.farSkinAttached && instance.farSkin.armor &&
            instance.base->farSkin != instance.farSkin.armor) return false;
        if (instance.faceAttached && instance.face.textureSet) {
            if (!instance.base->headRelatedData ||
                instance.base->headRelatedData->faceDetails != instance.face.textureSet) return false;
        }
        return true;
    }

    [[nodiscard]] bool CanRebaseFromRsv(const BaseInstance& instance)
    {
        if (!instance.base) return false;
        const auto sex = instance.base->GetSex();
        if (instance.skinAttached && instance.skin.armor &&
            instance.base->skin != instance.skin.armor &&
            ArmorUsesRsv(instance.base->skin, sex)) return true;
        if (instance.farSkinAttached && instance.farSkin.armor &&
            instance.base->farSkin != instance.farSkin.armor &&
            ArmorUsesRsv(instance.base->farSkin, sex)) return true;
        if (instance.faceAttached && instance.face.textureSet) {
            auto* currentFace = CurrentFaceTexture(instance.base);
            if (currentFace != instance.face.textureSet && TextureSetUsesRsv(currentFace)) return true;
        }
        return false;
    }

    [[nodiscard]] bool SourceStillMatches(const BaseInstance& instance)
    {
        return instance.base && instance.base->skin == instance.originalSkin &&
            instance.base->farSkin == instance.originalFarSkin &&
            CurrentFaceTexture(instance.base) == instance.originalFace;
    }

    [[nodiscard]] bool UnownedSourceChanged(const BaseInstance& instance)
    {
        if (!instance.base) return false;
        if (!instance.skinAttached && instance.base->skin != instance.originalSkin) {
            return true;
        }
        if (!instance.farSkinAttached &&
            instance.base->farSkin != instance.originalFarSkin) {
            return true;
        }
        return !instance.faceAttached &&
            CurrentFaceTexture(instance.base) != instance.originalFace;
    }

    void RefreshLoadedActors(RE::TESNPC* base)
    {
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!base || !processes) return;
        processes->ForAllActors([base](RE::Actor* actor) {
            if (actor && actor->GetActorBase() == base && actor->Is3DLoaded()) {
                actor->DoReset3D(false);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    void RestoreOwnedPointers(BaseInstance& instance)
    {
        if (!instance.base) return;
        if (instance.skinAttached && instance.skin.armor &&
            instance.base->skin == instance.skin.armor) {
            instance.base->skin = instance.originalSkin;
        }
        if (instance.farSkinAttached && instance.farSkin.armor &&
            instance.base->farSkin == instance.farSkin.armor) {
            instance.base->farSkin = instance.originalFarSkin;
        }
        if (instance.faceAttached && instance.face.textureSet &&
            instance.base->headRelatedData &&
            instance.base->headRelatedData->faceDetails == instance.face.textureSet) {
            instance.base->SetFaceTexture(instance.originalFace);
        }
        instance.skinAttached = false;
        instance.farSkinAttached = false;
        instance.faceAttached = false;
    }

    [[nodiscard]] std::optional<BaseInstance> BuildInstance(
        RE::Actor* actor, const bcn::SkinProfile& profile,
        const bcn::SkinUvLayout runtimeUvLayout)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* sourceSkin = actor ? actor->GetSkin() : nullptr;
        if (!base || !sourceSkin) return std::nullopt;

        BaseInstance instance;
        instance.base = base;
        instance.ownerActor = actor->GetFormID();
        instance.originalSkin = base->skin;
        instance.originalFarSkin = base->farSkin;
        instance.originalFace = CurrentFaceTexture(base);
        // Official UBE naked ARMAs omit NAM1/NAM3 and keep their baseline
        // paths in the NIF. Materialize that missing TXST only when this
        // profile actually declares body-atlas layers; a face-only profile
        // must not alter an otherwise untouched UBE body material graph.
        const auto synthesizeMissingUbeTexture = !profile.body.empty();
        auto skin = CloneArmorGraph(sourceSkin, base->GetSex(), runtimeUvLayout,
            synthesizeMissingUbeTexture);
        if (!skin || !skin->armor) return std::nullopt;
        instance.skin = std::move(*skin);
        if (base->farSkin) {
            auto farSkin = CloneArmorGraph(base->farSkin, base->GetSex(), runtimeUvLayout,
                synthesizeMissingUbeTexture);
            if (!farSkin) return std::nullopt;
            instance.farSkin = std::move(*farSkin);
        }
        if (instance.originalFace) {
            auto face = CloneTexture(instance.originalFace,
                static_cast<std::uint32_t>(RE::BGSBipedObjectForm::BipedObjectSlot::kHead),
                runtimeUvLayout);
            if (!face) return std::nullopt;
            face->role = TextureRole::unmanaged;
            instance.face = std::move(*face);
        }
        return instance;
    }

    [[nodiscard]] bool RequestStillCurrent(
        const RE::FormID baseId, const std::uint64_t generation,
        const std::string_view desired)
    {
        std::scoped_lock lock(g_lock);
        const auto found = g_instances.find(baseId);
        return found != g_instances.end() && found->second.generation == generation &&
            found->second.desiredProfileId == desired;
    }

    [[nodiscard]] bool NativeLayersAvailable(
        const bcn::skin_plan::ApplicationPlan& plan);

    void ApplyNow(RE::ActorHandle handle, bcn::SkinProfile profile,
        bcn::skin_plan::ApplicationPlan plan, const RE::FormID baseId,
        const std::uint64_t generation)
    {
        const auto actor = handle.get();
        if (!actor || !RequestStillCurrent(baseId, generation, profile.id) ||
            profile.contentHash != bcn::SkinProfiles::Get().ContentHash(profile.id)) return;
        auto* base = actor->GetActorBase();
        if (!base || base->GetFormID() != baseId) return;

        std::unique_lock lock(g_lock);
        auto found = g_instances.find(baseId);
        if (found == g_instances.end() || found->second.generation != generation ||
            found->second.desiredProfileId != profile.id) return;
        auto& instance = found->second;
        const auto graphAction = bcn::native_skin::ResolveGraphAction(
            instance.skin.armor != nullptr, !instance.appliedProfileId.empty(),
            OwnsCurrentPointers(instance), SourceStillMatches(instance),
            CanRebaseFromRsv(instance), UnownedSourceChanged(instance));
        if (graphAction == bcn::native_skin::GraphAction::rejectForeignOwner) {
            SKSE::log::warn("Body Change NG stopped native skin apply for ActorBase {:08X}: another provider replaced an owned form pointer",
                baseId);
            return;
        }
        if (graphAction == bcn::native_skin::GraphAction::rebuildFromCurrentSource &&
            instance.skin.armor) {
            const auto desired = instance.desiredProfileId;
            const auto currentGeneration = instance.generation;
            const auto owner = instance.ownerActor;
            // RSV may replace only one member of the graph. Detach every
            // remaining pointer that BCNG still owns before taking the new
            // provider snapshot, otherwise an old BCNG face/far-skin clone
            // would become its own restoration source and survive Default.
            RestoreOwnedPointers(instance);
            instance = {};
            instance.base = base;
            instance.ownerActor = owner;
            instance.desiredProfileId = desired;
            instance.generation = currentGeneration;
            instance.tracked = true;
        }
        if (!instance.skin.armor) {
            auto built = BuildInstance(actor.get(), profile, plan.runtimeUvLayout);
            if (!built) {
                SKSE::log::error("Body Change NG could not clone the native Skin Armor graph for actor {:08X}",
                    actor->GetFormID());
                return;
            }
            built->desiredProfileId = instance.desiredProfileId;
            built->generation = instance.generation;
            built->tracked = true;
            instance = std::move(*built);
        }

        if (instance.appliedProfileId == profile.id &&
            instance.appliedContentHash == profile.contentHash && OwnsCurrentPointers(instance)) {
            lock.unlock();
            bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), profile.id, false);
            return;
        }

        if (!NativeLayersAvailable(plan)) {
            SKSE::log::error("Body Change NG aborted native TXST apply for '{}' before mutation because a declared layer was unavailable or outside TXST indices 0-7",
                profile.name);
            return;
        }

        if (!GraphSupportsPlan(instance, plan)) {
            SKSE::log::error(
                "Body Change NG aborted native TXST apply for '{}': the current Skin Armor graph has no exact TXST target for one or more declared parts",
                profile.name);
            return;
        }

        const auto bodyGraphRequired = bcn::native_skin::RequiredRoleMask(
            plan.broadSharedAtlas, !plan.body.empty(), !plan.hands.empty(),
            !plan.feet.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::cbbe &&
                !plan.cbbeGenitalAnal.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::unp &&
                !plan.unpGenitalAnal.empty()) != 0U;
        if ((bodyGraphRequired && !ApplyGraph(instance.skin, plan)) ||
            (bodyGraphRequired && instance.farSkin.armor &&
                !ApplyGraph(instance.farSkin, plan)) ||
            (!plan.face.empty() && (!instance.face.textureSet ||
                !ApplyLayers(instance.face, plan.face, "skin-face")))) {
            SKSE::log::error("Body Change NG aborted native TXST apply for '{}' because its full declared layer set was unavailable",
                profile.name);
            return;
        }

        if (bodyGraphRequired) {
            instance.base->skin = instance.skin.armor;
            instance.skinAttached = true;
            if (instance.farSkin.armor) {
                instance.base->farSkin = instance.farSkin.armor;
                instance.farSkinAttached = true;
            }
        } else {
            if (instance.skinAttached && instance.base->skin == instance.skin.armor) {
                instance.base->skin = instance.originalSkin;
            }
            if (instance.farSkinAttached &&
                instance.base->farSkin == instance.farSkin.armor) {
                instance.base->farSkin = instance.originalFarSkin;
            }
            instance.skinAttached = false;
            instance.farSkinAttached = false;
        }
        if (instance.face.textureSet && !plan.face.empty()) {
            instance.base->SetFaceTexture(instance.face.textureSet);
            instance.faceAttached = true;
        } else if (instance.faceAttached && instance.base->headRelatedData &&
            instance.base->headRelatedData->faceDetails == instance.face.textureSet) {
            instance.base->SetFaceTexture(instance.originalFace);
            instance.faceAttached = false;
        }
        instance.appliedProfileId = profile.id;
        instance.appliedContentHash = profile.contentHash;
        instance.appliedDefault = false;
        lock.unlock();
        bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), profile.id, false);
        RefreshLoadedActors(base);
        SKSE::log::info("Body Change NG applied native TXST skin '{}' to ActorBase {:08X} from actor {:08X}",
            profile.name, baseId, actor->GetFormID());
    }

    void ClearNow(RE::ActorHandle handle, const RE::FormID baseId,
        const std::uint64_t generation)
    {
        const auto actor = handle.get();
        if (!actor || !RequestStillCurrent(baseId, generation, {})) return;
        auto* base = actor->GetActorBase();
        if (!base || base->GetFormID() != baseId) return;
        std::unique_lock lock(g_lock);
        const auto found = g_instances.find(baseId);
        if (found == g_instances.end() || found->second.generation != generation ||
            !found->second.desiredProfileId.empty()) return;
        auto& instance = found->second;
        RestoreOwnedPointers(instance);
        instance.appliedProfileId.clear();
        instance.appliedContentHash = 0U;
        instance.appliedDefault = true;
        instance.tracked = true;
        lock.unlock();
        bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), {}, true);
        RefreshLoadedActors(base);
        SKSE::log::info("Body Change NG restored native default Skin Armor for ActorBase {:08X}", baseId);
    }

    [[nodiscard]] std::vector<bcn::runtime_assets::TexturePreparation> Preparations(
        const bcn::skin_plan::ApplicationPlan& plan)
    {
        std::vector<bcn::runtime_assets::TexturePreparation> paths;
        const auto add = [&paths](const TextureLayers& layers, const std::string_view nameSpace) {
            for (const auto& layer : layers) paths.push_back({ layer.path, std::string{ nameSpace } });
        };
        add(plan.body, "skin");
        add(plan.hands, "skin");
        add(plan.feet, "skin");
        // A Legacy pack may contain both optional atlas layouts. Preserve
        // both in the catalog, but prepare and consume only the one matching
        // this actor's concrete runtime family.
        if (bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::cbbe)) {
            add(plan.cbbeGenitalAnal, "skin");
        } else if (bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::unp)) {
            add(plan.unpGenitalAnal, "skin");
        }
        add(plan.face, "skin-face");
        return paths;
    }

    [[nodiscard]] bool NativeLayersAvailable(
        const bcn::skin_plan::ApplicationPlan& plan)
    {
        const auto available = [](const TextureLayers& layers, const std::string_view nameSpace) {
            return std::ranges::all_of(layers, [nameSpace](const bcn::SkinTextureLayer& layer) {
                if (layer.shaderTextureIndex >= kTextureCount) return false;
                const auto cached = bcn::runtime_assets::ExpectedTexturePathFromGameRelative(
                    layer.path, nameSpace);
                return !cached.empty() && bcn::runtime_assets::CachedTextureExists(cached);
            });
        };
        const auto genitalAvailable =
            (!bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::cbbe) ||
                available(plan.cbbeGenitalAnal, "skin")) &&
            (!bcn::skin_plan::RoutesGenitalAnalAtlas(plan, bcn::SkinUvLayout::unp) ||
                available(plan.unpGenitalAnal, "skin"));
        return available(plan.body, "skin") && available(plan.hands, "skin") &&
            available(plan.feet, "skin") && genitalAvailable &&
            available(plan.face, "skin-face");
    }
}

namespace bcn::native_skin
{
    SkinApplyResult QueueApply(RE::Actor* actor, std::string profileId,
        std::function<void()> beforeQueue)
    {
        if (!frame_tasks::Active()) return SkinApplyResult::noTaskInterface;
        if (!actor) return SkinApplyResult::invalidActor;
        if (runtime::ResolveGameBranch(REL::Module::get().version()) ==
            runtime::GameBranch::unsupported) return SkinApplyResult::unsupportedRuntime;
        auto* base = actor->GetActorBase();
        if (!base || !actor->GetSkin()) return SkinApplyResult::actorBaseUnavailable;
        const auto profile = SkinProfiles::Get().Find(profileId);
        if (!profile) return SkinApplyResult::missingProfile;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        if ((female && profile->sex != SkinSex::female) ||
            (!female && profile->sex != SkinSex::male)) return SkinApplyResult::incompatibleSex;
        if (!SkinRaceMatchesActor(profile->race, ResolveActorSkinRace(actor))) {
            return SkinApplyResult::incompatibleRace;
        }
        const auto actorFamily = body_family::ResolveActor(actor);
        const auto compatibility = SkinProfileCompatibility(*profile,
            female ? SkinSex::female : SkinSex::male, ResolveActorSkinRace(actor),
            actorFamily);
        if (!compatibility.Compatible()) {
            if (compatibility.status == SkinCompatibilityStatus::unknownProfileLayout) {
                return SkinApplyResult::ambiguousProfileLayout;
            }
            if (compatibility.status == SkinCompatibilityStatus::unknownActorLayout) {
                return SkinApplyResult::ambiguousActorLayout;
            }
            return SkinApplyResult::incompatibleBodyFamily;
        }
        if (!SKSE::GetTaskInterface()) return SkinApplyResult::noTaskInterface;

        const auto baseId = base->GetFormID();
        const auto actorId = actor->GetFormID();
        std::uint64_t generation{};
        {
            std::scoped_lock lock(g_lock);
            auto& instance = g_instances[baseId];
            if (instance.skin.armor && !instance.appliedProfileId.empty() &&
                !OwnsCurrentPointers(instance) && !CanRebaseFromRsv(instance)) {
                return SkinApplyResult::ownershipConflict;
            }
            const auto sharedAction = bcn::native_skin::ResolveSharedBaseAction(
                instance.ownerActor, actorId, instance.desiredProfileId, profile->id);
            if (sharedAction == bcn::native_skin::SharedBaseAction::rejectConflict) {
                return SkinApplyResult::sharedActorBaseConflict;
            }
            if (instance.ownerActor == 0U ||
                sharedAction == bcn::native_skin::SharedBaseAction::transferOwner) {
                instance.ownerActor = actorId;
            }
            if (instance.desiredProfileId != profile->id || instance.generation == 0U) {
                instance.desiredProfileId = profile->id;
                instance.generation = g_nextGeneration.fetch_add(1U, std::memory_order_relaxed);
            }
            instance.tracked = true;
            generation = instance.generation;
        }

        if (beforeQueue) beforeQueue();
        const auto plan = BuildPlan(*profile, base, CurrentFaceTexture(base), actorFamily);
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actorId, [handle, profile = *profile, plan, baseId, generation]() mutable {
            if (!RequestStillCurrent(baseId, generation, profile.id)) return;
            const auto lease = frame_tasks::CurrentLease();
            auto continueApply = [lease, handle, profile, plan, baseId, generation](const bool prepared) mutable {
                if (!prepared) {
                    SKSE::log::error("Body Change NG could not prepare every native TXST asset for '{}'",
                        profile.name);
                    return;
                }
                static_cast<void>(frame_tasks::Continue(lease,
                    [handle, profile = std::move(profile), plan = std::move(plan), baseId, generation]() mutable {
                        ApplyNow(handle, std::move(profile), std::move(plan), baseId, generation);
                    }));
            };
            if (!runtime_assets::PrepareTexturePathsAsync(
                    (static_cast<std::uint64_t>(baseId) << 1U) | 1U,
                    Preparations(plan), continueApply,
                    async_work::FrameTaskQueue::InteractiveLease(lease))) {
                continueApply(false);
            }
        }, 1U, appearance::WorkChannel::skinApply);
        return SkinApplyResult::queued;
    }

    SkinApplyResult QueueClear(RE::Actor* actor, std::function<void()> beforeQueue)
    {
        if (!frame_tasks::Active()) return SkinApplyResult::noTaskInterface;
        if (!actor) return SkinApplyResult::invalidActor;
        if (runtime::ResolveGameBranch(REL::Module::get().version()) ==
            runtime::GameBranch::unsupported) return SkinApplyResult::unsupportedRuntime;
        auto* base = actor->GetActorBase();
        if (!base) return SkinApplyResult::actorBaseUnavailable;
        if (!SKSE::GetTaskInterface()) return SkinApplyResult::noTaskInterface;

        const auto baseId = base->GetFormID();
        const auto actorId = actor->GetFormID();
        std::uint64_t generation{};
        {
            std::scoped_lock lock(g_lock);
            auto& instance = g_instances[baseId];
            const auto sharedAction = bcn::native_skin::ResolveSharedBaseAction(
                instance.ownerActor, actorId, instance.desiredProfileId, {});
            if (sharedAction == bcn::native_skin::SharedBaseAction::rejectConflict) {
                return SkinApplyResult::sharedActorBaseConflict;
            }
            if (instance.ownerActor == 0U ||
                sharedAction == bcn::native_skin::SharedBaseAction::transferOwner) {
                instance.ownerActor = actorId;
            }
            if (!instance.base) {
                instance.base = base;
                instance.originalSkin = base->skin;
                instance.originalFarSkin = base->farSkin;
                instance.originalFace = CurrentFaceTexture(base);
            }
            instance.desiredProfileId.clear();
            instance.tracked = true;
            instance.generation = g_nextGeneration.fetch_add(1U, std::memory_order_relaxed);
            generation = instance.generation;
        }
        if (beforeQueue) beforeQueue();
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actorId,
            [handle, baseId, generation] { ClearNow(handle, baseId, generation); },
            1U, appearance::WorkChannel::skinApply);
        return SkinApplyResult::queued;
    }

    std::optional<std::string> CurrentProfileId(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return std::nullopt;
        std::scoped_lock lock(g_lock);
        const auto found = g_instances.find(base->GetFormID());
        if (found == g_instances.end() || found->second.desiredProfileId.empty()) return std::nullopt;
        return found->second.desiredProfileId;
    }

    bool HasTrackedSelection(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return false;
        std::scoped_lock lock(g_lock);
        const auto found = g_instances.find(base->GetFormID());
        return found != g_instances.end() && found->second.tracked;
    }

    std::optional<bool> LiveStateMatches(const RE::Actor* actor,
        const std::string_view profileId, const bool expectDefault)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return std::nullopt;
        std::scoped_lock lock(g_lock);
        const auto found = g_instances.find(base->GetFormID());
        if (found == g_instances.end() || !found->second.tracked) return false;
        const auto& instance = found->second;
        if (expectDefault) {
            return instance.appliedDefault && instance.appliedProfileId.empty();
        }
        return instance.appliedProfileId == profileId &&
            instance.appliedContentHash == SkinProfiles::Get().ContentHash(profileId) &&
            OwnsCurrentPointers(instance);
    }

    void ResetSessionState()
    {
        std::vector<RE::TESNPC*> changed;
        {
            std::scoped_lock lock(g_lock);
            changed.reserve(g_instances.size());
            for (auto& [baseId, instance] : g_instances) {
                if (instance.base) changed.push_back(instance.base);
                RestoreOwnedPointers(instance);
                instance.ownerActor = 0U;
                instance.desiredProfileId.clear();
                instance.appliedProfileId.clear();
                instance.appliedContentHash = 0U;
                instance.generation = g_nextGeneration.fetch_add(1U, std::memory_order_relaxed);
                instance.tracked = false;
                instance.appliedDefault = false;
            }
        }
        for (auto* base : changed) RefreshLoadedActors(base);
    }
}
