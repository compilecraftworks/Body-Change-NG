#include "BodyChangeNG/Settings.h"

#include "BodyChangeNG/PathMigration.h"
#include "BodyChangeNG/PathText.h"

#include <SKSE/Logger.h>

#include <cmath>
#include <ranges>
#include <unordered_set>

namespace
{
    constexpr auto kSchemaVersion = 1;
    constexpr auto kMinimumUiScale = 0.75F;
    constexpr auto kMaximumUiScale = 1.50F;
    constexpr auto kMaximumWindowCoordinate = 32768.0F;

    [[nodiscard]] std::filesystem::path LegacySettingsPath()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "BodyChangerNG" / "settings.json";
    }

    template <class T>
    void ReadIfPresent(const nlohmann::json& object, const char* key, T& destination)
    {
        if (const auto found = object.find(key); found != object.end()) {
            destination = found->get<T>();
        }
    }

    [[nodiscard]] bool IsSupportedScale(const float value)
    {
        return std::isfinite(value) && value >= kMinimumUiScale && value <= kMaximumUiScale;
    }

    [[nodiscard]] bool IsSupportedWindowCoordinate(const float value)
    {
        return std::isfinite(value) && std::abs(value) <= kMaximumWindowCoordinate;
    }

    void NormalizeFavorites(std::vector<std::string>& favorites, const bool skin)
    {
        std::unordered_set<std::string> seen;
        std::vector<std::string> normalized;
        normalized.reserve(favorites.size());
        for (auto id : favorites) {
            // v0.2.0 generated one skin id per nested texture directory. The
            // catalog now deliberately exposes one row per top-level pack and
            // sex, so migrate old ids instead of making the saved star appear
            // dead after an upgrade.
            if (skin && id.starts_with("auto:")) {
                const auto sexSeparator = id.find_last_of(':');
                if (sexSeparator != std::string::npos) {
                    const auto sex = id.substr(sexSeparator + 1U);
                    if (sex == "female" || sex == "male") {
                        const auto packSeparator = id.find(':', 5U);
                        if (packSeparator != std::string::npos && packSeparator < sexSeparator) {
                            id = id.substr(0U, packSeparator) + ':' + sex;
                        }
                    }
                }
            }
            if (seen.insert(id).second) normalized.push_back(std::move(id));
        }
        favorites = std::move(normalized);
    }
}

namespace bcn
{
    Settings& Settings::Get()
    {
        static Settings settings;
        return settings;
    }

    std::filesystem::path Settings::Path()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "BodyChangeNG" / "settings.json";
    }

    void Settings::Load()
    {
        const auto path = Path();
        const auto source = path_migration::ResolveFile(path, LegacySettingsPath());
        std::scoped_lock lock(lock_);
        data_ = {};
        if (!std::filesystem::exists(source.path)) {
            SKSE::log::info("Body Change NG uses the default opening shortcut {}", data_.openHotkey.DisplayName());
            return;
        }

        try {
            std::ifstream stream(source.path);
            const auto root = nlohmann::json::parse(stream);
            const auto version = root.value("schemaVersion", 0);
            if (version != kSchemaVersion) {
                SKSE::log::warn("Body Change NG ignored unsupported settings schema {}", version);
                return;
            }
            const auto& hotkey = root.at("openHotkey");
            data_.openHotkey.key = hotkey.value("key", data_.openHotkey.key);
            data_.openHotkey.ctrl = hotkey.value("ctrl", false);
            data_.openHotkey.shift = hotkey.value("shift", false);
            data_.openHotkey.alt = hotkey.value("alt", false);
            if (!data_.openHotkey.IsValid()) data_.openHotkey = {};

            int language = static_cast<int>(data_.language);
            int position = static_cast<int>(data_.characterPosition);
            ReadIfPresent(root, "language", language);
            ReadIfPresent(root, "characterPosition", position);
            if (language >= static_cast<int>(UiLanguage::automatic) &&
                language <= static_cast<int>(UiLanguage::chineseSimplified)) {
                data_.language = static_cast<UiLanguage>(language);
            }
            if (position >= static_cast<int>(CharacterPosition::left) &&
                position <= static_cast<int>(CharacterPosition::disabled)) {
                data_.characterPosition = static_cast<CharacterPosition>(position);
            }
            if (const auto found = root.find("textScale"); found != root.end() && found->is_number()) {
                const auto scale = found->get<float>();
                if (IsSupportedScale(scale)) data_.textScale = scale;
            }
            if (const auto found = root.find("mainWindowPosition"); found != root.end() && found->is_object()) {
                const auto x = found->find("x");
                const auto y = found->find("y");
                if (x != found->end() && y != found->end() && x->is_number() && y->is_number()) {
                    const auto windowX = x->get<float>();
                    const auto windowY = y->get<float>();
                    if (IsSupportedWindowCoordinate(windowX) && IsSupportedWindowCoordinate(windowY)) {
                        data_.mainWindowPositionSet = true;
                        data_.mainWindowPositionX = windowX;
                        data_.mainWindowPositionY = windowY;
                    }
                }
            }
            ReadIfPresent(root, "pauseGameWhenOpen", data_.pauseGameWhenOpen);
            if (const auto positions = root.find("popupPositions");
                positions != root.end() && positions->is_object()) {
                for (std::size_t index{}; index < popup_placement::keys.size(); ++index) {
                    const auto entry = positions->find(popup_placement::keys[index]);
                    if (entry == positions->end() || !entry->is_object()) continue;
                    const auto x = entry->find("x");
                    const auto y = entry->find("y");
                    if (x == entry->end() || y == entry->end() ||
                        !x->is_number() || !y->is_number()) continue;
                    const auto px = x->get<float>();
                    const auto py = y->get<float>();
                    if (popup_placement::Valid(px, py)) data_.popupPositions[index] = { true, px, py };
                }
            }
            ReadIfPresent(root, "performanceMode", data_.performanceMode);
            ReadIfPresent(root, "preserveOtherMorphs", data_.preserveOtherMorphs);
            int femaleNpcBodyType = static_cast<int>(data_.femaleNpcBodyType);
            int maleNpcBodyType = static_cast<int>(data_.maleNpcBodyType);
            ReadIfPresent(root, "femaleNpcBodyType", femaleNpcBodyType);
            ReadIfPresent(root, "maleNpcBodyType", maleNpcBodyType);
            if (femaleNpcBodyType >= static_cast<int>(FemaleNpcBodyType::cbbe3ba) &&
                femaleNpcBodyType <= static_cast<int>(FemaleNpcBodyType::vanilla)) {
                data_.femaleNpcBodyType = static_cast<FemaleNpcBodyType>(femaleNpcBodyType);
            }
            if (maleNpcBodyType >= static_cast<int>(MaleNpcBodyType::himbo) &&
                maleNpcBodyType <= static_cast<int>(MaleNpcBodyType::vanilla)) {
                data_.maleNpcBodyType = static_cast<MaleNpcBodyType>(maleNpcBodyType);
            }
            ReadIfPresent(root, "orefitEnabled", data_.orefitEnabled);
            ReadIfPresent(root, "orefitNippleMorphing", data_.orefitNippleMorphing);
            ReadIfPresent(root, "nippleRandomization", data_.nippleRandomization);
            ReadIfPresent(root, "genitalRandomization", data_.genitalRandomization);
            if (const auto found = root.find("favoriteBodyPresets"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteBodyPresets.size() < 4096U) {
                        data_.favoriteBodyPresets.push_back(value.get<std::string>());
                    }
                }
            }
            if (const auto found = root.find("favoriteSkinProfiles"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteSkinProfiles.size() < 4096U) {
                        data_.favoriteSkinProfiles.push_back(value.get<std::string>());
                    }
                }
            }
            if (const auto found = root.find("favoriteFutanariSkins"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteFutanariSkins.size() < 4096U) {
                        data_.favoriteFutanariSkins.push_back(value.get<std::string>());
                    }
                }
            }
            if (const auto found = root.find("favoriteTintPacks"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteTintPacks.size() < 4096U) {
                        data_.favoriteTintPacks.push_back(value.get<std::string>());
                    }
                }
            }
            if (const auto found = root.find("favoriteOverlays"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteOverlays.size() < 4096U) {
                        data_.favoriteOverlays.push_back(value.get<std::string>());
                    }
                }
            }
            NormalizeFavorites(data_.favoriteBodyPresets, false);
            NormalizeFavorites(data_.favoriteSkinProfiles, true);
            NormalizeFavorites(data_.favoriteFutanariSkins, false);
            NormalizeFavorites(data_.favoriteTintPacks, false);
            NormalizeFavorites(data_.favoriteOverlays, false);
            // Player tint baselines are save-specific (TINT co-save v2).
            // Legacy global backups cannot be attributed to a character safely.
            if (source.legacy) {
                std::error_code error;
                std::filesystem::create_directories(path.parent_path(), error);
                if (!error) {
                    std::filesystem::copy_file(source.path, path,
                        std::filesystem::copy_options::overwrite_existing, error);
                }
                if (error) {
                    SKSE::log::warn("Body Change NG loaded legacy settings but could not migrate {} to {}: {}",
                        bcn::path_text::Utf8(source.path), bcn::path_text::Utf8(path), error.message());
                } else {
                    SKSE::log::info("Body Change NG migrated legacy settings from {} to {}",
                        bcn::path_text::Utf8(source.path), bcn::path_text::Utf8(path));
                }
            }
            SKSE::log::info("Body Change NG loaded opening shortcut {}", data_.openHotkey.DisplayName());
        } catch (const std::exception& exception) {
            data_ = {};
            SKSE::log::error("Body Change NG could not read {}: {}",
                bcn::path_text::Utf8(source.path), exception.what());
        }
    }

    bool Settings::Save() const
    {
        const auto path = Path();
        SettingsData copy;
        {
            std::scoped_lock lock(lock_);
            copy = data_;
        }

        try {
            std::filesystem::create_directories(path.parent_path());
            nlohmann::json root{
                { "schemaVersion", kSchemaVersion },
                { "openHotkey", {
                    { "key", copy.openHotkey.key },
                    { "ctrl", copy.openHotkey.ctrl },
                    { "shift", copy.openHotkey.shift },
                    { "alt", copy.openHotkey.alt }
                } },
                { "language", static_cast<int>(copy.language) },
                { "characterPosition", static_cast<int>(copy.characterPosition) },
                { "textScale", copy.textScale },
                { "pauseGameWhenOpen", copy.pauseGameWhenOpen },
                { "performanceMode", copy.performanceMode },
                { "preserveOtherMorphs", copy.preserveOtherMorphs },
                { "femaleNpcBodyType", static_cast<int>(copy.femaleNpcBodyType) },
                { "maleNpcBodyType", static_cast<int>(copy.maleNpcBodyType) },
                { "orefitEnabled", copy.orefitEnabled },
                { "orefitNippleMorphing", copy.orefitNippleMorphing },
                { "nippleRandomization", copy.nippleRandomization },
                { "genitalRandomization", copy.genitalRandomization },
                { "favoriteBodyPresets", copy.favoriteBodyPresets },
                { "favoriteSkinProfiles", copy.favoriteSkinProfiles },
                { "favoriteFutanariSkins", copy.favoriteFutanariSkins },
                { "favoriteTintPacks", copy.favoriteTintPacks },
                { "favoriteOverlays", copy.favoriteOverlays }
            };
            if (copy.mainWindowPositionSet) {
                root["mainWindowPosition"] = {
                    { "x", copy.mainWindowPositionX },
                    { "y", copy.mainWindowPositionY }
                };
            }
            auto temporary = path;
            for (std::size_t index{}; index < popup_placement::keys.size(); ++index) {
                const auto position = copy.popupPositions[index];
                if (position.set && popup_placement::Valid(position.x, position.y)) {
                    root["popupPositions"][popup_placement::keys[index]] = {
                        { "x", position.x }, { "y", position.y }
                    };
                }
            }
            temporary += ".new";
            {
                std::ofstream stream(temporary, std::ios::trunc | std::ios::binary);
                stream << root.dump(2) << '\n';
                stream.flush();
                if (!stream.good()) throw std::runtime_error("write failed");
            }
            {
                std::ifstream verification(temporary, std::ios::binary);
                const auto parsed = nlohmann::json::parse(verification);
                if (!parsed.is_object() || parsed.value("schemaVersion", 0) != kSchemaVersion) {
                    throw std::runtime_error("temporary settings verification failed");
                }
            }
            std::error_code error;
            if (!MoveFileExW(temporary.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const auto code = GetLastError();
                std::filesystem::remove(temporary, error);
                throw std::system_error(static_cast<int>(code), std::system_category(),
                    "atomic settings replace");
            }
            SKSE::log::info("Body Change NG saved settings to {}", bcn::path_text::Utf8(path));
            return true;
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not save {}: {}",
                bcn::path_text::Utf8(path), exception.what());
            return false;
        }
    }

    SettingsData Settings::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return data_;
    }

    UiLanguage Settings::Language() const
    {
        std::scoped_lock lock(lock_);
        return data_.language;
    }

    float Settings::TextScale() const
    {
        std::scoped_lock lock(lock_);
        return data_.textScale;
    }

    bool Settings::PerformanceMode() const
    {
        std::scoped_lock lock(lock_);
        return data_.performanceMode;
    }

    bool Settings::OutfitCorrectionEnabled() const
    {
        std::scoped_lock lock(lock_);
        return data_.orefitEnabled;
    }

    std::uint32_t Settings::BodyApplicationOptions() const
    {
        std::scoped_lock lock(lock_);
        return (data_.nippleRandomization ? 1U : 0U) |
            (data_.genitalRandomization ? 2U : 0U) |
            (data_.preserveOtherMorphs ? 4U : 0U);
    }

    BodyMorphOptions Settings::MorphOptions() const
    {
        std::scoped_lock lock(lock_);
        return {
            .preserveOtherMorphs = data_.preserveOtherMorphs,
            .outfitCorrection = data_.orefitEnabled,
            .outfitNippleCorrection = data_.orefitNippleMorphing,
            .nippleRandomization = data_.nippleRandomization,
            .genitalRandomization = data_.genitalRandomization
        };
    }

    void Settings::Update(const SettingsData& a_data)
    {
        std::scoped_lock lock(lock_);
        data_ = a_data;
        if (!data_.openHotkey.IsValid()) data_.openHotkey = {};
    }

    popup_placement::Position Settings::PopupPosition(const popup_placement::Kind kind) const
    {
        std::scoped_lock lock(lock_);
        return data_.popupPositions.at(static_cast<std::size_t>(kind));
    }

    bool Settings::RememberPopupPosition(const popup_placement::Kind kind, const float x, const float y)
    {
        std::scoped_lock lock(lock_);
        auto& position = data_.popupPositions.at(static_cast<std::size_t>(kind));
        if (!popup_placement::Changed(position, x, y)) return false;
        position = { true, x, y };
        return true;
    }
}
