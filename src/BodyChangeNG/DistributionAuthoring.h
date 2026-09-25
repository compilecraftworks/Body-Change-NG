#pragma once

#include "BodyChangeNG/Distribution.h"
#include <charconv>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

// File-format adapters only: no engine pointers, file I/O or live catalog scans.
namespace bcn::distribution_authoring
{
    using Json = nlohmann::json;
    inline constexpr std::string_view kAssetPrefix = "@BCNG-asset:";
    inline constexpr std::string_view kTargetPrefix = "@BCNG-target:";
    inline constexpr std::array<std::string_view, 11> kScopes{
        "all", "npc", "name", "faction", "plugin", "race", "customFollowers",
        "elders", "keyword", "class", "combatStyle"
    };

    inline void Check(bool valid, std::string_view message)
    {
        if (!valid) throw std::runtime_error(std::string(message));
    }
    inline void Keys(const Json& value, std::initializer_list<std::string_view> allowed)
    {
        Check(value.is_object(), "expected a JSON object");
        for (const auto& [key, ignored] : value.items()) {
            (void)ignored;
            Check(std::ranges::find(allowed, key) != allowed.end(), "unknown field: " + key);
        }
    }
    [[nodiscard]] inline std::string Text(const Json& value)
    {
        Check(value.is_string(), "expected a name/string");
        const auto text = value.get<std::string>();
        Check(!text.empty() && text.size() <= 1024U, "name/string must contain 1..1024 UTF-8 bytes");
        return text;
    }
    [[nodiscard]] inline std::uint32_t FormID(const Json& value)
    {
        std::uint32_t result{};
        if (value.is_number_unsigned()) {
            const auto number = value.get<std::uint64_t>();
            Check(number <= 0xFFFFFFU, "formId is outside the local range");
            result = static_cast<std::uint32_t>(number);
        }
        else {
            const auto text = Text(value);
            auto digits = std::string_view(text);
            if (digits.starts_with("0x") || digits.starts_with("0X")) digits.remove_prefix(2);
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), result, 16);
            Check(parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size(), "invalid hexadecimal local formId");
        }
        Check(result != 0U && result <= 0xFFFFFFU, "formId must be a local ID, not a runtime RefID");
        return result;
    }
    [[nodiscard]] inline std::string HexID(std::uint32_t value)
    {
        char digits[16]{};
        const auto end = std::to_chars(digits, digits + sizeof(digits), value, 16).ptr;
        return "0x" + std::string(digits, end);
    }
    [[nodiscard]] inline std::uint32_t Color(const Json& value)
    {
        auto text = Text(value);
        Check(text.starts_with('#') && (text.size() == 7 || text.size() == 9), "color must be #RRGGBB or #RRGGBBAA");
        if (text.size() == 7) text += "FF";
        std::uint32_t rgba{};
        const auto parsed = std::from_chars(text.data() + 1, text.data() + text.size(), rgba, 16);
        Check(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(), "invalid color hex digits");
        return (rgba >> 8U) | (rgba << 24U);
    }
    [[nodiscard]] inline std::string ColorText(std::uint32_t argb)
    {
        constexpr char digits[] = "0123456789ABCDEF";
        const auto rgba = (argb << 8U) | (argb >> 24U);
        std::string result(9, '#');
        for (unsigned index{}; index < 8; ++index) result[index + 1] = digits[(rgba >> (28U - 4U * index)) & 15U];
        return result;
    }
    [[nodiscard]] inline std::string Asset(const Json& input)
    {
        if (input.is_string()) return Asset(Json{{"name", Text(input)}});
        Keys(input, {"name", "file", "type", "texture", "id"});
        if (input.contains("id")) {
            Check(input.size() == 1, "id cannot be combined with asset name qualifiers");
            const auto id = Text(input.at("id"));
            Check(!id.starts_with(kAssetPrefix) && !id.starts_with(kTargetPrefix), "reserved internal reference prefix");
            return id;
        }
        Check(input.contains("name"), "asset requires name or id");
        for (const auto& [key, value] : input.items()) { (void)key; (void)Text(value); }
        const auto encoded = std::string(kAssetPrefix) + input.dump();
        Check(encoded.size() <= 4096U, "qualified asset reference is too long");
        return encoded;
    }
    [[nodiscard]] inline Json AssetValue(std::string_view value)
    {
        if (!value.starts_with(kAssetPrefix)) return Json{{"id", value}};
        auto object = Json::parse(value.substr(kAssetPrefix.size()));
        if (object.size() == 1 && object.contains("name")) return object.at("name");
        return object;
    }
    [[nodiscard]] inline Json Pool(const Json& value)
    {
        auto result = Json::array();
        if (value.is_array()) for (const auto& entry : value) result.push_back(Asset(entry));
        else result.push_back(Asset(value));
        return result;
    }
    [[nodiscard]] inline Json PoolValue(const std::vector<std::string>& pool)
    {
        auto result = Json::array();
        for (const auto& value : pool) result.push_back(AssetValue(value));
        return result;
    }
    [[nodiscard]] inline bool NamedTarget(std::string_view value) { return value.starts_with(kTargetPrefix); }
    [[nodiscard]] inline Json TargetValue(std::string_view value) { return Json::parse(value.substr(kTargetPrefix.size())); }
    [[nodiscard]] inline std::string TargetLabel(std::string_view value)
    {
        if (!NamedTarget(value)) return std::string(value);
        const auto object = TargetValue(value);
        return object.is_string() ? object.get<std::string>() : object.value("name", std::string{});
    }

    [[nodiscard]] inline Json DecodeRule(const Json& source, int schema)
    {
        if (schema < 8) {
            auto result = source;
            // Older releases deliberately ignored this retired toggle.
            if (result.is_object()) result["enabled"] = true;
            return result;
        }
        Keys(source, {"id", "name", "nameKey", "enabled", "sex", "scope", "target", "presets", "skins",
            "futaSkins", "overlays", "excludeCustomFollowers", "excludeElders"});
        const auto scopeName = Text(source.at("scope"));
        const auto found = std::ranges::find(kScopes, scopeName);
        Check(found != kScopes.end(), "unknown scope: " + scopeName);
        const auto scope = static_cast<unsigned>(found - kScopes.begin());
        const auto sex = Text(source.at("sex"));
        Check(sex == "female" || sex == "male", "sex must be female or male");
        Json result{{"scope", scope}, {"female", sex == "female"},
            {"enabled", source.value("enabled", true)},
            {"includeCustomFollowers", !source.value("excludeCustomFollowers", true)},
            {"includeElderNPCs", !source.value("excludeElders", true)}};
        for (const auto key : {"id", "name", "nameKey"}) if (source.contains(key)) result[key] = Text(source.at(key));
        if (scope == 0U || scope == 6U || scope == 7U) {
            Check(!source.contains("target"), "this scope takes no target");
        } else {
            const auto& target = source.at("target");
            if (scope == 2U || scope == 4U) {
                Check(target.is_string(), "name/plugin target must be a string");
                Check(target.get_ref<const std::string&>().size() <= 512U, "name/plugin target is too long");
                result["target"] = target;
            }
            else if (target.is_object() && target.contains("formId")) {
                Keys(target, {"plugin", "formId", "label"});
                result[scope == 1U ? "npcPlugin" : "targetPlugin"] = Text(target.at("plugin"));
                result[scope == 1U ? "npcLocalFormID" : "targetLocalFormID"] = FormID(target.at("formId"));
                if (target.contains("label")) result["targetLabel"] = Text(target.at("label"));
            } else {
                if (target.is_string()) Check(target.get_ref<const std::string&>().size() <= 1024U, "target is too long");
                else {
                    Keys(target, {"name", "plugin"});
                    (void)Text(target.at("name"));
                    if (target.contains("plugin")) (void)Text(target.at("plugin"));
                }
                const auto encoded = std::string(kTargetPrefix) + target.dump();
                Check(encoded.size() <= 4096U, "qualified target is too long");
                result["target"] = encoded;
            }
        }
        for (const auto& [readable, internal] : std::array{
            std::pair{"presets", "presetIds"}, std::pair{"skins", "skinProfileIds"}, std::pair{"futaSkins", "futanariSkinIds"}}) {
            if (source.contains(readable)) result[internal] = Pool(source.at(readable));
        }
        if (source.contains("overlays")) {
            const auto& overlays = source.at("overlays");
            Keys(overlays, {"face", "body", "hands", "feet"});
            result["overlayIds"] = Json::array();
            result["overlayColors"] = Json::array();
            for (const auto area : overlay::kAreas) {
                auto pool = Json::array();
                auto colors = Json::object();
                const auto key = std::string(overlay::StableName(area));
                if (overlays.contains(key)) {
                    auto entries = overlays.at(key);
                    if (!entries.is_array()) entries = Json::array({entries});
                    for (auto entry : entries) {
                        auto color = 0xFFFFFFFFU;
                        if (entry.is_object() && entry.contains("color")) {
                            color = Color(entry.at("color"));
                            entry.erase("color");
                        }
                        const auto reference = Asset(entry);
                        Check(!colors.contains(reference), "duplicate overlay candidate in one area");
                        pool.push_back(reference);
                        colors[reference] = color;
                    }
                }
                result["overlayIds"].push_back(std::move(pool));
                result["overlayColors"].push_back(std::move(colors));
            }
        }
        return result;
    }

    [[nodiscard]] inline Json Decode(const Json& root)
    {
        Check(root.is_object(), "distribution root must be an object");
        const auto schema = root.value("schemaVersion", 0);
        Check(schema >= 3 && schema <= 8 && root.contains("rules") && root.at("rules").is_array(), "unsupported distribution schema");
        if (schema == 8) Keys(root, {"schemaVersion", "rules"});
        Json result{{"schemaVersion", schema}, {"rules", Json::array()}};
        std::size_t index{};
        for (const auto& rule : root.at("rules")) {
            try { result["rules"].push_back(DecodeRule(rule, schema)); }
            catch (const std::exception& error) { throw std::runtime_error("rules[" + std::to_string(index) + "]: " + error.what()); }
            ++index;
        }
        return result;
    }

    [[nodiscard]] inline Json Encode(const std::vector<DistributionRule>& rules)
    {
        Json root{{"schemaVersion", 8}, {"rules", Json::array()}};
        for (const auto& rule : rules) {
            const auto scope = static_cast<unsigned>(rule.scope);
            Check(scope < kScopes.size(), "invalid scope on save");
            Json row{{"id", rule.id}, {"name", rule.name}, {"sex", rule.female ? "female" : "male"}, {"scope", kScopes[scope]}};
            if (rule.name.empty()) row.erase("name");
            if (rule.id.empty()) row.erase("id");
            if (!rule.nameKey.empty()) row["nameKey"] = rule.nameKey;
            if (!rule.enabled) row["enabled"] = false;
            row["excludeCustomFollowers"] = !rule.includeCustomFollowers;
            row["excludeElders"] = !rule.includeElderNPCs;
            if (scope != 0U && scope != 6U && scope != 7U) {
                if (NamedTarget(rule.target)) row["target"] = TargetValue(rule.target);
                else if (scope == 2U || scope == 4U) row["target"] = rule.target;
                else {
                    const auto& plugin = scope == 1U ? rule.npcPlugin : rule.targetPlugin;
                    const auto local = scope == 1U ? rule.npcLocalFormID : rule.targetLocalFormID;
                    if (!plugin.empty() && local != 0U) {
                        row["target"] = Json{{"plugin", plugin}, {"formId", HexID(local)}};
                        if (!rule.targetLabel.empty()) row["target"]["label"] = rule.targetLabel;
                        else if (!rule.target.empty()) row["target"]["label"] = rule.target;
                    } else row["target"] = rule.target;
                }
            }
            if (!rule.presetIds.empty()) row["presets"] = PoolValue(rule.presetIds);
            if (!rule.skinProfileIds.empty()) row["skins"] = PoolValue(rule.skinProfileIds);
            if (!rule.futanariSkinIds.empty()) row["futaSkins"] = PoolValue(rule.futanariSkinIds);
            for (const auto area : overlay::kAreas) {
                const auto index = overlay::Index(area);
                if (rule.overlayIds[index].empty()) continue;
                auto pool = Json::array();
                for (const auto& reference : rule.overlayIds[index]) {
                    auto entry = AssetValue(reference);
                    if (entry.is_string()) entry = Json{{"name", entry}};
                    entry["color"] = ColorText(DistributionOverlayColor(rule, area, reference));
                    pool.push_back(std::move(entry));
                }
                row["overlays"][std::string(overlay::StableName(area))] = std::move(pool);
            }
            root["rules"].push_back(std::move(row));
        }
        return root;
    }

    struct AssetEntry { std::string id, name, file, type, texture; };
    class AssetIndex final
    {
    public:
        void Assign(std::vector<AssetEntry> entries, const bool caseInsensitiveAssets = false) {
            caseInsensitiveAssets_ = caseInsensitiveAssets;
            entries_ = std::move(entries); names_.clear(); ids_.clear();
            for (std::size_t i{}; i < entries_.size(); ++i) {
                names_[Key(entries_[i].name)].push_back(i); ids_[Key(entries_[i].id)] = i;
            }
        }
        [[nodiscard]] std::vector<std::string> Candidates(std::string_view reference) const {
            if (!reference.starts_with(kAssetPrefix)) {
                const auto found = ids_.find(Key(reference));
                return {found == ids_.end() ? std::string(reference) : entries_[found->second].id};
            }
            const auto value = Json::parse(reference.substr(kAssetPrefix.size()));
            const auto found = names_.find(Key(value.at("name").get_ref<const std::string&>()));
            if (found == names_.end()) return {};
            std::vector<std::string> result;
            for (auto index : found->second) {
                const auto& entry = entries_[index];
                if (value.contains("file") && !Matches(value.at("file").get_ref<const std::string&>(), entry.file)) continue;
                if (value.contains("type") && !Matches(value.at("type").get_ref<const std::string&>(), entry.type)) continue;
                if (value.contains("texture") && !Matches(value.at("texture").get_ref<const std::string&>(), entry.texture)) continue;
                result.push_back(entry.id);
            }
            return result;
        }
        [[nodiscard]] std::string Describe(std::string_view id) const {
            if (id.starts_with(kAssetPrefix)) return std::string(id);
            const auto found = ids_.find(Key(id));
            if (found == ids_.end()) return std::string(id); // Missing assets retain exact old IDs.
            const auto& entry = entries_[found->second];
            Json value{{"name", entry.name}};
            // Qualifiers preserve the exact UI selection, even across same-name packs.
            if (!entry.file.empty()) value["file"] = entry.file;
            if (!entry.type.empty()) value["type"] = entry.type;
            if (!entry.texture.empty()) value["texture"] = entry.texture;
            try {
                const auto reference = Asset(value);
                const auto candidates = Candidates(reference);
                return candidates.size() == 1 && Matches(candidates.front(), id) ? reference : std::string(id);
            } catch (const std::exception&) {
                // A catalog name that cannot be represented must not discard a
                // valid legacy rule or throw through the in-game Save handler.
                return std::string(id);
            }
        }
    private:
        [[nodiscard]] std::string Key(std::string_view value) const {
            std::string result(value);
            if (caseInsensitiveAssets_) for (auto& byte : result)
                byte = static_cast<char>(asset_identity::Fold(static_cast<unsigned char>(byte)));
            return result;
        }
        [[nodiscard]] bool Matches(std::string_view left, std::string_view right) const noexcept {
            return caseInsensitiveAssets_ ? asset_identity::Equal{}(left, right) : left == right;
        }
        bool caseInsensitiveAssets_{};
        std::vector<AssetEntry> entries_;
        std::unordered_map<std::string, std::vector<std::size_t>> names_;
        std::unordered_map<std::string, std::size_t> ids_;
    };

    [[nodiscard]] inline bool EqualName(std::string_view left, std::string_view right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i{}; i < left.size(); ++i) {
            const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
            if (lower(left[i]) != lower(right[i])) return false;
        }
        return true;
    }
    struct TargetEntry { DistributionScope scope; std::uint32_t id, local; std::string plugin, name, editor; };
    [[nodiscard]] inline const TargetEntry* FindTarget(const std::vector<TargetEntry>& options, DistributionScope scope, const Json& reference) {
        const auto name = reference.is_string() ? reference.get<std::string>() : reference.value("name", std::string{});
        const auto plugin = reference.is_object() ? reference.value("plugin", std::string{}) : std::string{};
        if (name.empty()) return nullptr;
        // EditorIDs outrank translated names. A duplicate within the selected plugin is still ambiguous.
        for (bool editor : {true, false}) {
            const TargetEntry* found{};
            for (const auto& option : options) {
                if (option.scope != scope || (!plugin.empty() && !EqualName(plugin, option.plugin)) ||
                    !EqualName(name, editor ? option.editor : option.name)) continue;
                if (found && found->id != option.id) return nullptr;
                found = &option;
            }
            if (found) return found;
        }
        return nullptr;
    }

    struct ResolvedPool {
        std::vector<std::string> ids;
        std::map<std::string, std::string, std::less<>> references;
    };
    template<class Compatible>
    [[nodiscard]] ResolvedPool ResolvePool(const std::vector<std::string>& pool,
        const AssetIndex& catalog, Compatible&& compatible)
    {
        ResolvedPool result;
        std::vector<std::string> candidates;
        std::vector<std::pair<std::size_t, std::size_t>> spans;
        for (const auto& reference : pool) {
            auto entries = catalog.Candidates(reference);
            const auto begin = candidates.size();
            candidates.insert(candidates.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));
            spans.emplace_back(begin, candidates.size());
        }
        // Actor/body/provider compatibility is evaluated once for the whole pool,
        // not once per named candidate (which would repeat engine-side work).
        const auto accepted = compatible(candidates);
        const std::unordered_set<std::string> allowed(accepted.begin(), accepted.end());
        for (std::size_t i{}; i < pool.size(); ++i) {
            const std::string* match{};
            std::size_t count{};
            for (auto j = spans[i].first; j < spans[i].second; ++j) {
                if (allowed.contains(candidates[j])) { match = &candidates[j]; ++count; }
            }
            // Name-only input must have exactly one compatible match; never pick a random duplicate.
            if (count != 1) continue;
            result.ids.push_back(*match);
            result.references.try_emplace(*match, pool[i]);
        }
        return result;
    }
}
