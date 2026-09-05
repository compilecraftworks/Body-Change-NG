#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/FutanariRouting.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/RuntimeAssetCache.h"

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
            std::string_view{ "vaginalanalcanal2" },
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

    bool HasExactMaterialChannels(
        const std::vector<bcn::SkinTextureLayer>& layers, const std::string_view stem)
    {
        const std::array expected{
            std::pair{ 0U, std::string{ stem } + ".dds" },
            std::pair{ 1U, std::string{ stem } + "_msn.dds" },
            std::pair{ 2U, std::string{ stem } + "_sk.dds" },
            std::pair{ 7U, std::string{ stem } + "_s.dds" }
        };
        return layers.size() == expected.size() &&
            std::ranges::all_of(expected, [&](const auto& item) {
                const auto& [index, filename] = item;
                return std::ranges::any_of(layers, [&](const auto& layer) {
                    return layer.shaderTextureIndex == index &&
                        Filename(Lower(layer.path)) == filename;
                });
            });
    }
}

int main(const int argc, char** argv)
{
    if (argc == 3 || argc == 4) {
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
            collect(skin.cbbeGenitalAnal);
            collect(skin.unpGenitalAnal);
            collect(skin.hands);
            collect(skin.feet);
            collect(skin.face);
            collect(skin.vampireFace);
            collect(skin.elderBody);
            collect(skin.elderHands);
            collect(skin.elderFace);
            for (const auto& raceFace : skin.raceFace) collect(raceFace);
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
        std::size_t futanariSkins{};
        if (argc == 4) {
            const auto profiles = bcn::FutanariSkinProfiles::ScanDirectory(
                std::filesystem::path{ argv[3] });
            if (!Require(!profiles.empty(), "real futanari root produced no profiles")) return 1;
            futanariSkins = profiles.size();
        }
        std::cout << "skins=" << skins.size() << " mapped-skin-dds=" << verifiedSkinTextures
                  << " unrelated-dds=" << unrelatedDdsFiles << " tints=" << tints.size()
                  << " futanari=" << futanariSkins << '\n';
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
    if (!Require(migratedSnapshot.femaleNpcBodyType == bcn::FemaleNpcBodyType::cbbe3ba &&
            migratedSnapshot.maleNpcBodyType == bcn::MaleNpcBodyType::himbo,
            "legacy settings migration did not preserve the new CBBE 3BA/HIMBO defaults")) return 1;

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
    for (const auto* file : {
             "astridbody.dds", "astridhands_msn.dds", "astridhead_s.dds",
             "femalebodyafflicted.dds", "femalehandsafflicted.dds", "femaleheadafflicted.dds" }) {
        Touch(female / file);
    }
    const auto femaleOld = female.parent_path() / "femaleold";
    Touch(femaleOld / "FemaleBody_1_msn.dds");
    Touch(femaleOld / "FemaleHands_1_msn.dds");
    Touch(femaleOld / "FemaleHead_msn.dds");
    const std::array raceFaceDirectories{
        std::pair{ "nordfemale", "femalehead_msn.dds" },
        std::pair{ "bretonfemale", "femalehead_msn.dds" },
        std::pair{ "darkelffemale", "femalehead_msn.dds" },
        std::pair{ "highelffemale", "femalehead_msn.dds" },
        std::pair{ "imperialfemale", "femalehead_msn.dds" },
        std::pair{ "femaleorc", "femaleheadorc_msn.dds" },
        std::pair{ "redguardfemale", "femalehead_msn.dds" },
        std::pair{ "woodelffemale", "femalehead_msn.dds" }
    };
    for (const auto& [directory, filename] : raceFaceDirectories) {
        Touch(female.parent_path() / directory / filename);
    }
    const auto embeddedTint = female.parent_path() / "character assets" / "tintmasks";
    Touch(embeddedTint / "femalehead_lips.dds");

    const std::string unpSkinPackName{ "BnP BHUNP UNP" };
    const auto unpFemale = sandbox / "BodySkin" / unpSkinPackName /
        "Textures" / "actors" / "character" / "female";
    for (const auto* file : {
             "femalebody_1.dds", "femalebody_1_msn.dds", "femalebody_1_sk.dds", "femalebody_1_s.dds",
             "femalehands_1.dds", "femalehands_1_msn.dds", "femalehands_1_sk.dds", "femalehands_1_s.dds",
             "femalefeet_1.dds", "femalefeet_1_msn.dds", "femalefeet_1_sk.dds", "femalefeet_1_s.dds",
             "femalehead.dds", "femalehead_msn.dds", "femalehead_sk.dds", "femalehead_s.dds" }) {
        Touch(unpFemale / file);
    }
    for (const auto* file : {
             "VaginalAnalCanal2.dds", "VaginalAnalCanal2_msn.dds",
             "VaginalAnalCanal2_sk.dds", "VaginalAnalCanal2_s.dds" }) {
        Touch(unpFemale / "BakaUNP" / file);
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
    const auto sosRegular = malePartial.parent_path() / "SOS" / "VectorPlexus Regular";
    for (const auto* file : { "malegenitals_1.dds", "malegenitals_1_msn.dds",
             "malegenitals_1_sk.dds", "malegenitals_1_s.dds" }) {
        Touch(sosRegular / file);
    }
    Touch(sosRegular / "malegenitals_argonian_1.dds");
    Touch(sosRegular / "malegenitals_argonian_1_msn.dds");
    Touch(sosRegular / "malegenitals_khajiit_1.dds");
    Touch(sosRegular / "malegenitals_old_1_msn.dds");
    const auto sosMuscular = malePartial.parent_path() / "SOS" / "VectorPlexus Muscular";
    Touch(sosMuscular / "malegenitals_1_msn.dds");
    Touch(sosMuscular / "malegenitals_argonian_1_msn.dds");
    const auto sosSmurfRaceOnly = malePartial.parent_path() / "SOS" / "Smurf Average";
    Touch(sosSmurfRaceOnly / "malegenitals_khajiit_1_s.dds");

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
    if (!Require(skins.size() == 10U, "skin scanner did not preserve humanoid and beast-race skin rows")) return 1;
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
        if (skin.name == unpSkinPackName) {
            if (!Require(skin.sex == bcn::SkinSex::female &&
                    skin.bodyFamilies == bcn::body_family::Bit(bcn::body_family::Family::unp),
                    "BHUNP/UNP skin was not isolated to the UNP family")) return 1;
            if (!Require(HasExactMaterialChannels(skin.body, "femalebody_1") &&
                    HasExactMaterialChannels(skin.hands, "femalehands_1") &&
                    HasExactMaterialChannels(skin.feet, "femalefeet_1") &&
                    HasExactMaterialChannels(skin.face, "femalehead"),
                    "BHUNP/UNP body, hands, feet, or face channels crossed parts or material slots")) return 1;
            if (!Require(skin.cbbeGenitalAnal.empty() &&
                    HasExactMaterialChannels(skin.unpGenitalAnal, "vaginalanalcanal2"),
                    "BHUNP/UNP genital/anal/canal atlas was omitted or routed as CBBE")) return 1;
            if (!Require(skin.id.ends_with(":female"),
                    "BHUNP/UNP conventional profile lost its stable id")) return 1;
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
            if (!Require(skin.maleGenitals.size() == 3U,
                    "SOS addon texture directories were not kept as separate variants")) return 1;
            const auto regular = std::ranges::find(
                skin.maleGenitals, "VectorPlexus Regular",
                &bcn::MaleGenitalTextureVariant::addonDirectory);
            const auto muscular = std::ranges::find(
                skin.maleGenitals, "VectorPlexus Muscular",
                &bcn::MaleGenitalTextureVariant::addonDirectory);
            const auto smurf = std::ranges::find(
                skin.maleGenitals, "Smurf Average",
                &bcn::MaleGenitalTextureVariant::addonDirectory);
            if (!Require(regular != skin.maleGenitals.end() && regular->humanoid.size() == 4U &&
                    regular->argonian.size() == 2U && regular->khajiit.size() == 1U &&
                    regular->elder.size() == 1U,
                    "SOS Regular humanoid/race/elder channels were not mapped exactly")) return 1;
            if (!Require(muscular != skin.maleGenitals.end() && muscular->humanoid.size() == 1U &&
                    muscular->humanoid.front().shaderTextureIndex == 1U &&
                    muscular->argonian.size() == 1U,
                    "SOS Muscular normal-only material was expanded into invented channels")) return 1;
            if (!Require(smurf != skin.maleGenitals.end() && smurf->humanoid.empty() &&
                    smurf->argonian.empty() && smurf->khajiit.size() == 1U && smurf->elder.empty() &&
                    smurf->khajiit.front().shaderTextureIndex == 7U,
                    "SOS race-only partial atlas was not registered independently")) return 1;
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
        constexpr std::array expectedCBBEGenitalAnalChannels{
            std::pair{ 0U, std::string_view{ "femalebody_etc_v2_1.dds" } },
            std::pair{ 1U, std::string_view{ "femalebody_etc_v2_1_msn.dds" } },
            std::pair{ 2U, std::string_view{ "femalebody_etc_v2_1_sk.dds" } },
            std::pair{ 7U, std::string_view{ "femalebody_etc_v2_1_s.dds" } }
        };
        if (!Require(skin.cbbeGenitalAnal.size() == expectedCBBEGenitalAnalChannels.size() &&
                skin.unpGenitalAnal.empty(),
                "CBBE 3BA genital/anal texture set crossed into BHUNP/UNP")) return 1;
        for (const auto& [index, filename] : expectedCBBEGenitalAnalChannels) {
            if (!Require(std::ranges::any_of(skin.cbbeGenitalAnal, [=](const auto& layer) {
                    return layer.shaderTextureIndex == index && Filename(Lower(layer.path)) == filename;
                }), "CBBE 3BA genital/anal diffuse/normal/subsurface/specular mapping is incorrect")) return 1;
        }
        if (!Require(skin.bodyFamilies == bcn::body_family::Bit(bcn::body_family::Family::cbbe),
                "CBBE 3BA genital atlas did not narrow the skin to the CBBE family")) return 1;
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
        if (!Require(skin.elderBody.size() == 1U && skin.elderHands.size() == 1U &&
                skin.elderFace.size() == 1U &&
                skin.elderBody.front().shaderTextureIndex == 1U &&
                skin.elderHands.front().shaderTextureIndex == 1U &&
                skin.elderFace.front().shaderTextureIndex == 1U,
                "partial femaleold normals were not kept on their exact elder body/hand/face slots")) return 1;
        for (const auto race : {
                 bcn::HumanoidSkinRace::nord, bcn::HumanoidSkinRace::breton,
                 bcn::HumanoidSkinRace::darkElf, bcn::HumanoidSkinRace::highElf,
                 bcn::HumanoidSkinRace::imperial, bcn::HumanoidSkinRace::orc,
                 bcn::HumanoidSkinRace::redguard, bcn::HumanoidSkinRace::woodElf }) {
            const auto& layers = skin.raceFace[static_cast<std::size_t>(race)];
            if (!Require(layers.size() == 1U && layers.front().shaderTextureIndex == 1U,
                    "race-specific female face normal was not retained as a conditional face slot")) return 1;
        }
        if (!Require(skin.raceFace[static_cast<std::size_t>(bcn::HumanoidSkinRace::generic)].empty(),
                "a conditional race face was misclassified as the generic face")) return 1;
        const auto isExcludedBodySkinFile = [](const auto& layer) {
            const auto path = Lower(layer.path);
            return path.find("astrid") != std::string::npos ||
                path.find("afflicted") != std::string::npos ||
                path.find("tintmasks") != std::string::npos;
        };
        bool foundExcluded{};
        const auto inspectExcluded = [&](const auto& layers) {
            foundExcluded = foundExcluded || std::ranges::any_of(layers, isExcludedBodySkinFile);
        };
        inspectExcluded(skin.body);
        inspectExcluded(skin.cbbeGenitalAnal);
        inspectExcluded(skin.unpGenitalAnal);
        inspectExcluded(skin.hands);
        inspectExcluded(skin.feet);
        inspectExcluded(skin.face);
        inspectExcluded(skin.vampireFace);
        inspectExcluded(skin.elderBody);
        inspectExcluded(skin.elderHands);
        inspectExcluded(skin.elderFace);
        for (const auto& raceFace : skin.raceFace) inspectExcluded(raceFace);
        inspectExcluded(skin.faceDetails);
        if (!Require(!foundExcluded,
                "Astrid, Afflicted, or embedded tintmask files leaked into Body Skin mapping")) return 1;
    }
    if (!Require(argonianRows == 2U && khajiitRows == 2U,
            "beast-race packs did not produce separate female and male rows")) return 1;

    const auto futanariRoot = sandbox / "Futanari";
    const auto futaUbe = futanariRoot / "UBE TRX" / "Textures" / "!UBE" / "Body";
    Touch(futaUbe / "malebody_1_d.dds");
    Touch(futaUbe / "malebody_1_n.dds");
    const auto futaTrx = futanariRoot / "CBBE TRX" / "textures" /
        "[TRX] Futa addon" / "Regular" / "Default";
    for (const auto* file : { "schlong.dds", "schlong_msn.dds", "schlong_sk.dds",
             "schlong_s.dds", "wetschlong_110_s.dds" }) Touch(futaTrx / file);
    // An unrelated optional atlas in the same pack must not create a second
    // or incorrectly typed row.
    Touch(futanariRoot / "CBBE TRX" / "textures" / "Dw3BA" / "Default" / "schlong.dds");
    const auto futaErf = futanariRoot / "ERF" / "textures" /
        "erf_futanari" / "fairskincbbe";
    for (const auto* file : { "futanari_schlong.dds", "futanari_schlong_msn.dds",
             "futanari_schlong_sk.dds", "futanari_schlong_s.dds" }) Touch(futaErf / file);

    const auto futanari = bcn::FutanariSkinProfiles::ScanDirectory(futanariRoot);
    if (!Require(futanari.size() == 3U,
            "futanari catalog did not isolate UBE TRX, CBBE TRX, and ERF rows")) return 1;
    const auto requireFutanari = [&](const bcn::FutanariSkinType type,
        const std::size_t layerCount, const std::string_view stem) {
        const auto found = std::ranges::find(futanari, type, &bcn::FutanariSkinProfile::type);
        return found != futanari.end() && found->layers.size() == layerCount &&
            std::ranges::all_of(found->layers, [&](const auto& layer) {
                return Filename(Lower(layer.path)).starts_with(stem);
            });
    };
    if (!Require(requireFutanari(bcn::FutanariSkinType::ubeTrx, 2U, "malebody_1_") &&
            requireFutanari(bcn::FutanariSkinType::cbbeTrx, 4U, "schlong") &&
            requireFutanari(bcn::FutanariSkinType::erf, 4U, "futanari_schlong"),
            "futanari skin files crossed addon/body types or material channels")) return 1;
    if (!Require(
            bcn::futanari::ClassifyEvidence(
                R"(meshes\[TRX] Futa addon\Regular\trx_schlong_1.nif)", {}, {}) ==
                bcn::futanari::AddonKind::trx &&
            bcn::futanari::ClassifyEvidence(
                R"(meshes\!UBE\SOS_Addon\ube_penis_1.nif)", {}, {}) ==
                bcn::futanari::AddonKind::ube &&
            bcn::futanari::ClassifyEvidence({}, "Penis",
                R"(Textures\!UBE\Body\malebody_1_d.dds)") ==
                bcn::futanari::AddonKind::ube &&
            bcn::futanari::ClassifyEvidence({}, "CBBE_Shlong", {}) ==
                bcn::futanari::AddonKind::trx &&
            bcn::futanari::ClassifyEvidence(
                R"(meshes\ERF_Futanari\futanari_1.nif)", {}, {}) ==
                bcn::futanari::AddonKind::erf &&
            bcn::futanari::ClassifyEvidence({}, "CBBE Schlong", {}) ==
                bcn::futanari::AddonKind::erf &&
            bcn::futanari::ClassifyEvidence({}, "FemaleBody", {}) ==
                bcn::futanari::AddonKind::none,
            "futanari geometry evidence confused normal body nodes or addon families")) return 1;
    if (!Require(bcn::skin_geometry::IsCBBEGenitalAnal("3BA_Vagina") &&
            bcn::skin_geometry::IsCBBEGenitalAnal("3bbb_vagina") &&
            bcn::skin_geometry::IsCBBEGenitalAnal("3BA_Anus") &&
            bcn::skin_geometry::IsCBBEGenitalAnal("3bbb_anus") &&
            bcn::skin_geometry::IsUNPGenitalAnal("BaseShapeVagina") &&
            bcn::skin_geometry::IsUNPGenitalAnal("BaseShapeAnus") &&
            bcn::skin_geometry::IsUNPGenitalAnal("BaseShapeCanal") &&
            bcn::skin_geometry::Matches("3BA", bcn::skin_geometry::BodySelection::regular) &&
            !bcn::skin_geometry::Matches("3BA_Vagina", bcn::skin_geometry::BodySelection::regular) &&
            !bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::regular) &&
            !bcn::skin_geometry::Matches("BaseShapeCanal", bcn::skin_geometry::BodySelection::regular) &&
            bcn::skin_geometry::Matches("3BA_Vagina", bcn::skin_geometry::BodySelection::cbbeGenitalAnal) &&
            bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::cbbeGenitalAnal) &&
            !bcn::skin_geometry::Matches("BaseShapeAnus", bcn::skin_geometry::BodySelection::cbbeGenitalAnal) &&
            bcn::skin_geometry::Matches("BaseShapeVagina", bcn::skin_geometry::BodySelection::unpGenitalAnal) &&
            bcn::skin_geometry::Matches("BaseShapeAnus", bcn::skin_geometry::BodySelection::unpGenitalAnal) &&
            bcn::skin_geometry::Matches("BaseShapeCanal", bcn::skin_geometry::BodySelection::unpGenitalAnal) &&
            !bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::unpGenitalAnal) &&
            bcn::skin_geometry::Matches("RenamedShape", bcn::skin_geometry::BodySelection::cbbeGenitalAnal,
                R"(textures\actors\character\female\femalebody_etc_v2_1.dds)") &&
            bcn::skin_geometry::Matches("BaseShapeAnus", bcn::skin_geometry::BodySelection::cbbeGenitalAnal,
                R"(textures\actors\character\female\femalebody_etc_v2_1.dds)") &&
            !bcn::skin_geometry::Matches("BaseShapeAnus", bcn::skin_geometry::BodySelection::unpGenitalAnal,
                R"(textures\actors\character\female\femalebody_etc_v2_1.dds)") &&
            bcn::skin_geometry::Matches("RenamedShape", bcn::skin_geometry::BodySelection::unpGenitalAnal,
                R"(textures\actors\character\female\BakaUNP\VaginalAnalCanal2.dds)") &&
            bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::unpGenitalAnal,
                R"(textures\actors\character\female\BakaUNP\VaginalAnalCanal2.dds)") &&
            !bcn::skin_geometry::Matches("3BA_Anus", bcn::skin_geometry::BodySelection::cbbeGenitalAnal,
                R"(textures\actors\character\female\BakaUNP\VaginalAnalCanal2.dds)") &&
            bcn::skin_geometry::Matches("RenamedShape", bcn::skin_geometry::BodySelection::unpGenitalAnal,
                "textures/actors/character/female/BakaUNP/VaginalAnalCanal2.dds") &&
            bcn::skin_geometry::Matches("MaleGenitals", bcn::skin_geometry::BodySelection::maleGenitals) &&
            bcn::skin_geometry::Matches("RenamedShape", bcn::skin_geometry::BodySelection::maleGenitals,
                R"(textures\actors\character\SOS\VectorPlexus Regular\malegenitals_1.dds)") &&
            !bcn::skin_geometry::Matches("MaleBody", bcn::skin_geometry::BodySelection::maleGenitals,
                R"(textures\actors\character\male\malebody_1.dds)"),
            "CBBE 3BA and BHUNP/UNP genital/anal geometry routing crossed body or family boundaries")) return 1;
    if (!Require(bcn::skin_geometry::NeedsFixedBipedFallback(true, 0U) &&
            !bcn::skin_geometry::NeedsFixedBipedFallback(true, 1U) &&
            !bcn::skin_geometry::NeedsFixedBipedFallback(false, 0U),
            "an unusable exact hands/feet slot did not preserve the bounded Skin Armor fallback")) return 1;
    if (!Require(bcn::skin_geometry::NeedsStandardBodyFallback(true, 0U) &&
            !bcn::skin_geometry::NeedsStandardBodyFallback(true, 1U) &&
            !bcn::skin_geometry::NeedsStandardBodyFallback(false, 0U),
            "UBE body routing did not fall back only when its slot-53 target was absent")) return 1;
    if (!Require(
            bcn::skin_geometry::IsBodyGeometryCandidate("3BA",
                R"(textures\actors\character\female\femalebody_1.dds)") &&
            bcn::skin_geometry::IsBodyGeometryCandidate("BaseShape",
                R"(textures\!UBE\Body\femalebody_1_d.dds)") &&
            bcn::skin_geometry::IsBodyGeometryCandidate("MaleBody",
                R"(textures\actors\character\male\malebody_1.dds)") &&
            !bcn::skin_geometry::IsBodyGeometryCandidate("FemaleHands",
                R"(textures\actors\character\female\femalehands_1.dds)") &&
            !bcn::skin_geometry::IsBodyGeometryCandidate("FemaleHead",
                R"(textures\actors\character\female\femalehead.dds)") &&
            !bcn::skin_geometry::IsBodyGeometryCandidate("3BA_Vagina",
                R"(textures\actors\character\female\femalebody_etc_v2_1.dds)"),
            "cross-slot outfit body routing accepted a non-body skin atlas")) return 1;
    if (!Require(
            bcn::skin_geometry::MatchesLimb(bcn::skin_geometry::LimbSelection::hands,
                "FemaleHands", R"(textures\actors\character\female\femalehands_1.dds)") &&
            !bcn::skin_geometry::MatchesLimb(bcn::skin_geometry::LimbSelection::hands,
                "FemaleBody", R"(textures\actors\character\female\femalebody_1.dds)") &&
            bcn::skin_geometry::MatchesLimb(bcn::skin_geometry::LimbSelection::feet,
                "FemaleFoot", R"(textures\actors\character\female\femalefeet_1.dds)") &&
            !bcn::skin_geometry::MatchesLimb(bcn::skin_geometry::LimbSelection::feet,
                "FemaleHands", R"(textures\actors\character\female\femalehands_1.dds)"),
            "body, hands, and feet fallback routing crossed texture-part boundaries")) return 1;
    constexpr std::uint32_t bodyMask = 1U << 2U;
    constexpr std::uint32_t handsMask = 1U << 3U;
    constexpr std::uint32_t feetMask = 1U << 4U;
    if (!Require(
            bcn::skin_geometry::MatchesRequestedPart(bodyMask, bodyMask, handsMask, feetMask,
                "FemaleBody", R"(textures\actors\character\female\femalebody_1.dds)") &&
            !bcn::skin_geometry::MatchesRequestedPart(bodyMask, bodyMask, handsMask, feetMask,
                "FemaleHands", R"(textures\actors\character\female\femalehands_1.dds)") &&
            bcn::skin_geometry::MatchesRequestedPart(handsMask, bodyMask, handsMask, feetMask,
                "FemaleHands", R"(textures\actors\character\female\femalehands_1.dds)") &&
            !bcn::skin_geometry::MatchesRequestedPart(handsMask, bodyMask, handsMask, feetMask,
                "FemaleFeet", R"(textures\actors\character\female\femalefeet_1.dds)") &&
            bcn::skin_geometry::MatchesRequestedPart(feetMask, bodyMask, handsMask, feetMask,
                "FemaleFeet", R"(textures\actors\character\female\femalefeet_1.dds)") &&
            !bcn::skin_geometry::MatchesRequestedPart(feetMask, bodyMask, handsMask, feetMask,
                "FemaleBody", R"(textures\actors\character\female\femalebody_1.dds)"),
            "a multi-slot naked Skin Armor routed body, hands, or feet into the wrong part")) return 1;
    if (!Require(
            bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                handsMask, handsMask, feetMask, "Hands") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                feetMask, handsMask, feetMask, "Hands") &&
            bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                feetMask, handsMask, feetMask, "Feet") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                handsMask, handsMask, feetMask, "Feet") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                handsMask, handsMask, feetMask, "glove") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                feetMask, handsMask, feetMask, "boota") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                feetMask, handsMask, feetMask, "stock") &&
            !bcn::skin_geometry::MatchesExplicitRequestedLimbNode(
                handsMask, handsMask, feetMask, "BaseShape"),
            "shared-atlas limbs accepted an outfit or wrong-part node")) return 1;
    if (!Require(
            bcn::skin_geometry::NeedsBodyAtlasForFeet(0U) &&
            !bcn::skin_geometry::NeedsBodyAtlasForFeet(1U) &&
            !bcn::skin_geometry::NeedsBodyAtlasForFeet(4U),
            "standard feet did not use body atlas fallback only when an explicit feet atlas was absent")) return 1;
    struct FakeNifGeometry final
    {
        std::string_view node;
        std::string_view diffuse;
    };
    constexpr std::array fakeMultiSlotNif{
        FakeNifGeometry{ "3BA", R"(textures\actors\character\female\femalebody_1.dds)" },
        FakeNifGeometry{ "FemaleHands", R"(textures\actors\character\female\femalehands_1.dds)" },
        FakeNifGeometry{ "FemaleFeet", R"(textures\actors\character\female\femalefeet_1.dds)" },
        FakeNifGeometry{ "3BA_Vagina", R"(textures\actors\character\female\femalebody_etc_v2_1.dds)" }
    };
    const auto fakeNifPartCount = [&](const std::uint32_t requestedMask) {
        return std::ranges::count_if(fakeMultiSlotNif, [&](const FakeNifGeometry& geometry) {
            return bcn::skin_geometry::MatchesRequestedPart(
                requestedMask, bodyMask, handsMask, feetMask, geometry.node, geometry.diffuse);
        });
    };
    if (!Require(fakeNifPartCount(bodyMask) == 2U &&
            fakeNifPartCount(handsMask) == 1U && fakeNifPartCount(feetMask) == 1U,
            "fake multi-slot NIF did not isolate body/genital, hands, and feet node sets")) return 1;
    const auto standardFamily = bcn::body_family::Bit(bcn::body_family::Family::cbbe);
    const auto unpFamily = bcn::body_family::Bit(bcn::body_family::Family::unp);
    const auto ubeFamily = bcn::body_family::Bit(bcn::body_family::Family::ube);
    const auto ubeSkin = std::ranges::find(skins, "UBE 2.0 Momo Skin", &bcn::SkinProfile::name);
    const auto standardSkin = std::ranges::find(skins, skinPackName, &bcn::SkinProfile::name);
    const auto unpSkin = std::ranges::find(skins, unpSkinPackName, &bcn::SkinProfile::name);
    if (!Require(ubeSkin != skins.end() && standardSkin != skins.end() && unpSkin != skins.end(),
            "expected skin rows are missing")) return 1;
    if (!Require(bcn::SkinMatchesActor(ubeSkin->bodyFamilies, ubeFamily) &&
            !bcn::SkinMatchesActor(ubeSkin->bodyFamilies, standardFamily),
            "UBE skin compatibility leaked into CBBE")) return 1;
    if (!Require(bcn::SkinMatchesActor(standardSkin->bodyFamilies, standardFamily) &&
            !bcn::SkinMatchesActor(standardSkin->bodyFamilies, unpFamily) &&
            !bcn::SkinMatchesActor(standardSkin->bodyFamilies, ubeFamily),
            "CBBE 3BA skin compatibility leaked into BHUNP/UNP or UBE")) return 1;
    if (!Require(bcn::SkinMatchesActor(unpSkin->bodyFamilies, unpFamily) &&
            !bcn::SkinMatchesActor(unpSkin->bodyFamilies, standardFamily) &&
            !bcn::SkinMatchesActor(unpSkin->bodyFamilies, ubeFamily),
            "BHUNP/UNP skin compatibility leaked into CBBE 3BA or UBE")) return 1;
    if (!Require(bcn::SkinRaceMatchesActor(bcn::SkinRace::argonian, bcn::SkinRace::argonian) &&
            !bcn::SkinRaceMatchesActor(bcn::SkinRace::argonian, bcn::SkinRace::khajiit) &&
            !bcn::SkinRaceMatchesActor(bcn::SkinRace::khajiit, bcn::SkinRace::humanoid),
            "beast-race skin compatibility leaked across races")) return 1;
    if (!Require(bcn::SkinRaceFromEditorID("ArgonianRaceVampire") == bcn::SkinRace::argonian &&
            bcn::SkinRaceFromEditorID("KhajiitRace") == bcn::SkinRace::khajiit &&
            bcn::SkinRaceFromEditorID("NordRace") == bcn::SkinRace::humanoid,
            "actor race EditorID classification did not preserve beast and humanoid boundaries")) return 1;
    if (!Require(
            bcn::HumanoidSkinRaceFromEditorID("NordRace") == bcn::HumanoidSkinRace::nord &&
            bcn::HumanoidSkinRaceFromEditorID("BretonRace") == bcn::HumanoidSkinRace::breton &&
            bcn::HumanoidSkinRaceFromEditorID("DarkElfRaceVampire") == bcn::HumanoidSkinRace::darkElf &&
            bcn::HumanoidSkinRaceFromEditorID("HighElfRace") == bcn::HumanoidSkinRace::highElf &&
            bcn::HumanoidSkinRaceFromEditorID("ImperialRace") == bcn::HumanoidSkinRace::imperial &&
            bcn::HumanoidSkinRaceFromEditorID("OrcRace") == bcn::HumanoidSkinRace::orc &&
            bcn::HumanoidSkinRaceFromEditorID("RedguardRace") == bcn::HumanoidSkinRace::redguard &&
            bcn::HumanoidSkinRaceFromEditorID("WoodElfRace") == bcn::HumanoidSkinRace::woodElf,
            "humanoid race EditorID classification did not select the matching face variant")) return 1;
    if (!Require(bcn::IsElderSkinVariant("ElderRace", "FemaleEvenToned") &&
            bcn::IsElderSkinVariant("NordRace", "FemaleOldGrumpy") &&
            !bcn::IsElderSkinVariant("NordRace", "FemaleEvenToned"),
            "elder race/voice classification did not preserve the distribution condition semantics")) return 1;

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
    // Content refresh must distinguish equal-size DDS replacements even if
    // an archive extraction preserves timestamps. Cache aliases must differ.
    const auto refreshSource = sandbox / "refresh.dds";
    std::ofstream(refreshSource, std::ios::binary) << "abc";
    const auto sourceTime = std::filesystem::last_write_time(refreshSource);
    const auto refreshKey = "BodySkin\\Refresh\\body.dds";
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(refreshKey, refreshSource);
    const auto beforeHash = bcn::runtime_assets::SourceContentHash(refreshKey);
    std::filesystem::current_path(sandbox);
    const auto beforePath = bcn::runtime_assets::TexturePathFromGameRelative(refreshKey, "test");
    std::ofstream(refreshSource, std::ios::binary) << "xyz";
    std::filesystem::last_write_time(refreshSource, sourceTime);
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(refreshKey, refreshSource);
    const auto afterHash = bcn::runtime_assets::SourceContentHash(refreshKey);
    const auto afterPath = bcn::runtime_assets::TexturePathFromGameRelative(refreshKey, "test");
    const auto expectedAfterPath =
        bcn::runtime_assets::ExpectedTexturePathFromGameRelative(refreshKey, "test");
    if (!Require(expectedAfterPath == afterPath &&
            bcn::runtime_assets::CachedTextureExists(afterPath),
            "exact runtime texture cache alias was not discoverable")) return 1;

    // A user may clean MO2 overwrite while the RaceMenu co-save still points
    // at a BCNG alias. Detection must be metadata-only, and the next normal
    // materialization must recreate the same stable path from the source.
    std::filesystem::remove(sandbox / "Data" / std::filesystem::path{ afterPath });
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(refreshKey, refreshSource);
    if (!Require(!bcn::runtime_assets::CachedTextureExists(afterPath),
            "deleted runtime texture cache alias was accepted as live")) return 1;
    const auto repairedPath = bcn::runtime_assets::TexturePathFromGameRelative(refreshKey, "test");
    if (!Require(repairedPath == afterPath &&
            bcn::runtime_assets::CachedTextureExists(repairedPath),
            "deleted runtime texture cache alias was not repaired from its source")) return 1;

    // Mu Dynamic NormalMap derives optional detail, mask, and overlay files
    // from the active normal-map basename. A private cache directory prevents
    // collisions, but the basename and installed companions must survive.
    const auto normalDirectory = sandbox / "mu-normal";
    std::filesystem::create_directories(normalDirectory);
    const auto normalSource = normalDirectory / "femalebody_1_msn.dds";
    const auto detailSource = normalDirectory / "femalebody_1_n.dds";
    const auto maskSource = normalDirectory / "femalebody_1_n_m.dds";
    const auto overlaySource = normalDirectory / "femalebody_1_n_ov.dds";
    std::ofstream(normalSource, std::ios::binary) << "normal";
    std::ofstream(detailSource, std::ios::binary) << "detail";
    std::ofstream(maskSource, std::ios::binary) << "mask-a";
    std::ofstream(overlaySource, std::ios::binary) << "overlay";
    constexpr std::string_view normalKey =
        "BodySkin\\Mu\\Textures\\actors\\character\\female\\femalebody_1_msn.dds";
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(normalKey, normalSource);
    const auto normalHash = bcn::runtime_assets::SourceContentHash(normalKey);
    const auto cachedNormal = bcn::runtime_assets::TexturePathFromGameRelative(normalKey, "skin");
    const auto cachedDirectory = sandbox / "Data" /
        std::filesystem::path{ cachedNormal }.parent_path();
    if (!Require(Filename(cachedNormal) == "femalebody_1_msn.dds" &&
            std::filesystem::is_regular_file(cachedDirectory / "femalebody_1_n.dds") &&
            std::filesystem::is_regular_file(cachedDirectory / "femalebody_1_n_m.dds") &&
            std::filesystem::is_regular_file(cachedDirectory / "femalebody_1_n_ov.dds"),
            "Mu normal-map basename or optional sibling bundle was lost in the runtime cache")) return 1;
    const auto maskTime = std::filesystem::last_write_time(maskSource);
    std::ofstream(maskSource, std::ios::binary) << "mask-b";
    std::filesystem::last_write_time(maskSource, maskTime);
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(normalKey, normalSource);
    const auto changedNormalHash = bcn::runtime_assets::SourceContentHash(normalKey);
    const auto changedCachedNormal =
        bcn::runtime_assets::TexturePathFromGameRelative(normalKey, "skin");
    if (!Require(changedNormalHash != normalHash && changedCachedNormal != cachedNormal &&
            Filename(changedCachedNormal) == "femalebody_1_msn.dds",
            "a same-size/timestamp Mu mask replacement did not invalidate the normal bundle")) return 1;

    // TRX wet integrations derive a sibling wet specular from the active
    // Schlong_s location. It must follow the selected profile into the same
    // private cache without replacing the normal slot-7 texture.
    constexpr std::string_view trxSpecularKey =
        "Futanari\\CBBE TRX\\textures\\[TRX] Futa addon\\Regular\\Default\\schlong_s.dds";
    bcn::runtime_assets::ClearGameRelativeSources("Futanari\\");
    bcn::runtime_assets::RegisterGameRelativeSource(trxSpecularKey, futaTrx / "schlong_s.dds");
    const auto cachedTrxSpecular =
        bcn::runtime_assets::TexturePathFromGameRelative(trxSpecularKey, "futanari");
    const auto cachedTrxDirectory = sandbox / "Data" /
        std::filesystem::path{ cachedTrxSpecular }.parent_path();
    if (!Require(Filename(cachedTrxSpecular) == "schlong_s.dds" &&
            std::filesystem::is_regular_file(cachedTrxDirectory / "wetschlong_110_s.dds"),
            "TRX wet specular did not follow the selected futanari skin into the runtime cache")) return 1;

    Touch(sandbox / "Data" / "textures" / "BodyChangeNG" / "outside.dds");
    if (!Require(!bcn::runtime_assets::CachedTextureExists(
            "textures\\BodyChangeNG\\Cache\\..\\outside.dds") &&
            !bcn::runtime_assets::CachedTextureExists("C:\\outside.dds"),
            "runtime texture cache existence check accepted an unsafe path")) return 1;
    std::filesystem::current_path(originalCurrentPath);
    if (!Require(beforeHash != afterHash && !beforePath.empty() && beforePath != afterPath,
            "same size/time DDS edit retained content signature or texture cache alias")) return 1;
    bcn::runtime_assets::ClearGameRelativeSources("BodySkin\\");
    bcn::runtime_assets::RegisterGameRelativeSource(refreshKey, refreshSource);
    if (!Require(afterHash == bcn::runtime_assets::SourceContentHash(refreshKey),
            "unchanged refresh invalidated content hash")) return 1;

    // MO2 may expose one backing DDS both through its provider directory and
    // through the virtual Data tree. Windows path aliases for that same file
    // must share one source identity; otherwise large packs are fully hashed
    // once per catalog root. Mutating the file between alias registrations
    // makes an accidental second hash observable without exposing test-only
    // cache internals.
    const auto extendedRefreshSource = std::filesystem::path{
        std::wstring{ LR"(\\?\)" } + refreshSource.wstring()
    };
    std::ofstream(refreshSource, std::ios::binary) << "uvw";
    bcn::runtime_assets::RegisterGameRelativeSource(refreshKey, extendedRefreshSource);
    if (!Require(afterHash == bcn::runtime_assets::SourceContentHash(refreshKey),
            "one physical DDS was rehashed through a second Windows path alias")) return 1;
    std::filesystem::remove_all(sandbox);
    return 0;
}
