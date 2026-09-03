#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/Settings.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <cctype>
#include <ranges>
#include <unordered_set>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    void Touch(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary).put('\0');
    }

    std::string Lower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    std::string Filename(const std::string_view path)
    {
        const auto separator = path.find_last_of("\\/");
        return std::string{ separator == std::string_view::npos ? path : path.substr(separator + 1U) };
    }

    bool IsStandardSkinTexture(const std::string_view filename)
    {
        constexpr std::array stems{
            std::string_view{ "femalebody_1" }, std::string_view{ "femalebody_etc_v2_1" },
            std::string_view{ "femalehands_1" },
            std::string_view{ "femalefeet_1" }, std::string_view{ "femalehead" },
            std::string_view{ "femaleheadvampire" }, std::string_view{ "malebody_1" },
            std::string_view{ "malehands_1" }, std::string_view{ "malefeet_1" },
            std::string_view{ "malehead" }, std::string_view{ "maleheadvampire" },
            std::string_view{ "argonianfemalebody" }, std::string_view{ "argonianfemalehands" },
            std::string_view{ "argonianfemalefeet" }, std::string_view{ "argonianfemalehead" },
            std::string_view{ "argonianmalebody" }, std::string_view{ "argonianmalehands" },
            std::string_view{ "argonianmalefeet" }, std::string_view{ "argonianmalehead" },
            std::string_view{ "femalebody" }, std::string_view{ "femalehands" },
            std::string_view{ "femalefeet" }, std::string_view{ "bodymale" },
            std::string_view{ "handsmale" }, std::string_view{ "feetmale" },
            std::string_view{ "headmale" }
        };
        if (filename == "blankdetailmap.dds" || filename.starts_with("femaleheaddetail_") ||
            filename.starts_with("maleheaddetail_")) return filename.ends_with(".dds");
        for (const auto stem : stems) {
            if (filename == std::string{ stem } + ".dds" ||
                filename == std::string{ stem } + "_msn.dds" ||
                filename == std::string{ stem } + "_sk.dds" ||
                filename == std::string{ stem } + "_s.dds") return true;
        }
        if (filename == "femalebody_1_d.dds" || filename == "femalebody_1_n.dds" ||
            filename == "femalehead_d.dds" || filename == "femalehead_n.dds") return true;
        return false;
    }
}

int main(const int argc, char** argv)
{
    if (argc == 3) {
        std::cout << "scanning real skin root\n" << std::flush;
        const auto skins = bcn::SkinProfiles::ScanDirectory(std::filesystem::path{ argv[1] });
        std::cout << "scanning real tint root\n" << std::flush;
        const auto tints = bcn::player_tint::Catalog::ScanDirectory(std::filesystem::path{ argv[2] });
        if (!Require(!skins.empty(), "real skin root produced no profiles")) return 1;
        if (!Require(!tints.empty(), "real tint root produced no assets")) return 1;
        for (const auto& tint : tints) {
            if (!Require(!tint.pack.empty() && tint.pack != ".." && !tint.id.starts_with(".."),
                    "real tint root produced an escaped pack or id")) return 1;
        }
        std::size_t verifiedSkinTextures{};
        std::size_t unrelatedDdsFiles{};
        for (const auto& skin : skins) {
            std::cout << "verifying " << skin.name << '\n' << std::flush;
            std::unordered_set<std::string> mapped;
            const auto collect = [&mapped](const auto& layers) {
                for (const auto& layer : layers) {
                    // layer.path is already UTF-8. Reconstructing a Windows
                    // filesystem::path from it would run it through the
                    // current ANSI code page and fails for Korean/Chinese pack
                    // names even though the DDS filename itself is ASCII.
                    mapped.insert(Lower(Filename(layer.path)));
                }
            };
            collect(skin.body);
            collect(skin.vagina);
            collect(skin.hands);
            collect(skin.feet);
            collect(skin.face);
            collect(skin.vampireFace);
            collect(skin.faceDetails);

            std::error_code error;
            for (std::filesystem::directory_iterator it(skin.source,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                if (!it->is_regular_file(error) || error) continue;
                const auto filename = Lower(it->path().filename().string());
                if (!filename.ends_with(".dds")) continue;
                if (!IsStandardSkinTexture(filename)) {
                    ++unrelatedDdsFiles;
                    continue;
                }
                if (!Require(mapped.contains(filename), "a standard skin DDS was omitted from the generated profile")) return 1;
                ++verifiedSkinTextures;
            }
        }
        if (!Require(verifiedSkinTextures != 0U, "real skin packs exposed no standard DDS channels")) return 1;
        std::cout << "skins=" << skins.size() << " mapped-skin-dds=" << verifiedSkinTextures
                  << " unrelated-dds=" << unrelatedDdsFiles << " tints=" << tints.size() << '\n';
        return 0;
    }

    const auto sandbox = std::filesystem::temp_directory_path() / "BodyChangeNGAssetCatalogTests";
    std::filesystem::remove_all(sandbox);

    const auto originalCurrentPath = std::filesystem::current_path();
    const auto legacySettings = sandbox / "Data" / "SKSE" / "Plugins" /
        "BodyChangerNG" / "settings.json";
    std::filesystem::create_directories(legacySettings.parent_path());
    {
        std::ofstream stream(legacySettings);
        stream << R"({"schemaVersion":1,"openHotkey":{"key":66,"ctrl":true,"shift":false,"alt":false},"language":2,"characterPosition":1,"textScale":1.25})";
    }
    std::filesystem::current_path(sandbox);
    bcn::Settings::Get().Load();
    std::filesystem::current_path(originalCurrentPath);
    const auto migratedSettings = sandbox / "Data" / "SKSE" / "Plugins" /
        "BodyChangeNG" / "settings.json";
    const auto migratedSnapshot = bcn::Settings::Get().Snapshot();
    if (!Require(std::filesystem::is_regular_file(migratedSettings),
            "legacy settings were not copied to the BodyChangeNG path")) return 1;
    if (!Require(migratedSnapshot.openHotkey.key == 66U && migratedSnapshot.openHotkey.ctrl,
            "legacy settings values were not preserved during migration")) return 1;

    const std::string skinPackName{ "피부팩 简体" };
    const auto female = sandbox / "BodySkin" / std::filesystem::path{ L"피부팩 简体" } /
        "Textures" / "actors" / "character" / "female";
    for (const auto* file : {
             "femalebody_1.dds", "femalebody_1_msn.dds", "femalebody_1_sk.dds", "femalebody_1_s.dds",
             "femalebody_etc_v2_1.dds", "femalebody_etc_v2_1_msn.dds",
             "femalebody_etc_v2_1_sk.dds", "femalebody_etc_v2_1_s.dds",
             "femalehands_1.dds", "femalehead.dds", "femalehead_msn.dds", "femalehead_s.dds",
             "blankdetailmap.dds", "femaleheaddetail_frekles.DDS" }) {
        Touch(female / file);
    }

    const auto ubeBody = sandbox / "BodySkin" / "UBE 2.0 Momo Skin" /
        "Textures" / "!UBE" / "Body";
    const auto ubeHead = sandbox / "BodySkin" / "UBE 2.0 Momo Skin" /
        "Textures" / "!UBE" / "Head";
    for (const auto* file : { "femalebody_1_d.dds", "femalebody_1_n.dds", "femalebody_1_sk.dds" }) {
        Touch(ubeBody / file);
    }
    for (const auto* file : { "femalehead_d.dds", "femalehead_n.dds", "femalehead_sk.dds" }) {
        Touch(ubeHead / file);
    }

    const auto malePartial = sandbox / "BodySkin" / "Male Partial" /
        "Textures" / "actors" / "character" / "male";
    Touch(malePartial / "malebody_1.dds");
    Touch(malePartial / "malebody_1_s.dds");
    Touch(malePartial / "malehead_msn.dds");

    const auto handsOnly = sandbox / "BodySkin" / "Hands Only" /
        "Textures" / "actors" / "character" / "male";
    Touch(handsOnly / "malehands_1_msn.dds");

    const auto ubeHeadOnly = sandbox / "BodySkin" / "UBE Head Only" /
        "Textures" / "!UBE" / "Head";
    Touch(ubeHeadOnly / "femalehead_d.dds");

    const auto argonianFemale = sandbox / "BodySkin" / "Argonian Complete" /
        "Textures" / "actors" / "character" / "argonianfemale";
    Touch(argonianFemale / "argonianfemalebody.dds");
    Touch(argonianFemale / "argonianfemalebody_msn.dds");
    Touch(argonianFemale / "argonianfemalehands_sk.dds");
    Touch(argonianFemale / "argonianfemalehead_s.dds");
    const auto argonianMale = sandbox / "BodySkin" / "Argonian Complete" /
        "Textures" / "actors" / "character" / "argonianmale";
    Touch(argonianMale / "argonianmalebody.dds");
    Touch(argonianMale / "argonianmalehands_msn.dds");
    Touch(argonianMale / "argonianmalehead.dds");

    const auto khajiitFemale = sandbox / "BodySkin" / "Khajiit Complete" /
        "Textures" / "actors" / "character" / "khajiitfemale";
    Touch(khajiitFemale / "femalebody.dds");
    Touch(khajiitFemale / "femalehands_s.dds");
    Touch(khajiitFemale / "femalehead_msn.dds");
    const auto khajiitMale = sandbox / "BodySkin" / "Khajiit Complete" /
        "Textures" / "actors" / "character" / "khajiitmale";
    Touch(khajiitMale / "bodymale.dds");
    Touch(khajiitMale / "handsmale_msn.dds");
    Touch(khajiitMale / "headmale_s.dds");

    const auto skins = bcn::SkinProfiles::ScanDirectory(sandbox / "BodySkin");
    const auto discoveredSkinRoots = bcn::catalog_roots::Discover(sandbox / "BodySkin");
    std::error_code equivalentError;
    if (!Require(!discoveredSkinRoots.empty() &&
            std::filesystem::equivalent(discoveredSkinRoots.front(), sandbox / "BodySkin", equivalentError) &&
            !equivalentError,
            "catalog root discovery climbed above the physical BodySkin provider")) return 1;
    if (!Require(skins.size() == 9U, "skin scanner did not preserve humanoid and beast-race skin rows")) return 1;
    std::size_t argonianRows{};
    std::size_t khajiitRows{};
    for (const auto& skin : skins) {
        if (skin.name == "Argonian Complete") {
            ++argonianRows;
            if (!Require(skin.race == bcn::SkinRace::argonian &&
                    skin.id.ends_with(skin.sex == bcn::SkinSex::female ? ":female:argonian" : ":male:argonian"),
                    "Argonian skin row lost its race, sex, or stable id")) return 1;
            if (!Require(!skin.body.empty() && !skin.hands.empty() && !skin.face.empty() && skin.feet.empty(),
                    "Argonian partial parts were omitted or copied into feet")) return 1;
            const auto expectedBody = skin.sex == bcn::SkinSex::female ? "argonianfemalebody" : "argonianmalebody";
            if (!Require(std::ranges::all_of(skin.body, [expectedBody](const auto& layer) {
                    return Filename(Lower(layer.path)).starts_with(expectedBody);
                }), "Argonian body channels crossed into another part or sex")) return 1;
            continue;
        }
        if (skin.name == "Khajiit Complete") {
            ++khajiitRows;
            if (!Require(skin.race == bcn::SkinRace::khajiit &&
                    skin.id.ends_with(skin.sex == bcn::SkinSex::female ? ":female:khajiit" : ":male:khajiit"),
                    "Khajiit skin row lost its race, sex, or stable id")) return 1;
            if (!Require(!skin.body.empty() && !skin.hands.empty() && !skin.face.empty() && skin.feet.empty(),
                    "Khajiit partial parts were omitted or copied into feet")) return 1;
            const auto expectedBody = skin.sex == bcn::SkinSex::female ? "femalebody" : "bodymale";
            if (!Require(std::ranges::all_of(skin.body, [expectedBody](const auto& layer) {
                    return Filename(Lower(layer.path)).starts_with(expectedBody);
                }), "Khajiit body channels crossed into another part or sex")) return 1;
            continue;
        }
        if (skin.name == "UBE 2.0 Momo Skin") {
            if (!Require(skin.sex == bcn::SkinSex::female, "UBE skin leaked into the wrong sex")) return 1;
            if (!Require(skin.bodyFamilies == bcn::body_family::Bit(bcn::body_family::Family::ube),
                    "UBE texture namespace was not classified as UBE")) return 1;
            if (!Require(skin.body.size() == 3U && skin.face.size() == 3U &&
                    skin.hands.empty() && skin.feet.empty(),
                    "UBE Body/Head d, n, and sk channels were not mapped exactly")) return 1;
            if (!Require(skin.id.ends_with(":female:ube"), "UBE profile id can collide with a conventional profile")) return 1;
            continue;
        }
        if (skin.name == "UBE Head Only") {
            if (!Require(skin.sex == bcn::SkinSex::female && skin.body.empty() &&
                    skin.hands.empty() && skin.feet.empty() && skin.face.size() == 1U,
                    "partial UBE head pack borrowed or invented another body part")) return 1;
            continue;
        }
        if (skin.name == "Male Partial") {
            if (!Require(skin.sex == bcn::SkinSex::male && skin.body.size() == 2U &&
                    skin.hands.empty() && skin.feet.empty() && skin.face.size() == 1U,
                    "partial male pack lost a supplied channel or synthesized a missing part")) return 1;
            if (!Require(std::ranges::all_of(skin.body, [](const auto& layer) {
                    return Filename(Lower(layer.path)).starts_with("malebody_1");
                }) && std::ranges::all_of(skin.face, [](const auto& layer) {
                    return Filename(Lower(layer.path)).starts_with("malehead");
                }), "male body and face files crossed part boundaries")) return 1;
            continue;
        }
        if (skin.name == "Hands Only") {
            if (!Require(skin.sex == bcn::SkinSex::male && skin.body.empty() &&
                    skin.hands.size() == 1U && skin.feet.empty() && skin.face.empty(),
                    "hands-only pack was rejected or copied into body, feet, or face")) return 1;
            if (!Require(skin.hands.front().shaderTextureIndex == 1U,
                    "hands normal map was not kept on the hands normal channel")) return 1;
            continue;
        }
        if (!Require(skin.sex == bcn::SkinSex::female && !skin.body.empty() &&
                !skin.hands.empty() && !skin.face.empty(),
                "conventional female skin lost one of its supplied parts")) return 1;
        if (!Require(skin.feet.empty(), "missing feet incorrectly borrowed the body texture")) return 1;
        if (!Require(skin.body.size() == 4U && std::ranges::all_of(skin.body, [](const auto& layer) {
                return Filename(Lower(layer.path)).starts_with("femalebody_1");
            }), "female genital atlas crossed into the regular body channels")) return 1;
        constexpr std::array expectedVaginaChannels{
            std::pair{ 0U, std::string_view{ "femalebody_etc_v2_1.dds" } },
            std::pair{ 1U, std::string_view{ "femalebody_etc_v2_1_msn.dds" } },
            std::pair{ 2U, std::string_view{ "femalebody_etc_v2_1_sk.dds" } },
            std::pair{ 7U, std::string_view{ "femalebody_etc_v2_1_s.dds" } }
        };
        if (!Require(skin.vagina.size() == expectedVaginaChannels.size(),
                "female genital texture set did not retain all four material channels")) return 1;
        for (const auto& [index, filename] : expectedVaginaChannels) {
            if (!Require(std::ranges::any_of(skin.vagina, [=](const auto& layer) {
                    return layer.shaderTextureIndex == index && Filename(Lower(layer.path)) == filename;
                }), "female genital diffuse/normal/subsurface/specular channel mapping is incorrect")) return 1;
        }
        if (!Require((skin.bodyFamilies & bcn::body_family::Bit(bcn::body_family::Family::ube)) == 0U,
                "conventional skin leaked into the UBE family")) return 1;
        if (!Require(skin.name.contains(skinPackName), "skin pack name was not preserved as UTF-8")) return 1;
        if (!Require(skin.body.front().path.starts_with("BodySkin\\" + skinPackName + "\\Textures\\"),
                "skin path escaped the virtual Data root or lost UTF-8")) return 1;
        const auto bodySkinTint = std::ranges::find_if(skin.body, [](const auto& layer) {
            return layer.path.ends_with("_sk.dds");
        });
        if (!Require(bodySkinTint != skin.body.end() && bodySkinTint->shaderTextureIndex == 2U,
                "_sk texture was not mapped to BSTextureSet subsurface slot 2")) return 1;
        const auto bodySpecular = std::ranges::find_if(skin.body, [](const auto& layer) {
            return layer.path.ends_with("_s.dds");
        });
        if (!Require(bodySpecular != skin.body.end() && bodySpecular->shaderTextureIndex == 7U,
                "_s texture was not mapped to BSTextureSet specular slot 7")) return 1;
        if (!Require(!skin.faceDetails.empty() && skin.faceDetails.front().shaderTextureIndex == 3U,
                "FaceGen detail texture was not mapped to BSTextureSet detail slot 3")) return 1;
    }
    if (!Require(argonianRows == 2U && khajiitRows == 2U,
            "beast-race packs did not produce separate female and male rows")) return 1;
    if (!Require(bcn::skin_geometry::IsVagina("3BA_Vagina") &&
            bcn::skin_geometry::IsVagina("3bbb_vagina") &&
            !bcn::skin_geometry::IsVagina("3BA_Anus") &&
            bcn::skin_geometry::Matches("3BA", bcn::skin_geometry::BodySelection::regular) &&
            !bcn::skin_geometry::Matches("3BA_Vagina", bcn::skin_geometry::BodySelection::regular) &&
            bcn::skin_geometry::Matches("3BA_Vagina", bcn::skin_geometry::BodySelection::vagina) &&
            !bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::vagina),
            "female genital geometry routing crossed into body or anus")) return 1;
    const auto standardFamily = bcn::body_family::Bit(bcn::body_family::Family::cbbe);
    const auto ubeFamily = bcn::body_family::Bit(bcn::body_family::Family::ube);
    const auto ubeSkin = std::ranges::find(skins, "UBE 2.0 Momo Skin", &bcn::SkinProfile::name);
    const auto standardSkin = std::ranges::find(skins, skinPackName, &bcn::SkinProfile::name);
    if (!Require(ubeSkin != skins.end() && standardSkin != skins.end(), "expected skin rows are missing")) return 1;
    if (!Require(bcn::SkinMatchesActor(ubeSkin->bodyFamilies, ubeFamily) &&
            !bcn::SkinMatchesActor(ubeSkin->bodyFamilies, standardFamily),
            "UBE skin compatibility leaked into CBBE")) return 1;
    if (!Require(bcn::SkinMatchesActor(standardSkin->bodyFamilies, standardFamily) &&
            !bcn::SkinMatchesActor(standardSkin->bodyFamilies, ubeFamily),
            "conventional skin compatibility leaked into UBE")) return 1;
    if (!Require(bcn::SkinRaceMatchesActor(bcn::SkinRace::argonian, bcn::SkinRace::argonian) &&
            !bcn::SkinRaceMatchesActor(bcn::SkinRace::argonian, bcn::SkinRace::khajiit) &&
            !bcn::SkinRaceMatchesActor(bcn::SkinRace::khajiit, bcn::SkinRace::humanoid),
            "beast-race skin compatibility leaked across races")) return 1;
    if (!Require(bcn::SkinRaceFromEditorID("ArgonianRaceVampire") == bcn::SkinRace::argonian &&
            bcn::SkinRaceFromEditorID("KhajiitRace") == bcn::SkinRace::khajiit &&
            bcn::SkinRaceFromEditorID("NordRace") == bcn::SkinRace::humanoid,
            "actor race EditorID classification did not preserve beast and humanoid boundaries")) return 1;

    const std::string tintPackName{ "틴트包" };
    const auto tintA = sandbox / "TintMask" / std::filesystem::path{ L"틴트包" } / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintA / "femalehead_lips.DDS");
    Touch(tintA / "femaleheadhuman_nose.dds");
    Touch(tintA / "not-a-tint.dds");
    const auto tintB = sandbox / "TintMask" / "Pack B" / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintB / "maleheadnord_lips.dds");
    const auto tintUbe = sandbox / "TintMask" / "UBE Makeup" / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintUbe / "femalehead_lips.dds");
    const auto tintCotr = sandbox / "TintMask" / "COtR Makeup" / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintCotr / "femalehead_eyeliner.dds");

    const auto tints = bcn::player_tint::Catalog::ScanDirectory(sandbox / "TintMask");
    if (!Require(tints.size() == 5U, "tint scanner did not classify the expected DDS files")) return 1;
    if (!Require(std::ranges::any_of(tints, [&](const auto& tint) { return tint.pack == tintPackName; }),
            "tint pack name was not preserved as UTF-8")) return 1;
    for (const auto& tint : tints) {
        if (!Require(tint.path.starts_with("TintMask\\"), "tint path escaped the virtual Data root")) return 1;
        if (!Require(!tint.id.starts_with(".."), "tint id contains a parent-directory escape")) return 1;
    }
    const auto ubeTint = std::ranges::find(tints, "UBE Makeup", &bcn::player_tint::Asset::pack);
    const auto cotrTint = std::ranges::find(tints, "COtR Makeup", &bcn::player_tint::Asset::pack);
    const auto maleTint = std::ranges::find(tints, "Pack B", &bcn::player_tint::Asset::pack);
    if (!Require(ubeTint != tints.end() &&
            ubeTint->bodyFamilies == bcn::body_family::Bit(bcn::body_family::Family::ube),
            "UBE tint pack was not isolated to UBE")) return 1;
    if (!Require(cotrTint != tints.end() &&
            bcn::player_tint::TintMatchesActor(cotrTint->bodyFamilies, standardFamily) &&
            bcn::player_tint::TintMatchesActor(cotrTint->bodyFamilies, ubeFamily),
            "COtR-compatible tint pack was not exposed to both female layouts")) return 1;
    if (!Require(maleTint != tints.end() && maleTint->sex == bcn::player_tint::Sex::male &&
            (maleTint->bodyFamilies & bcn::body_family::kMaleFamilies) != 0U,
            "male tint was not retained as a male-family asset")) return 1;

    std::filesystem::remove_all(sandbox);
    return 0;
}
