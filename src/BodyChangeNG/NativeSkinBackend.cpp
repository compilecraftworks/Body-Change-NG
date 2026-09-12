#include "BodyChangeNG/NativeSkinBackend.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/NativeSkinRouting.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/FaceSkinOverrides.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/NativeAddonCopy.h"
#include "BodyChangeNG/NativeTextureData.h"
#include "BodyChangeNG/NativeTexturePath.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/RuntimeCompatibility.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinTextureOwnership.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinRefreshTargets.h"

#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSListForm.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSModelDB.h>
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/B/BSTextureSet.h>
#include <RE/B/BSVisit.h>
#include <RE/C/ConcreteFormFactory.h>
#include <RE/M/MemoryManager.h>
#include <RE/N/NiNode.h>
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
#include <new>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    using TextureRole = bcn::native_skin::TextureRole;
    using TextureLayers = std::vector<bcn::SkinTextureLayer>;
    constexpr std::size_t kTextureCount = RE::BSTextureSet::Textures::kUsedTotal;
    constexpr auto kGenitalSlot = static_cast<std::uint32_t>(
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);

    [[nodiscard]] bool IsTngSkinArmor(const RE::TESObjectARMO* armor)
    {
        return armor && armor->HasKeywordString("TNG_CustomSkin");
    }

    void RemoveTngSkinMarker(RE::TESObjectARMO* armor)
    {
        if (!armor) return;
        RE::BGSKeyword* marker{};
        armor->ForEachKeyword([&marker](RE::BGSKeyword* keyword) {
            const auto* editorId = keyword ? keyword->GetFormEditorID() : nullptr;
            if (editorId && std::string_view{ editorId } == "TNG_CustomSkin") {
                marker = keyword;
                return RE::BSContainer::ForEachResult::kStop;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if (marker) armor->RemoveKeyword(marker);
    }

    struct TextureBinding final
    {
        RE::BGSTextureSet* textureSet{};
        TextureRole role{ TextureRole::unmanaged };
        std::optional<TextureRole> fixedRole;
        std::uint32_t slotMask{};
        std::array<std::string, kTextureCount> originalPaths;
        bcn::native_skin::FormTextureSlots formSlots{};
    };

    struct ArmorGraph final
    {
        RE::TESObjectARMO* armor{};
        std::vector<TextureBinding> textures;
    };

    struct ModelTextureTarget final
    {
        TextureRole role{ TextureRole::unmanaged };
        std::uint32_t index3D{};
        std::string name3D;
        std::array<std::string, kTextureCount> paths;
    };

    struct PendingModelTexture final
    {
        ModelTextureTarget target;
        TextureBinding binding;
    };

    struct BaseInstance final
    {
        RE::TESNPC* base{};
        RE::FormID ownerActor{};
        RE::FormID sourceRace{};
        RE::SEX sourceSex{ RE::SEX::kMale };
        bcn::body_family::Mask sourceFamily{};
        RE::TESObjectARMO* originalSkin{};
        RE::TESObjectARMO* originalFarSkin{};
        ArmorGraph skin;
        ArmorGraph farSkin;
        std::string desiredProfileId;
        std::string appliedProfileId;
        std::uint64_t appliedContentHash{};
        std::uint64_t generation{};
        bool tracked{};
        bool appliedDefault{};
        bool skinAttached{};
        bool farSkinAttached{};
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

    [[nodiscard]] bool PrepareFormSlots(TextureBinding& binding)
    {
        const auto slots = bcn::native_skin::DiscoverFormTextureSlots(*binding.textureSet,
            [&](const std::size_t shader) { return TexturePath(binding.textureSet, shader); });
        if (!slots) {
            SKSE::log::error("BCNG cannot resolve private TXST shader-to-record slots form={:08X}",
                binding.textureSet->GetFormID());
            return false;
        }
        binding.formSlots = *slots;
        return true;
    }

    [[nodiscard]] bool WriteTexturePath(TextureBinding& binding,
        const std::size_t index, const std::string& path)
    {
        auto* form = binding.textureSet;
        if (!form || index >= binding.formSlots.size() ||
            !bcn::native_skin::WriteFormTexturePath(*form, binding.formSlots[index], path.c_str())) return false;
        const auto actual = TexturePath(form, index);
        if (actual == path) return true;
        SKSE::log::error("BCNG TXST readback mismatch form={:08X} index={} expected='{}' actual='{}'",
            form->GetFormID(), index, path, actual);
        return false;
    }

    [[nodiscard]] std::optional<TextureBinding> CloneTexture(
        RE::BGSTextureSet* source, const std::uint32_t slotMask,
        const bcn::SkinUvLayout layout)
    {
        auto* clone = DuplicateForm(source);
        if (!clone) return std::nullopt;
        // BGSTextureSet also inherits the no-op TESForm::Copy. In particular
        // the model-space-normal flag must survive along with the DDS paths.
        clone->boundData = source->boundData;
        clone->flags = source->flags;
        if (source->decalData) {
            clone->decalData = RE::malloc<RE::DecalData>();
            if (!clone->decalData) return std::nullopt;
            *clone->decalData = *source->decalData;
        }
        TextureBinding result;
        result.textureSet = clone;
        result.slotMask = slotMask;
        if (!PrepareFormSlots(result)) return std::nullopt;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            result.originalPaths[index] = TexturePath(source, index);
            if (!WriteTexturePath(result, index, result.originalPaths[index])) return std::nullopt;
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
        if (!PrepareFormSlots(result)) return std::nullopt;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            result.originalPaths[index] = bcn::native_skin::UbeBaselineTexturePath(index);
            if (!WriteTexturePath(result, index, result.originalPaths[index])) return std::nullopt;
        }
        result.role = bcn::native_skin::ResolveTextureRole(
            slotMask, result.originalPaths[RE::BSTextureSet::Textures::kDiffuse], layout);
        return result;
    }

    [[nodiscard]] std::string NativeFormPath(std::string path)
    {
        std::ranges::replace(path, '/', '\\');
        constexpr std::string_view prefix = "textures\\";
        if (path.size() >= prefix.size() &&
            bcn::skin_geometry::ContainsIgnoreAsciiCase(
                std::string_view(path).substr(0, prefix.size()), prefix)) {
            path.erase(0, prefix.size());
        }
        return path;
    }

    [[nodiscard]] std::optional<std::vector<ModelTextureTarget>> DiscoverModelTextureTargets(
        const RE::TESModelTextureSwap& model, const bcn::SkinUvLayout layout)
    {
        if (layout != bcn::SkinUvLayout::cbbe &&
            layout != bcn::SkinUvLayout::unp) return std::vector<ModelTextureTarget>{};
        const auto* modelPath = model.GetModel();
        if (!modelPath || modelPath[0] == '\0') return std::vector<ModelTextureTarget>{};

        RE::NiPointer<RE::NiNode> root;
        RE::BSModelDB::DBTraits::ArgsType args;
        const auto loaded = RE::BSModelDB::Demand(modelPath, root, args);
        if (loaded != RE::BSResource::ErrorCode::kNone || !root) {
            SKSE::log::warn("BCNG could not inspect native model '{}' for embedded skin atlases error={}",
                modelPath, static_cast<unsigned>(loaded));
            // A model that cannot be inspected may still be a conventional
            // body/hand/foot ARMA whose ordinary skin TXST is sufficient. Do
            // not reject every skin here. The graph-coverage guard later in
            // the apply path still rejects a profile that actually declares
            // a genital/anal atlas when no exact native target was found.
            return std::vector<ModelTextureTarget>{};
        }

        std::vector<ModelTextureTarget> targets;
        std::uint32_t index3D{};
        bool invalid{};
        RE::BSVisit::TraverseScenegraphGeometries(root.get(), [&](RE::BSGeometry* geometry) {
            const auto currentIndex = index3D++;
            if (!geometry) return RE::BSVisit::BSVisitControl::kContinue;
            auto* shader = geometry->lightingShaderProp_cast();
            auto* material = shader ?
                static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
            auto* textureSet = material ? material->textureSet.get() : nullptr;
            const auto* name = geometry->name.c_str();
            const auto* diffuse = textureSet ?
                textureSet->GetTexturePath(RE::BSTextureSet::Textures::kDiffuse) : nullptr;
            const std::string_view nodeName = name ? std::string_view{ name } : std::string_view{};
            const std::string_view diffusePath = diffuse ?
                std::string_view{ diffuse } : std::string_view{};
            const auto role = layout == bcn::SkinUvLayout::cbbe &&
                    bcn::skin_geometry::IsCBBEGenitalAnal(nodeName, diffusePath) ?
                TextureRole::cbbeGenitalAnal :
                layout == bcn::SkinUvLayout::unp &&
                    bcn::skin_geometry::IsUNPGenitalAnal(nodeName, diffusePath) ?
                TextureRole::unpGenitalAnal : TextureRole::unmanaged;
            if (role == TextureRole::unmanaged) {
                return RE::BSVisit::BSVisitControl::kContinue;
            }
            if (nodeName.empty() || !textureSet) {
                invalid = true;
                return RE::BSVisit::BSVisitControl::kStop;
            }
            ModelTextureTarget target{
                .role = role,
                .index3D = currentIndex,
                .name3D = std::string{ nodeName }
            };
            for (std::size_t index{}; index < target.paths.size(); ++index) {
                const auto* path = textureSet->GetTexturePath(
                    static_cast<RE::BSTextureSet::Texture>(index));
                target.paths[index] = NativeFormPath(path ? path : "");
            }
            targets.push_back(std::move(target));
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        if (invalid) return std::nullopt;
        return targets;
    }

    [[nodiscard]] std::optional<TextureBinding> CreateModelTexture(
        const ModelTextureTarget& target, const std::uint32_t slotMask)
    {
        auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
        auto* textureSet = factory ? factory->Create() : nullptr;
        if (!textureSet) {
            SKSE::log::error(
                "BCNG could not create private embedded TXST index3D={} name='{}' role={}",
                target.index3D, target.name3D, static_cast<unsigned>(target.role));
            return std::nullopt;
        }
        TextureBinding result;
        result.textureSet = textureSet;
        result.role = target.role;
        result.fixedRole = target.role;
        result.slotMask = slotMask;
        if (!PrepareFormSlots(result)) return std::nullopt;
        result.originalPaths = target.paths;
        for (std::size_t index{}; index < result.originalPaths.size(); ++index) {
            if (!WriteTexturePath(result, index, result.originalPaths[index])) {
                SKSE::log::error(
                    "BCNG could not initialize private embedded TXST form={:08X} index3D={} name='{}' shader-index={} path='{}'",
                    textureSet->GetFormID(), target.index3D, target.name3D, index,
                    result.originalPaths[index]);
                return std::nullopt;
            }
        }
        return result;
    }

    [[nodiscard]] bool SetModelAlternateTextures(RE::TESModelTextureSwap& model,
        const std::vector<PendingModelTexture>& pending)
    {
        if (pending.empty()) return true;
        if (model.numAlternateTextures != 0U && !model.alternateTextures) return false;

        using Entry = RE::TESModelTextureSwap::AlternateTexture;
        const auto oldCount = model.numAlternateTextures;
        std::uint32_t additions{};
        for (const auto& item : pending) {
            if (!item.binding.textureSet) return false;
            const auto exists = std::ranges::any_of(
                std::span{ model.alternateTextures, oldCount }, [&](const Entry& entry) {
                    return entry.index3D == item.target.index3D &&
                        entry.name3D == std::string_view{ item.target.name3D };
                });
            if (!exists) ++additions;
        }
        const auto capacity = static_cast<std::size_t>(oldCount) + additions;
        auto* replacement = RE::calloc<Entry>(capacity);
        if (!replacement) return false;
        for (std::uint32_t index{}; index < oldCount; ++index) {
            std::construct_at(replacement + index, model.alternateTextures[index]);
        }
        auto used = oldCount;
        for (const auto& item : pending) {
            const auto current = std::span{ replacement, used };
            const auto match = std::ranges::find_if(
                current, [&](const Entry& entry) {
                    return entry.index3D == item.target.index3D &&
                        entry.name3D == std::string_view{ item.target.name3D };
                });
            if (match != current.end()) {
                match->textureSet = item.binding.textureSet;
                continue;
            }
            if (used >= capacity) {
                std::destroy_n(replacement, used);
                RE::free(replacement);
                return false;
            }
            std::construct_at(replacement + used, Entry{
                .textureSet = item.binding.textureSet,
                .index3D = item.target.index3D,
                .unk0C = 0U,
                .name3D = RE::BSFixedString(item.target.name3D)
            });
            ++used;
        }
        if (model.alternateTextures) {
            std::destroy_n(model.alternateTextures, oldCount);
            RE::free(model.alternateTextures);
        }
        model.alternateTextures = replacement;
        model.numAlternateTextures = used;
        return true;
    }

    [[nodiscard]] bool MaterializeEmbeddedSkinAtlases(RE::TESModelTextureSwap& model,
        const std::uint32_t slotMask, const bcn::SkinUvLayout layout,
        std::vector<TextureBinding>& bindings)
    {
        const auto targets = DiscoverModelTextureTargets(model, layout);
        if (!targets) return false;
        std::vector<PendingModelTexture> pending;
        pending.reserve(targets->size());
        for (auto target : *targets) {
            // Preserve an existing ARMA MODS provider as Default. The NIF
            // paths are only the fallback when this shape had no native
            // alternate texture entry before BCNG cloned the graph.
            for (std::uint32_t index{}; index < model.numAlternateTextures; ++index) {
                const auto& entry = model.alternateTextures[index];
                if (entry.index3D != target.index3D ||
                    entry.name3D != std::string_view{ target.name3D } ||
                    !entry.textureSet) continue;
                for (std::size_t slot{}; slot < target.paths.size(); ++slot) {
                    target.paths[slot] = TexturePath(entry.textureSet, slot);
                }
                break;
            }
            auto binding = CreateModelTexture(target, slotMask);
            if (!binding) {
                SKSE::log::error(
                    "BCNG skipped one embedded skin atlas model='{}' index3D={} name='{}'; ordinary body/hand/foot/face TXST remains usable",
                    model.GetModel(), target.index3D, target.name3D);
                continue;
            }
            pending.push_back({ std::move(target), std::move(*binding) });
        }
        if (!SetModelAlternateTextures(model, pending)) return false;
        for (auto& item : pending) {
            bindings.push_back(std::move(item.binding));
        }
        return true;
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
        const bool synthesizeMissingUbeTexture, const bool prepareForTng = false)
    {
        if (!source) return ArmorGraph{};
        auto* armor = DuplicateForm(source);
        const auto* sourceAddons = EffectiveArmorAddons(source);
        if (!armor || !sourceAddons) return std::nullopt;
        if (armor->GetSlotMask() != source->GetSlotMask() ||
            (!armor->armorAddons.empty() && armor->armorAddons.data() == sourceAddons->data())) {
            SKSE::log::error("BCNG rejected invalid/shared ARMO clone source={:08X} clone={:08X}",
                source->GetFormID(), armor->GetFormID());
            return std::nullopt;
        }

        ArmorGraph graph{ .armor = armor };
        if (prepareForTng) {
            // A TNG child is private to its provider map. Use its current body
            // graph as the source, but turn BCNG's clone into a conventional
            // non-TNG parent with an empty slot 52. TNG can then compose its
            // selected addon normally without any DLL hook or private API.
            RemoveTngSkinMarker(armor);
            armor->RemoveSlotFromMask(
                RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);
        }
        armor->armorAddons.clear();
        armor->armorAddons.reserve(sourceAddons->size());
        // The clone owns a fully materialized addon list. Keeping TNAM would
        // let the engine walk back into the provider's source armor and would
        // defeat both TXST isolation and ownership-aware restoration.
        armor->templateArmor = nullptr;
        const auto sexIndex = static_cast<std::uint32_t>(sex);
        for (auto* sourceAddon : *sourceAddons) {
            if (prepareForTng && sourceAddon &&
                (static_cast<std::uint32_t>(sourceAddon->GetSlotMask().underlying()) &
                    kGenitalSlot) != 0U) continue;
            auto* addon = DuplicateForm(sourceAddon);
            if (!addon || sexIndex >= RE::SEXES::kTotal) return std::nullopt;
            bcn::native_skin::CopyAddonData(*addon, *sourceAddon,
                [](auto& destination, auto& original) { destination.CopyComponent(&original); });
            const auto sameModel = [](const RE::TESModelTextureSwap& copy,
                                       const RE::TESModelTextureSwap& original) {
                if (copy.model != original.model ||
                    copy.numAlternateTextures != original.numAlternateTextures) return false;
                if (original.numAlternateTextures && (!copy.alternateTextures ||
                    copy.alternateTextures == original.alternateTextures)) return false;
                for (std::uint32_t i{}; i < original.numAlternateTextures; ++i) {
                    const auto& left = copy.alternateTextures[i];
                    const auto& right = original.alternateTextures[i];
                    if (left.textureSet != right.textureSet || left.index3D != right.index3D ||
                        left.name3D != right.name3D) return false;
                }
                return true;
            };
            if (!bcn::native_skin::SameAddonGeometry(*addon, *sourceAddon, sameModel)) {
                SKSE::log::error("BCNG rejected incomplete ARMA clone source={:08X} clone={:08X}; original skin retained",
                    sourceAddon->GetFormID(), addon->GetFormID());
                return std::nullopt;
            }
            const auto slotMask = static_cast<std::uint32_t>(
                sourceAddon->GetSlotMask().underlying());
            if (!MaterializeEmbeddedSkinAtlases(
                    addon->bipedModels[sexIndex], slotMask, layout, graph.textures) ||
                !MaterializeEmbeddedSkinAtlases(
                    addon->bipedModel1stPersons[sexIndex], slotMask, layout, graph.textures)) {
                // Embedded genital/anal atlases are optional children of an
                // otherwise valid native skin graph. Never turn one malformed
                // or unsupported add-on material into a total body/hand/foot/
                // face failure. Its role stays unavailable and the partial
                // role mask below preserves that shape's provider baseline.
                SKSE::log::error(
                    "BCNG skipped unsupported embedded genital/anal TXST targets source={:08X}; continuing ordinary native skin graph",
                    sourceAddon->GetFormID());
            }
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
                // FLST has no Copy override either. Materialize the current
                // list into private storage before replacing any texture entry.
                list->forms = sourceList->forms;
                if (sourceList->scriptAddedTempForms) {
                    for (const auto id : *sourceList->scriptAddedTempForms) {
                        if (auto* form = RE::TESForm::LookupByID(id)) list->forms.push_back(form);
                    }
                }
                for (RE::BSTArray<RE::TESForm*>::size_type index{};
                     index < list->forms.size(); ++index) {
                    auto* listedTexture = list->forms[index] &&
                        list->forms[index]->GetFormType() == RE::FormType::TextureSet ?
                        static_cast<RE::BGSTextureSet*>(list->forms[index]) : nullptr;
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
            if (bcn::skin_texture::ownership::IsRacialSkinVarianceTexturePath(
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
        const std::string_view faceDetailBaseline, const bcn::body_family::Mask actorFamily)
    {
        const auto* race = base ? base->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        return bcn::skin_plan::Build(profile, {
            .elder = bcn::IsElderActor(base),
            .vampire = IsVampireRace(base),
            .humanoidRace = bcn::HumanoidSkinRaceFromEditorID(
                editorID ? std::string_view{ editorID } : std::string_view{}),
            .faceDetailFilename = faceDetailBaseline,
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

    [[nodiscard]] bool ValidateResources(const BaseInstance& instance,
        const bcn::skin_plan::ApplicationPlan& plan, const bool bodyRequired)
    {
        std::unordered_set<std::string> checked;
        const auto validate = [&](const TextureLayers& layers, const std::string_view nameSpace) {
            for (const auto& layer : layers) {
                if (layer.shaderTextureIndex >= kTextureCount) continue;
                const auto cached = bcn::runtime_assets::ExpectedTexturePathFromGameRelative(layer.path, nameSpace);
                if (cached.empty() || !bcn::runtime_assets::CachedTextureExists(cached)) return false;
                const auto paths = bcn::native_skin::PathsFromCache(cached);
                if (!paths) {
                    SKSE::log::error("BCNG invalid native TXST cache path='{}'", cached);
                    return false;
                }
                if (!checked.insert(paths->resource).second) continue;
                RE::BSResourceNiBinaryStream stream(paths->resource);
                std::array<char, 4> magic{};
                if (!stream.good() || !stream.read(magic.data(), 4U) ||
                    std::string_view(magic.data(), magic.size()) != "DDS ") {
                    SKSE::log::error("BCNG TXST DDS unavailable to engine path='{}'", cached);
                    return false;
                }
            }
            return true;
        };
        if (bodyRequired) {
            for (const auto* graph : { &instance.skin, &instance.farSkin }) {
                for (const auto& binding : graph->textures) {
                    const auto role = binding.fixedRole.value_or(
                        bcn::native_skin::ResolveTextureRole(binding.slotMask,
                            binding.originalPaths[RE::BSTextureSet::Textures::kDiffuse],
                            plan.runtimeUvLayout));
                    if (!validate(LayersFor(plan, role), "skin")) return false;
                }
            }
        }
        return validate(plan.face, "skin-face");
    }

    [[nodiscard]] bool ApplyLayers(TextureBinding& binding,
        const TextureLayers& layers, const std::string_view nameSpace)
    {
        if (!binding.textureSet) return false;
        for (std::size_t index{}; index < kTextureCount; ++index) {
            if (!WriteTexturePath(binding, index, binding.originalPaths[index])) return false;
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
            const auto paths = bcn::native_skin::PathsFromCache(cached);
            if (!paths || !WriteTexturePath(binding, layer.shaderTextureIndex, paths->native)) return false;
        }
        return true;
    }

    [[nodiscard]] bool ApplyGraph(
        ArmorGraph& graph, const bcn::skin_plan::ApplicationPlan& plan)
    {
        for (auto& binding : graph.textures) {
            binding.role = binding.fixedRole.value_or(
                bcn::native_skin::ResolveTextureRole(binding.slotMask,
                    binding.originalPaths[RE::BSTextureSet::Textures::kDiffuse], plan.runtimeUvLayout));
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
                binding.fixedRole.value_or(
                    bcn::native_skin::ResolveTextureRole(binding.slotMask,
                        binding.originalPaths[RE::BSTextureSet::Textures::kDiffuse], layout)));
        }
        return result;
    }

    [[nodiscard]] bcn::native_skin::TextureRoleMask RequestedRoles(
        const bcn::skin_plan::ApplicationPlan& plan)
    {
        return bcn::native_skin::RequiredRoleMask(
            plan.broadSharedAtlas, !plan.body.empty(), !plan.hands.empty(),
            !plan.feet.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::cbbe &&
                !plan.cbbeGenitalAnal.empty(),
            plan.runtimeUvLayout == bcn::SkinUvLayout::unp &&
                !plan.unpGenitalAnal.empty());
    }

    [[nodiscard]] bool IsTngChildOfGraph(
        RE::TESObjectARMO* candidate, const ArmorGraph& parent)
    {
        if (!IsTngSkinArmor(candidate) || !parent.armor) return false;
        std::size_t bodyAddons{};
        for (auto* expected : parent.armor->armorAddons) {
            if (!expected ||
                (static_cast<std::uint32_t>(expected->GetSlotMask().underlying()) &
                    kGenitalSlot) != 0U) continue;
            ++bodyAddons;
            if (std::ranges::find(candidate->armorAddons, expected) ==
                candidate->armorAddons.end()) return false;
        }
        // TNG's public composition copies the parent ARMA pointers and adds
        // slot 52. Requiring at least one shared non-genital ARMA prevents an
        // unrelated TNG Skin Armor from being mistaken for BCNG's child.
        return bodyAddons != 0U;
    }

    [[nodiscard]] bool OwnsCurrentPointers(const BaseInstance& instance)
    {
        if (!instance.base) return false;
        if (instance.skinAttached && instance.skin.armor &&
            instance.base->skin != instance.skin.armor &&
            !IsTngChildOfGraph(instance.base->skin, instance.skin)) return false;
        if (instance.farSkinAttached && instance.farSkin.armor &&
            instance.base->farSkin != instance.farSkin.armor) return false;
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
        return false;
    }

    [[nodiscard]] bool SourceStillMatches(const BaseInstance& instance)
    {
        return instance.base && instance.base->skin == instance.originalSkin &&
            instance.base->farSkin == instance.originalFarSkin;
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
        return false;
    }

    void RefreshLoadedActors(RE::Actor* selected,
        const std::function<void(RE::Actor*)>& refresh)
    {
        auto* base = selected ? selected->GetActorBase() : nullptr;
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!base || !refresh) return;
        bcn::native_skin::VisitRefreshTargets(selected,
            [processes](const auto& visit) {
                if (!processes) return;
                processes->ForAllActors([&visit](RE::Actor* actor) {
                    visit(actor);
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            },
            [base](RE::Actor* actor) {
                return actor->GetActorBase() == base && actor->Is3DLoaded();
            }, [&](RE::Actor* actor) {
                refresh(actor);
            });
    }

    void RestoreOwnedPointers(BaseInstance& instance)
    {
        if (!instance.base) return;
        if (instance.skinAttached && instance.skin.armor &&
            (instance.base->skin == instance.skin.armor ||
                IsTngChildOfGraph(instance.base->skin, instance.skin))) {
            instance.base->skin = instance.originalSkin;
        }
        if (instance.farSkinAttached && instance.farSkin.armor &&
            instance.base->farSkin == instance.farSkin.armor) {
            instance.base->farSkin = instance.originalFarSkin;
        }
        instance.skinAttached = false;
        instance.farSkinAttached = false;
    }

    [[nodiscard]] std::optional<BaseInstance> BuildInstance(
        RE::Actor* actor, const bcn::SkinProfile& profile,
        const bcn::SkinUvLayout runtimeUvLayout)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* currentSkin = actor ? actor->GetSkin() : nullptr;
        if (!base || !currentSkin) return std::nullopt;
        const auto prepareForTng = IsTngSkinArmor(currentSkin);

        BaseInstance instance;
        instance.base = base;
        instance.ownerActor = actor->GetFormID();
        instance.originalSkin = currentSkin;
        instance.originalFarSkin = base->farSkin;
        // Official UBE naked ARMAs omit NAM1/NAM3 and keep their baseline
        // paths in the NIF. Materialize that missing TXST only when this
        // profile actually declares body-atlas layers; a face-only profile
        // must not alter an otherwise untouched UBE body material graph.
        const auto synthesizeMissingUbeTexture = !profile.body.empty();
        auto skin = CloneArmorGraph(currentSkin, base->GetSex(), runtimeUvLayout,
            synthesizeMissingUbeTexture, prepareForTng);
        if (!skin || !skin->armor) return std::nullopt;
        instance.skin = std::move(*skin);
        if (base->farSkin) {
            auto farSkin = CloneArmorGraph(base->farSkin, base->GetSex(), runtimeUvLayout,
                synthesizeMissingUbeTexture);
            if (!farSkin) return std::nullopt;
            instance.farSkin = std::move(*farSkin);
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
        const std::uint64_t generation,
        const bcn::body_family::Mask sourceFamily,
        std::function<void(RE::Actor*)> afterMutation)
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
            instance.skin.armor != nullptr,
            !instance.appliedProfileId.empty(),
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
            built->sourceFamily = sourceFamily;
            built->sourceRace = actor->GetRace() ? actor->GetRace()->GetFormID() : 0U;
            built->sourceSex = base->GetSex();
            built->generation = instance.generation;
            built->tracked = true;
            instance = std::move(*built);
        }

        if (instance.appliedProfileId == profile.id &&
            instance.appliedContentHash == profile.contentHash && OwnsCurrentPointers(instance)) {
            lock.unlock();
            const auto action = bcn::face_skin::ResolveReapply(
                bcn::face_skin::Matches(actor.get(), profile.id),
                bcn::face_skin::Pending(actor.get(), profile.id));
            if (action == bcn::face_skin::ReapplyAction::complete) {
                bcn::ActorRegistry::Get().MarkSkinApplied(actor.get(), profile.id, false);
            } else if (action == bcn::face_skin::ReapplyAction::applyFace) {
                bcn::face_skin::Apply(actor.get(), plan.face, profile.id,
                    [handle, id = profile.id, baseId, generation](bool success) {
                        if (success && RequestStillCurrent(baseId, generation, id)) {
                            if (auto current = handle.get())
                                bcn::ActorRegistry::Get().MarkSkinApplied(current.get(), id, false);
                        }
                    });
            }
            // The body graph is current: never rebuild it to retry a face,
            // and never report a pending/failed face as whole-skin success.
            return;
        }

        if (!NativeLayersAvailable(plan)) {
            SKSE::log::error("Body Change NG aborted native TXST apply for '{}' before mutation because a declared layer was unavailable or outside TXST indices 0-7",
                profile.name);
            return;
        }

        const auto requestedRoles = RequestedRoles(plan);
        const auto availableRoles = AvailableRoles(instance.skin, plan.runtimeUvLayout);
        const auto applicableRoles = bcn::native_skin::ApplicableRoleMask(
            availableRoles, requestedRoles);
        const auto skippedRoles = static_cast<bcn::native_skin::TextureRoleMask>(
            requestedRoles & static_cast<bcn::native_skin::TextureRoleMask>(~availableRoles));
        if (skippedRoles != 0U) {
            SKSE::log::warn(
                "BCNG partial skin '{}' skipped native roles mask={:02X}; available DDS roles mask={:02X} continue independently",
                profile.name, skippedRoles, applicableRoles);
        }
        const auto bodyGraphRequired = applicableRoles != 0U;
        if (!bodyGraphRequired && plan.face.empty()) {
            SKSE::log::error(
                "Body Change NG found no applicable DDS target for partial skin '{}' on the current actor",
                profile.name);
            return;
        }
        // Preflight every resource before mutating even an already-attached
        // private graph; a missing file must not leave a half-selected skin.
        if (!ValidateResources(instance, plan, bodyGraphRequired)) return;
        if ((bodyGraphRequired && !ApplyGraph(instance.skin, plan)) ||
            (bodyGraphRequired && instance.farSkin.armor &&
                !ApplyGraph(instance.farSkin, plan))) {
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
        instance.appliedProfileId = profile.id;
        instance.appliedContentHash = profile.contentHash;
        instance.appliedDefault = false;
        lock.unlock();
        bcn::face_skin::Apply(actor.get(), plan.face, profile.id,
            [handle, id = profile.id, baseId, generation](bool success) {
                if (success && RequestStillCurrent(baseId, generation, id)) {
                    if (auto current = handle.get()) bcn::ActorRegistry::Get().MarkSkinApplied(current.get(), id, false);
                }
            }, static_cast<bool>(afterMutation) && actor->Is3DLoaded());
        RefreshLoadedActors(actor.get(), afterMutation);
    }

    void ClearNow(RE::ActorHandle handle, const RE::FormID baseId,
        const std::uint64_t generation,
        std::function<void(RE::Actor*)> afterMutation)
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
        bcn::face_skin::Clear(actor.get(), [handle, baseId, generation](bool success) {
            if (success && RequestStillCurrent(baseId, generation, {})) {
                if (auto current = handle.get()) bcn::ActorRegistry::Get().MarkSkinApplied(current.get(), {}, true);
            }
        }, static_cast<bool>(afterMutation) && actor->Is3DLoaded());
        RefreshLoadedActors(actor.get(), afterMutation);
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
        std::function<void(RE::Actor*)> afterMutation)
    {
        body_family::SetSkinFamilyResolver(&SourceBodyFamily);
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
        std::string faceDetailBaseline;
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
            auto* currentFace = CurrentFaceTexture(base);
            const auto currentDetail = currentFace ? TexturePath(currentFace,
                RE::BSTextureSet::Textures::kDetailMap) : std::string{};
            faceDetailBaseline = currentDetail;
        }

        const auto plan = BuildPlan(*profile, base, faceDetailBaseline, actorFamily);
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actorId, [handle, profile = *profile, plan, baseId, generation, actorFamily,
                                      afterMutation = std::move(afterMutation)]() mutable {
            if (!RequestStillCurrent(baseId, generation, profile.id)) return;
            const auto lease = frame_tasks::CurrentLease();
            auto continueApply = [lease, handle, profile, plan, baseId, generation, actorFamily,
                                     afterMutation = std::move(afterMutation)](const bool prepared) mutable {
                if (!prepared) {
                    SKSE::log::error("Body Change NG could not prepare every native TXST asset for '{}'",
                        profile.name);
                    return;
                }
                static_cast<void>(frame_tasks::Continue(lease,
                    [handle, profile = std::move(profile), plan = std::move(plan), baseId, generation, actorFamily,
                        afterMutation = std::move(afterMutation)]() mutable {
                        ApplyNow(handle, std::move(profile), std::move(plan), baseId,
                            generation, actorFamily, std::move(afterMutation));
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

    SkinApplyResult QueueClear(RE::Actor* actor,
        std::function<void(RE::Actor*)> afterMutation)
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
            }
            instance.desiredProfileId.clear();
            instance.tracked = true;
            instance.generation = g_nextGeneration.fetch_add(1U, std::memory_order_relaxed);
            generation = instance.generation;
        }
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actorId,
            [handle, baseId, generation,
                afterMutation = std::move(afterMutation)]() mutable {
                ClearNow(handle, baseId, generation, std::move(afterMutation));
            },
            1U, appearance::WorkChannel::skinApply);
        return SkinApplyResult::queued;
    }

    std::optional<std::uint32_t> SourceBodyFamily(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return std::nullopt;
        std::scoped_lock lock(g_lock);
        const auto found = g_instances.find(base->GetFormID());
        if (found == g_instances.end()) return std::nullopt;
        const auto& instance = found->second;
        const auto race = actor->GetRace() ? actor->GetRace()->GetFormID() : 0U;
        if (!instance.sourceFamily || instance.sourceRace != race ||
            instance.sourceSex != base->GetSex()) return std::nullopt;
        const auto* skin = actor->GetSkin();
        if ((instance.skinAttached && skin == instance.skin.armor) ||
            (instance.appliedDefault && skin == instance.originalSkin)) return instance.sourceFamily;
        return std::nullopt;
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
            return instance.appliedDefault && instance.appliedProfileId.empty() &&
                bcn::face_skin::Matches(actor, {});
        }
        return instance.appliedProfileId == profileId &&
            instance.appliedContentHash == SkinProfiles::Get().ContentHash(profileId) &&
            OwnsCurrentPointers(instance) && bcn::face_skin::Matches(actor, profileId);
    }

    void ResetSessionState()
    {
        {
            std::scoped_lock lock(g_lock);
            for (auto& [baseId, instance] : g_instances) {
                RestoreOwnedPointers(instance);
                // TESNPC and the private duplicate forms are game-data forms,
                // not save-specific references.  Reuse the already allocated
                // graph across save loads after clearing every selection and
                // ownership marker; rebuilding one graph per load would leak
                // duplicate forms for the lifetime of the Skyrim process.
                instance.ownerActor = 0U;
                instance.desiredProfileId.clear();
                instance.appliedProfileId.clear();
                instance.appliedContentHash = 0U;
                instance.generation = g_nextGeneration.fetch_add(1U, std::memory_order_relaxed);
                instance.tracked = false;
                instance.appliedDefault = false;
            }
        }
        // This runs only at the pre/post save-load boundary. The game rebuilds
        // the incoming world's actor 3D itself; scheduling an additional reset
        // here would race that lifecycle and duplicate the facade refresh.
    }
}
