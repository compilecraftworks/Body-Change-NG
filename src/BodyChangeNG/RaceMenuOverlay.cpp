#include "BodyChangeNG/RaceMenuOverlay.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/OverlayPolicy.h"
#include "BodyChangeNG/OverlayProviderConfig.h"
#include "BodyChangeNG/OverlayReplacementState.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuCompatibility.h"
#include "BodyChangeNG/RaceMenuLegacyStringABI.h"
#include "BodyChangeNG/RaceMenuOverrideABI.h"
#include "BodyChangeNG/RuntimeCompatibility.h"

#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BSLightingShaderMaterialFacegenTint.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSVisit.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <Windows.h>

namespace
{
    using bcn::racemenu_compat::LegacyNodeName;
    using namespace bcn::racemenu_abi;

    // RaceMenu 0.4.16 and early AE releases return the concrete Overlay v1
    // object, not the later public wrapper. Save/Load are therefore real
    // vtable entries between IPluginInterface and HasOverlays, and the calls
    // have no v2 defer parameter. Never cast this object to the v2 interface.
    class IOverlayInterfaceV1 : public IPluginInterface
    {
    public:
        virtual void ReservedSave() = 0;
        virtual void ReservedLoad() = 0;
        virtual bool HasOverlays(RE::TESObjectREFR*) = 0;
        virtual void AddOverlays(RE::TESObjectREFR*) = 0;
        virtual void RemoveOverlays(RE::TESObjectREFR*) = 0;
        virtual void RevertOverlays(RE::TESObjectREFR*, bool) = 0;
        virtual void RevertOverlay(RE::TESObjectREFR*, LegacyNodeName,
            std::uint32_t, std::uint32_t, bool) = 0;
        virtual void EraseOverlays(RE::TESObjectREFR*) = 0;
        virtual void RevertHeadOverlays(RE::TESObjectREFR*, bool) = 0;
        virtual void RevertHeadOverlay(RE::TESObjectREFR*, LegacyNodeName,
            std::uint32_t, std::uint32_t, bool) = 0;
    };

    // Public programmer interface introduced by the later RaceMenu v2
    // Overlay wrapper.
    class IOverlayInterfaceV2 : public IPluginInterface
    {
    public:
        enum class OverlayType { Normal, Spell };
        enum class OverlayLocation { Body, Hand, Feet, Face };
        using OverlayInstallCallback = void (*)(RE::TESObjectREFR*, RE::NiAVObject*);

        virtual bool HasOverlays(RE::TESObjectREFR*) = 0;
        virtual void AddOverlays(RE::TESObjectREFR*, bool) = 0;
        virtual void RemoveOverlays(RE::TESObjectREFR*, bool) = 0;
        virtual void RevertOverlays(RE::TESObjectREFR*, bool, bool) = 0;
        virtual void RevertOverlay(RE::TESObjectREFR*, const char*, std::uint32_t,
            std::uint32_t, bool, bool) = 0;
        virtual void EraseOverlays(RE::TESObjectREFR*, bool) = 0;
        virtual void RevertHeadOverlays(RE::TESObjectREFR*, bool, bool) = 0;
        virtual void RevertHeadOverlay(RE::TESObjectREFR*, const char*, std::uint32_t,
            std::uint32_t, bool, bool) = 0;
        virtual std::uint32_t GetOverlayCount(OverlayType, OverlayLocation) = 0;
        virtual const char* GetOverlayFormat(OverlayType, OverlayLocation) = 0;
        virtual bool RegisterInstallCallback(const char*, OverlayInstallCallback) = 0;
        virtual bool UnregisterInstallCallback(const char*) = 0;
    };


    class StringVariant final : public IOverrideInterfaceV2::SetVariant
    {
    public:
        explicit StringVariant(std::string value) : value_(std::move(value)) {}
        Type GetType() override { return Type::String; }
        const char* String() override { return value_.c_str(); }
    private:
        std::string value_;
    };

    class IntVariant final : public IOverrideInterfaceV2::SetVariant
    {
    public:
        explicit IntVariant(const std::int32_t value) : value_(value) {}
        Type GetType() override { return Type::Int; }
        std::int32_t Int() override { return value_; }
    private:
        std::int32_t value_{};
    };

    class FloatVariant final : public IOverrideInterfaceV2::SetVariant
    {
    public:
        explicit FloatVariant(const float value) : value_(value) {}
        Type GetType() override { return Type::Float; }
        float Float() override { return value_; }
    private:
        float value_{};
    };

    class StringVisitor final : public IOverrideInterfaceV2::GetVariant
    {
    public:
        void Int(std::int32_t) override {}
        void Float(float) override {}
        void String(const char* value) override { value_ = value ? value : ""; }
        void Bool(bool) override {}
        void TextureSet(const RE::BGSTextureSet*) override {}
        [[nodiscard]] const std::string& Value() const noexcept { return value_; }
    private:
        std::string value_;
    };

    struct Interfaces final
    {
        IPluginInterface* overlay{};
        IPluginInterface* override{};
        bcn::racemenu_compat::OverlayStackAbi abi{
            bcn::racemenu_compat::OverlayStackAbi::unsupported };
        std::uint32_t overlayVersion{};
        std::uint32_t overrideVersion{};
    };

    std::mutex g_interfaceLock;
    Interfaces g_interfaces;
    bool g_interfaceRejected{};

    using PreviewState = bcn::overlay::PreviewState;

    std::mutex g_previewLock;
    std::unordered_map<std::uint64_t, PreviewState> g_previews;

    [[nodiscard]] std::uint64_t PreviewKey(const RE::FormID actorFormID,
        const bcn::overlay::Area area) noexcept
    {
        return (static_cast<std::uint64_t>(actorFormID) << 8U) |
            static_cast<std::uint64_t>(bcn::overlay::Index(area));
    }

    [[nodiscard]] std::optional<PreviewState> PreviewFor(const RE::FormID actorFormID,
        const bcn::overlay::Area area)
    {
        std::scoped_lock lock(g_previewLock);
        const auto found = g_previews.find(PreviewKey(actorFormID, area));
        return found == g_previews.end() ? std::nullopt : std::optional{ found->second };
    }

    void StorePreview(const RE::FormID actorFormID, const bcn::overlay::Area area,
        PreviewState state)
    {
        std::scoped_lock lock(g_previewLock);
        g_previews.insert_or_assign(PreviewKey(actorFormID, area), std::move(state));
    }

    void ErasePreview(const RE::FormID actorFormID, const bcn::overlay::Area area)
    {
        std::scoped_lock lock(g_previewLock);
        g_previews.erase(PreviewKey(actorFormID, area));
    }

    [[nodiscard]] Interfaces InterfacesNow()
    {
        std::scoped_lock lock(g_interfaceLock);
        if (g_interfaces.abi != bcn::racemenu_compat::OverlayStackAbi::unsupported ||
            g_interfaceRejected) return g_interfaces;
        const auto runtimeVersion = REL::Module::get().version();
        const auto branch = bcn::runtime::ResolveGameBranch(runtimeVersion);
        if (branch == bcn::runtime::GameBranch::unsupported) return {};
        auto* overlayBase = static_cast<IPluginInterface*>(bcn::racemenu::QueryInterface("Overlay"));
        auto* overrideBase = static_cast<IPluginInterface*>(bcn::racemenu::QueryInterface("Override"));
        // Interface exchange may run before RaceMenu has published every
        // provider. A missing pointer remains retryable instead of being
        // latched unavailable for the whole game session.
        if (!overlayBase || !overrideBase) return {};
        const auto overlayVersion = overlayBase->GetVersion();
        const auto overrideVersion = overrideBase->GetVersion();
        const auto abi = bcn::racemenu_compat::ResolveOverlayStackAbi(
            overlayVersion, overrideVersion, branch);
        if (abi == bcn::racemenu_compat::OverlayStackAbi::unsupported) {
            g_interfaceRejected = true;
            SKSE::log::error(
                "Body Change NG rejected RaceMenu Overlay v{} / Override v{} on runtime {} ({})",
                overlayVersion, overrideVersion, runtimeVersion.string(),
                bcn::runtime::GameBranchLabel(branch));
            return {};
        }
        g_interfaces = {
            .overlay = overlayBase,
            .override = overrideBase,
            .abi = abi,
            .overlayVersion = overlayVersion,
            .overrideVersion = overrideVersion
        };
        if (bcn::racemenu_compat::UsesOverlayFallback(overlayVersion, overrideVersion)) {
            SKSE::log::warn("Body Change NG Overlay v{} / Override v{} is newer than audited: using the v2 public wrapper prefixes; requires provider backward ABI compatibility",
                overlayVersion, overrideVersion);
        }
        SKSE::log::info("Body Change NG initialized isolated RaceMenu {} adapter on runtime {} ({})",
            bcn::racemenu_compat::OverlayStackAbiLabel(abi), runtimeVersion.string(),
            bcn::runtime::GameBranchLabel(branch));
        return g_interfaces;
    }

    [[nodiscard]] IOverlayInterfaceV2::OverlayLocation LocationV2(
        const bcn::overlay::Area area)
    {
        switch (area) {
        case bcn::overlay::Area::face: return IOverlayInterfaceV2::OverlayLocation::Face;
        case bcn::overlay::Area::hands: return IOverlayInterfaceV2::OverlayLocation::Hand;
        case bcn::overlay::Area::feet: return IOverlayInterfaceV2::OverlayLocation::Feet;
        default: return IOverlayInterfaceV2::OverlayLocation::Body;
        }
    }

    [[nodiscard]] const wchar_t* LegacySection(const bcn::overlay::Area area) noexcept
    {
        switch (area) {
        case bcn::overlay::Area::face: return L"Overlays/Face";
        case bcn::overlay::Area::hands: return L"Overlays/Hands";
        case bcn::overlay::Area::feet: return L"Overlays/Feet";
        default: return L"Overlays/Body";
        }
    }

    std::mutex g_providerCountsLock;
    std::optional<bcn::overlay::ProviderCounts> g_providerCounts;

    [[nodiscard]] bcn::overlay::ProviderCounts LegacyIniCounts()
    {
        const auto directory = std::filesystem::path{ REL::Module::get().filePath() }.parent_path() /
            "Data" / "SKSE" / "Plugins";
        const auto ini = (directory / "skee64.ini").wstring();
        const auto custom = (directory / "skee64_custom.ini").wstring();
        const auto read = [&](const wchar_t* section, const wchar_t* key, std::uint32_t fallback) {
            wchar_t probe[8]{};
            const auto overridden = ::GetPrivateProfileStringW(section, key, L"", probe,
                static_cast<DWORD>(std::size(probe)), custom.c_str()) != 0U;
            return ::GetPrivateProfileIntW(section, key, fallback,
                overridden ? custom.c_str() : ini.c_str());
        };
        bcn::overlay::ProviderCounts result{};
        if (read(L"Features", L"bEnableOverlays", 1U) == 0U) return result;
        for (const auto area : bcn::overlay::kAreas) {
            result[bcn::overlay::Index(area)] = (std::min)(read(LegacySection(area),
                L"iNumOverlays", bcn::overlay::LegacyOverlayDefaultCount(area)), 127U);
        }
        // Only a pre-HUD fallback: the provider snapshot resolves the section
        // move across releases that share interface v1 without guessing a DLL version.
        if (read(L"Features", L"bEnableFaceOverlays", 1U) == 0U ||
            read(L"Overlays", L"bEnableFaceOverlays", 1U) == 0U) result[0] = 0U;
        return result;
    }

    [[nodiscard]] std::uint32_t LegacyOverlayCount(const bcn::overlay::Area area)
    {
        std::scoped_lock lock(g_providerCountsLock);
        if (!g_providerCounts) g_providerCounts = LegacyIniCounts();
        return (*g_providerCounts)[bcn::overlay::Index(area)];
    }

    [[nodiscard]] bool HasOverlays(const Interfaces& interfaces, RE::Actor* actor)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            return static_cast<IOverlayInterfaceV1*>(interfaces.overlay)->HasOverlays(actor);
        }
        return static_cast<IOverlayInterfaceV2*>(interfaces.overlay)->HasOverlays(actor);
    }

    void AddOverlays(const Interfaces& interfaces, RE::Actor* actor)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            static_cast<IOverlayInterfaceV1*>(interfaces.overlay)->AddOverlays(actor);
        } else {
            static_cast<IOverlayInterfaceV2*>(interfaces.overlay)->AddOverlays(actor, true);
        }
    }

    [[nodiscard]] std::uint32_t OverlayCount(const Interfaces& interfaces,
        const bcn::overlay::Area area)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            return LegacyOverlayCount(area);
        }
        return (std::min)(static_cast<IOverlayInterfaceV2*>(interfaces.overlay)->GetOverlayCount(
            IOverlayInterfaceV2::OverlayType::Normal, LocationV2(area)), 254U);
    }

    [[nodiscard]] bcn::appearance::WorkChannel Channel(const bcn::overlay::Area area)
    {
        switch (area) {
        case bcn::overlay::Area::face: return bcn::appearance::WorkChannel::overlayFaceApply;
        case bcn::overlay::Area::hands: return bcn::appearance::WorkChannel::overlayHandsApply;
        case bcn::overlay::Area::feet: return bcn::appearance::WorkChannel::overlayFeetApply;
        default: return bcn::appearance::WorkChannel::overlayBodyApply;
        }
    }

    [[nodiscard]] bool Female(const RE::Actor* actor)
    {
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return base && base->GetSex() == RE::SEX::kFemale;
    }

    [[nodiscard]] bool HasFaceOverlaySource(RE::Actor* actor)
    {
        auto* root = actor ? actor->GetFaceNodeSkinned() : nullptr;
        if (!root) return false;
        bool found{};
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
            const auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
            const auto* material = shader ?
                static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
            const std::string_view name = geometry ? geometry->name.c_str() : "";
            const auto isPaint = name.starts_with("Face [Ovl") || name.starts_with("Face [SOvl");
            if (material && bcn::overlay::IsFaceSourceFeature(
                    static_cast<std::uint32_t>(material->GetFeature()), isPaint)) {
                found = true;
                return RE::BSVisit::BSVisitControl::kStop;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return found;
    }

    [[nodiscard]] std::optional<std::string> NodeName(const Interfaces& interfaces,
        const bcn::overlay::Area area, const std::uint32_t slot)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            return std::format("{} [Ovl{}]", bcn::overlay::LegacyOverlayNodePrefix(area), slot);
        }
        const auto* format = static_cast<IOverlayInterfaceV2*>(interfaces.overlay)->GetOverlayFormat(
            IOverlayInterfaceV2::OverlayType::Normal, LocationV2(area));
        if (!format || format[0] == '\0') return std::nullopt;
        try { return std::vformat(format, std::make_format_args(slot)); }
        catch (...) { return std::nullopt; }
    }

    [[nodiscard]] LegacyOverrideVariant* LegacyNodeOverride(const Interfaces& interfaces,
        RE::Actor* actor, const bool female, const std::string& node,
        const std::uint16_t key, const std::uint8_t index)
    {
        return static_cast<IOverrideInterfaceV1*>(interfaces.override)->GetNodeOverride(
            actor, female, RE::BSFixedString(node), key, index);
    }

    [[nodiscard]] bool HasNodeOverride(const Interfaces& interfaces, RE::Actor* actor,
        const bool female, const std::string& node, const std::uint16_t key,
        const std::uint8_t index)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            return LegacyNodeOverride(interfaces, actor, female, node, key, index) != nullptr;
        }
        return static_cast<IOverrideInterfaceV2*>(interfaces.override)->HasNodeOverride(
            actor, female, node.c_str(), key, index);
    }

    [[nodiscard]] std::optional<std::string> NodeTexture(const Interfaces& interfaces,
        RE::Actor* actor, const bool female, const std::string& node, const std::uint8_t index)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            const auto* value = LegacyNodeOverride(interfaces, actor, female, node,
                bcn::overlay::kTextureKey, index);
            if (!value || value->type != LegacyOverrideVariant::kTypeString ||
                !value->string) return std::nullopt;
            return bcn::overlay::NormalizeTexturePath(value->string->c_str());
        }
        auto* api = static_cast<IOverrideInterfaceV2*>(interfaces.override);
        if (!api->HasNodeOverride(actor, female, node.c_str(),
            bcn::overlay::kTextureKey, index)) return std::nullopt;
        StringVisitor visitor;
        if (!api->GetNodeOverride(actor, female, node.c_str(),
                bcn::overlay::kTextureKey, index, visitor)) return std::nullopt;
        return bcn::overlay::NormalizeTexturePath(visitor.Value());
    }

    [[nodiscard]] bool NodeMatchesPath(const Interfaces& interfaces, RE::Actor* actor,
        const bool female, const std::string& node, const std::string_view packed)
    {
        return bcn::overlay::StoredPathsMatch(packed, [&](const std::uint8_t index) {
            return NodeTexture(interfaces, actor, female, node, index);
        });
    }

    void LogOwnershipConflict(const Interfaces& interfaces, RE::Actor* actor,
        const std::string& node, const std::string_view expected, const char* stage)
    {
        std::string stored;
        for (std::uint8_t index{}; index < 8U; ++index) {
            const auto value = NodeTexture(interfaces, actor, Female(actor), node, index);
            stored += std::format("{}='{}' ", index, value.value_or("<absent>"));
        }
        SKSE::log::warn("BCNG overlay ownership conflict actor={:08X} node='{}' stage={} expected='{}' stored=[{}] tintKey={} alphaKey={}",
            actor->GetFormID(), node, stage, expected, stored,
            HasNodeOverride(interfaces, actor, Female(actor), node, bcn::overlay::kTintKey, bcn::overlay::kScalarIndex),
            HasNodeOverride(interfaces, actor, Female(actor), node, bcn::overlay::kAlphaKey, bcn::overlay::kScalarIndex));

    }

    [[nodiscard]] bool NodeIsFree(const Interfaces& interfaces, RE::Actor* actor,
        const bool female, const std::string& node)
    {
        for (std::uint8_t index{}; index < 8U; ++index) {
            if (HasNodeOverride(interfaces, actor, female, node,
                bcn::overlay::kTextureKey, index)) return false;
        }
        if (HasNodeOverride(interfaces, actor, female, node,
                bcn::overlay::kTintKey, bcn::overlay::kScalarIndex) ||
            HasNodeOverride(interfaces, actor, female, node,
                bcn::overlay::kAlphaKey, bcn::overlay::kScalarIndex)) return false;
        for (const bool firstPerson : { false, true }) {
            auto* root = actor->Get3D(firstPerson);
            auto* object = root ? root->GetObjectByName(RE::BSFixedString(node)) : nullptr;
            auto* geometry = object ? object->AsGeometry() : nullptr;
            auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
            const auto* material = shader ?
                static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
            const auto* diffuse = material && material->diffuseTexture ?
                material->diffuseTexture->name.c_str() : "";
            if (!bcn::overlay::CanClaimNode(false, diffuse)) return false;
        }
        return true;
    }

    [[nodiscard]] bool LiveNodeMatches(RE::Actor* actor, const std::string& node,
        const std::string_view packed)
    {
        auto* root = actor ? actor->Get3D(false) : nullptr;
        auto* object = root ? root->GetObjectByName(RE::BSFixedString(node)) : nullptr;
        auto* geometry = object ? object->AsGeometry() : nullptr;
        auto* shader = geometry ? geometry->lightingShaderProp_cast() : nullptr;
        const auto* material = shader ?
            static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
        if (!material || !material->textureSet) return false; // Alpha zero is a valid user choice.
        const auto layers = bcn::overlay::TextureLayers(packed);
        if (layers.empty()) return false;
        return std::ranges::all_of(layers, [material](const auto& layer) {
            const auto* path = material->textureSet->GetTexturePath(
                static_cast<RE::BSTextureSet::Texture>(layer.index));
            return path && bcn::overlay::TextureIdentity(path) == bcn::overlay::TextureIdentity(layer.path);
        });
    }

    [[nodiscard]] bool NodeOwnedBySelection(const Interfaces& interfaces,
        RE::Actor* actor, const bool female, const std::string& node,
        const std::string_view packed)
    {
        return bcn::overlay::OwnsRegisteredOrLive(
            NodeMatchesPath(interfaces, actor, female, node, packed),
            LiveNodeMatches(actor, node, packed));
    }

    void RemoveNodeOverride(const Interfaces& interfaces, RE::Actor* actor,
        const bool female, const std::string& node, const std::uint16_t key,
        const std::uint8_t index)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            static_cast<IOverrideInterfaceV1*>(interfaces.override)->RemoveNodeOverride(
                actor, female, RE::BSFixedString(node), key, index);
        } else {
            static_cast<IOverrideInterfaceV2*>(interfaces.override)->RemoveNodeOverride(
                actor, female, node.c_str(), key, index);
        }
    }

    void RemoveExactNodeOverrides(const Interfaces& interfaces, RE::Actor* actor,
        const bool female, const std::string& node)
    {
        for (std::uint8_t index{}; index < 8U; ++index) {
            RemoveNodeOverride(interfaces, actor, female, node,
                bcn::overlay::kTextureKey, index);
        }
        RemoveNodeOverride(interfaces, actor, female, node,
            bcn::overlay::kTintKey, bcn::overlay::kScalarIndex);
        RemoveNodeOverride(interfaces, actor, female, node,
            bcn::overlay::kAlphaKey, bcn::overlay::kScalarIndex);
    }

    bool FinalizeNodeProperties(RE::Actor* actor, std::string node, std::string packed,
        std::string previousPath, const std::uint32_t color, std::function<void()> committed)
    {
        // RaceMenu's install/reset tasks run after the current SKSE task.
        // Apply all three stored properties together on the following tick,
        // after those tasks, to the exact paint node in each loaded view.
        const auto handle = actor->GetHandle();
        return bcn::frame_tasks::Continue(
            bcn::frame_tasks::CurrentLease(), [handle, node = std::move(node), packed = std::move(packed),
                previousPath = std::move(previousPath), color, committed = std::move(committed)] {
                const auto current = handle.get();
                if (!current) return;
                const auto interfaces = InterfacesNow();
                if (!interfaces.override) return;
                if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
                    auto* root = current->Get3D(false);
                    const RE::BSFixedString name(node);
                    if (!root || !root->GetObjectByName(name)) return;
                    auto* api = static_cast<IOverrideInterfaceV1*>(interfaces.override);
                    const auto female = Female(current.get());
                    const auto ownsPrevious = !previousPath.empty() &&
                        NodeOwnedBySelection(interfaces, current.get(), female, node, previousPath);
                    const auto ownsIncoming = NodeOwnedBySelection(
                        interfaces, current.get(), female, node, packed);
                    if (!ownsPrevious && !ownsIncoming &&
                        !NodeIsFree(interfaces, current.get(), female, node)) {
                        LogOwnershipConflict(interfaces, current.get(), node, previousPath, "finalize");
                        return;
                    }
                    // Old concrete AddNodeOverride merely copies the variant;
                    // it does NOT intern its string in RaceMenu's save table.
                    // Set the exact live paint, then read it through RaceMenu
                    // to obtain its own interned string before persisting it.
                    std::vector<LegacyOverrideVariant> prepared;
                    for (const auto& layer : bcn::overlay::TextureLayers(packed)) {
                        auto write = LegacyOverrideVariant::String(
                            bcn::overlay::kTextureKey, layer.index, layer.path);
                        api->SetNodeProperty(current.get(), name, &write, true);
                        LegacyOverrideVariant interned;
                        interned.key = bcn::overlay::kTextureKey;
                        interned.index = static_cast<std::int8_t>(layer.index);
                        api->GetNodeProperty(current.get(), false, name, &interned);
                        if (interned.type != LegacyOverrideVariant::kTypeString || !interned.string ||
                            bcn::overlay::TextureIdentity(interned.string->c_str()) !=
                                bcn::overlay::TextureIdentity(layer.path)) {
                            SKSE::log::error("BCNG could not read back RaceMenu paint node '{}' texture {}", node, layer.index);
                            return;
                        }
                        prepared.push_back(std::move(interned));
                    }
                    auto tint = LegacyOverrideVariant::Int(bcn::overlay::kTintKey,
                        bcn::overlay::kScalarIndex, static_cast<std::int32_t>(color & 0xFFFFFFU));
                    auto alpha = LegacyOverrideVariant::Float(bcn::overlay::kAlphaKey,
                        bcn::overlay::kScalarIndex, static_cast<float>(color >> 24U) / 255.0F);
                    // RaceMenu upserts each key. Never clear the old registration
                    // before every replacement string has been prepared.
                    std::size_t nextValue{};
                    bcn::overlay::ReplaceRegisteredPaint(previousPath, packed,
                        [&](const bcn::overlay::TextureLayer&) {
                            api->AddNodeOverride(current.get(), female, name, prepared[nextValue++]);
                        }, [&] {
                            api->AddNodeOverride(current.get(), female, name, tint);
                            api->AddNodeOverride(current.get(), female, name, alpha);
                        }, [&](const std::uint8_t index) {
                            RemoveNodeOverride(interfaces, current.get(), female, node, bcn::overlay::kTextureKey, index);
                        });
                    api->SetNodeProperty(current.get(), name, &tint, true);
                    api->SetNodeProperty(current.get(), name, &alpha, true);
                    committed();
                    return; // v1 setters already process both views; no second texture upload.
                }
                const auto female = Female(current.get());
                const auto ownsPrevious = !previousPath.empty() &&
                    NodeOwnedBySelection(interfaces, current.get(), female, node, previousPath);
                const auto ownsIncoming = NodeOwnedBySelection(
                    interfaces, current.get(), female, node, packed);
                if (!ownsPrevious && !ownsIncoming &&
                    !NodeIsFree(interfaces, current.get(), female, node)) {
                    LogOwnershipConflict(interfaces, current.get(), node, previousPath, "finalize-v2");
                    return;
                }
                auto* api = static_cast<IOverrideInterfaceV2*>(interfaces.override);
                bcn::overlay::ReplaceRegisteredPaint(previousPath, packed,
                    [&](const bcn::overlay::TextureLayer& layer) {
                        StringVariant value(layer.path);
                        api->AddNodeOverride(current.get(), female, node.c_str(),
                            bcn::overlay::kTextureKey, layer.index, value);
                    }, [&] {
                        IntVariant tint(static_cast<std::int32_t>(color & 0xFFFFFFU));
                        FloatVariant alpha(static_cast<float>(color >> 24U) / 255.0F);
                        api->AddNodeOverride(current.get(), female, node.c_str(),
                            bcn::overlay::kTintKey, bcn::overlay::kScalarIndex, tint);
                        api->AddNodeOverride(current.get(), female, node.c_str(),
                            bcn::overlay::kAlphaKey, bcn::overlay::kScalarIndex, alpha);
                    }, [&](const std::uint8_t index) {
                        RemoveNodeOverride(interfaces, current.get(), female, node,
                            bcn::overlay::kTextureKey, index);
                    });
                RE::NiAVObject* last{};
                for (const bool firstPerson : { false, true }) {
                    auto* root = current->Get3D(firstPerson);
                    auto* object = root ? root->GetObjectByName(RE::BSFixedString(node)) : nullptr;
                    if (!object || object == last) continue;
                    static_cast<IOverrideInterfaceV2*>(interfaces.override)->ApplyNodeOverrides(
                        current.get(), object, true);
                    last = object;
                }
                committed();
            }, 1U);
    }

    void RevertExactNode(const Interfaces& interfaces, RE::Actor* actor,
        const bcn::overlay::Area area, const std::string& node, const bool resetDiffuse)
    {
        if (interfaces.abi == bcn::racemenu_compat::OverlayStackAbi::legacyV1) {
            auto* api = static_cast<IOverlayInterfaceV1*>(interfaces.overlay);
            if (area == bcn::overlay::Area::face) {
                api->RevertHeadOverlay(actor, RE::BSFixedString(node),
                    static_cast<std::uint32_t>(RE::BGSHeadPart::HeadPartType::kFace),
                    static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGen),
                    resetDiffuse);
            } else {
                const auto mask = bcn::overlay::BipedMask(area);
                api->RevertOverlay(actor, RE::BSFixedString(node), mask, mask, resetDiffuse);
            }
        } else {
            auto* api = static_cast<IOverlayInterfaceV2*>(interfaces.overlay);
            if (area == bcn::overlay::Area::face) {
                api->RevertHeadOverlay(actor, node.c_str(),
                    static_cast<std::uint32_t>(RE::BGSHeadPart::HeadPartType::kFace),
                    static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGen),
                    resetDiffuse, true);
            } else {
                const auto mask = bcn::overlay::BipedMask(area);
                api->RevertOverlay(actor, node.c_str(), mask, mask, resetDiffuse, true);
            }
        }
    }

    [[nodiscard]] bool RemoveExactOwnedOverlay(const Interfaces& interfaces,
        RE::Actor* actor, const bcn::overlay::Area area, const bcn::OverlayItemState& selected)
    {
        if (!actor || selected.ownedSlot == bcn::overlay::kNoOwnedSlot ||
            selected.texturePath.empty()) return false;
        const auto node = NodeName(interfaces, area, selected.ownedSlot);

        if (!node || !NodeOwnedBySelection(interfaces, actor, Female(actor), *node,
                selected.texturePath)) return false;
        RemoveExactNodeOverrides(interfaces, actor, Female(actor), *node);
        if (actor->Is3DLoaded()) RevertExactNode(interfaces, actor, area, *node, true);
        return true;
    }

    void ClearPendingResetNow(RE::Actor* actor, const bcn::overlay::Area area)
    {
        const auto state = bcn::ActorRegistry::Get().Snapshot(actor);
        if (!state) return;
        const auto& selected = state->overlay.areas[bcn::overlay::Index(area)];
        if (!selected.useDefault || selected.items.empty()) return;
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return;
        for (const auto& item : selected.items) {
            // No matching stored/live path means this is now a foreign slot;
            // relinquish our claim without changing that mod's registration.
            static_cast<void>(RemoveExactOwnedOverlay(interfaces, actor, area, item));
        }
        bcn::ActorRegistry::Get().CompleteOverlayReset(actor, area, selected.resetRevision);
    }

    [[nodiscard]] bcn::overlay::ApplyResult ApplyNow(RE::Actor* actor,
        const bcn::overlay::Area area, const bcn::overlay::Entry& entry,
        const bcn::overlay::ApplyMode mode, std::uint8_t* appliedSlot = nullptr,
        const std::optional<bcn::OverlayItemState>* replacementSource = nullptr,
        const bool force = false, std::function<void()> committed = {},
        const std::optional<std::uint32_t> requestedColor = std::nullopt)
    {
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return bcn::overlay::ApplyResult::unsupportedInterface;
        if (!actor) return bcn::overlay::ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return bcn::overlay::ApplyResult::actor3DUnavailable;
        if (bcn::overlay::TextureLayers(entry.texturePath).empty()) {
            return bcn::overlay::ApplyResult::missingEntry;
        }
        if (area == bcn::overlay::Area::face && !HasFaceOverlaySource(actor)) {
            return bcn::overlay::ApplyResult::unsupportedFace;
        }
        ClearPendingResetNow(actor, area);
        const auto state = bcn::ActorRegistry::Get().Snapshot(actor);
        const auto resetRevision = state ? state->overlay.areas[bcn::overlay::Index(area)].resetRevision : 0U;
        if (!HasOverlays(interfaces, actor)) AddOverlays(interfaces, actor);
        const auto female = Female(actor);
        const auto count = OverlayCount(interfaces, area);
        if (count == 0U) return bcn::overlay::ApplyResult::noFreeSlot;

        const auto previous = replacementSource ? *replacementSource :
            std::optional<bcn::OverlayItemState>{};
        if (!force && previous && (!requestedColor || *requestedColor == previous->color) &&
            previous->selectedId == entry.id &&
            previous->ownedSlot < count) {
            const auto existingNode = NodeName(interfaces, area, previous->ownedSlot);
            if (existingNode && NodeMatchesPath(interfaces, actor, female,
                    *existingNode, entry.texturePath) &&
                LiveNodeMatches(actor, *existingNode, entry.texturePath)) {
                if (appliedSlot) *appliedSlot = previous->ownedSlot;
                const auto accepted = bcn::ActorRegistry::Get().CompleteOverlayApply(actor, area,
                    { entry.id, entry.texturePath, previous->ownedSlot, previous->color }, mode, resetRevision);
                if (accepted && committed) committed();
                return bcn::overlay::ApplyResult::queued;
            }
        }
        std::optional<std::uint8_t> slot;
        if (previous && previous->ownedSlot < count && !previous->texturePath.empty()) {
            if (const auto node = NodeName(interfaces, area, previous->ownedSlot);
                node && (NodeOwnedBySelection(interfaces, actor, female, *node,
                             previous->texturePath) || NodeIsFree(interfaces, actor, female, *node))) {
                slot = previous->ownedSlot;
            }
            if (!slot) {
                if (const auto blockedNode = NodeName(interfaces, area, previous->ownedSlot)) {
                    LogOwnershipConflict(interfaces, actor, *blockedNode, previous->texturePath, "replace");
                }
                return bcn::overlay::ApplyResult::ownershipConflict;
            }
        }
        if (!slot) {
            for (std::uint32_t candidate{}; candidate < count; ++candidate) {
                const auto node = NodeName(interfaces, area, candidate);
                if (node && NodeIsFree(interfaces, actor, female, *node)) {
                    slot = static_cast<std::uint8_t>(candidate);
                    break;
                }
            }
        }
        if (!slot) return bcn::overlay::ApplyResult::noFreeSlot;
        const auto node = NodeName(interfaces, area, *slot);
        if (!node) return bcn::overlay::ApplyResult::unsupportedInterface;
        const auto color = requestedColor.value_or(previous ? previous->color : 0xFFFFFFFFU);

        const auto previousPath = previous && previous->ownedSlot == *slot ?
            previous->texturePath : std::string{};
        const auto obsolete = bcn::overlay::ObsoleteTextureIndices(previousPath, entry.texturePath);
        // A diffuse/color-only replacement needs no RaceMenu reset task. Only
        // removed companion maps require restoring the source material first.
        // Stored ownership remains present throughout that deferred reset.
        if (!obsolete.empty()) RevertExactNode(interfaces, actor, area, *node, true);
        if (appliedSlot) *appliedSlot = *slot;
        const auto handle = actor->GetHandle();
        if (!FinalizeNodeProperties(actor, *node, entry.texturePath, previousPath, color,
            [handle, area, entry, mode, resetRevision, slot = *slot, color, committed = std::move(committed)] {
                const auto current = handle.get();
                if (!current) return;
                const auto accepted = bcn::ActorRegistry::Get().CompleteOverlayApply(current.get(), area,
                    { entry.id, entry.texturePath, slot, color }, mode, resetRevision);
                if (accepted && committed) committed();
            })) return bcn::overlay::ApplyResult::noTaskInterface;
        return bcn::overlay::ApplyResult::queued;
    }

    void RemovePreviewLiveValue(RE::Actor* actor, const bcn::overlay::Area area,
        const PreviewState& preview)
    {
        if (preview.liveDefault || !preview.live) return;
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return;
        static_cast<void>(RemoveExactOwnedOverlay(interfaces, actor, area,
            *preview.live));
    }

    [[nodiscard]] bcn::overlay::ApplyResult RemoveOneNow(RE::Actor* actor,
        const bcn::overlay::Area area, const bcn::OverlayItemState& selected)
    {
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return bcn::overlay::ApplyResult::unsupportedInterface;
        if (!actor) return bcn::overlay::ApplyResult::invalidActor;
        return RemoveExactOwnedOverlay(interfaces, actor, area, selected) ?
            bcn::overlay::ApplyResult::queued : bcn::overlay::ApplyResult::ownershipConflict;
    }

    void RestorePreviewNow(RE::Actor* actor, const bcn::overlay::Area area)
    {
        if (!actor) return;
        const auto preview = PreviewFor(actor->GetFormID(), area);
        if (!preview) return;
        ErasePreview(actor->GetFormID(), area);
        if (!preview->liveDefault) {
            RemovePreviewLiveValue(actor, area, *preview);
            return;
        }
        // Default preview removes every BCNG-owned item in the area. Restore
        // each saved item independently; no foreign RaceMenu slot is touched.
        for (const auto& item : preview->original) {
            const bcn::overlay::Entry original{ .id = item.selectedId,
                .name = item.selectedId, .texturePath = item.texturePath, .area = area };
            const std::optional<bcn::OverlayItemState> replacement{ item };
            const auto restored = ApplyNow(actor, area, original,
                bcn::overlay::ApplyMode::restore, nullptr, &replacement, true);
            if (restored != bcn::overlay::ApplyResult::queued) {
                SKSE::log::warn("Body Change NG could not restore overlay preview actor={:08X} area={} id='{}' result={}",
                    actor->GetFormID(), bcn::overlay::StableName(area), item.selectedId,
                    static_cast<std::uint32_t>(restored));
            }
        }
    }

    [[nodiscard]] bcn::overlay::ApplyResult ClearNow(RE::Actor* actor,
        const bcn::overlay::Area area, const bcn::overlay::ApplyMode mode)
    {
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return bcn::overlay::ApplyResult::unsupportedInterface;
        if (!actor) return bcn::overlay::ApplyResult::invalidActor;
        auto result = bcn::overlay::ApplyResult::queued;
        for (const auto& previous : bcn::ActorRegistry::Get().SelectedOverlays(actor, area)) {
            const auto removed = RemoveOneNow(actor, area, previous);
            if (removed == bcn::overlay::ApplyResult::actor3DUnavailable) return removed;
            if (removed == bcn::overlay::ApplyResult::ownershipConflict) result = removed;
        }
        if (mode == bcn::overlay::ApplyMode::manualCommit) {
            bcn::ActorRegistry::Get().ClearManualOverlays(actor, area);
        }
        return result;
    }
}

namespace bcn::overlay
{
    void UpdateProviderCounts(RE::GFxMovieView* movie)
    {
        if (!movie) return;
        RE::GFxValue enabled;
        if (!movie->GetVariable(&enabled, "_global.skse.plugins.NiOverride.bEnableOverlays") ||
            !enabled.IsBool()) return;
        const auto counts = ReadProviderCounts(enabled.GetBool(),
            [&](const char* path) -> std::optional<double> {
                RE::GFxValue value;
                if (!movie->GetVariable(&value, path) || !value.IsNumber()) return {};
                return value.GetNumber();
            });
        if (counts) {
            std::scoped_lock lock(g_providerCountsLock);
            g_providerCounts = *counts;
        }
    }

    bool IsReady() noexcept
    {
        const auto interfaces = InterfacesNow();
        return interfaces.overlay && interfaces.override;
    }

    std::uint32_t InterfaceVersion() noexcept { return InterfacesNow().overlayVersion; }

    SlotUsage CurrentSlotUsage(RE::Actor* actor, const Area area)
    {
        SlotUsage result;
        if (!actor || area == Area::count || !actor->Is3DLoaded()) return result;
        const auto interfaces = InterfacesNow();
        if (!interfaces.overlay || !interfaces.override) return result;
        const auto total = OverlayCount(interfaces, area);
        const auto female = Female(actor);
        const auto committed = ActorRegistry::Get().SelectedOverlays(actor, area);
        const auto preview = PreviewFor(actor->GetFormID(), area);
        SlotAccounting accounting;
        for (std::uint32_t slot{}; slot < total; ++slot) {
            const auto node = NodeName(interfaces, area, slot);
            if (!node) continue;
            const auto committedOwns = std::ranges::any_of(committed, [&](const auto& item) {
                return item.ownedSlot == slot && NodeOwnedBySelection(interfaces, actor,
                    female, *node, item.texturePath);
            });
            const auto previewOwns = preview && !preview->liveDefault && preview->live &&
                preview->live->ownedSlot == slot && NodeOwnedBySelection(interfaces, actor,
                    female, *node, preview->live->texturePath);
            accounting.Observe(!NodeIsFree(interfaces, actor, female, *node),
                committedOwns, previewOwns);
        }
        result = accounting.Result(total, committed.size());
        return result;
    }

    std::optional<std::string> CurrentSelectionId(const RE::Actor* actor, const Area area)
    {
        const auto selected = ActorRegistry::Get().SelectedOverlays(actor, area);
        return selected.empty() ? std::nullopt : std::optional{ selected.front().selectedId };
    }

    std::vector<std::string> CurrentSelectionIds(const RE::Actor* actor, const Area area)
    {
        const auto selected = ActorRegistry::Get().SelectedOverlays(actor, area);
        std::vector<std::string> result;
        result.reserve(selected.size());
        for (const auto& item : selected) result.push_back(item.selectedId);
        return result;
    }

    bool HasActivePreview(const RE::Actor* actor)
    {
        if (!actor) return false;
        std::scoped_lock lock(g_previewLock);
        for (const auto area : kAreas) {
            if (g_previews.contains(PreviewKey(actor->GetFormID(), area))) return true;
        }
        return false;
    }

    ApplyResult QueueApply(RE::Actor* actor, const Area area, std::string entryId,
        const ApplyMode mode, const std::optional<std::uint32_t> color)
    {
        if (!actor || area == Area::count) return ApplyResult::invalidActor;
        auto entry = Find(entryId);
        if ((!entry || entry->area != area) && mode == ApplyMode::restore) {
            if (const auto selected = ActorRegistry::Get().SelectedOverlay(actor, area, entryId);
                selected && !selected->texturePath.empty()) {
                entry = Entry{ .id = selected->selectedId, .name = selected->selectedId,
                    .texturePath = selected->texturePath, .area = area };
                ResolveInstalledEntryMetadata(*entry);
            }
        }
        if (!entry || entry->area != area) return ApplyResult::missingEntry;
        if (!EntryMatchesActor(entry->layout, entry->sex,
                body_family::ResolveActor(actor), Female(actor))) {
            return ApplyResult::incompatibleActor;
        }
        if (!IsReady()) return ApplyResult::unavailable;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        if (mode == ApplyMode::manualCommit &&
            ActorRegistry::Get().SelectedOverlay(actor, area, entryId)) {
            return QueueRemove(actor, area, std::move(entryId));
        }
        const auto hadPreview = PreviewFor(actor->GetFormID(), area).has_value();
        if (mode == ApplyMode::preview && !hadPreview) {
            PreviewState intent;
            intent.original = ActorRegistry::Get().SelectedOverlays(actor, area);
            StorePreview(actor->GetFormID(), area, std::move(intent));
        }
        const auto handle = actor->GetHandle();
        if (!frame_tasks::Queue(actor->GetFormID(), [handle, area, entry = std::move(*entry), mode, color] {
                const auto current = handle.get();
                if (!current) return;
                auto* actorNow = current.get();
                const auto actorFormID = actorNow->GetFormID();
                auto preview = PreviewFor(actorFormID, area);
                if (mode == ApplyMode::manualCommit && preview &&
                    !preview->liveDefault && preview->live &&
                    preview->live->selectedId == entry.id &&
                    (!color || *color == preview->live->color) &&
                    preview->live->texturePath == entry.texturePath &&
                    preview->live->ownedSlot != kNoOwnedSlot) {
                    const auto interfaces = InterfacesNow();
                    const auto node = NodeName(interfaces, area, preview->live->ownedSlot);
                    if (interfaces.override && node &&
                        NodeOwnedBySelection(interfaces, actorNow, Female(actorNow),
                            *node, preview->live->texturePath)) {
                        ActorRegistry::Get().AddManualOverlay(actorNow, area, entry.id,
                            entry.texturePath, preview->live->ownedSlot, false);
                        ActorRegistry::Get().SetOverlayColor(actorNow, area, entry.id,
                            preview->live->color);
                        ErasePreview(actorFormID, area);
                        return;
                    }
                }
                if (preview && preview->liveDefault) {
                    RestorePreviewNow(actorNow, area);
                    const auto handleAgain = actorNow->GetHandle();
                    [[maybe_unused]] const auto queued = frame_tasks::Continue(
                        frame_tasks::CurrentLease(), [handleAgain, area, id = entry.id, mode, color] {
                            const auto restored = handleAgain.get();
                            if (restored) [[maybe_unused]] const auto result =
                                QueueApply(restored.get(), area, id, mode, color);
                        }, 3U);
                    return;
                }
                if (preview) RemovePreviewLiveValue(actorNow, area, *preview);
                std::optional<OverlayItemState> previous;
                if (mode == ApplyMode::automatic) {
                    const auto saved = ActorRegistry::Get().SelectedOverlays(actorNow, area);
                    if (!saved.empty()) previous = saved.front();
                } else if (mode == ApplyMode::restore) {
                    previous = ActorRegistry::Get().SelectedOverlay(actorNow, area, entry.id);
                }
                const auto appliedSlot = std::make_shared<std::uint8_t>(kNoOwnedSlot);
                const auto original = preview ? preview->original :
                    ActorRegistry::Get().SelectedOverlays(actorNow, area);
                const auto committed = [handle, actorFormID, area, entry, mode, previous, original, appliedSlot, color] {
                    const auto actor = handle.get();
                    if (!actor) return;
                    if (mode == ApplyMode::preview) {
                        const auto slot = *appliedSlot;
                        PreviewState next;
                        next.original = original;
                        next.live = OverlayItemState{ .selectedId = entry.id,
                            .texturePath = entry.texturePath, .ownedSlot = slot,
                            .color = color.value_or(previous ? previous->color : 0xFFFFFFFFU) };
                        StorePreview(actorFormID, area, std::move(next));
                    } else {
                        ErasePreview(actorFormID, area);
                    }
                };
                const auto result = ApplyNow(actorNow, area, entry, mode, appliedSlot.get(), &previous, false, committed, color);
                if (result != ApplyResult::queued) {
                    // No destructive preview removal preceded this attempt.
                    // Keep the previous live ownership on failure.
                    SKSE::log::warn("Body Change NG overlay apply actor={:08X} area={} id='{}' result={}",
                        actorFormID, StableName(area), entry.id,
                        static_cast<std::uint32_t>(result));
                }
            }, 1U, Channel(area))) {
            if (mode == ApplyMode::preview && !hadPreview) ErasePreview(actor->GetFormID(), area);
            return ApplyResult::noTaskInterface;
        }
        return ApplyResult::queued;
    }

    std::optional<std::uint32_t> CurrentColor(const RE::Actor* actor, const Area area,
        const std::string_view entryId)
    {
        if (!actor || area == Area::count || entryId.empty()) return std::nullopt;
        if (const auto preview = PreviewFor(actor->GetFormID(), area);
            preview && preview->live && preview->live->selectedId == entryId) return preview->live->color;
        const auto selected = ActorRegistry::Get().SelectedOverlay(actor, area, entryId);
        return selected ? std::optional{ selected->color } : std::nullopt;
    }

    ApplyResult QueueColor(RE::Actor* actor, const Area area, std::string entryId,
        const std::uint32_t color)
    {
        if (!actor || area == Area::count) return ApplyResult::invalidActor;
        if (!IsReady()) return ApplyResult::unavailable;
        if (!CurrentColor(actor, area, entryId)) return ApplyResult::missingEntry;
        const auto handle = actor->GetHandle();
        if (!frame_tasks::Queue(actor->GetFormID(), [handle, area, entryId = std::move(entryId), color] {
                const auto current = handle.get();
                if (!current) return;
                auto preview = PreviewFor(current->GetFormID(), area);
                const auto coloringPreview = preview && preview->live &&
                    preview->live->selectedId == entryId;
                auto previous = coloringPreview ? preview->live :
                    ActorRegistry::Get().SelectedOverlay(current.get(), area, entryId);
                if (!previous || previous->texturePath.empty()) return;
                const auto interfaces = InterfacesNow();
                const auto node = NodeName(interfaces, area, previous->ownedSlot);

                previous->color = color;
                if (node && NodeOwnedBySelection(interfaces, current.get(), Female(current.get()),
                        *node, previous->texturePath)) {
                    const auto rgb = static_cast<std::int32_t>(color & 0xFFFFFFU);
                    const auto alpha = static_cast<float>(color >> 24U) / 255.0F;
                    if (interfaces.abi == racemenu_compat::OverlayStackAbi::legacyV1) {
                        auto* api = static_cast<IOverrideInterfaceV1*>(interfaces.override);
                        const RE::BSFixedString name(*node);
                        auto tintValue = LegacyOverrideVariant::Int(kTintKey, kScalarIndex, rgb);
                        auto alphaValue = LegacyOverrideVariant::Float(kAlphaKey, kScalarIndex, alpha);
                        api->AddNodeOverride(current.get(), Female(current.get()), name, tintValue);
                        api->AddNodeOverride(current.get(), Female(current.get()), name, alphaValue);
                        api->SetNodeProperty(current.get(), name, &tintValue, true);
                        api->SetNodeProperty(current.get(), name, &alphaValue, true);
                    } else {
                        auto* api = static_cast<IOverrideInterfaceV2*>(interfaces.override);
                        IntVariant tintValue(rgb);
                        FloatVariant alphaValue(alpha);
                        api->AddNodeOverride(current.get(), Female(current.get()), node->c_str(), kTintKey, kScalarIndex, tintValue);
                        api->AddNodeOverride(current.get(), Female(current.get()), node->c_str(), kAlphaKey, kScalarIndex, alphaValue);
                        for (const bool firstPerson : { false, true }) {
                            if (!current->Get3D(firstPerson)) continue;
                            api->SetNodeProperty(current.get(), firstPerson, node->c_str(), kTintKey, kScalarIndex, tintValue, true);
                            api->SetNodeProperty(current.get(), firstPerson, node->c_str(), kAlphaKey, kScalarIndex, alphaValue, true);
                        }
                    }
                    if (coloringPreview) {
                        preview->live->color = color;
                        StorePreview(current->GetFormID(), area, *preview);
                    } else {
                        ActorRegistry::Get().AddManualOverlay(current.get(), area, previous->selectedId,
                            previous->texturePath, previous->ownedSlot, false);
                        ActorRegistry::Get().SetOverlayColor(current.get(), area, entryId, color);
                        ErasePreview(current->GetFormID(), area);
                    }
                    return; // Color dragging does not reload DDS or allocate paint nodes.
                }
                const Entry entry{ .id = previous->selectedId, .name = previous->selectedId,
                    .texturePath = previous->texturePath, .area = area };
                const auto slot = std::make_shared<std::uint8_t>(kNoOwnedSlot);
                const auto mode = coloringPreview ? ApplyMode::preview : ApplyMode::manualCommit;
                const auto result = ApplyNow(current.get(), area, entry,
                    mode, slot.get(), &previous, true,
                    [handle, area, entry, color, preview, coloringPreview, slot] {
                        const auto actor = handle.get();
                        if (!actor) return;
                        if (coloringPreview) {
                            auto next = *preview;
                            next.live = OverlayItemState{ .selectedId = entry.id,
                                .texturePath = entry.texturePath, .ownedSlot = *slot,
                                .color = color };
                            next.liveDefault = false;
                            StorePreview(actor->GetFormID(), area, std::move(next));
                        } else {
                            ErasePreview(actor->GetFormID(), area);
                        }
                    });
                if (result != ApplyResult::queued) {
                    SKSE::log::warn("BCNG overlay color actor={:08X} area={} result={}",
                        current->GetFormID(), StableName(area), static_cast<unsigned>(result));
                }
            }, 1U, Channel(area))) return ApplyResult::noTaskInterface;
        return ApplyResult::queued;
    }

    ApplyResult QueueClear(RE::Actor* actor, const Area area, const ApplyMode mode)
    {
        if (!actor || area == Area::count) return ApplyResult::invalidActor;
        if (!IsReady()) return ApplyResult::unavailable;
        const auto hadPreview = PreviewFor(actor->GetFormID(), area).has_value();
        if (mode == ApplyMode::preview && !hadPreview) {
            PreviewState intent;
            intent.original = ActorRegistry::Get().SelectedOverlays(actor, area);
            StorePreview(actor->GetFormID(), area, std::move(intent));
        }
        const auto handle = actor->GetHandle();
        if (!frame_tasks::Queue(actor->GetFormID(), [handle, area, mode] {
                const auto current = handle.get();
                if (!current) return;
                auto* actorNow = current.get();
                const auto actorFormID = actorNow->GetFormID();
                auto preview = PreviewFor(actorFormID, area);
                const auto alreadyDefault = preview && preview->liveDefault;
                if (mode == ApplyMode::manualCommit && preview && preview->liveDefault) {
                    ActorRegistry::Get().ClearManualOverlays(actorNow, area);
                    ErasePreview(actorFormID, area);
                    return;
                }
                if (preview) {
                    RemovePreviewLiveValue(actorNow, area, *preview);
                    ErasePreview(actorFormID, area);
                }
                const auto original = preview ? preview->original :
                    ActorRegistry::Get().SelectedOverlays(actorNow, area);
                // Paint previews are additive: removing only their live node
                // leaves committed layers intact. Default must clear those too.
                const auto result = alreadyDefault && mode == ApplyMode::preview ?
                    ApplyResult::queued : ClearNow(actorNow, area, mode);
                if (result == ApplyResult::queued && mode == ApplyMode::preview) {
                    PreviewState next;
                    next.original = std::move(original);
                    next.liveDefault = true;
                    StorePreview(actorFormID, area, std::move(next));
                }
                if (result != ApplyResult::queued && result != ApplyResult::ownershipConflict) {
                    SKSE::log::warn("Body Change NG overlay clear actor={:08X} area={} result={}",
                        actorFormID, StableName(area), static_cast<std::uint32_t>(result));
                }
            }, 1U, Channel(area))) {
            if (mode == ApplyMode::preview && !hadPreview) ErasePreview(actor->GetFormID(), area);
            return ApplyResult::noTaskInterface;
        }
        return ApplyResult::queued;
    }

    ApplyResult QueueReset(RE::Actor* actor, const Area area)
    {
        if (!actor || area == Area::count) return ApplyResult::invalidActor;
        if (!IsReady()) return ApplyResult::unavailable;
        const auto handle = actor->GetHandle();
        if (!frame_tasks::Queue(actor->GetFormID(), [handle, area] {
                const auto current = handle.get();
                if (!current) return;
                if (const auto preview = PreviewFor(current->GetFormID(), area)) {
                    RemovePreviewLiveValue(current.get(), area, *preview);
                    ErasePreview(current->GetFormID(), area);
                }
                ClearPendingResetNow(current.get(), area);
            }, 1U, Channel(area))) return ApplyResult::noTaskInterface;
        return ApplyResult::queued;
    }

    ApplyResult QueueRemove(RE::Actor* actor, const Area area, std::string entryId)
    {
        if (!actor || area == Area::count || entryId.empty()) return ApplyResult::invalidActor;
        const auto selected = ActorRegistry::Get().SelectedOverlay(actor, area, entryId);
        if (!selected) return ApplyResult::missingEntry;
        if (!IsReady()) return ApplyResult::unavailable;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        const auto handle = actor->GetHandle();
        if (!frame_tasks::Queue(actor->GetFormID(), [handle, area, entryId = std::move(entryId), selected = *selected] {
                const auto current = handle.get();
                if (!current) return;
                if (const auto preview = PreviewFor(current->GetFormID(), area)) {
                    if (preview->liveDefault) {
                        ErasePreview(current->GetFormID(), area);
                        for (const auto& original : preview->original) {
                            if (original.selectedId == entryId) continue;
                            const Entry restore{ .id = original.selectedId, .name = original.selectedId,
                                .texturePath = original.texturePath, .area = area };
                            const std::optional<OverlayItemState> replacement{ original };
                            [[maybe_unused]] const auto restored = ApplyNow(current.get(), area,
                                restore, ApplyMode::restore, nullptr, &replacement, true);
                        }
                        ActorRegistry::Get().RemoveManualOverlay(current.get(), area, entryId);
                        return;
                    }
                    RemovePreviewLiveValue(current.get(), area, *preview);
                    ErasePreview(current->GetFormID(), area);
                }
                const auto result = RemoveOneNow(current.get(), area, selected);
                if (result == ApplyResult::queued) {
                    ActorRegistry::Get().RemoveManualOverlay(current.get(), area, entryId);
                } else {
                    SKSE::log::warn("BCNG overlay remove actor={:08X} area={} id='{}' result={}",
                        current->GetFormID(), StableName(area), entryId, static_cast<unsigned>(result));
                }
            }, 1U, Channel(area))) return ApplyResult::noTaskInterface;
        return ApplyResult::queued;
    }

    void DiscardPreviewsForReset(RE::Actor* actor)
    {
        if (!actor) return;
        const auto state = ActorRegistry::Get().Snapshot(actor);
        if (!state) return;
        for (const auto area : kAreas) {
            const auto& selected = state->overlay.areas[Index(area)];
            if (!selected.useDefault) continue;
            if (const auto preview = PreviewFor(actor->GetFormID(), area); preview && preview->live) {
                [[maybe_unused]] const auto accepted = ActorRegistry::Get().CompleteOverlayApply(
                    actor, area, *preview->live, ApplyMode::preview, selected.resetRevision - 1U);
            }
            ErasePreview(actor->GetFormID(), area);
        }
    }

    void QueueCancelPreviews(RE::Actor* actor)
    {
        const auto queueOne = [](RE::Actor* target, const Area area) {
            if (!target || !PreviewFor(target->GetFormID(), area)) return;
            const auto handle = target->GetHandle();
            [[maybe_unused]] const auto queued = frame_tasks::Queue(target->GetFormID(),
                [handle, area] {
                    const auto current = handle.get();
                    if (current) RestorePreviewNow(current.get(), area);
                }, 1U, Channel(area));
        };
        if (actor) {
            for (const auto area : kAreas) queueOne(actor, area);
            return;
        }
        std::vector<RE::FormID> actors;
        {
            std::scoped_lock lock(g_previewLock);
            actors.reserve(g_previews.size());
            for (const auto& [key, unused] : g_previews) {
                static_cast<void>(unused);
                actors.push_back(static_cast<RE::FormID>(key >> 8U));
            }
        }
        std::ranges::sort(actors);
        actors.erase(std::unique(actors.begin(), actors.end()), actors.end());
        for (const auto actorFormID : actors) {
            if (auto* target = RE::TESForm::LookupByID<RE::Actor>(actorFormID)) {
                for (const auto area : kAreas) queueOne(target, area);
            }
        }
    }

    void QueueReapplySaved(RE::Actor* actor)
    {
        if (!actor || HasActivePreview(actor)) return;
        const auto state = ActorRegistry::Get().Snapshot(actor);
        if (!state) return;
        for (const auto area : kAreas) {
            // Restore is lower priority than an accepted user choice or a new
            // distribution result already queued for this anatomical area.
            if (frame_tasks::HasActorChannelWork(actor->GetFormID(), Channel(area))) continue;
            const auto& selected = state->overlay.areas[Index(area)];
            if (selected.useDefault) {
                if (!selected.items.empty()) [[maybe_unused]] const auto reset = QueueReset(actor, area);
                continue;
            }
            if (selected.items.empty()) continue;
            const auto handle = actor->GetHandle();
            const auto items = selected.items;
            [[maybe_unused]] const auto queued = frame_tasks::Queue(actor->GetFormID(),
                [handle, area, items] {
                    const auto current = handle.get();
                    if (!current) return;
                    for (const auto& item : items) {
                        if (item.selectedId.empty() || item.texturePath.empty()) continue;
                        Entry entry{ .id = item.selectedId, .name = item.selectedId,
                            .texturePath = item.texturePath, .area = area };
                        ResolveInstalledEntryMetadata(entry);
                        const std::optional<OverlayItemState> replacement{ item };
                        [[maybe_unused]] const auto result = ApplyNow(current.get(), area,
                            entry, ApplyMode::restore, nullptr, &replacement, true);
                    }
                }, 1U, Channel(area));
        }
    }

    void ForgetActorState(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0U) return;
        std::vector<std::pair<Area, PreviewState>> abandoned;
        {
            std::scoped_lock lock(g_previewLock);
            for (const auto area : kAreas) {
                const auto found = g_previews.find(PreviewKey(actorFormID, area));
                if (found == g_previews.end()) continue;
                abandoned.emplace_back(area, std::move(found->second));
                g_previews.erase(found);
            }
        }
        if (abandoned.empty()) return;
        // Detach has cancelled the actor lease. Use a session-scoped cleanup
        // job, with value-only ownership, so preview keys are not abandoned.
        [[maybe_unused]] const auto queued = frame_tasks::Queue(0U,
            [actorFormID, abandoned = std::move(abandoned)] {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
                if (!actor || !ActorRegistry::Get().Snapshot(actor)) return;
                for (const auto& [area, preview] : abandoned) {
                    if (!preview.live) continue;
                    const auto committed = ActorRegistry::Get().SelectedOverlay(
                        actor, area, preview.live->selectedId);
                    if (committed && committed->ownedSlot == preview.live->ownedSlot &&
                        committed->texturePath == preview.live->texturePath) continue;
                    RemovePreviewLiveValue(actor, area, preview);
                }
                // If it has already reattached, restore Default-preview originals.
                // A newer accepted request wins over this restore.
                if (actor->Is3DLoaded()) QueueReapplySaved(actor);
            });
    }

    void ResetSessionState()
    {
        ResetCatalogSessionState();
        {
            std::scoped_lock lock(g_previewLock);
            g_previews.clear();
        }
        {
            std::scoped_lock lock(g_interfaceLock);
            g_interfaces = {};
            g_interfaceRejected = false;
        }
    }
}
