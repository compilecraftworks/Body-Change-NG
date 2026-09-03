#include "BodyChangeNG/BodyFamily.h"

#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <bit>
#include <format>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    using bcn::body_family::Family;
    using bcn::body_family::Mask;
    using bcn::body_family::Sex;

    struct ActorSignature final
    {
        RE::FormID skinFormID{};
        RE::FormID raceFormID{};
        std::uintptr_t thirdPersonRoot{};
        Sex sex{ Sex::male };
        [[nodiscard]] bool operator==(const ActorSignature&) const = default;
    };

    struct ActorCacheEntry final
    {
        ActorSignature signature;
        Mask family{};
    };

    std::mutex g_cacheLock;
    std::unordered_map<RE::FormID, ActorCacheEntry> g_actorCache;
    std::array<std::optional<Mask>, 2> g_installedFamiliesCache;

    [[nodiscard]] constexpr std::size_t SexIndex(const Sex sex) noexcept
    {
        return sex == Sex::female ? 1U : 0U;
    }

    [[nodiscard]] Sex ActorSex(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return base && base->GetSex() == RE::SEX::kFemale ? Sex::female : Sex::male;
    }

    void AppendSignal(std::string& target, const std::string_view signal)
    {
        if (signal.empty()) return;
        if (!target.empty()) target.push_back(' ');
        target.append(signal);
    }

    void AppendFormSignals(std::string& target, const RE::TESForm* form)
    {
        if (!form) return;
        if (const auto* file = form->GetFile(0)) AppendSignal(target, file->GetFilename());
        AppendSignal(target, form->GetFormEditorID());
        if (const auto* name = form->GetName(); name && name[0] != '\0') AppendSignal(target, name);
    }

    [[nodiscard]] Mask SelectSingleExplicitFamily(const Mask detected, const Sex sex)
    {
        const auto candidates = detected & bcn::body_family::NonVanillaFamilies(sex);
        return std::popcount(candidates) == 1 ? candidates : 0U;
    }

    [[nodiscard]] Mask DetectSkinMetadata(const RE::TESObjectARMO* skin, const Sex sex)
    {
        if (!skin) return 0U;
        std::string common;
        AppendFormSignals(common, skin);
        for (const auto* keyword : skin->GetKeywords()) AppendFormSignals(common, keyword);

        std::string signals = common;
        const auto index = sex == Sex::female ? 1U : 0U;
        for (const auto* addon : skin->armorAddons) {
            if (!addon) continue;
            AppendFormSignals(signals, addon);
            const auto appendModel = [&](const RE::TESModelTextureSwap& model) {
                if (const auto* path = model.GetModel(); path && path[0] != '\0') AppendSignal(signals, path);
            };
            appendModel(addon->bipedModels[index]);
            appendModel(addon->bipedModel1stPersons[index]);
        }
        return SelectSingleExplicitFamily(bcn::body_family::DetectText(signals, sex), sex);
    }

    struct LoadedShape final
    {
        std::string name;
        std::string diffuse;
        std::string scenePath;
    };

    void CollectShapes(RE::NiAVObject* object, std::unordered_set<RE::NiAVObject*>& visited,
        std::vector<LoadedShape>& shapes, const std::string_view parentPath)
    {
        if (!object || !visited.insert(object).second) return;
        const std::string objectName = object->name.empty() ? "<unnamed>" : object->name.c_str();
        const auto scenePath = std::string(parentPath) + '/' + objectName;
        if (auto* geometry = object->AsGeometry()) {
            auto* shader = geometry->GetGeometryRuntimeData().shaderProperty.get();
            auto* texture = shader ? shader->GetBaseTexture() : nullptr;
            shapes.push_back({
                .name = objectName,
                .diffuse = texture && !texture->name.empty() ? std::string{ texture->name.c_str() } : std::string{},
                .scenePath = scenePath
            });
        }
        if (auto* node = object->AsNode()) {
            for (const auto& child : node->GetChildren()) CollectShapes(child.get(), visited, shapes, scenePath);
        }
    }

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    struct LoadedSkinEvidence final
    {
        Mask explicitFamilies{};
        bool hasStandardTexture{};
        bool hasUbeTexture{};
        bool hasStandardHeadTexture{};
        bool hasUbeHeadTexture{};

        [[nodiscard]] bcn::body_family::SkinTextureLayout Layout() const noexcept
        {
            return bcn::body_family::ResolveLoadedSkinTextureLayout(
                hasStandardTexture, hasUbeTexture,
                hasStandardHeadTexture, hasUbeHeadTexture);
        }
    };

    [[nodiscard]] LoadedSkinEvidence DetectLoadedSkin(
        RE::Actor* actor, RE::TESObjectARMO* skin, const Sex sex)
    {
        LoadedSkinEvidence evidence;
        if (!actor || !actor->Is3DLoaded()) return evidence;
        std::vector<std::string> formTokens;
        if (skin) {
            formTokens.push_back(std::format("{:08X}", skin->GetFormID()));
            for (const auto* addon : skin->armorAddons) {
                if (addon) formTokens.push_back(std::format("{:08X}", addon->GetFormID()));
            }
        }
        std::unordered_set<RE::NiAVObject*> visited;
        std::vector<LoadedShape> shapes;
        CollectShapes(actor->Get3D(false), visited, shapes, "3rd-person");
        for (const auto& shape : shapes) {
            const auto belongsToSkin = !formTokens.empty() &&
                std::ranges::any_of(formTokens, [&](const auto& token) { return shape.scenePath.contains(token); });
            auto normalizedTexture = LowerAscii(shape.diffuse);
            std::ranges::replace(normalizedTexture, '\\', '/');
            const auto headStem = sex == Sex::female ? std::string_view{ "femalehead" } :
                std::string_view{ "malehead" };
            const auto actorHead = normalizedTexture.contains(headStem) &&
                (normalizedTexture.ends_with(".dds") ||
                    normalizedTexture.contains(std::string(headStem) + '_'));
            switch (bcn::body_family::DetectSkinTextureLayout(shape.diffuse, sex)) {
            case bcn::body_family::SkinTextureLayout::standard:
                if (belongsToSkin || actorHead) evidence.hasStandardTexture = true;
                if (actorHead) evidence.hasStandardHeadTexture = true;
                break;
            case bcn::body_family::SkinTextureLayout::ube:
                // Do not classify a CBBE NPC as UBE merely because an equipped
                // outfit is stored under meshes/textures/!UBE. UBE's live head
                // atlas (or the actual Skin Armor geometry) is authoritative.
                if (belongsToSkin || actorHead) evidence.hasUbeTexture = true;
                if (actorHead) evidence.hasUbeHeadTexture = true;
                break;
            default:
                break;
            }
            // Family-looking names on unrelated hair, jewelry, and weapons
            // must not classify the actor. Restrict explicit mesh-name signals
            // to the naked Skin Armor/Addons when their FormIDs are present in
            // the scene path; texture namespaces above remain safe to inspect
            // globally, including UBE's always-loaded head.
            if (belongsToSkin) {
                evidence.explicitFamilies |= bcn::body_family::DetectText(
                    shape.name + ' ' + shape.diffuse + ' ' + shape.scenePath, sex);
            }
        }
        return evidence;
    }

    [[nodiscard]] Mask FrameworkPluginFamily(const std::string_view filename, const Sex sex)
    {
        const auto name = LowerAscii(std::string(filename));
        if (sex == Sex::female) {
            if (name == "cbbe.esp" || name == "3ba.esp" || name.contains("racemenumorphscbbe") ||
                name.contains("cbbe3ba") || name.contains("cbbe 3ba")) return bcn::body_family::Bit(Family::cbbe);
            if (name == "bhunp.esp" || name == "bhunp3bbb.esp" || name.contains("racemenumorphsbhunp") ||
                name.contains("racemenumorphsuunp")) return bcn::body_family::Bit(Family::unp);
            if (name == "ube.esp" || name.starts_with("ube_") || name.contains("ultimatebodyenhancer")) {
                return bcn::body_family::Bit(Family::ube);
            }
            return 0U;
        }
        if (name == "himbo.esp" || name.contains("racemenumorphshimbo")) return bcn::body_family::Bit(Family::himbo);
        if (name == "sam.esp" || name == "sam.esm" || name == "samlight.esp" || name == "sam_light.esp" ||
            name.starts_with("sam_light_") || name.contains("racemenumorphssam")) return bcn::body_family::Bit(Family::sam);
        return 0U;
    }

    [[nodiscard]] Mask DetectInstalledFamilies(const Sex sex)
    {
        const auto index = SexIndex(sex);
        {
            std::scoped_lock lock(g_cacheLock);
            if (g_installedFamiliesCache[index]) return *g_installedFamiliesCache[index];
        }
        Mask detected{};
        if (const auto* handler = RE::TESDataHandler::GetSingleton()) {
            const auto collect = [&](const RE::TESFile* const* files, const std::size_t count) {
                if (!files) return;
                for (std::size_t fileIndex{}; fileIndex < count; ++fileIndex) {
                    if (const auto* file = files[fileIndex]; file && !file->GetFilename().empty()) {
                        detected |= FrameworkPluginFamily(file->GetFilename(), sex);
                    }
                }
            };
            collect(handler->GetLoadedMods(), handler->GetLoadedModCount());
            collect(handler->GetLoadedLightMods(), handler->GetLoadedLightModCount());
        }
        {
            std::scoped_lock lock(g_cacheLock);
            g_installedFamiliesCache[index] = detected;
        }
        return detected;
    }
}

namespace bcn::body_family
{
    Mask ResolveActor(RE::Actor* actor)
    {
        if (!actor) return 0U;
        const auto sex = ActorSex(actor);
        auto* skin = actor->GetSkin();
        const ActorSignature signature{
            .skinFormID = skin ? skin->GetFormID() : 0U,
            .raceFormID = actor->GetRace() ? actor->GetRace()->GetFormID() : 0U,
            .thirdPersonRoot = reinterpret_cast<std::uintptr_t>(actor->Get3D(false)),
            .sex = sex
        };
        {
            std::scoped_lock lock(g_cacheLock);
            if (const auto found = g_actorCache.find(actor->GetFormID());
                found != g_actorCache.end() && found->second.signature == signature) return found->second.family;
        }

        const auto installedFamilies = DetectInstalledFamilies(sex);
        auto family = DetectSkinMetadata(skin, sex);
        const auto loaded = DetectLoadedSkin(actor, skin, sex);
        if (family == 0U) {
            family = ResolveSkinTextureFamily(
                loaded.explicitFamilies, installedFamilies, loaded.Layout(), sex);
        }
        if (family == 0U) {
            family = ResolveSkinTextureFamily(0U, installedFamilies, SkinTextureLayout::unknown, sex);
        }
        // Still unknown or conflicting means no filter.  Never guess Vanilla
        // and accidentally hide the selected actor's usable presets.
        {
            std::scoped_lock lock(g_cacheLock);
            g_actorCache.insert_or_assign(actor->GetFormID(), ActorCacheEntry{ signature, family });
        }
        SKSE::log::info(
            "Body family actor={:08X} skin={:08X} mask={} installed={} loaded-explicit={} texture-layout={}",
            actor->GetFormID(), signature.skinFormID, family, installedFamilies, loaded.explicitFamilies,
            loaded.Layout() == SkinTextureLayout::ube ? "UBE" :
                loaded.Layout() == SkinTextureLayout::standard ? "standard" : "unknown");
        return family;
    }

    void ResetRuntimeCaches()
    {
        std::scoped_lock lock(g_cacheLock);
        g_actorCache.clear();
        g_installedFamiliesCache = {};
    }
}
