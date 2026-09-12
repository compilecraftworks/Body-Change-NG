#include "BodyChangeNG/NativeAddonSkinBackend.h"
#include "BodyChangeNG/NativeAddonLayout.h"
#include "BodyChangeNG/NativeAddonPatterns.h"
#include "BodyChangeNG/NativeTextureData.h"
#include "BodyChangeNG/NativeTexturePath.h"
#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/SkinTextureOwnership.h"

#include <RE/B/BGSTextureSet.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BipedAnim.h>
#include <RE/C/ConcreteFormFactory.h>
#include <SKSE/Trampoline.h>
#include <SKSE/Logger.h>
#include <atomic>
#include <memory>

#ifndef EXCLUSIVE_SKYRIM_FLAT
#error Native addon texture supply is SE/AE flat only.
#endif

namespace
{
    using namespace bcn::native_addon;
    Selections g_selections;
    Baselines g_baselines;
    std::atomic_bool g_available{};
    using Visit = std::uint32_t (*)(VisitorContext*, RE::NiAVObject*);
    Visit g_visit{};
    SKSE::Trampoline g_trampoline{ "BCNG native addon TXST" };

    [[nodiscard]] std::string NativePath(std::string path)
    {
        std::ranges::replace(path, '/', '\\');
        if (path.size() >= 9 && bcn::skin_texture::ownership::ContainsNormalized(
                std::string_view(path).substr(0, 9), "textures\\")) path.erase(0, 9);
        return path;
    }

    [[nodiscard]] bool ReadProviderPaths(RE::BSGeometry& geometry,
        RE::BGSTextureSet* original, Paths& paths)
    {
        auto* shader = geometry.lightingShaderProp_cast();
        auto* material = shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        auto* textures = bcn::skin_target::StableTextureSet(material);
        if (!original && !textures) return false;
        for (std::size_t i{}; i < paths.size(); ++i) {
            const auto slot = static_cast<RE::BSTextureSet::Texture>(i);
            const auto* path = original ? original->GetTexturePath(slot) : textures->GetTexturePath(slot);
            paths[i] = NativePath(path ? path : "");
            // The actual BIPOBJECT source is never replaced by this adapter,
            // so a provider's new TXST supersedes an old baseline. With a NIF-
            // only source, never capture our already-selected material as Default.
            if (!original && bcn::skin_texture::ownership::IsOwnedTexturePath(paths[i])) return false;
        }
        return true;
    }

    [[nodiscard]] bool Matches(const Selection& selection, const RE::BIPOBJECT& part,
        std::string_view name)
    {
        if (!part.item || !part.addon) return false;
        return std::ranges::any_of(selection.targets, [&](const Target& target) {
            return target.armor == part.item->GetFormID() &&
                target.addon == part.addon->GetFormID() &&
                std::ranges::find(target.nodes, name) != target.nodes.end();
        });
    }

    // The native material RETAINS the BSTextureSet subobject (material+0x78).
    // Never delete the TESForm at visitor return: the renderer may still own it.
    // Factory construction starts with ONE reference (SE ctor 0x2D126E).
    // Adopt that reference, then let native NiPointer owners control deletion.
    struct PrivateTexture final
    {
        RE::NiPointer<RE::BSTextureSet> reference;
        PrivateTexture() = default;
        explicit PrivateTexture(RE::BGSTextureSet* created) :
            reference(AdoptFactoryTexture<RE::BSTextureSet, RE::NiPointer<RE::BSTextureSet>>(created)) {}
        [[nodiscard]] RE::BGSTextureSet* get() const
        { return static_cast<RE::BGSTextureSet*>(reference.get()); }
        [[nodiscard]] explicit operator bool() const { return reference != nullptr; }
        [[nodiscard]] RE::BGSTextureSet* operator->() const { return get(); }
        [[nodiscard]] RE::BGSTextureSet& operator*() const { return *get(); }
    };

    [[nodiscard]] PrivateTexture MakeTexture(const Paths& paths)
    {
        auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
        PrivateTexture texture{ factory ? factory->Create() : nullptr };
        if (!texture) {
            SKSE::log::warn("Native addon TXST factory returned null");
            return {};
        }
        // Create(), not CreateDuplicateForm(). Skyrim may assign a normal
        // dynamic FFxxxxxx FormID here. BCNG never installs this form in an
        // ARMO/ARMA or serializes its identity; ownership is exclusively the
        // verified intrusive reference held by the native material/renderer.
        const auto slots = bcn::native_skin::DiscoverFormTextureSlots(*texture,
            [&](std::size_t index) -> std::string_view {
                const auto* path = texture->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(index));
                return path ? path : "";
            });
        if (!slots) {
            SKSE::log::warn("Native addon TXST shader/record slot discovery failed");
            return {};
        }
        for (std::size_t i{}; i < paths.size(); ++i) {
            if (paths[i].size() + 15U >= 260U ||
                !bcn::native_skin::WriteFormTexturePath(*texture, (*slots)[i], paths[i].c_str())) return {};
        }
        return texture;
    }

    // Preparation must finish before invoking the native visitor. Exceptions
    // cannot cross its C ABI boundary or cause the visitor to execute twice.
    struct PreparedSupply final
    {
        PrivateTexture texture;
    };

    PreparedSupply PrepareSupply(VisitorContext* context, RE::NiAVObject* object)
    {
        // All ordinary body/face calls are passed through untouched. Never
        // infer a genital from shader type or geometry name alone.
        if (!g_available.load(std::memory_order_acquire) ||
            !context || context->index != 22 || !context->parts || !object)
            return {};
        auto* geometry = object->AsGeometry();
        if (!geometry) { return {}; }
        const auto* parts = reinterpret_cast<const RE::BIPOBJECT*>(context->parts);
        const auto& part = parts[22];
        if (!part.addon || !AcceptSlot(context->index,
                part.addon->GetSlotMask().underlying())) {
            return {};
        }
        auto* shader = geometry->lightingShaderProp_cast();
        // Genital SkinTint (feature 5) has its own material on engine clone.
        // Do not route hair, cloth or an arbitrary non-skin lighting material.
        if (!shader || !shader->material || shader->material->GetFeature() !=
                RE::BSShaderMaterial::Feature::kFaceGenRGBTint ||
            !shader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kFaceGenRGBTint))
        {
            return {};
        }

        const auto ref = RE::TESObjectREFR::LookupByHandle(context->actorHandle);
        auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!base) { return {}; }
        const auto channel = base->GetSex() == RE::SEX::kFemale ? Channel::futanari : Channel::maleGenitals;
        const auto selection = g_selections.Get(actor->GetFormID());
        const bool selected = selection && selection->channel == channel &&
            Matches(*selection, part, geometry->name.c_str());
        if (selection && !selected) {
            return {};
        }
        const BaselineIdentity identity{
            actor->GetFormID(), part.item ? part.item->GetFormID() : 0U,
            part.addon->GetFormID(), channel, geometry->name.c_str()
        };
        // Unselected actors never need a BCNG baseline. Retain an existing
        // one only long enough to restore a prior BCNG material to Default.
        Paths paths;
        if (!selected && !g_baselines.Get(identity, paths)) return {};
        const bool liveProvider = ReadProviderPaths(*geometry, part.skinTexture, paths);
        if (liveProvider && selected) g_baselines.Put(identity, paths);
        else if (!liveProvider && !g_baselines.Get(identity, paths)) {
            return {};
        }
        // When no BCNG selection exists, the original provider TXST/NIF is
        // already authoritative. A private supply is needed only if the live
        // material still contains the prior BCNG-owned cached paths.
        if (!selected && liveProvider) return {};
        if (selected) paths = ResolvePaths(std::move(paths), selection->overrides);
        auto texture = MakeTexture(paths);
        if (!texture) { return {}; }
        return { std::move(texture) };
    }

    std::uint32_t VisitSkin(VisitorContext* context, RE::NiAVObject* geometry)
    {
        PreparedSupply suppliedTexture;
        try {
            suppliedTexture = PrepareSupply(context, geometry);
        } catch (const std::exception& error) {
            SKSE::log::error("Native addon TXST preparation failed: {}", error.what());
        } catch (...) {
            SKSE::log::error("Native addon TXST preparation failed");
        }
        if (!suppliedTexture.texture) {
            return g_visit(context, geometry);
        }
        ScopedSupply supplied(*context, suppliedTexture.texture.get());
        const auto result = g_visit(&supplied.context, geometry);
        return result;
    }

    bool ExecutableRange(std::uintptr_t address, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!size || !VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || info.AllocationBase != reinterpret_cast<void*>(REL::Module::get().base()) ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto protection = info.Protect & 0xFFU;
        const bool executable = protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
        const auto begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        return executable && address >= begin && address - begin <= info.RegionSize &&
            size <= info.RegionSize - (address - begin);
    }

    [[nodiscard]] std::optional<std::uint32_t> ResolveRva(std::uint64_t id)
    {
        if (id == kSleepImport) {
            const auto base = REL::Module::get().base();
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) return {};
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IAT) return {};
            const auto& iat = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT];
            if (!iat.VirtualAddress || !iat.Size || iat.Size > 0x100000 ||
                iat.VirtualAddress > nt->OptionalHeader.SizeOfImage ||
                iat.Size > nt->OptionalHeader.SizeOfImage - iat.VirtualAddress) return {};
            MEMORY_BASIC_INFORMATION info{};
            const auto address = base + iat.VirtualAddress;
            if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
                info.State != MEM_COMMIT || info.AllocationBase != reinterpret_cast<void*>(base) ||
                (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                address < reinterpret_cast<std::uintptr_t>(info.BaseAddress) ||
                iat.Size > info.RegionSize - (address-reinterpret_cast<std::uintptr_t>(info.BaseAddress))) return {};
            const auto kernel = GetModuleHandleW(L"kernel32.dll");
            const auto sleep = kernel ? GetProcAddress(kernel, "Sleep") : nullptr;
            return FindUniqueImport({ reinterpret_cast<const std::uint8_t*>(address), iat.Size },
                iat.VirtualAddress, reinterpret_cast<std::uintptr_t>(sleep));
        }
        const auto offset = REL::ID(id).offset();
        if (!offset || offset > UINT32_MAX) return {};
        return static_cast<std::uint32_t>(offset);
    }

    [[nodiscard]] std::optional<std::uint32_t> FindCallerBranch(
        std::uint64_t callerId, std::string_view prefix, std::uint8_t opcode,
        std::uint32_t visitorRva)
    {
        const auto start = ResolveRva(callerId);
        if (!start) return {};
        const auto base = REL::Module::get().base();
        DWORD64 imageBase{};
        const auto* function = RtlLookupFunctionEntry(base + *start, &imageBase, nullptr);
        if (!function || imageBase != base || function->BeginAddress != *start ||
            function->EndAddress <= *start || function->EndAddress - *start > 0x4000 ||
            !ExecutableRange(base + *start, function->EndAddress - *start)) return {};
        return FindVisitorBranch(
            { reinterpret_cast<const std::uint8_t*>(base + *start), function->EndAddress - *start },
            *start, prefix, opcode, visitorRva);
    }

    using Branch = std::array<std::uint8_t, 5>;
    std::optional<Branch> RelativeBranch(std::uint8_t opcode,
        std::uintptr_t from, std::uintptr_t to)
    {
        const auto distance = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from + 5);
        if (distance < INT32_MIN || distance > INT32_MAX) return std::nullopt;
        Branch bytes{ opcode };
        const auto displacement = static_cast<std::int32_t>(distance);
        std::memcpy(bytes.data() + 1, &displacement, 4);
        return bytes;
    }
}

namespace bcn::native_addon
{
    bool Available() noexcept { return g_available.load(std::memory_order_acquire); }

    void Install()
    {
        static std::atomic_bool attempted{};
        if (attempted.exchange(true)) return;
        const auto version = REL::Module::get().version();
        const auto layout = ResolveRuntimeLayout(version);
        if (!layout) {
            SKSE::log::warn("Native addon TXST: unsupported game {}", version.string());
            return;
        }
        const auto base = REL::Module::get().base();
        const std::span<const RelocatedPattern> codePatterns = layout->ae ?
            std::span<const RelocatedPattern>{ patterns::ae } :
            std::span<const RelocatedPattern>{ patterns::se };
        for (const auto& pattern : codePatterns) {
            const auto rva = ResolveRva(pattern.functionID);
            if (!rva || !ExecutableRange(base + *rva, pattern.bytes.size()) ||
                !MatchRelocatedCode(pattern,
                    { reinterpret_cast<const std::uint8_t*>(base + *rva), pattern.bytes.size() },
                    *rva, ResolveRva)) {
                SKSE::log::warn("Native addon TXST: unrecognized ownership code in {} at ID {}; no hooks installed",
                    version.string(), pattern.functionID);
                return;
            }
        }
        const auto address = [base](std::uint64_t id) { return base + ResolveRva(id).value_or(0); };
        auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
        const auto materialTable = REL::Relocation<std::uintptr_t*>{ RE::BSLightingShaderMaterialFacegenTint::VTABLE[0] };
        const auto textureTable = REL::Relocation<std::uintptr_t*>{ RE::BGSTextureSet::VTABLE[1] };
        if (!factory || (*reinterpret_cast<std::uintptr_t**>(factory))[1] != address(layout->factory) ||
            materialTable.get()[8] != address(layout->materialLoader) ||
            textureTable.get()[0] != address(layout->textureDestructor) ||
            textureTable.get()[1] != address(layout->deleteThis)) {
            SKSE::log::warn("Native addon TXST: factory/material ownership virtuals differ; no hooks installed");
            return;
        }
        const auto visitorRva = ResolveRva(layout->visitor);
        if (!visitorRva) return;
        const auto directRva = FindCallerBranch(layout->directCaller,
            directVisitorPrefix, 0xE8, *visitorRva);
        const auto recursiveRva = FindCallerBranch(layout->recursiveCaller,
            layout->ae ? aeRecursiveVisitorPrefix : seRecursiveVisitorPrefix, 0xE9, *visitorRva);
        if (!directRva || !recursiveRva) {
            SKSE::log::warn("Native addon TXST: unrecognized or ambiguous visitor call sites in {}; no hooks installed",
                version.string());
            return;
        }
        const auto direct = base + *directRva;
        const auto recursive = base + *recursiveRva;
        const auto original = base + *visitorRva;
        const auto expectedCall = RelativeBranch(0xE8, direct, original);
        const auto expectedJump = RelativeBranch(0xE9, recursive, original);
        if (!expectedCall || !expectedJump || !ExecutableRange(direct, 5) || !ExecutableRange(recursive, 5) ||
            std::memcmp(reinterpret_cast<void*>(direct), expectedCall->data(), 5) != 0 ||
            std::memcmp(reinterpret_cast<void*>(recursive), expectedJump->data(), 5) != 0) {
            SKSE::log::warn("Native addon TXST: visitor call sites already changed; no hooks installed");
            return;
        }
        // Allocate the entire relay before modifying either engine instruction.
        // Both verified original instructions are exactly five bytes long.
        g_trampoline.create(64);
        auto* relay = g_trampoline.allocate<std::array<std::uint8_t, 14>>();
        *relay = { 0xFF, 0x25, 0, 0, 0, 0 };
        const auto destination = reinterpret_cast<std::uintptr_t>(&VisitSkin);
        std::memcpy(relay->data() + 6, &destination, 8);
        FlushInstructionCache(GetCurrentProcess(), relay->data(), relay->size());
        const auto patchedCall = RelativeBranch(0xE8, direct, reinterpret_cast<std::uintptr_t>(relay));
        const auto patchedJump = RelativeBranch(0xE9, recursive, reinterpret_cast<std::uintptr_t>(relay));
        if (!patchedCall || !patchedJump) {
            g_trampoline = SKSE::Trampoline{ "BCNG native addon TXST" };
            return;
        }
        g_visit = reinterpret_cast<Visit>(original);
        if (!REL::safe_write(direct, patchedCall->data(), 5, expectedCall->data(), 5)) return;
        if (!REL::safe_write(recursive, patchedJump->data(), 5, expectedJump->data(), 5)) {
            const bool restored = REL::safe_write(direct, expectedCall->data(), 5, patchedCall->data(), 5);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(direct), 5);
            SKSE::log::error("Native addon TXST: second hook failed; first restored={}", restored);
            // Keep relay valid even when rollback was displaced by another plugin.
            return;
        }
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(direct), 5);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(recursive), 5);
        g_available.store(true, std::memory_order_release);
        SKSE::log::info("Native addon TXST visitor installed for {}: relocated ownership proof, provider forms untouched", version.string());
    }

    MutationResult Apply(std::uint32_t actor, Channel channel,
        const std::vector<skin_target::LoadedPartTarget>& targets,
        const std::vector<SkinTextureLayer>& layers, std::string_view cacheNamespace)
    {
        MutationResult result;
        if (!Available() || !actor || layers.empty() || targets.empty()) return result;
        Selection next;
        next.channel = channel;
        for (const auto& layer : layers) {
            if (layer.shaderTextureIndex >= next.overrides.size()) return {};
            const auto cached = runtime_assets::ExpectedTexturePathFromGameRelative(layer.path, cacheNamespace);
            const auto path = native_skin::PathsFromCache(cached);
            if (!path || !runtime_assets::CachedTextureExists(cached)) return {};
            next.overrides[layer.shaderTextureIndex] = path->native;
        }
        for (const auto& target : targets) {
            if (!target.armor || !target.addon || !AcceptSlot(22, target.slotMask)) continue;
            Target item{ target.armor->GetFormID(), target.addon->GetFormID(), {} };
            for (const auto& view : target.views)
                for (const auto& node : view.nodes)
                    if (std::ranges::find(item.nodes, node) == item.nodes.end()) item.nodes.push_back(node);
            if (!item.nodes.empty()) {
                result.targets += item.nodes.size();
                next.targets.push_back(std::move(item));
            }
        }
        if (next.targets.empty()) return {};
        g_baselines.Retain(actor, channel, next.targets);
        result.changed = g_selections.Publish(actor, std::move(next));
        result.geometries = result.targets; // accepted targets, not a render-success claim
        result.textureLayers = layers.size();
        return result;
    }

    bool Clear(std::uint32_t actor) { return g_selections.Forget(actor); }
    void Reset()
    {
        g_selections.Reset();
        g_baselines.Reset();
    }
    void Forget(std::uint32_t actor)
    {
        g_selections.Forget(actor);
        g_baselines.Forget(actor);
    }
    bool HasSelection(std::uint32_t actor, Channel channel)
    {
        const auto state = g_selections.Get(actor);
        return state && state->channel == channel;
    }
    std::string SourceDiffuseTexture(std::uint32_t actor, Channel channel,
        std::uint32_t armor, std::uint32_t addon, const RE::BSGeometry& geometry)
    {
        return g_baselines.Diffuse({ actor, armor, addon, channel, geometry.name.c_str() });
    }
}
