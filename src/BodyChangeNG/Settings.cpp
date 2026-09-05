#include "BodyChangeNG/Settings.h"

#include "BodyChangeNG/PathMigration.h"

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
            ReadIfPresent(root, "performanceMode", data_.performanceMode);
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
            if (const auto found = root.find("favoriteTintPacks"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (value.is_string() && value.get_ref<const std::string&>().size() <= 1024U &&
                        data_.favoriteTintPacks.size() < 4096U) {
                        data_.favoriteTintPacks.push_back(value.get<std::string>());
                    }
                }
            }
            NormalizeFavorites(data_.favoriteBodyPresets, false);
            NormalizeFavorites(data_.favoriteSkinProfiles, true);
            NormalizeFavorites(data_.favoriteTintPacks, false);
            if (const auto found = root.find("playerTintBackups"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) {
                    if (!value.is_object() || data_.playerTintBackups.size() >= 15U) continue;
                    const auto type = value.value("type", -1);
                    const auto texturePath = value.value("texturePath", std::string{});
                    const auto red = value.value("red", -1);
                    const auto green = value.value("green", -1);
                    const auto blue = value.value("blue", -1);
                    const auto alpha = value.value("alpha", -1.0F);
                    if (type < 0 || type >= 15 || texturePath.empty() || texturePath.size() > 1024U ||
                        red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255 ||
                        alpha < 0.0F || alpha > 1.0F) {
                        continue;
                    }
                    const auto duplicate = std::ranges::find(data_.playerTintBackups, static_cast<std::uint8_t>(type),
                                                             &PlayerTintBackup::type);
                    if (duplicate == data_.playerTintBackups.end()) {
                        data_.playerTintBackups.push_back({
                            .type = static_cast<std::uint8_t>(type),
                            .texturePath = texturePath,
                            .color = { static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue) },
                            .alpha = alpha
                        });
                    }
                }
            }
            if (source.legacy) {
                std::error_code error;
                std::filesystem::create_directories(path.parent_path(), error);
                if (!error) {
                    std::filesystem::copy_file(source.path, path,
                        std::filesystem::copy_options::overwrite_existing, error);
                }
                if (error) {
                    SKSE::log::warn("Body Change NG loaded legacy settings but could not migrate {} to {}: {}",
                        source.path.string(), path.string(), error.message());
                } else {
                    SKSE::log::info("Body Change NG migrated legacy settings from {} to {}",
                        source.path.string(), path.string());
                }
            }
            SKSE::log::info("Body Change NG loaded opening shortcut {}", data_.openHotkey.DisplayName());
        } catch (const std::exception& exception) {
            data_ = {};
            SKSE::log::error("Body Change NG could not read {}: {}", source.path.string(), exception.what());
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
            nlohmann::json tintBackups = nlohmann::json::array();
            for (const auto& backup : copy.playerTintBackups) {
                tintBackups.push_back({
                    { "type", backup.type },
                    { "texturePath", backup.texturePath },
                    { "red", backup.color[0] },
                    { "green", backup.color[1] },
                    { "blue", backup.color[2] },
                    { "alpha", backup.alpha }
                });
            }
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
                { "femaleNpcBodyType", static_cast<int>(copy.femaleNpcBodyType) },
                { "maleNpcBodyType", static_cast<int>(copy.maleNpcBodyType) },
                { "orefitEnabled", copy.orefitEnabled },
                { "orefitNippleMorphing", copy.orefitNippleMorphing },
                { "nippleRandomization", copy.nippleRandomization },
                { "genitalRandomization", copy.genitalRandomization },
                { "favoriteBodyPresets", copy.favoriteBodyPresets },
                { "favoriteSkinProfiles", copy.favoriteSkinProfiles },
                { "favoriteTintPacks", copy.favoriteTintPacks },
                { "playerTintBackups", std::move(tintBackups) }
            };
            if (copy.mainWindowPositionSet) {
                root["mainWindowPosition"] = {
                    { "x", copy.mainWindowPositionX },
                    { "y", copy.mainWindowPositionY }
                };
            }
            const auto temporary = path.string() + ".new";
            {
                std::ofstream stream(temporary, std::ios::trunc);
                stream << root.dump(2) << '\n';
                if (!stream.good()) throw std::runtime_error("write failed");
            }
            std::error_code error;
            std::filesystem::rename(temporary, path, error);
            if (error) {
                std::filesystem::remove(path, error);
                error.clear();
                std::filesystem::rename(temporary, path, error);
            }
            if (error) throw std::filesystem::filesystem_error("rename", path, error);
            SKSE::log::info("Body Change NG saved settings to {}", path.string());
            return true;
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not save {}: {}", path.string(), exception.what());
            return false;
        }
    }

    SettingsData Settings::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return data_;
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

    std::uint32_t Settings::RandomizationOptions() const
    {
        std::scoped_lock lock(lock_);
        return (data_.nippleRandomization ? 1U : 0U) |
            (data_.genitalRandomization ? 2U : 0U);
    }

    BodyMorphOptions Settings::MorphOptions() const
    {
        std::scoped_lock lock(lock_);
        return {
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
}
