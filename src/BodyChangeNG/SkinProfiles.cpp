#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/RuntimeAssetCache.h"

#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <ranges>
#include <system_error>
#include <unordered_set>

namespace
{
    constexpr auto kSchemaVersion = 1;
    constexpr std::size_t kMaxProfiles = 512;
    constexpr std::size_t kMaxPathLength = 1024;
    constexpr std::uint8_t kDiffuseTextureIndex = 0U;
    constexpr std::uint8_t kNormalTextureIndex = 1U;
    // BSTextureSet uses slot 2 for subsurface/skin tint and slot 3 for the
    // FaceGen detail map.  Keeping these semantic names here avoids confusing
    // BodySlide's `_sk` suffix with the specular slot (which is slot 7).
    constexpr std::uint8_t kSkinTintTextureIndex = 2U;
    constexpr std::uint8_t kDetailTextureIndex = 3U;
    constexpr std::uint8_t kSpecularTextureIndex = 7U;

    [[nodiscard]] bool EqualsIgnoreCase(const std::string_view left, const std::string_view right)
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index{}; index < left.size(); ++index) {
            const auto lower = [](const char value) {
                return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
            };
            if (lower(left[index]) != lower(right[index])) return false;
        }
        return true;
    }

    // MO2 enumerates a merged virtual Data tree. Canonical relative() can
    // resolve an entry to its physical provider under the MO2 mods directory,
    // so logical Data children appear to escape through "..". Catalog paths
    // must stay lexical inside the virtual tree.
    [[nodiscard]] std::optional<std::filesystem::path> RelativeWithin(
        const std::filesystem::path& path, const std::filesystem::path& root)
    {
        auto relative = path.lexically_relative(root);
        if (relative.empty() || relative.is_absolute()) return std::nullopt;
        for (const auto& component : relative) {
            if (component == "..") return std::nullopt;
        }
        return relative;
    }

    [[nodiscard]] bool IsGameRelativeTexturePath(const std::filesystem::path& dataRoot,
                                                  const std::filesystem::path& profileDirectory,
                                                  std::string& path)
    {
        if (path.empty() || path.size() > kMaxPathLength) return false;
        std::ranges::replace(path, '/', '\\');
        if (path.find("..") != std::string::npos || path.find(':') != std::string::npos || path.starts_with("\\")) return false;
        if (!path.ends_with(".dds") && !path.ends_with(".DDS")) return false;

        // `Textures\\...` is deliberately profile-relative. It lets a skin pack
        // retain its original texture tree under BodySkin/<skin>/Textures.
        if (path.starts_with("Textures\\")) {
            const auto relative = RelativeWithin(profileDirectory, dataRoot);
            if (!relative) return false;
            path = bcn::path_text::GenericUtf8(*relative) + "\\" + path;
            std::ranges::replace(path, '/', '\\');
        }

        return path.starts_with("BodySkin\\") || path.starts_with("bodyskin\\") ||
            path.starts_with("textures\\") || path.starts_with("Textures\\");
    }

    [[nodiscard]] std::optional<bcn::SkinSex> ParseSex(const nlohmann::json& root)
    {
        const auto found = root.find("sex");
        if (found == root.end() || !found->is_string()) return std::nullopt;
        const auto value = found->get<std::string>();
        if (EqualsIgnoreCase(value, "female")) return bcn::SkinSex::female;
        if (EqualsIgnoreCase(value, "male")) return bcn::SkinSex::male;
        return std::nullopt;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> ParsePart(const std::filesystem::path& dataRoot,
                                                                 const std::filesystem::path& profileDirectory,
                                                                 const nlohmann::json& root, const char* key)
    {
        std::vector<bcn::SkinTextureLayer> layers;
        const auto node = root.find(key);
        if (node == root.end() || !node->is_array()) return layers;
        for (const auto& value : *node) {
            if (!value.is_object() || layers.size() >= 16U) continue;
            const auto index = value.value("index", -1);
            auto path = value.value("path", std::string{});
            if (index < 0 || index > 15 || !IsGameRelativeTexturePath(dataRoot, profileDirectory, path)) continue;
            bcn::runtime_assets::RegisterGameRelativeSource(path,
                dataRoot / bcn::path_text::FromUtf8(path));
            if (std::ranges::find(layers, static_cast<std::uint8_t>(index), &bcn::SkinTextureLayer::shaderTextureIndex) != layers.end()) {
                continue;
            }
            layers.push_back({ static_cast<std::uint8_t>(index), std::move(path) });
        }
        std::ranges::sort(layers, {}, &bcn::SkinTextureLayer::shaderTextureIndex);
        return layers;
    }

    [[nodiscard]] bcn::body_family::Mask InferProfileFamilies(
        const bcn::SkinSex sex, const std::initializer_list<const std::vector<bcn::SkinTextureLayer>*> parts)
    {
        if (sex == bcn::SkinSex::male) return bcn::StandardSkinFamilies(sex);
        bool ube{};
        bool standard{};
        for (const auto* part : parts) {
            if (!part) continue;
            for (const auto& layer : *part) {
                switch (bcn::body_family::DetectSkinTextureLayout(
                    layer.path, bcn::body_family::Sex::female)) {
                case bcn::body_family::SkinTextureLayout::ube:
                    ube = true;
                    break;
                case bcn::body_family::SkinTextureLayout::standard:
                    standard = true;
                    break;
                default:
                    break;
                }
            }
        }
        // A mixed explicit profile would paint UBE and conventional UVs in a
        // single operation. Reject it instead of guessing which half was the
        // author's intent.
        if (ube && standard) return 0U;
        return ube ? bcn::body_family::Bit(bcn::body_family::Family::ube) :
            bcn::StandardSkinFamilies(sex);
    }

    [[nodiscard]] std::optional<bcn::SkinProfile> ParseProfile(const std::filesystem::path& dataRoot,
                                                                const std::filesystem::path& root,
                                                                const std::filesystem::path& path)
    {
        try {
            std::ifstream stream(path);
            const auto json = nlohmann::json::parse(stream);
            if (!json.is_object() || json.value("schemaVersion", 0) != kSchemaVersion) return std::nullopt;
            auto id = json.value("id", std::string{});
            if (id.empty()) {
                const auto relative = RelativeWithin(path.parent_path(), root);
                if (!relative) return std::nullopt;
                id = bcn::path_text::GenericUtf8(*relative);
            }
            auto name = json.value("name", std::string{});
            if (name.empty()) name = id;
            if (id.size() > 256U || name.size() > 512U) return std::nullopt;
            const auto sex = ParseSex(json);
            if (!sex) {
                SKSE::log::warn("Body Change NG ignored skin profile {}: sex must be 'female' or 'male'", bcn::path_text::Utf8(path));
                return std::nullopt;
            }
            auto body = ParsePart(dataRoot, path.parent_path(), json, "body");
            auto hands = ParsePart(dataRoot, path.parent_path(), json, "hands");
            auto feet = ParsePart(dataRoot, path.parent_path(), json, "feet");
            auto face = ParsePart(dataRoot, path.parent_path(), json, "face");
            auto vampireFace = ParsePart(dataRoot, path.parent_path(), json, "vampireFace");
            auto faceDetails = ParsePart(dataRoot, path.parent_path(), json, "faceDetails");
            const auto bodyFamilies = InferProfileFamilies(*sex,
                { &body, &hands, &feet, &face, &vampireFace, &faceDetails });
            if (bodyFamilies == 0U) return std::nullopt;
            // Partial packs are intentional. Every absent body part and every
            // absent material channel keeps the actor's underlying texture;
            // only explicitly supplied DDS files become overrides.
            // A loose blank/detail map is not enough to infer the profile's
            // sex and would create a phantom opposite-sex row in a pack.
            if (body.empty() && hands.empty() && feet.empty() && face.empty() &&
                vampireFace.empty()) return std::nullopt;
            return bcn::SkinProfile{
                .id = std::move(id),
                .name = std::move(name),
                .sex = *sex,
                .bodyFamilies = bodyFamilies,
                .body = std::move(body),
                .hands = std::move(hands),
                .feet = std::move(feet),
                .face = std::move(face),
                .vampireFace = std::move(vampireFace),
                .faceDetails = std::move(faceDetails),
                .source = path
            };
        } catch (const std::exception& exception) {
            SKSE::log::warn("Body Change NG ignored skin profile {}: {}", bcn::path_text::Utf8(path), exception.what());
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<std::filesystem::path> FindTextureDirectory(const std::filesystem::path& skinDirectory)
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(skinDirectory, error), end; !error && it != end; it.increment(error)) {
            if (it->is_directory(error) && EqualsIgnoreCase(bcn::path_text::Utf8(it->path().filename()), "Textures")) return it->path();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::filesystem::path> FindTextureFile(const std::filesystem::path& textureDirectory,
                                                                        const std::string_view filename)
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(textureDirectory,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (it->is_regular_file(statusError) && !statusError &&
                EqualsIgnoreCase(bcn::path_text::Utf8(it->path().filename()), filename)) {
                return it->path();
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool IsAutoTextureAnchor(const std::string_view filename)
    {
        constexpr std::array standardStems{
            std::string_view{ "femalebody_1" }, std::string_view{ "femalehands_1" },
            std::string_view{ "femalefeet_1" }, std::string_view{ "femalehead" },
            std::string_view{ "femaleheadvampire" },
            std::string_view{ "malebody_1" }, std::string_view{ "malehands_1" },
            std::string_view{ "malefeet_1" }, std::string_view{ "malehead" },
            std::string_view{ "maleheadvampire" }
        };
        constexpr std::array standardSuffixes{
            std::string_view{ ".dds" }, std::string_view{ "_msn.dds" },
            std::string_view{ "_sk.dds" }, std::string_view{ "_s.dds" }
        };
        for (const auto stem : standardStems) {
            for (const auto suffix : standardSuffixes) {
                if (EqualsIgnoreCase(filename, std::string{ stem } + std::string{ suffix })) return true;
            }
        }
        constexpr std::array ubeStems{
            std::string_view{ "femalebody_1" }, std::string_view{ "femalehead" }
        };
        constexpr std::array ubeSuffixes{
            std::string_view{ "_d.dds" }, std::string_view{ "_n.dds" },
            std::string_view{ "_sk.dds" }
        };
        for (const auto stem : ubeStems) {
            for (const auto suffix : ubeSuffixes) {
                if (EqualsIgnoreCase(filename, std::string{ stem } + std::string{ suffix })) return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::vector<std::filesystem::path> FindTextureSetDirectories(const std::filesystem::path& skinDirectory)
    {
        std::vector<std::filesystem::path> directories;
        std::error_code error;
        // Do not require an immediate `Textures` child.  Skin packs commonly
        // keep the original Bethesda tree (Textures/actors/.../female), while
        // others use BodyChange-style CustomSet folders.  Discover a set from
        // the actual body diffuse file rather than querying every traversed
        // directory: on MO2's virtual filesystem, directory metadata can be
        // incomplete even when its files are available to Skyrim.
        for (std::filesystem::recursive_directory_iterator it(skinDirectory,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (!it->is_regular_file(statusError) || statusError) continue;
            const auto filename = bcn::path_text::Utf8(it->path().filename());
            if (!IsAutoTextureAnchor(filename)) continue;
            const auto directory = it->path().parent_path();
            if (std::ranges::find(directories, directory) == directories.end()) {
                directories.push_back(directory);
            }
        }
        std::ranges::sort(directories);
        return directories;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> AutoPart(const std::filesystem::path& dataRoot,
                                                               const std::filesystem::path& textureDirectory,
                                                               const std::string_view stem)
    {
        // Skyrim's BSTextureSet convention is TX00 diffuse, TX01 normal,
        // TX02 subsurface/skin tint and TX07 specular. `_sk` is not a
        // specular map; `_s` is.
        const std::array<std::pair<std::string, std::uint8_t>, 4> candidates{
            std::pair{ std::string{ stem } + ".dds", kDiffuseTextureIndex },
            std::pair{ std::string{ stem } + "_msn.dds", kNormalTextureIndex },
            std::pair{ std::string{ stem } + "_sk.dds", kSkinTintTextureIndex },
            std::pair{ std::string{ stem } + "_s.dds", kSpecularTextureIndex }
        };
        std::vector<bcn::SkinTextureLayer> layers;
        for (const auto& [filename, index] : candidates) {
            const auto file = FindTextureFile(textureDirectory, filename);
            if (!file) continue;
            const auto relative = RelativeWithin(*file, dataRoot);
            if (!relative) continue;
            auto path = bcn::path_text::GenericUtf8(*relative);
            if (!IsGameRelativeTexturePath(dataRoot, textureDirectory, path)) continue;
            bcn::runtime_assets::RegisterGameRelativeSource(path, *file);
            layers.push_back({ index, std::move(path) });
        }
        return layers;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> AutoUbePart(
        const std::filesystem::path& dataRoot, const std::filesystem::path& textureDirectory,
        const std::string_view stem)
    {
        // UBE's PBR-aware body/head materials use explicit diffuse, normal,
        // and skin/subsurface names. RFAOS/wet companions are deliberately
        // not forced into a guessed BSTextureSet slot; those remain owned by
        // the active material/PBR setup.
        const std::array<std::pair<std::string, std::uint8_t>, 3> candidates{
            std::pair{ std::string{ stem } + "_d.dds", kDiffuseTextureIndex },
            std::pair{ std::string{ stem } + "_n.dds", kNormalTextureIndex },
            std::pair{ std::string{ stem } + "_sk.dds", kSkinTintTextureIndex }
        };
        std::vector<bcn::SkinTextureLayer> layers;
        for (const auto& [filename, index] : candidates) {
            const auto file = FindTextureFile(textureDirectory, filename);
            if (!file) continue;
            const auto relative = RelativeWithin(*file, dataRoot);
            if (!relative) continue;
            auto path = bcn::path_text::GenericUtf8(*relative);
            if (!IsGameRelativeTexturePath(dataRoot, textureDirectory, path)) continue;
            bcn::runtime_assets::RegisterGameRelativeSource(path, *file);
            layers.push_back({ index, std::move(path) });
        }
        return layers;
    }

    [[nodiscard]] std::optional<std::filesystem::path> FindChildDirectory(
        const std::filesystem::path& parent, const std::string_view name)
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(parent,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (it->is_directory(statusError) && !statusError &&
                EqualsIgnoreCase(bcn::path_text::Utf8(it->path().filename()), name)) return it->path();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<bcn::SkinTextureLayer> AutoFaceDetails(
        const std::filesystem::path& dataRoot, const std::filesystem::path& textureDirectory,
        const bool female)
    {
        std::vector<bcn::SkinTextureLayer> layers;
        const auto prefix = female ? std::string{ "femaleheaddetail_" } : std::string{ "maleheaddetail_" };
        std::error_code error;
        for (std::filesystem::directory_iterator it(textureDirectory,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (!it->is_regular_file(statusError) || statusError) continue;
            auto filename = bcn::path_text::Utf8(it->path().filename());
            std::ranges::transform(filename, filename.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (!filename.ends_with(".dds") ||
                (!filename.starts_with(prefix) && filename != "blankdetailmap.dds")) continue;
            const auto relative = RelativeWithin(it->path(), dataRoot);
            if (!relative) continue;
            auto path = bcn::path_text::GenericUtf8(*relative);
            if (!IsGameRelativeTexturePath(dataRoot, textureDirectory, path)) continue;
            bcn::runtime_assets::RegisterGameRelativeSource(path, it->path());
            layers.push_back({ kDetailTextureIndex, std::move(path) });
        }
        std::ranges::sort(layers, {}, &bcn::SkinTextureLayer::path);
        return layers;
    }

    [[nodiscard]] std::vector<bcn::SkinProfile> AutoProfiles(const std::filesystem::path& dataRoot,
                                                              const std::filesystem::path& root,
                                                              const std::filesystem::path& skinDirectory,
                                                              const std::filesystem::path& textureRoot,
                                                              const std::filesystem::path& textureDirectory,
                                                              const bcn::SkinSex sex)
    {
        const auto female = sex == bcn::SkinSex::female;
        const auto layout = bcn::body_family::DetectSkinTextureLayout(
            bcn::path_text::GenericUtf8(textureDirectory),
            female ? bcn::body_family::Sex::female : bcn::body_family::Sex::male);
        if (layout == bcn::body_family::SkinTextureLayout::ube) {
            if (!female) return {};
            // A partial UBE pack may contain only Body or only Head. Resolve
            // both sibling atlases from whichever directory supplied the
            // discovery anchor.
            const auto atlasRoot = textureDirectory.parent_path();
            const auto bodyDirectory = FindChildDirectory(atlasRoot, "Body");
            const auto headDirectory = FindChildDirectory(atlasRoot, "Head");
            auto body = bodyDirectory ? AutoUbePart(dataRoot, *bodyDirectory, "femalebody_1") :
                std::vector<bcn::SkinTextureLayer>{};
            auto face = headDirectory ? AutoUbePart(dataRoot, *headDirectory, "femalehead") :
                std::vector<bcn::SkinTextureLayer>{};
            if (body.empty() && face.empty()) return {};
            const auto skinPath = RelativeWithin(skinDirectory, root);
            if (!skinPath) return {};
            const auto skinRelative = bcn::path_text::GenericUtf8(*skinPath);
            if (skinRelative.empty()) return {};
            return { bcn::SkinProfile{
                .id = "auto:" + skinRelative + ":female:ube",
                .name = bcn::path_text::Utf8(skinDirectory.filename()),
                .sex = bcn::SkinSex::female,
                .bodyFamilies = bcn::body_family::Bit(bcn::body_family::Family::ube),
                .body = std::move(body),
                .face = std::move(face),
                .source = textureDirectory
            } };
        }
        auto body = AutoPart(dataRoot, textureDirectory, female ? "femalebody_1" : "malebody_1");
        auto hands = AutoPart(dataRoot, textureDirectory, female ? "femalehands_1" : "malehands_1");
        auto feet = AutoPart(dataRoot, textureDirectory, female ? "femalefeet_1" : "malefeet_1");
        auto face = AutoPart(dataRoot, textureDirectory, female ? "femalehead" : "malehead");
        auto vampireFace = AutoPart(dataRoot, textureDirectory, female ? "femaleheadvampire" : "maleheadvampire");
        auto faceDetails = AutoFaceDetails(dataRoot, textureDirectory, female);
        // Detail maps augment a discovered face/body part but never create a
        // profile by themselves because blankdetailmap.dds is sex-ambiguous.
        if (body.empty() && hands.empty() && feet.empty() && face.empty() &&
            vampireFace.empty()) return {};
        const auto skinPath = RelativeWithin(skinDirectory, root);
        const auto setPath = RelativeWithin(textureDirectory, textureRoot);
        if (!skinPath || !setPath) return {};
        const auto skinRelative = bcn::path_text::GenericUtf8(*skinPath);
        if (skinRelative.empty()) return {};
        const auto suffix = female ? "female" : "male";
        // The user-facing unit is one top-level BodySkin folder per sex.  A
        // skin pack may contain optional detail maps or an extra nested set
        // directory, but those files must never explode into separate rows.
        // The stable id therefore belongs to the pack folder, not to an
        // individual DDS or nested texture directory.
        const auto baseID = "auto:" + skinRelative + ":" + suffix;
        const auto baseName = bcn::path_text::Utf8(skinDirectory.filename());
        return { bcn::SkinProfile{
            .id = baseID,
            .name = baseName,
            .sex = sex,
            .bodyFamilies = bcn::StandardSkinFamilies(sex),
            .body = std::move(body),
            .hands = std::move(hands),
            .feet = std::move(feet),
            .face = std::move(face),
            .vampireFace = std::move(vampireFace),
            .faceDetails = std::move(faceDetails),
            .source = textureDirectory
        } };
    }

    [[nodiscard]] std::string NormalizedGamePath(std::string path)
    {
        std::ranges::replace(path, '/', '\\');
        std::ranges::transform(path, path.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return path;
    }

    void AddMappedLayers(std::unordered_set<std::string>& paths,
        const std::vector<bcn::SkinTextureLayer>& layers)
    {
        for (const auto& layer : layers) paths.insert(NormalizedGamePath(layer.path));
    }

    [[nodiscard]] std::optional<std::filesystem::path> ProfilePackRoot(
        const std::filesystem::path& root, const bcn::SkinProfile& profile)
    {
        auto source = profile.source;
        std::error_code error;
        if (std::filesystem::is_regular_file(source, error)) source = source.parent_path();
        const auto relative = RelativeWithin(source, root);
        if (!relative) return std::nullopt;
        const auto first = relative->begin();
        if (first == relative->end()) return std::nullopt;
        return root / *first;
    }

    void AuditProfileDds(const std::filesystem::path& dataRoot,
        const std::filesystem::path& root, const bcn::SkinProfile& profile)
    {
        const auto packRoot = ProfilePackRoot(root, profile);
        if (!packRoot) return;

        std::unordered_set<std::string> activePaths;
        AddMappedLayers(activePaths, profile.body);
        AddMappedLayers(activePaths, profile.hands);
        AddMappedLayers(activePaths, profile.feet);
        AddMappedLayers(activePaths, profile.face);
        std::unordered_set<std::string> conditionalPaths;
        AddMappedLayers(conditionalPaths, profile.vampireFace);
        AddMappedLayers(conditionalPaths, profile.faceDetails);
        for (const auto& path : activePaths) conditionalPaths.erase(path);

        std::size_t total{};
        std::size_t active{};
        std::size_t conditional{};
        std::size_t unmapped{};
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(*packRoot,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (!it->is_regular_file(statusError) || statusError ||
                !EqualsIgnoreCase(bcn::path_text::Utf8(it->path().extension()), ".dds")) continue;
            ++total;
            const auto relative = RelativeWithin(it->path(), dataRoot);
            const auto gamePath = relative ?
                NormalizedGamePath(bcn::path_text::GenericUtf8(*relative)) : std::string{};
            const auto classification = activePaths.contains(gamePath) ? "active-material-slot" :
                conditionalPaths.contains(gamePath) ? "conditional-face-slot" : "unmapped-extra";
            if (activePaths.contains(gamePath)) ++active;
            else if (conditionalPaths.contains(gamePath)) ++conditional;
            else ++unmapped;
            SKSE::log::info(
                "SkinCatalogAudit pack='{}' sex={} file='{}' mapping={}",
                profile.name, profile.sex == bcn::SkinSex::female ? "female" : "male",
                bcn::path_text::Utf8(it->path()), classification);
        }
        SKSE::log::info(
            "SkinCatalogAudit pack='{}' sex={} dds-total={} active-slot-dds={} conditional-dds={} unmapped-dds={} body={} hands={} feet={} face={} vampire={} details={}",
            profile.name, profile.sex == bcn::SkinSex::female ? "female" : "male", total, active,
            conditional, unmapped, profile.body.size(), profile.hands.size(), profile.feet.size(),
            profile.face.size(), profile.vampireFace.size(), profile.faceDetails.size());
    }
}

namespace bcn
{
    std::string SkinFamilyLabel(const body_family::Mask families, const SkinSex sex)
    {
        if (sex == SkinSex::male) return "Male";
        if (families == body_family::Bit(body_family::Family::ube)) return "UBE";
        const auto conventional = StandardSkinFamilies(SkinSex::female);
        if ((families & conventional) != 0U) return "CBBE 3BA / BHUNP / UNP";
        return "Unclassified";
    }

    SkinProfiles& SkinProfiles::Get()
    {
        static SkinProfiles profiles;
        return profiles;
    }

    std::filesystem::path SkinProfiles::RootPath()
    {
        return std::filesystem::current_path() / "Data" / "BodySkin";
    }

    std::vector<SkinProfile> SkinProfiles::ScanDirectory(const std::filesystem::path& root)
    {
        const auto dataRoot = root.parent_path();
        std::vector<SkinProfile> loaded;
        std::error_code error;
        if (std::filesystem::exists(root, error)) {
            for (std::filesystem::recursive_directory_iterator it(root,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                std::error_code statusError;
                if (!it->is_regular_file(statusError) || statusError ||
                    !EqualsIgnoreCase(bcn::path_text::Utf8(it->path().filename()), "profile.json")) continue;
                if (loaded.size() >= kMaxProfiles) break;
                if (const auto profile = ParseProfile(dataRoot, root, it->path())) {
                    if (std::ranges::find(loaded, profile->id, &SkinProfile::id) == loaded.end()) {
                        loaded.push_back(*profile);
                    } else {
                        SKSE::log::warn("Body Change NG ignored duplicate skin profile id '{}'", profile->id);
                    }
                }
            }
            error.clear();
            for (std::filesystem::directory_iterator it(root,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                std::error_code statusError;
                if (!it->is_directory(statusError) || statusError || loaded.size() >= kMaxProfiles) continue;
                const auto skinDirectory = it->path();
                if (std::filesystem::exists(skinDirectory / "profile.json")) continue;
                for (const auto& setDirectory : FindTextureSetDirectories(skinDirectory)) {
                    if (loaded.size() >= kMaxProfiles) break;
                    for (const auto sex : { SkinSex::female, SkinSex::male }) {
                        if (loaded.size() >= kMaxProfiles) break;
                        for (const auto& profile : AutoProfiles(dataRoot, root, skinDirectory, skinDirectory, setDirectory, sex)) {
                            if (loaded.size() >= kMaxProfiles) break;
                            if (std::ranges::find(loaded, profile.id, &SkinProfile::id) == loaded.end()) {
                                loaded.push_back(profile);
                            }
                        }
                    }
                }
            }
        }
        if (error) SKSE::log::warn("Body Change NG could not scan {}: {}", bcn::path_text::Utf8(root), error.message());
        for (const auto& profile : loaded) AuditProfileDds(dataRoot, root, profile);
        std::ranges::sort(loaded, {}, &SkinProfile::name);
        return loaded;
    }

    void SkinProfiles::Refresh()
    {
        const auto root = RootPath();
        runtime_assets::ClearGameRelativeSources("BodySkin\\");
        std::vector<SkinProfile> loaded;
        for (const auto& scanRoot : catalog_roots::Discover(root)) {
            for (auto& profile : ScanDirectory(scanRoot)) {
                if (const auto found = std::ranges::find(loaded, profile.id, &SkinProfile::id);
                    found != loaded.end()) *found = std::move(profile);
                else loaded.push_back(std::move(profile));
            }
        }
        std::ranges::sort(loaded, {}, &SkinProfile::name);
        std::scoped_lock lock(lock_);
        profiles_ = std::move(loaded);
        SKSE::log::info("Body Change NG loaded {} shared texture skin profiles from {}", profiles_.size(), bcn::path_text::Utf8(root));
    }

    std::vector<SkinProfile> SkinProfiles::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return profiles_;
    }

    std::optional<SkinProfile> SkinProfiles::Find(const std::string_view id) const
    {
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(profiles_, id, &SkinProfile::id);
        return found != profiles_.end() ? std::optional<SkinProfile>{ *found } : std::nullopt;
    }

    std::vector<std::string> SkinProfiles::CompatibleIds(
        const std::vector<std::string>& ids, const SkinSex sex,
        const body_family::Mask actorFamily) const
    {
        std::scoped_lock lock(lock_);
        std::vector<std::string> compatible;
        compatible.reserve(ids.size());
        for (const auto& id : ids) {
            const auto found = std::ranges::find(profiles_, id, &SkinProfile::id);
            if (found != profiles_.end() && found->sex == sex &&
                SkinMatchesActor(found->bodyFamilies, actorFamily)) compatible.push_back(id);
        }
        return compatible;
    }
}
