set_xmakever("3.1.0")

local version = "1.1.0"
set_project("BodyChangeNG")
set_version(version)
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")
set_policy("package.requires_lock", true)
set_config("skse_xbyak", true)
set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", true)

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

target("BodyChangeNGHotkeyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/HotkeyTests.cpp", "src/BodyChangeNG/Hotkey.cpp")
    add_includedirs("src")

target("BodyChangeNGPresetCatalogTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/PresetCatalogTests.cpp", "src/BodyChangeNG/PresetCatalog.cpp", "src/BodyChangeNG/CatalogRoots.cpp",
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
        "src/BodyChangeNG/RuntimeAssetCache.cpp",
        "src/BodyChangeNG/Settings.cpp",
        "src/BodyChangeNG/SkinProfiles.cpp"
    )
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

target("BodyChangeNGSkinOverrideOwnershipTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/SkinOverrideOwnershipTests.cpp")
    add_includedirs("src")

target("BodyChangeNGBodyFamilyTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/BodyFamilyTests.cpp", "src/BodyChangeNG/BodyFamilyRules.cpp")
    add_includedirs("src")

target("BodyChangeNGOutfitRefitRulesTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_packages("nlohmann_json")
    add_files("tests/OutfitRefitRulesTests.cpp")
    add_includedirs("src")

target("BodyChangeNGActorStateTests")
    set_default(false)
    set_kind("binary")
    set_targetdir("build/v" .. version .. "/tests")
    set_encodings("utf-8")
    add_files("tests/ActorStateTests.cpp")
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
