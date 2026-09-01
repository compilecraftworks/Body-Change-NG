#include "BodyChangerNG/PlayerTint.h"
#include "BodyChangerNG/SkinProfiles.h"
#include "BodyChangerNG/CatalogRoots.h"

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
            std::string_view{ "femalebody_1" }, std::string_view{ "femalehands_1" },
            std::string_view{ "femalefeet_1" }, std::string_view{ "femalehead" },
            std::string_view{ "femaleheadvampire" }, std::string_view{ "malebody_1" },
            std::string_view{ "malehands_1" }, std::string_view{ "malefeet_1" },
            std::string_view{ "malehead" }, std::string_view{ "maleheadvampire" }
        };
        if (filename == "blankdetailmap.dds" || filename.starts_with("femaleheaddetail_") ||
            filename.starts_with("maleheaddetail_")) return filename.ends_with(".dds");
        for (const auto stem : stems) {
            if (filename == std::string{ stem } + ".dds" ||
                filename == std::string{ stem } + "_msn.dds" ||
                filename == std::string{ stem } + "_sk.dds" ||
                filename == std::string{ stem } + "_s.dds") return true;
        }
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

    const auto sandbox = std::filesystem::temp_directory_path() / "BodyChangerNGAssetCatalogTests";
    std::filesystem::remove_all(sandbox);

    const std::string skinPackName{ "피부팩 简体" };
    const auto female = sandbox / "BodySkin" / std::filesystem::path{ L"피부팩 简体" } /
        "Textures" / "actors" / "character" / "female";
    for (const auto* file : {
             "femalebody_1.dds", "femalebody_1_msn.dds", "femalebody_1_sk.dds", "femalebody_1_s.dds",
             "femalehands_1.dds", "femalehead.dds", "femalehead_msn.dds", "femalehead_s.dds",
             "blankdetailmap.dds", "femaleheaddetail_frekles.DDS" }) {
        Touch(female / file);
    }

    const auto skins = bcn::SkinProfiles::ScanDirectory(sandbox / "BodySkin");
    const auto discoveredSkinRoots = bcn::catalog_roots::Discover(sandbox / "BodySkin");
    std::error_code equivalentError;
    if (!Require(!discoveredSkinRoots.empty() &&
            std::filesystem::equivalent(discoveredSkinRoots.front(), sandbox / "BodySkin", equivalentError) &&
            !equivalentError,
            "catalog root discovery climbed above the physical BodySkin provider")) return 1;
    if (!Require(skins.size() == 1U, "skin scanner did not collapse one top-level folder into one pack row")) return 1;
    for (const auto& skin : skins) {
        if (!Require(skin.sex == bcn::SkinSex::female, "skin scanner leaked the pack into the wrong sex")) return 1;
        if (!Require(!skin.body.empty() && !skin.hands.empty() && !skin.face.empty(), "complete skin lost a required part")) return 1;
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

    const std::string tintPackName{ "틴트包" };
    const auto tintA = sandbox / "TintMask" / std::filesystem::path{ L"틴트包" } / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintA / "femalehead_lips.DDS");
    Touch(tintA / "femaleheadhuman_nose.dds");
    Touch(tintA / "not-a-tint.dds");
    const auto tintB = sandbox / "TintMask" / "Pack B" / "textures" / "actors" / "character" /
        "character assets" / "tintmasks";
    Touch(tintB / "maleheadnord_lips.dds");

    const auto tints = bcn::player_tint::Catalog::ScanDirectory(sandbox / "TintMask");
    if (!Require(tints.size() == 3U, "tint scanner did not classify the expected DDS files")) return 1;
    if (!Require(std::ranges::any_of(tints, [&](const auto& tint) { return tint.pack == tintPackName; }),
            "tint pack name was not preserved as UTF-8")) return 1;
    for (const auto& tint : tints) {
        if (!Require(tint.path.starts_with("TintMask\\"), "tint path escaped the virtual Data root")) return 1;
        if (!Require(!tint.id.starts_with(".."), "tint id contains a parent-directory escape")) return 1;
    }

    std::filesystem::remove_all(sandbox);
    return 0;
}
