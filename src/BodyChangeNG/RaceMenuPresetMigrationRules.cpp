#include "BodyChangeNG/RaceMenuPresetMigrationRules.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>

namespace
{
    constexpr std::string_view kLegacyPlugin{ "BodyChange.esp" };
    constexpr std::string_view kUbeRacePlugin{ "UBE_AllRace.esp" };

    [[nodiscard]] bool EqualsInsensitive(const std::string_view left, const std::string_view right)
    {
        return left.size() == right.size() && std::ranges::equal(left, right, [](const char a, const char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    }

    [[nodiscard]] std::string_view PluginFromIdentifier(const std::string_view identifier)
    {
        const auto separator = identifier.find('|');
        return separator == std::string_view::npos ? identifier : identifier.substr(0U, separator);
    }

    [[nodiscard]] bool ContainsUbeToken(const std::string_view text)
    {
        for (std::size_t index = 0U; index + 3U <= text.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(text[index])) != 'u' ||
                std::tolower(static_cast<unsigned char>(text[index + 1U])) != 'b' ||
                std::tolower(static_cast<unsigned char>(text[index + 2U])) != 'e') continue;
            const auto beforeIsAlphaNumeric = index != 0U &&
                std::isalnum(static_cast<unsigned char>(text[index - 1U])) != 0;
            const auto afterIsAlphaNumeric = index + 3U < text.size() &&
                std::isalnum(static_cast<unsigned char>(text[index + 3U])) != 0;
            if (!beforeIsAlphaNumeric && !afterIsAlphaNumeric) return true;
        }
        return false;
    }

    [[nodiscard]] bool HasUbePluginName(const nlohmann::json& value)
    {
        if (!value.is_string()) return false;
        const auto plugin = PluginFromIdentifier(value.get_ref<const std::string&>());
        return EqualsInsensitive(plugin, kUbeRacePlugin) || ContainsUbeToken(plugin);
    }

    [[nodiscard]] bool JsonContainsUbeFormSignal(const nlohmann::json& value)
    {
        if (value.is_array()) {
            return std::ranges::any_of(value, [](const auto& item) {
                return JsonContainsUbeFormSignal(item);
            });
        }
        if (value.is_object()) {
            return std::ranges::any_of(value.items(), [](const auto& item) {
                if ((item.key() == "formIdentifier" || item.key() == "headTexture") &&
                    HasUbePluginName(item.value())) return true;
                return JsonContainsUbeFormSignal(item.value());
            });
        }
        return false;
    }

    [[nodiscard]] bool IdentifierUsesPlugin(const nlohmann::json& value, const std::string_view plugin)
    {
        return value.is_string() && EqualsInsensitive(PluginFromIdentifier(value.get_ref<const std::string&>()), plugin);
    }

    [[nodiscard]] bool JsonReferencesPlugin(const nlohmann::json& value, const std::string_view plugin)
    {
        if (value.is_string()) return IdentifierUsesPlugin(value, plugin);
        if (value.is_array()) {
            return std::ranges::any_of(value, [&](const auto& item) { return JsonReferencesPlugin(item, plugin); });
        }
        if (value.is_object()) {
            return std::ranges::any_of(value.items(), [&](const auto& item) {
                return JsonReferencesPlugin(item.value(), plugin);
            });
        }
        return false;
    }

    [[nodiscard]] bool PresetReferencesPluginOutsideModNames(
        const nlohmann::json& preset, const std::string_view plugin)
    {
        if (!preset.is_object()) return false;
        return std::ranges::any_of(preset.items(), [&](const auto& item) {
            return item.key() != "modNames" && JsonReferencesPlugin(item.value(), plugin);
        });
    }

    [[nodiscard]] bool IsLegacyHeadPart(const nlohmann::json& part)
    {
        const auto identifier = part.find("formIdentifier");
        return part.is_object() && identifier != part.end() && IdentifierUsesPlugin(*identifier, kLegacyPlugin);
    }

    [[nodiscard]] bool IsTargetHeadPart(const nlohmann::json& part,
        const bcn::racemenu_preset_migration::HeadPartTarget& target)
    {
        const auto identifier = part.find("formIdentifier");
        return part.is_object() && identifier != part.end() && identifier->is_string() &&
               EqualsInsensitive(identifier->get_ref<const std::string&>(), target.formIdentifier);
    }

    void UpdateModNames(nlohmann::json& preset,
        const bcn::racemenu_preset_migration::HeadPartTarget& target)
    {
        auto& names = preset["modNames"];
        if (!names.is_array()) names = nlohmann::json::array();

        const auto stillUsesLegacy = PresetReferencesPluginOutsideModNames(preset, kLegacyPlugin);
        if (!stillUsesLegacy) {
            names.erase(std::remove_if(names.begin(), names.end(), [](const auto& value) {
                return value.is_string() &&
                       EqualsInsensitive(value.template get_ref<const std::string&>(), kLegacyPlugin);
            }), names.end());
        }

        const auto hasTarget = std::ranges::any_of(names, [&](const auto& value) {
            return value.is_string() &&
                   EqualsInsensitive(value.template get_ref<const std::string&>(), target.plugin);
        });
        if (!hasTarget) names.push_back(target.plugin);
    }
}

namespace bcn::racemenu_preset_migration
{
    bool HasUbeRaceDependency(const nlohmann::json& preset)
    {
        if (const auto names = preset.find("modNames"); names != preset.end() && names->is_array() &&
            std::ranges::any_of(*names, [](const auto& name) { return HasUbePluginName(name); })) return true;
        return JsonContainsUbeFormSignal(preset);
    }

    TransformResult TransformLegacyBodyChangeHeadParts(nlohmann::json& preset, const HeadPartTarget& target)
    {
        TransformResult result;
        if (!preset.is_object() || target.plugin.empty() || target.formIdentifier.empty()) return result;

        auto parts = preset.find("headParts");
        if (parts == preset.end() || !parts->is_array()) return result;
        if (!std::ranges::any_of(*parts, IsLegacyHeadPart)) return result;

        // UBE is a complete custom-race/head system. A UBE RaceMenu preset
        // cannot safely be redirected to a vanilla or High Poly Head face
        // merely because it also contains a stale BodyChange.esp reference.
        if (HasUbeRaceDependency(preset)) {
            result.skippedUBE = true;
            return result;
        }

        auto targetAlreadyPresent = std::ranges::any_of(*parts,
            [&](const auto& part) { return IsTargetHeadPart(part, target); });
        nlohmann::json migratedParts = nlohmann::json::array();
        migratedParts.get_ref<nlohmann::json::array_t&>().reserve(parts->size());

        for (auto& part : *parts) {
            if (!IsLegacyHeadPart(part)) {
                migratedParts.push_back(std::move(part));
                continue;
            }
            if (targetAlreadyPresent) {
                ++result.removedDuplicates;
                continue;
            }
            part["formIdentifier"] = target.formIdentifier;
            part["formId"] = target.runtimeFormID;
            migratedParts.push_back(std::move(part));
            targetAlreadyPresent = true;
            ++result.replaced;
        }
        *parts = std::move(migratedParts);
        if (result.Changed()) UpdateModNames(preset, target);
        return result;
    }
}
