#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/ContentSignature.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/PathText.h"

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using namespace std::literals;

    [[nodiscard]] std::string ToLowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    [[nodiscard]] bool IsUnpSet(const std::string& value)
    {
        const auto lowered = ToLowerAscii(value);
        return lowered.contains("unp") || lowered.contains("coco") || lowered.contains("bhunp") || lowered.contains("uunp");
    }

    [[nodiscard]] bool IsMaleSet(const std::string& value)
    {
        constexpr std::array markers{ "himbo"sv, "talos"sv, "sam"sv, "sos"sv, "savren"sv };
        const auto lowered = ToLowerAscii(value);
        return std::ranges::any_of(markers, [&lowered](const auto marker) { return lowered.contains(marker); });
    }

    [[nodiscard]] bool IsDefaultUnpSlider(const std::string_view name)
    {
        constexpr std::array names{ "Breasts"sv, "BreastsSmall"sv, "NippleDistance"sv, "NippleSize"sv,
            "ButtCrack"sv, "Butt"sv, "ButtSmall"sv, "Legs"sv, "Arms"sv, "ShoulderWidth"sv };
        return std::ranges::find(names, name) != names.end();
    }

    void AddSlider(bcn::BodyPreset& preset, const std::string_view name, const float value, const bool large)
    {
        if (name.empty()) return;
        const auto found = std::ranges::find(preset.sliders, name, &bcn::BodySlider::name);
        if (found == preset.sliders.end()) {
            preset.sliders.push_back({ .name = std::string(name) });
            if (large) preset.sliders.back().highWeight = value;
            else preset.sliders.back().lowWeight = value;
            return;
        }
        if (large) found->highWeight = value;
        else found->lowWeight = value;
    }

    [[nodiscard]] std::optional<bcn::BodyPreset> ParsePreset(const pugi::xml_node& node, const std::string& source)
    {
        const auto name = std::string(node.attribute("name").as_string());
        const auto bodySet = std::string(node.attribute("set").as_string());
        const auto isRefit = name.ends_with("-Refit");
        // Preset packs are frequently distributed inside outfit mods, and
        // valid reusable body presets often retain the outfit project name in
        // their name, set or source path.  Never discard a preset merely
        // because any of those strings contain clothing-related words.
        if (name.empty()) return std::nullopt;

        std::string groupNames;
        for (const auto group : node.children("Group")) {
            const auto groupName = std::string_view(group.attribute("name").as_string());
            if (groupName.empty()) continue;
            if (!groupNames.empty()) groupNames.push_back(' ');
            groupNames.append(groupName);
        }
        const auto classification = bcn::body_family::ClassifyPreset(bodySet, name, source, groupNames);

        bcn::BodyPreset preset{
            .name = name,
            .source = source,
            .family = bcn::body_family::PresetFamilyLabel(classification),
            .bodySet = bodySet,
            .isRefit = isRefit,
            .male = classification.families != 0U ? classification.male :
                (IsMaleSet(bodySet) || IsMaleSet(name) || IsMaleSet(source))
        };
        const auto invertUnp = IsUnpSet(bodySet);
        for (const auto slider : node.children("SetSlider")) {
            const auto sliderName = std::string_view(slider.attribute("name").as_string());
            auto value = slider.attribute("value").as_float() / 100.0F;
            if (invertUnp && IsDefaultUnpSlider(sliderName)) value = 1.0F - value;
            AddSlider(preset, sliderName, value, std::string_view(slider.attribute("size").as_string()) == "big");
        }
        return preset;
    }
}

namespace bcn
{
    std::string BodyPreset::PersistentId() const
    {
        return source + '\x1F' + name;
    }

    PresetCatalog& PresetCatalog::Get()
    {
        static PresetCatalog catalog;
        return catalog;
    }

    std::filesystem::path PresetCatalog::BodySlidePresetDirectory()
    {
        return std::filesystem::current_path() / "Data" / "CalienteTools" / "BodySlide" / "SliderPresets";
    }

    std::vector<BodyPreset> PresetCatalog::ScanDirectory(const std::filesystem::path& directory)
    {
        std::vector<BodyPreset> result;
        std::unordered_set<std::string> known;
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error) || error) return result;

        for (std::filesystem::recursive_directory_iterator iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied, error), end;
             !error && iterator != end; iterator.increment(error)) {
            const auto& entry = *iterator;
            if (!entry.is_regular_file(error) || error) {
                error.clear();
                continue;
            }
            if (ToLowerAscii(bcn::path_text::Utf8(entry.path().extension())) != ".xml") continue;
            const auto size = entry.file_size(error);
            if (error || size == 0 || size > 8U * 1024U * 1024U) {
                error.clear();
                continue;
            }

            pugi::xml_document document;
            if (!document.load_file(entry.path().c_str(), pugi::parse_default, pugi::encoding_auto)) continue;
            const auto source = bcn::path_text::GenericUtf8(entry.path().lexically_relative(directory));
            for (const auto node : document.child("SliderPresets").children("Preset")) {
                auto preset = ParsePreset(node, source);
                if (!preset) continue;
                if (known.insert(preset->PersistentId()).second) result.push_back(std::move(*preset));
            }
        }
        std::ranges::sort(result, {}, &BodyPreset::name);
        return result;
    }

    std::uint64_t BodyPreset::ContentHash() const
    {
        ContentSignature hash;
        hash.Text(family); hash.Text(bodySet); hash.Number(male); hash.Number(isRefit);
        for (const auto& slider : sliders) {
            hash.Text(slider.name); hash.Float(slider.lowWeight); hash.Float(slider.highWeight);
        }
        return hash.value;
    }

    std::optional<BodyPreset> PresetCatalog::Find(std::string_view id, bool refit) const
    {
        std::scoped_lock lock(lock_);
        const auto& catalog = refit ? refitPresets_ : presets_;
        const auto found = std::ranges::find(catalog, id, &BodyPreset::PersistentId);
        return found == catalog.end() ? std::nullopt : std::optional<BodyPreset>(*found);
    }

    std::optional<BodyPreset> PresetCatalog::FindRefit(const std::vector<std::string>& names, const bool male,
        const body_family::Mask actorFamily) const
    {
        std::scoped_lock lock(lock_);
        return SelectRefit(refitPresets_, names, male, actorFamily);
    }

    std::optional<BodyPreset> PresetCatalog::SelectRefit(
        const std::vector<BodyPreset>& presets, const std::vector<std::string>& names,
        const bool male, const body_family::Mask actorFamily)
    {
        for (const auto& name : names) {
            const auto found = std::ranges::find_if(presets, [&](const auto& preset) {
                return preset.male == male && preset.name == name &&
                    body_family::Matches(body_family::PresetMask(preset.family, preset.male), actorFamily);
            });
            if (found != presets.end()) return *found;
        }
        return {};
    }

    std::uint64_t PresetCatalog::ContentHash(std::string_view id) const
    {
        std::scoped_lock lock(lock_);
        const auto found = contentHashes_.find(std::string(id));
        return found == contentHashes_.end() ? 0 : found->second;
    }

    void PresetCatalog::Refresh()
    {
        std::vector<BodyPreset> scanned;
        for (const auto& root : catalog_roots::Discover(BodySlidePresetDirectory())) {
            for (auto& preset : ScanDirectory(root)) {
                const auto id = preset.PersistentId();
                if (const auto found = std::ranges::find(scanned, id, &BodyPreset::PersistentId);
                    found != scanned.end()) *found = std::move(preset);
                else scanned.push_back(std::move(preset));
            }
        }
        std::ranges::sort(scanned, {}, &BodyPreset::name);
        std::vector<BodyPreset> normal;
        std::vector<BodyPreset> refit;
        normal.reserve(scanned.size());
        refit.reserve(scanned.size());
        for (auto& preset : scanned) {
            preset.cachedContentHash = preset.ContentHash();
            (preset.isRefit ? refit : normal).push_back(std::move(preset));
        }
        std::array<std::size_t, 8> familyCounts{};
        const auto count = [&familyCounts](const std::vector<BodyPreset>& values) {
            for (const auto& preset : values) {
                const auto index = preset.family == "CBBE 3BA" ? 0U : preset.family == "BHUNP / UNP" ? 1U :
                    preset.family == "UBE" ? 2U : preset.family == "HIMBO" ? 3U : preset.family == "SAM" ? 4U :
                    preset.family.contains(" / ") ? 5U : preset.male ? 6U : 7U;
                ++familyCounts[index];
            }
        };
        count(normal);
        count(refit);
#if defined(BODY_CHANGE_NG_RUNTIME)
        SKSE::log::info("BodySlide preset families: CBBE/3BA={} BHUNP/UNP={} UBE={} HIMBO={} SAM={} "
                        "combined={} unknown-male={} unknown-female={} (normal={}, refit={})",
            familyCounts[0], familyCounts[1], familyCounts[2], familyCounts[3], familyCounts[4], familyCounts[5],
            familyCounts[6], familyCounts[7], normal.size(), refit.size());
#endif
        std::scoped_lock lock(lock_);
        presets_ = std::move(normal);
        refitPresets_ = std::move(refit);
        sliderUniverseCache_.clear();
        contentHashes_.clear();
        for (const auto& preset : presets_) contentHashes_[preset.PersistentId()] = preset.cachedContentHash;
        for (const auto& preset : refitPresets_) contentHashes_[preset.PersistentId()] = preset.cachedContentHash;
    }

    std::vector<BodyPreset> PresetCatalog::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return presets_;
    }

    std::vector<BodyPreset> PresetCatalog::RefitSnapshot() const
    {
        std::scoped_lock lock(lock_);
        return refitPresets_;
    }

    std::vector<std::string> PresetCatalog::CompatibleIds(
        const std::vector<std::string>& ids, const bool male,
        const body_family::Mask actorFamily) const
    {
        std::scoped_lock lock(lock_);
        std::vector<std::string> compatible;
        compatible.reserve(ids.size());
        for (const auto& id : ids) {
            const auto found = std::ranges::find(presets_, id, &BodyPreset::PersistentId);
            if (found == presets_.end() || found->male != male) continue;
            if (body_family::Matches(body_family::PresetMask(found->family, found->male), actorFamily)) {
                compatible.push_back(id);
            }
        }
        return compatible;
    }

    std::shared_ptr<const std::vector<std::string>> PresetCatalog::CompatibleSliderUniverse(
        const bool male, const body_family::Mask actorFamily) const
    {
        const auto cacheKey = (static_cast<std::uint64_t>(male) << 32U) | actorFamily;
        std::scoped_lock lock(lock_);
        if (const auto cached = sliderUniverseCache_.find(cacheKey);
            cached != sliderUniverseCache_.end()) {
            return cached->second;
        }

        auto sliderNames = std::make_shared<std::vector<std::string>>(
            CollectCompatibleSliderNames(presets_, male, actorFamily));
        std::shared_ptr<const std::vector<std::string>> result = std::move(sliderNames);
        sliderUniverseCache_.insert_or_assign(cacheKey, result);
        return result;
    }

    std::vector<std::string> PresetCatalog::CollectCompatibleSliderNames(
        const std::vector<BodyPreset>& presets, const bool male,
        const body_family::Mask actorFamily)
    {
        std::unordered_set<std::string> known;
        std::vector<std::string> sliderNames;
        const auto nonVanillaActorFamily = actorFamily & body_family::NonVanillaFamilies(
            male ? body_family::Sex::male : body_family::Sex::female);
        for (const auto& preset : presets) {
            // Unclassified presets remain visible through the normal catalog
            // fallback, and their explicitly authored sliders are still
            // applied when selected.  Do not, however, let an unrelated
            // unclassified preset expand a known 3BA/UBE/etc. actor's zeroing
            // universe and accidentally counteract another mod's morph.
            if (nonVanillaActorFamily != 0U && preset.family == "Unclassified") continue;
            if (preset.male != male ||
                !body_family::Matches(body_family::PresetMask(preset.family, preset.male), actorFamily)) {
                continue;
            }
            for (const auto& slider : preset.sliders) {
                if (known.insert(slider.name).second) sliderNames.push_back(slider.name);
            }
        }
        std::ranges::sort(sliderNames);
        return sliderNames;
    }
}
