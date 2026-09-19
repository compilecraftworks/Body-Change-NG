set_xmakever("3.1.0")

local version = "1.2.9"
set_project("BodyChangeNG")
set_version(version)
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")
set_policy("package.requires_lock", true)
set_config("skse_xbyak", true)
set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- CommonLibSSE-NG is an exact, independently vendored upstream stable tag.
includes("third_party/CommonLibSSE-NG")
add_requires("nlohmann_json v3.12.0")

target("BodyChangeNG")
    set_kind("shared")
    set_basename("BodyChangeNG")
    set_targetdir("build/v" .. version .. "/windows/x64/$(mode)")
    set_encodings("utf-8")
    -- CommonLib's plugin rule emits a Windows .rc file.  Keep the resource
    -- compiler inherited from XMake's detected MSVC/Windows SDK toolchain so
    -- the matching SDK include environment (notably winres.h) is preserved.

    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")
    add_rules("commonlibsse-ng.plugin", {
        name = "Body Change NG",
        author = "compilecraftworks",
        description = "Standalone BodySlide body and skin manager"
    })

    add_defines('BODY_CHANGE_NG_VERSION="' .. version .. '"')
    add_defines("BODY_CHANGE_NG_RUNTIME")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_files(
        "third_party/imgui/imgui.cpp",
        "third_party/imgui/imgui_draw.cpp",
        "third_party/imgui/imgui_tables.cpp",
        "third_party/imgui/imgui_widgets.cpp",
        "third_party/imgui/misc/cpp/imgui_stdlib.cpp",
        "third_party/imgui/backends/imgui_impl_dx11.cpp",
        "third_party/imgui/backends/imgui_impl_win32.cpp",
        "third_party/pugixml/src/pugixml.cpp"
    )
    add_includedirs("src", "third_party/imgui", "third_party/imgui/backends", "third_party/pugixml/src")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "windowscodecs", "ole32", "user32")
    set_pcxxheader("src/PCH.h")

target("BodyChangeNGRemovalPreparationTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/RemovalPreparationTests.cpp")
    add_includedirs("src")

target("BodyChangeNGDistributionTargetReadTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_includedirs("src")
    add_files("tests/DistributionTargetReadTests.cpp")

target("BodyChangeNGDistributionPersistenceTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_packages("nlohmann_json")
    add_includedirs("src")
    add_files("tests/DistributionPersistenceTests.cpp")
    on_load(function (target)
        local source = io.readfile("src/BodyChangeNG/Distribution.cpp")
        local start = assert(source:find("    [[nodiscard]] bool WriteDistributionFile(", 1, true))
        local finish = assert(source:find("\n    }", start, true)) + #"\n    }" - 1
        local schema = assert(source:match("constexpr auto kSchemaVersion = %d+;"))
        local generated = path.join(target:autogendir(), "distribution-persistence")
        os.mkdir(generated)
        local file = path.join(generated, "DistributionWriter.inl")
        local contents = schema .. "\n" .. source:sub(start, finish)
        if not os.isfile(file) or io.readfile(file) ~= contents then io.writefile(file, contents) end
        target:add("includedirs", generated)
    end)

target("BodyChangeNGCatalogRefreshTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/CatalogRefreshTests.cpp")
    add_includedirs("src")

target("BodyChangeNGDistributionLifecycleTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/DistributionLifecycleTests.cpp")
    add_includedirs("src")
    on_load(function (target)
        local generated = path.join(target:autogendir(), "distribution-lifecycle")
        target:add("includedirs", generated)
        local source = io.readfile("src/BodyChangeNG/UI.cpp")
        local functions = {}
        for _, name in ipairs({"ResetDistributionEditor", "ClearDistributionCatalogSelection",
                               "ResetDistributionSelectionSession", "DiscardDistributionDraft"}) do
            local start = assert(source:find("    void " .. name .. "()", 1, true))
            local finish = assert(source:find("\n    }", start, true)) + #"\n    }" - 1
            table.insert(functions, source:sub(start, finish))
        end
        os.mkdir(generated)
        local file = path.join(generated, "DistributionLifecycleFunctions.inl")
        local content = table.concat(functions, "\n\n") .. "\n"
        if not os.isfile(file) or io.readfile(file) ~= content then io.writefile(file, content) end
        local queue = io.readfile("src/BodyChangeNG/FrameTasks.cpp")
        local start = assert(queue:find("    bool CurrentWorkAllowed()", 1, true))
        local finish = assert(queue:find("\n    }", start, true)) + #"\n    }" - 1
        file = path.join(generated, "MutationCheckpoint.inl")
        content = queue:sub(start, finish) .. "\n"
        if not os.isfile(file) or io.readfile(file) ~= content then io.writefile(file, content) end
    end)

target("BodyChangeNGPopupPlacementTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_includedirs("src", "third_party/imgui")
    add_files("tests/PopupPlacementTests.cpp", "third_party/imgui/imgui.cpp",
        "third_party/imgui/imgui_draw.cpp", "third_party/imgui/imgui_tables.cpp",
        "third_party/imgui/imgui_widgets.cpp")

target("BodyChangeNGNativeAddonPolicyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/NativeAddonPolicyTests.cpp")
    add_includedirs("src")

target("BodyChangeNGHotkeyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/HotkeyTests.cpp", "src/BodyChangeNG/Hotkey.cpp")
    add_includedirs("src")

target("BodyChangeNGCatalogPerformanceProbe")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    add_includedirs("src", "third_party/imgui")
    add_files("tests/CatalogPerformanceProbe.cpp", "third_party/imgui/imgui.cpp",
        "third_party/imgui/imgui_draw.cpp", "third_party/imgui/imgui_tables.cpp",
        "third_party/imgui/imgui_widgets.cpp")

target("BodyChangeNGMouseInputReplayTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    add_includedirs("src", "third_party/imgui")
    add_files("tests/MouseInputReplayTests.cpp", "third_party/imgui/imgui.cpp",
        "third_party/imgui/imgui_draw.cpp", "third_party/imgui/imgui_tables.cpp",
        "third_party/imgui/imgui_widgets.cpp")

target("BodyChangeNGPresetCatalogTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/PresetCatalogTests.cpp", "src/BodyChangeNG/PresetCatalog.cpp", "src/BodyChangeNG/CatalogRoots.cpp",
        "src/BodyChangeNG/BodyFamilyRules.cpp", "third_party/pugixml/src/pugixml.cpp")
    add_includedirs("src", "third_party/pugixml/src")

target("BodyChangeNGUbeMorphTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/UbeMorphTests.cpp", "src/BodyChangeNG/PresetCatalog.cpp", "src/BodyChangeNG/CatalogRoots.cpp",
        "src/BodyChangeNG/BodyFamilyRules.cpp", "third_party/pugixml/src/pugixml.cpp")
    add_includedirs("src", "third_party/pugixml/src")

target("BodyChangeNGAssetCatalogTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")
    add_files(
        "tests/AssetCatalogTests.cpp",
        "src/BodyChangeNG/BodyFamily.cpp",
        "src/BodyChangeNG/BodyFamilyRules.cpp",
        "src/BodyChangeNG/CatalogRoots.cpp",
        "src/BodyChangeNG/Hotkey.cpp",
        "src/BodyChangeNG/PlayerTint.cpp",
        "src/BodyChangeNG/FrameTasks.cpp",
        "src/BodyChangeNG/RuntimeAssetCache.cpp",
        "src/BodyChangeNG/Settings.cpp",
        "src/BodyChangeNG/SlaveTatsCatalog.cpp",
        "src/BodyChangeNG/SkinProfiles.cpp",
        "src/BodyChangeNG/SkinApplicationPlan.cpp"
    )
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

target("BodyChangeNGSkinArchitectureTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/SkinArchitectureTests.cpp", "src/BodyChangeNG/SkinApplicationPlan.cpp")
    add_includedirs("src")

target("BodyChangeNGFaceSkinPolicyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/FaceSkinPolicyTests.cpp")
    add_includedirs("src")

for _, probeName in ipairs({"BodyChangeNGFailurePathTests", "BodyChangeNGOfflineMemoryProbe"}) do
target(probeName)
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    add_files("tests/FailurePathTests.cpp")
    add_includedirs("src")
    if probeName == "BodyChangeNGOfflineMemoryProbe" then
        add_defines("BCNG_OFFLINE_MEMORY_PROBE")
        set_targetdir("build/v" .. version .. "/trials/offline-memory")
    end
    on_load(function (target)
        -- Compile exact production function bodies with fake VM/allocator
        -- dependencies. Generated includes are diagnostic build artifacts.
        local generated = path.join(target:autogendir(), "failure-paths")
        target:add("includedirs", generated)
        local function extract(file, first, last, output)
            local source = io.readfile(file)
            local begin = assert(source:find(first, 1, true))
            local finish = assert(source:find(last, begin + #first, true))
            io.writefile(path.join(generated, output), source:sub(begin, finish - 1))
        end
        extract("src/BodyChangeNG/FaceSkinOverrides.cpp", "    class Callback final", "    struct Batch :", "face_callback.inc")
        extract("src/BodyChangeNG/FaceSkinOverrides.cpp", "        template<class... Args>", "        void Read(bool saved", "face_call.inc")
        extract("src/BodyChangeNG/FaceSkinOverrides.cpp", "        void Finish(bool success)", "        void RollbackChannel()", "face_finish.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    template <class T>", "    [[nodiscard]] RE::BGSTextureSet* CurrentFaceTexture", "duplicate_form.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    [[nodiscard]] std::optional<TextureBinding> CloneTexture(", "    [[nodiscard]] std::string NativeFormPath", "clone_texture.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    [[nodiscard]] std::optional<TextureBinding> CreateModelTexture(", "    [[nodiscard]] bool SetModelAlternateTextures", "model_texture.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    struct PendingModelTexture final", "    struct ModelArrayHeap", "pending_model.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    [[nodiscard]] bool SetModelAlternateTextures", "    [[nodiscard]] bool MaterializeEmbeddedSkinAtlases", "model_array.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    bool RestoreOwnedPointers(BaseInstance& instance)", "    std::shared_ptr<AppliedSnapshot> CaptureApplied", "restore_owned.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    [[nodiscard]] std::optional<BaseInstance> BuildInstance(", "    [[nodiscard]] bool RequestStillCurrent(", "build_instance.inc")
    end)
end

target("BodyChangeNGPreviewRestoreTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/PreviewRestoreTests.cpp")
    add_includedirs("src")
    on_load(function (target)
        local generated = path.join(target:autogendir(), "preview-restore")
        target:add("includedirs", generated)
        local function extract(file, first, last, output)
            local source = io.readfile(file)
            local begin = assert(source:find(first, 1, true))
            local finish = assert(source:find(last, begin + #first, true))
            local destination = path.join(generated, output)
            local content = source:sub(begin, finish - 1)
            if not os.isfile(destination) or io.readfile(destination) ~= content then
                io.writefile(destination, content)
            end
        end
        extract("src/BodyChangeNG/ActorRegistry.cpp", "    void ActorRegistry::ForgetTransient(", "    void ActorRegistry::Revert(", "registry_forget.inc")
        extract("src/BodyChangeNG/RaceMenuOverlay.cpp", "    void RemoveDetachedPreviewValue(", "    [[nodiscard]] bcn::overlay::ApplyResult RemoveOneNow(", "overlay_cleanup.inc")
        extract("src/BodyChangeNG/RaceMenuOverlay.cpp", "    void ForgetActorState(", "    void ResetSessionState(", "overlay_forget.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    void CompleteMutation(", "    [[nodiscard]] std::optional<BaseInstance> BuildInstance(", "native_complete.inc")
        extract("src/BodyChangeNG/NativeSkinBackend.cpp", "    void ClearNow(", "    [[nodiscard]] std::vector<bcn::runtime_assets::TexturePreparation> Preparations(", "native_clear.inc")
        extract("src/BodyChangeNG/SkinApplication.cpp", "    ApplyResult QueueClear(", "    std::optional<std::string> CurrentProfileId(", "skin_clear.inc")
        extract("src/BodyChangeNG/SkinApplication.cpp", "    ApplyResult QueueApply(", "    ApplyResult QueueClear(", "skin_apply.inc")
    end)

target("BodyChangeNGFormDeleteGuardProbe")
    set_default(false)
    set_kind("shared")
    set_targetdir("build/v" .. version .. "/trials/form-delete-probe")
    set_encodings("utf-8")
    add_defines("BODY_CHANGE_NG_GUARD_PROBE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_files("tests/FormDeleteGuardProbeBridge.cpp", "src/BodyChangeNG/RaceMenuFormDeleteGuardCore.cpp")
    add_includedirs("src")

target("BodyChangeNGFormDeletePolicyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/FormDeletePolicyTests.cpp")
    add_includedirs("src")

target("BodyChangeNGCodePatternTests")
    set_default(false)
    set_kind("binary")
    set_rundir(os.projectdir()) -- this test also guards source-level queue routing
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/CodePatternTests.cpp")
    add_includedirs("src")

target("BodyChangeNGTexturePathIsolationTests")
    set_default(false)
    set_kind("binary")
    set_rundir(os.projectdir()) -- this test inspects repository source files
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/TexturePathIsolationTests.cpp")

target("BodyChangeNGOverlayPolicyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_includedirs("src")
    add_files("tests/OverlayPolicyTests.cpp")

target("BodyChangeNGSkinSessionStateTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/SkinSessionStateTests.cpp", "src/BodyChangeNG/SkinSessionState.cpp")
    add_includedirs("src")

target("BodyChangeNGBodyFamilyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/BodyFamilyTests.cpp", "src/BodyChangeNG/BodyFamilyRules.cpp")
    add_includedirs("src")

target("BodyChangeNGRenderedOutfitTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/RenderedOutfitTests.cpp")
    add_includedirs("src")

target("BodyChangeNGOutfitRefitRulesTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_packages("nlohmann_json")
    add_files("tests/OutfitRefitRulesTests.cpp")
    add_includedirs("src")

target("BodyChangeNGBodyMorphKeyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    add_includedirs("src")
    add_files("tests/BodyMorphKeyTests.cpp")

target("BodyChangeNGAsyncWorkGuardTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/AsyncWorkGuardTests.cpp")
    add_includedirs("src")

target("BodyChangeNGFrameTaskQueueTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/FrameTaskQueueTests.cpp")
    add_includedirs("src")

target("BodyChangeNGActorStateTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/ActorStateTests.cpp")
    add_includedirs("src")

target("BodyChangeNGGenitalPersistenceTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/GenitalPersistenceTests.cpp", "src/BodyChangeNG/SkinSessionState.cpp")
    add_includedirs("src")

target("BodyChangeNGRuntimeLayoutTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")
    add_files("tests/RuntimeLayoutTests.cpp")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

target("BodyChangeNGPathMigrationTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/PathMigrationTests.cpp")
    add_includedirs("src")

target("BodyChangeNGRaceMenuPresetMigrationTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_packages("nlohmann_json")
    add_files("tests/RaceMenuPresetMigrationTests.cpp", "src/BodyChangeNG/RaceMenuPresetMigrationRules.cpp")
    add_includedirs("src")
