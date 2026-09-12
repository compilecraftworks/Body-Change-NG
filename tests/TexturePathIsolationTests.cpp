#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
    [[nodiscard]] std::filesystem::path RepositoryRoot()
    {
        auto source = std::filesystem::path{ __FILE__ };
        if (source.is_relative()) source = std::filesystem::current_path() / source;
        return source.lexically_normal().parent_path().parent_path();
    }

    [[nodiscard]] std::string Read(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return { std::istreambuf_iterator<char>{ stream },
            std::istreambuf_iterator<char>{} };
    }
}

int main()
{
    const auto root = RepositoryRoot();
    constexpr std::array<std::string_view, 11> forbidden{
        "AddNodeOverride",
        "RemoveNodeOverride",
        "GetNodeOverride",
        "SetNodeProperty",
        "AddSkinOverride",
        "RemoveSkinOverride",
        "GetSkinOverride",
        "AddArmorOverride",
        "RemoveArmorOverride",
        "GetArmorOverride",
        "IOverrideInterface"
    };

    bool clean = true;
    const auto sourceRoot = root / "src" / "BodyChangeNG";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceRoot)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".h") continue;
        const auto& path = entry.path();
        const auto relative = std::filesystem::relative(path, root).generic_string();
        const auto contents = Read(path);
        if (contents.empty()) {
            std::cerr << "FAILED: could not inspect " << path.string() << '\n';
            clean = false;
            continue;
        }
        // Two isolated adapters: paint nodes and the user's face-only skin
        // replacement. Native body and genital paths must not regain these APIs.
        if (relative == "src/BodyChangeNG/RaceMenuOverlay.cpp") continue;
        if (relative == "src/BodyChangeNG/RaceMenuOverrideABI.h" ||
            relative == "src/BodyChangeNG/FaceSkinNodeAccess.cpp") continue;
        for (const auto token : forbidden) {
            if (relative == "src/BodyChangeNG/FaceSkinOverrides.cpp" &&
                (token == "AddNodeOverride" || token == "RemoveNodeOverride" || token == "GetNodeOverride")) continue;
            if (contents.find(token) == std::string::npos) continue;
            std::cerr << "FAILED: forbidden texture-override API token '" << token
                      << "' in " << relative << '\n';
            clean = false;
        }
    }

    constexpr std::array removedCompatibilityFiles{
        "src/BodyChangeNG/SkinOverrides.cpp",
        "src/BodyChangeNG/SkinOverrides.h",
        "src/BodyChangeNG/SkinOverrideBackend.cpp",
        "src/BodyChangeNG/SkinOverrideBackend.h",
        "src/BodyChangeNG/RaceMenuOverrideRouting.h",
        "src/BodyChangeNG/NativeFaceAttachment.cpp",
        "src/BodyChangeNG/NativeFaceAttachment.h",
        "src/BodyChangeNG/NativeFaceAttachmentPolicy.h",
        "src/BodyChangeNG/NativeFaceRefresh.h",
        "src/BodyChangeNG/NativeAppearanceRuntime.h",
        "src/BodyChangeNG/NativeFaceCallPattern.h",
        "src/BodyChangeNG/NativeFunctionBoundary.h",
        "src/BodyChangeNG/LiveAddonSkinBackend.cpp",
        "src/BodyChangeNG/LiveAddonSkinBackend.h",
        "src/BodyChangeNG/AddonLiveMaterialPolicy.h"
    };
    for (const auto* relative : removedCompatibilityFiles) {
        if (!std::filesystem::exists(root / relative)) continue;
        std::cerr << "FAILED: obsolete compatibility file returned: " << relative << '\n';
        clean = false;
    }

    constexpr std::array isolatedOverlayFiles{
        "src/BodyChangeNG/OverlayCatalog.cpp",
        "src/BodyChangeNG/OverlayPolicy.h",
        "src/BodyChangeNG/OverlayTypes.h",
        "src/BodyChangeNG/RaceMenuOverlay.cpp",
        "src/BodyChangeNG/RaceMenuOverlay.h"
    };
    for (const auto* relative : isolatedOverlayFiles) {
        if (std::filesystem::exists(root / relative)) continue;
        std::cerr << "FAILED: isolated RaceMenu overlay adapter is missing: " << relative << '\n';
        clean = false;
    }

    const auto overlayCatalog = Read(sourceRoot / "OverlayCatalog.cpp");
    const auto overlayAdapter = Read(sourceRoot / "RaceMenuOverlay.cpp");
    const auto overrideABI = Read(sourceRoot / "RaceMenuOverrideABI.h");
    const auto pluginMain = Read(root / "src" / "main.cpp");
    if (overlayCatalog.find("kMenuName{ \"HUD Menu\" }") == std::string::npos ||
        overlayCatalog.find("RequestWhenHudReady") == std::string::npos ||
        pluginMain.find("kPostLoadGame") == std::string::npos ||
        pluginMain.find("kNewGame") == std::string::npos ||
        pluginMain.find("overlay::RequestCatalog(player)") == std::string::npos) {
        std::cerr << "FAILED: overlay catalog again depends on opening the RaceMenu editor UI\n";
        clean = false;
    }
    if (overlayAdapter.find("class IOverlayInterfaceV1") == std::string::npos ||
        overlayAdapter.find("class IOverlayInterfaceV2") == std::string::npos ||
        overrideABI.find("class IOverrideInterfaceV1") == std::string::npos ||
        overrideABI.find("class IOverrideInterfaceV2") == std::string::npos ||
        overlayAdapter.find("ResolveOverlayStackAbi") == std::string::npos ||
        overlayAdapter.find("LegacyOverlayCount") == std::string::npos ||
        overlayAdapter.find("g_interfaceAttempted") != std::string::npos) {
        std::cerr << "FAILED: RaceMenu 0.4.16 v1 and public v2 overlay ABIs are no longer isolated\n";
        clean = false;
    }

    const auto nativeSkinBackend = Read(sourceRoot / "NativeSkinBackend.cpp");
    const auto faceAdapter = Read(sourceRoot / "FaceSkinOverrides.cpp");
    const auto facePolicy = Read(sourceRoot / "FaceSkinPolicy.h");
    const auto faceNode = Read(sourceRoot / "FaceSkinNodeAccess.cpp");
    if (faceNode.find("QueryInterface(\"Override\")") == std::string::npos ||
        faceNode.find("ResolveNodeOverrideAbi(") == std::string::npos ||
        faceNode.find("api->AddNodeOverride(actor, female, name, interned)") == std::string::npos ||
        faceNode.find("QueryInterface(\"Overlay\")") != std::string::npos ||
        faceNode.find("ApplyNodeOverrides(") != std::string::npos ||
        faceAdapter.find("batch->nodeAccess = NodeAccess::Connect()") == std::string::npos) {
        std::cerr << "FAILED: versioned face-only immediate adapter / legacy string registration regressed\n";
        clean = false;
    }
    const auto emptyGuard = faceAdapter.find("if (path.empty())", faceAdapter.find("void Write("));
    const auto faceWrite = faceAdapter.find("Call(\"AddNodeOverrideString\"");
    if (emptyGuard == std::string::npos || faceWrite == std::string::npos || emptyGuard > faceWrite ||
        faceAdapter.find("visible = CaptureVisiblePath(visible,") == std::string::npos ||
        faceAdapter.find("visible = OriginalDetailPath(") == std::string::npos ||
        faceAdapter.find("SetMaterial(") != std::string::npos ||
        faceAdapter.find("SetTextureSet(") != std::string::npos) {
        std::cerr << "FAILED: actual face baseline / empty-load protection / read-only material access regressed\n";
        clean = false;
    }
    if (faceAdapter.find("GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace)") == std::string::npos ||
        faceAdapter.find("DispatchStaticCall(\"NiOverride\"") == std::string::npos ||
        facePolicy.find("kChannels{ 0, 1, 2, 3, 7 }") == std::string::npos ||
        faceAdapter.find("QueueNiNodeUpdate(") != std::string::npos ||
        nativeSkinBackend.find("SetFaceTexture(") != std::string::npos ||
        nativeSkinBackend.find("face_skin::Apply(") == std::string::npos) {
        std::cerr << "FAILED: face-only public API / tint exclusion / native isolation regressed\n";
        clean = false;
    }
    const auto skinApplication = Read(sourceRoot / "SkinApplication.cpp");
    const auto nativeAddon = Read(sourceRoot / "NativeAddonSkinBackend.cpp");
    if (nativeAddon.find("AdoptFactoryTexture<") == std::string::npos ||
        nativeAddon.find("ScopedSupply supplied(") == std::string::npos ||
        nativeAddon.find("g_baselines.Put(identity, paths)") == std::string::npos ||
        nativeAddon.find("NiStringsExtraData") != std::string::npos ||
        nativeAddon.find("GetFormID() != 0U") != std::string::npos ||
        nativeAddon.find("unexpected FormID") != std::string::npos ||
        nativeAddon.find("SetMaterial(") != std::string::npos ||
        nativeAddon.find("BSShaderTextureSet::Create(") != std::string::npos ||
        nativeAddon.find("SetTextureSet(") != std::string::npos ||
        nativeAddon.find("EquipItem(") != std::string::npos ||
        nativeAddon.find("UnequipItem(") != std::string::npos ||
        nativeAddon.find("DoReset3D(") != std::string::npos ||
        nativeAddon.find("QueueNiNodeUpdate(") != std::string::npos ||
        nativeAddon.find("delete static_cast<RE::TESForm*>") != std::string::npos ||
        skinApplication.find("native_addon::Reset()") == std::string::npos ||
        skinApplication.find("native_addon::Forget(actorFormID)") == std::string::npos ||
        pluginMain.find("native_addon::Install()") == std::string::npos) {
        std::cerr << "FAILED: genital native TXST ownership/cleanup/isolation regressed\n";
        clean = false;
    }
    const auto bodyMorphAdapter = Read(sourceRoot / "RaceMenuBodyMorph.cpp");
    if (nativeSkinBackend.find("PathsFromCache(cached)") == std::string::npos ||
        nativeSkinBackend.find("BSResourceNiBinaryStream stream(paths->resource)") == std::string::npos ||
        nativeSkinBackend.find("WriteTexturePath(binding, layer.shaderTextureIndex, paths->native)") == std::string::npos ||
        nativeSkinBackend.find("WriteTexturePath(binding, layer.shaderTextureIndex, cached)") != std::string::npos) {
        std::cerr << "FAILED: native TXST path domain is no longer separated from DDS resource preflight\n";
        clean = false;
    }
    const auto canonicalGuard = pluginMain.find("if (!IsCanonicalPluginModule()) return false;");
    const auto skseInit = pluginMain.find("SKSE::Init(skse);");
    if (canonicalGuard == std::string::npos || skseInit == std::string::npos ||
        canonicalGuard > skseInit ||
        pluginMain.find("L\"BodyChangeNG.dll\"") == std::string::npos) {
        std::cerr << "FAILED: non-canonical backup DLLs can initialize beside BodyChangeNG.dll\n";
        clean = false;
    }
    if (nativeSkinBackend.find("DoReset3D(") != std::string::npos ||
        nativeSkinBackend.find("QueueNiNodeUpdate(") != std::string::npos) {
        std::cerr << "FAILED: native TXST backend regained a second actor-3D refresh route\n";
        clean = false;
    }
    if (skinApplication.find("QueueNiNodeUpdate(*vm, actor.get())") == std::string::npos ||
        skinApplication.find("face_skin::QueueRebuild(actor, [handle]") == std::string::npos) {
        std::cerr << "FAILED: the single facade-owned native skin refresh route is missing\n";
        clean = false;
    }
    const auto actorEvents = Read(sourceRoot / "ActorEvents.cpp");
    if (nativeSkinBackend.find("static_cast<bool>(afterMutation) && actor->Is3DLoaded()") == std::string::npos ||
        faceAdapter.find("request.rebuild.CanApply(") == std::string::npos ||
        faceAdapter.find("request.rebuild.Begin(") == std::string::npos ||
        actorEvents.find("face_skin::OnNiNodeUpdate(actor)") == std::string::npos ||
        actorEvents.find("face_skin::Reapply(actor)") != std::string::npos) {
        std::cerr << "FAILED: face selection/rebuild ordering barrier or event invalidation is missing\n";
        clean = false;
    }
    const auto mapStore = bodyMorphAdapter.find("g_interfaceMap.store(interfaceMap");
    const auto morphQuery = bodyMorphAdapter.find("QueryInterface(\"BodyMorph\")");
    if (mapStore == std::string::npos || morphQuery == std::string::npos || mapStore > morphQuery) {
        std::cerr << "FAILED: Overlay availability is coupled to BodyMorph initialization again\n";
        clean = false;
    }

    if (!clean) return 1;
    std::cout << "Texture path isolation tests passed\n";
    return 0;
}
