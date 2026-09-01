#include "BodyChangerNG/Distribution.h"

#include "BodyChangerNG/PresetCatalog.h"
#include "BodyChangerNG/RaceMenuBodyMorph.h"
#include "BodyChangerNG/Settings.h"
#include "BodyChangerNG/SkinOverrides.h"

#include <SKSE/Logger.h>
#include <RE/P/ProcessLists.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace
{
    constexpr auto kSchemaVersion = 2;
    constexpr auto kPreviousSchemaVersion = 1;
    [[nodiscard]] bool IsEligibleNPC(RE::Actor* actor, RE::Actor* player)
    {
        return actor && actor != player && !actor->IsDisabled() && !actor->IsDead() && actor->Is3DLoaded() &&
            actor->HasKeywordString("ActorTypeNPC");
    }

    [[nodiscard]] bool EqualIgnoreCase(const std::string_view left, const std::string_view right)
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index{}; index < left.size(); ++index) {
            const auto toLower = [](const char value) {
                return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
            };
            if (toLower(left[index]) != toLower(right[index])) return false;
        }
        return true;
    }

    [[nodiscard]] bool ContainsIgnoreCase(const std::string_view text, const std::string_view fragment)
    {
        if (fragment.empty()) return true;
        if (fragment.size() > text.size()) return false;
        for (std::size_t offset{}; offset + fragment.size() <= text.size(); ++offset) {
            if (EqualIgnoreCase(text.substr(offset, fragment.size()), fragment)) return true;
        }
        return false;
    }

    [[nodiscard]] bool IsOfficialBaseGameFile(const std::string_view filename)
    {
        constexpr std::array officialFiles{
            std::string_view{ "Skyrim.esm" },
            std::string_view{ "Update.esm" },
            std::string_view{ "Dawnguard.esm" },
            std::string_view{ "HearthFires.esm" },
            std::string_view{ "Dragonborn.esm" }
        };
        return std::ranges::any_of(officialFiles, [filename](const auto official) {
            return EqualIgnoreCase(filename, official);
        });
    }

    [[nodiscard]] bool IsCustomFollower(RE::Actor* actor, RE::TESNPC* base)
    {
        if (!actor || !base) return false;
        const auto* sourceFile = base->GetFile(0);
        const auto filename = sourceFile ? sourceFile->GetFilename() : std::string_view{};
        if (filename.empty() || IsOfficialBaseGameFile(filename)) return false;
        const auto lookupFaction = [](const RE::FormID formID) -> RE::TESFaction* {
            auto* form = RE::TESForm::LookupByID(formID);
            return form && form->GetFormType() == RE::FormType::Faction ?
                static_cast<RE::TESFaction*>(form) : nullptr;
        };
        const auto* potentialFollower = lookupFaction(0x0005C84D);
        const auto* currentFollower = lookupFaction(0x0005C84E);
        return actor->IsPlayerTeammate() ||
            (potentialFollower && actor->IsInFaction(potentialFollower)) ||
            (currentFollower && actor->IsInFaction(currentFollower));
    }

    [[nodiscard]] bool IsElderNPC(RE::TESNPC* base)
    {
        if (!base) return false;
        if (const auto* race = base->GetRace(); race && ContainsIgnoreCase(race->GetFormEditorID(), "elder")) {
            return true;
        }
        const auto* voice = base->GetVoiceType();
        const auto voiceEditorID = voice ? std::string_view{ voice->GetFormEditorID() } : std::string_view{};
        return ContainsIgnoreCase(voiceEditorID, "old") || ContainsIgnoreCase(voiceEditorID, "elder");
    }

    [[nodiscard]] bool IsValidScope(const bcn::DistributionScope scope)
    {
        using enum bcn::DistributionScope;
        return scope == allNPCs || scope == npcBaseForm || scope == npcName || scope == factionEditorID ||
            scope == pluginFile || scope == raceEditorID || scope == modInstalledFollower || scope == elderNPC;
    }

    [[nodiscard]] bool MatchesTarget(const bcn::DistributionRule& rule, RE::Actor* actor)
    {
        if (!rule.enabled || !actor) return false;
        const auto base = actor->GetActorBase();
        if (!base || (base->GetSex() == RE::SEX::kFemale) != rule.female) return false;
        switch (rule.scope) {
        case bcn::DistributionScope::allNPCs:
            return true;
        case bcn::DistributionScope::npcBaseForm:
            return base->GetFormID() == rule.npcBaseFormID;
        case bcn::DistributionScope::npcName:
            return EqualIgnoreCase(base->GetName(), rule.target);
        case bcn::DistributionScope::factionEditorID: {
            const auto* form = rule.target.empty() ? nullptr : RE::TESForm::LookupByEditorID(rule.target);
            const auto* faction = form && form->GetFormType() == RE::FormType::Faction ?
                static_cast<const RE::TESFaction*>(form) : nullptr;
            // CommonLib exposes this query as a non-const member even though it
            // does not mutate the NPC.  The faction form comes from the loaded
            // data handler and is not modified here.
            return faction && base->IsInFaction(const_cast<RE::TESFaction*>(faction));
        }
        case bcn::DistributionScope::pluginFile: {
            const auto* file = base->GetFile(0);
            return file && EqualIgnoreCase(file->GetFilename(), rule.target);
        }
        case bcn::DistributionScope::raceEditorID: {
            const auto* race = base->GetRace();
            return race && EqualIgnoreCase(race->GetFormEditorID(), rule.target);
        }
        case bcn::DistributionScope::modInstalledFollower:
            return IsCustomFollower(actor, base);
        case bcn::DistributionScope::elderNPC:
            return IsElderNPC(base);
        }
        return false;
    }

    [[nodiscard]] std::uint64_t StableHash(const std::string_view text, const std::uint32_t first, const std::uint32_t second)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto append = [&hash](const std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ULL; };
        for (const auto character : text) append(static_cast<std::uint8_t>(character));
        for (const auto value : { first, second }) {
            append(static_cast<std::uint8_t>(value));
            append(static_cast<std::uint8_t>(value >> 8U));
            append(static_cast<std::uint8_t>(value >> 16U));
            append(static_cast<std::uint8_t>(value >> 24U));
        }
        return hash;
    }

    struct RuleSelection final
    {
        bool bodyExcluded{};
        bool skinExcluded{};
        std::optional<std::string> presetId;
        std::optional<std::string> skinProfileId;

        [[nodiscard]] bool HasSelection() const noexcept
        {
            return presetId.has_value() || skinProfileId.has_value();
        }
    };

    [[nodiscard]] std::optional<std::string> ChooseFromPool(const bcn::DistributionRule& rule,
                                                              const std::vector<std::string>& pool,
                                                              RE::Actor* actor, const std::string_view kind)
    {
        if (pool.empty()) return std::nullopt;
        const auto base = actor->GetActorBase();
        const auto key = rule.id + ":" + std::string{ kind };
        const auto index = static_cast<std::size_t>(StableHash(key, base->GetFormID(), actor->GetFormID()) % pool.size());
        return pool[index];
    }

    [[nodiscard]] RuleSelection ChooseRuleSelection(const std::vector<bcn::DistributionRule>& rules, RE::Actor* actor)
    {
        for (const auto& rule : rules) {
            if (!MatchesTarget(rule, actor)) continue;
            return RuleSelection{
                .bodyExcluded = rule.bodyExcluded,
                .skinExcluded = rule.skinExcluded,
                .presetId = rule.bodyExcluded ? std::nullopt : ChooseFromPool(rule, rule.presetIds, actor, "body"),
                .skinProfileId = rule.skinExcluded ? std::nullopt : ChooseFromPool(rule, rule.skinProfileIds, actor, "skin")
            };
        }
        return {};
    }

    [[nodiscard]] std::string GenerateRuleId(const std::size_t index)
    {
        return "rule-" + std::to_string(index + 1U);
    }

    [[nodiscard]] std::vector<bcn::DistributionRule> DefaultExclusionRules()
    {
        using bcn::DistributionRule;
        using bcn::DistributionScope;
        return {
            DistributionRule{
                .id = "default-exclude-mod-follower-female",
                .name = "커스텀 팔로워 바디 배포 제외 (여성)",
                .female = true,
                .scope = DistributionScope::modInstalledFollower,
                .bodyExcluded = true },
            DistributionRule{
                .id = "default-exclude-mod-follower-male",
                .name = "커스텀 팔로워 바디 배포 제외 (남성)",
                .female = false,
                .scope = DistributionScope::modInstalledFollower,
                .bodyExcluded = true },
            DistributionRule{
                .id = "default-exclude-elder-female",
                .name = "노인 NPC 바디 배포 제외 (여성)",
                .female = true,
                .scope = DistributionScope::elderNPC,
                .bodyExcluded = true },
            DistributionRule{
                .id = "default-exclude-elder-male",
                .name = "노인 NPC 바디 배포 제외 (남성)",
                .female = false,
                .scope = DistributionScope::elderNPC,
                .bodyExcluded = true },
            DistributionRule{
                .id = "default-exclude-skin-argonian-female",
                .name = "아르고니안 여성 스킨 배포 제외",
                .female = true,
                .scope = DistributionScope::raceEditorID,
                .target = "ArgonianRace",
                .skinExcluded = true },
            DistributionRule{
                .id = "default-exclude-skin-argonian-male",
                .name = "아르고니안 남성 스킨 배포 제외",
                .female = false,
                .scope = DistributionScope::raceEditorID,
                .target = "ArgonianRace",
                .skinExcluded = true },
            DistributionRule{
                .id = "default-exclude-skin-khajiit-female",
                .name = "카짓 여성 스킨 배포 제외",
                .female = true,
                .scope = DistributionScope::raceEditorID,
                .target = "KhajiitRace",
                .skinExcluded = true },
            DistributionRule{
                .id = "default-exclude-skin-khajiit-male",
                .name = "카짓 남성 스킨 배포 제외",
                .female = false,
                .scope = DistributionScope::raceEditorID,
                .target = "KhajiitRace",
                .skinExcluded = true }
        };
    }

    void PrependDefaultExclusionRules(std::vector<bcn::DistributionRule>& rules)
    {
        auto defaults = DefaultExclusionRules();
        defaults.insert(defaults.end(), std::make_move_iterator(rules.begin()), std::make_move_iterator(rules.end()));
        rules = std::move(defaults);
    }

    [[nodiscard]] std::vector<bcn::DistributionRule> NormalizeRules(std::vector<bcn::DistributionRule> rules)
    {
        if (rules.size() > 256U) rules.resize(256U);
        std::unordered_set<std::string> known;
        for (std::size_t index{}; index < rules.size(); ++index) {
            auto& rule = rules[index];
            // There is deliberately no separate Use checkbox. Every row is
            // an active condition whose Body/Skin channel independently says
            // Distribute or Exclude.
            rule.enabled = true;
            if (rule.id.empty() || !known.insert(rule.id).second) {
                std::size_t suffix = index;
                do {
                    rule.id = GenerateRuleId(suffix++);
                } while (!known.insert(rule.id).second);
            }
            if (rule.name.empty()) rule.name = rule.female ? "All female NPCs" : "All male NPCs";
            if (!IsValidScope(rule.scope)) rule.scope = bcn::DistributionScope::allNPCs;
            if (rule.excluded) {
                rule.bodyExcluded = true;
                rule.skinExcluded = true;
                rule.excluded = false;
            }
            if (rule.target.size() > 512U) rule.target.clear();
            if (rule.bodyFamily.size() > 256U) rule.bodyFamily.clear();
            if (rule.bodyFamily == "CBBE") rule.bodyFamily = "CBBE 3BA";
            std::erase_if(rule.presetIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
            std::erase_if(rule.skinProfileIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
        }
        return rules;
    }

    [[nodiscard]] bool ParseHex(const std::string_view text, std::uint32_t& value)
    {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    }

    [[nodiscard]] bool ParseOBodyLocalFormID(std::string_view source, const bool lightPlugin, std::uint32_t& value)
    {
        if (source.starts_with("0x") || source.starts_with("0X")) source.remove_prefix(2);
        const auto digits = lightPlugin ? 3U : 6U;
        if (source.size() > digits) source.remove_prefix(source.size() - digits);
        return !source.empty() && ParseHex(source, value);
    }

    [[nodiscard]] std::vector<std::string> StringsFromJsonArray(const nlohmann::json& value)
    {
        std::vector<std::string> output;
        if (!value.is_array()) return output;
        for (const auto& entry : value) {
            if (entry.is_string() && entry.get_ref<const std::string&>().size() <= 1024U) {
                output.push_back(entry.get<std::string>());
            }
        }
        return output;
    }

    [[nodiscard]] bool WriteDistributionFile(const std::filesystem::path& path,
        const std::vector<bcn::DistributionRule>& rules,
        const std::vector<bcn::ManualAssignment>& manualAssignments)
    {
        try {
            std::filesystem::create_directories(path.parent_path());
            nlohmann::json serializedRules = nlohmann::json::array();
            for (const auto& rule : rules) {
                serializedRules.push_back({
                    { "id", rule.id },
                    { "name", rule.name },
                    { "enabled", rule.enabled },
                    { "female", rule.female },
                    { "scope", static_cast<std::uint8_t>(rule.scope) },
                    { "npcBaseFormID", rule.npcBaseFormID },
                    { "target", rule.target },
                    { "bodyFamily", rule.bodyFamily },
                    { "presetIds", rule.presetIds },
                    { "skinProfileIds", rule.skinProfileIds },
                    { "excluded", rule.bodyExcluded && rule.skinExcluded },
                    { "bodyExcluded", rule.bodyExcluded },
                    { "skinExcluded", rule.skinExcluded }
                });
            }
            nlohmann::json serializedManualAssignments = nlohmann::json::array();
            for (const auto& assignment : manualAssignments) {
                serializedManualAssignments.push_back({
                    { "actorFormID", assignment.actorFormID },
                    { "presetId", assignment.presetId },
                    { "skinProfileId", assignment.skinProfileId },
                    { "useDefaultBody", assignment.useDefaultBody },
                    { "useDefaultSkin", assignment.useDefaultSkin }
                });
            }
            const nlohmann::json root{
                { "schemaVersion", kSchemaVersion },
                { "rules", std::move(serializedRules) },
                { "manualAssignments", std::move(serializedManualAssignments) }
            };
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
            return true;
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Changer NG could not save distribution rules: {}", exception.what());
            return false;
        }
    }
}

namespace bcn
{
    Distribution& Distribution::Get()
    {
        static Distribution distribution;
        return distribution;
    }

    std::filesystem::path Distribution::Path()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "BodyChangerNGdistribution.json";
    }

    bool Distribution::Load()
    {
        const auto path = Path();
        std::vector<DistributionRule> loaded;
        std::vector<ManualAssignment> loadedManualAssignments;
        bool migrated{};
        try {
            if (!std::filesystem::exists(path)) {
                std::scoped_lock lock(lock_);
                rules_ = DefaultExclusionRules();
                manualAssignments_.clear();
                return false;
            }
            std::ifstream stream(path);
            const auto root = nlohmann::json::parse(stream);
            const auto schemaVersion = root.value("schemaVersion", 0);
            if ((schemaVersion != kPreviousSchemaVersion && schemaVersion != kSchemaVersion) ||
                !root.contains("rules") || !root["rules"].is_array()) {
                throw std::runtime_error("unsupported distribution schema");
            }
            for (const auto& source : root["rules"]) {
                if (!source.is_object() || loaded.size() >= 256U) continue;
                DistributionRule rule{
                    .id = source.value("id", std::string{}),
                    .name = source.value("name", std::string{}),
                    .enabled = source.value("enabled", true),
                    .female = source.value("female", true),
                    .scope = static_cast<DistributionScope>(source.value("scope", 0)),
                    .npcBaseFormID = source.value("npcBaseFormID", 0U),
                    .target = source.value("target", std::string{}),
                    .bodyFamily = source.value("bodyFamily", std::string{}),
                    .presetIds = source.value("presetIds", std::vector<std::string>{}),
                    .skinProfileIds = source.value("skinProfileIds", std::vector<std::string>{}),
                    .excluded = source.value("excluded", false),
                    .bodyExcluded = source.value("bodyExcluded", false),
                    .skinExcluded = source.value("skinExcluded", false)
                };
                if (rule.id.empty()) rule.id = GenerateRuleId(loaded.size());
                if (rule.name.empty()) rule.name = rule.female ? "All female NPCs" : "All male NPCs";
                if (!IsValidScope(rule.scope)) continue;
                if (rule.target.size() > 512U) rule.target.clear();
                if (rule.bodyFamily.size() > 256U) rule.bodyFamily.clear();
                if (rule.bodyFamily == "CBBE") rule.bodyFamily = "CBBE 3BA";
                std::erase_if(rule.presetIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                std::erase_if(rule.skinProfileIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                loaded.push_back(std::move(rule));
            }
            if (const auto assignments = root.find("manualAssignments"); assignments != root.end() && assignments->is_array()) {
                std::unordered_set<RE::FormID> knownActors;
                for (const auto& source : *assignments) {
                    if (!source.is_object() || loadedManualAssignments.size() >= 512U) continue;
                    const auto actorFormID = source.value("actorFormID", 0U);
                    auto presetId = source.value("presetId", std::string{});
                    auto skinProfileId = source.value("skinProfileId", std::string{});
                    const auto useDefaultBody = source.value("useDefaultBody", false);
                    const auto useDefaultSkin = source.value("useDefaultSkin", false);
                    if (actorFormID == 0 || (!useDefaultBody && !useDefaultSkin && presetId.empty() && skinProfileId.empty()) || presetId.size() > 1024U ||
                        skinProfileId.size() > 1024U || !knownActors.insert(actorFormID).second) {
                        continue;
                    }
                    loadedManualAssignments.push_back({ actorFormID, std::move(presetId), std::move(skinProfileId), useDefaultBody, useDefaultSkin });
                }
            }
            if (schemaVersion == kPreviousSchemaVersion) {
                PrependDefaultExclusionRules(loaded);
                migrated = true;
            }
            loaded = NormalizeRules(std::move(loaded));
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Changer NG could not load distribution rules: {}", exception.what());
            std::scoped_lock lock(lock_);
            rules_ = DefaultExclusionRules();
            manualAssignments_.clear();
            return false;
        }
        {
            std::scoped_lock lock(lock_);
            rules_ = std::move(loaded);
            manualAssignments_ = std::move(loadedManualAssignments);
        }
        if (migrated && !Save()) {
            SKSE::log::warn("Body Changer NG loaded legacy distribution data but could not save it to {}", path.string());
        }
        return true;
    }

    bool Distribution::Save() const
    {
        const auto path = Path();
        std::vector<DistributionRule> rules;
        std::vector<ManualAssignment> manualAssignments;
        {
            std::scoped_lock lock(lock_);
            rules = rules_;
            manualAssignments = manualAssignments_;
        }
        return WriteDistributionFile(path, rules, manualAssignments);
    }

    bool Distribution::SaveRulesForNextGame(std::vector<DistributionRule> rules) const
    {
        rules = NormalizeRules(std::move(rules));
        std::vector<ManualAssignment> manualAssignments;
        {
            std::scoped_lock lock(lock_);
            manualAssignments = manualAssignments_;
        }
        return WriteDistributionFile(Path(), rules, manualAssignments);
    }

    std::vector<DistributionRule> Distribution::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return rules_;
    }

    void Distribution::SetRules(std::vector<DistributionRule> rules)
    {
        rules = NormalizeRules(std::move(rules));
        std::scoped_lock lock(lock_);
        rules_ = std::move(rules);
    }

    void Distribution::SetManualAssignment(RE::Actor* actor, std::string presetId)
    {
        if (!actor || presetId.empty() || presetId.size() > 1024U) return;
        const auto actorFormID = actor->GetFormID();
        if (actorFormID == 0) return;
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(manualAssignments_, actorFormID, &ManualAssignment::actorFormID);
        if (found != manualAssignments_.end()) {
            found->presetId = std::move(presetId);
            found->useDefaultBody = false;
            return;
        }
        if (manualAssignments_.size() < 512U) {
            manualAssignments_.push_back({ actorFormID, std::move(presetId), {}, false, false });
        }
    }

    void Distribution::SetManualSkinAssignment(RE::Actor* actor, std::string profileId)
    {
        if (!actor || profileId.empty() || profileId.size() > 1024U) return;
        const auto actorFormID = actor->GetFormID();
        if (actorFormID == 0) return;
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(manualAssignments_, actorFormID, &ManualAssignment::actorFormID);
        if (found != manualAssignments_.end()) {
            found->skinProfileId = std::move(profileId);
            found->useDefaultSkin = false;
            return;
        }
        if (manualAssignments_.size() < 512U) {
            manualAssignments_.push_back({ actorFormID, {}, std::move(profileId), false, false });
        }
    }

    void Distribution::SetManualDefaultBody(RE::Actor* actor)
    {
        if (!actor) return;
        const auto actorFormID = actor->GetFormID();
        if (actorFormID == 0) return;
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(manualAssignments_, actorFormID, &ManualAssignment::actorFormID);
        if (found != manualAssignments_.end()) {
            found->presetId.clear();
            found->useDefaultBody = true;
            return;
        }
        if (manualAssignments_.size() < 512U) {
            manualAssignments_.push_back({ actorFormID, {}, {}, true, false });
        }
    }

    void Distribution::SetManualDefaultSkin(RE::Actor* actor)
    {
        if (!actor) return;
        const auto actorFormID = actor->GetFormID();
        if (actorFormID == 0) return;
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(manualAssignments_, actorFormID, &ManualAssignment::actorFormID);
        if (found != manualAssignments_.end()) {
            found->skinProfileId.clear();
            found->useDefaultSkin = true;
            return;
        }
        if (manualAssignments_.size() < 512U) {
            manualAssignments_.push_back({ actorFormID, {}, {}, false, true });
        }
    }

    bool Distribution::RemoveManualAssignment(RE::Actor* actor)
    {
        if (!actor) return false;
        const auto actorFormID = actor->GetFormID();
        std::scoped_lock lock(lock_);
        const auto oldSize = manualAssignments_.size();
        std::erase_if(manualAssignments_, [actorFormID](const auto& assignment) {
            return assignment.actorFormID == actorFormID;
        });
        return manualAssignments_.size() != oldSize;
    }

    bool Distribution::RemoveManualBodyAssignment(RE::Actor* actor)
    {
        if (!actor) return false;
        const auto actorFormID = actor->GetFormID();
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(manualAssignments_, actorFormID, &ManualAssignment::actorFormID);
        if (found == manualAssignments_.end() || (found->presetId.empty() && !found->useDefaultBody)) return false;
        found->presetId.clear();
        found->useDefaultBody = false;
        if (found->skinProfileId.empty() && !found->useDefaultSkin) manualAssignments_.erase(found);
        return true;
    }

    void Distribution::ClearManualAssignments()
    {
        std::scoped_lock lock(lock_);
        manualAssignments_.clear();
    }

    void Distribution::ClearManualBodyAssignments()
    {
        std::scoped_lock lock(lock_);
        for (auto& assignment : manualAssignments_) {
            assignment.presetId.clear();
            assignment.useDefaultBody = false;
        }
        std::erase_if(manualAssignments_, [](const auto& assignment) { return assignment.skinProfileId.empty() && !assignment.useDefaultSkin; });
    }

    bool Distribution::HasManualAssignment(const RE::Actor* actor) const
    {
        if (!actor) return false;
        const auto actorFormID = actor->GetFormID();
        std::scoped_lock lock(lock_);
        return std::ranges::any_of(manualAssignments_, [actorFormID](const auto& assignment) {
            return assignment.actorFormID == actorFormID;
        });
    }

    std::size_t Distribution::ApplyLoadedNPCs() const
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes) return 0;

        std::size_t queued{};
        std::unordered_set<RE::FormID> seen;
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!IsEligibleNPC(actor, player) || !seen.insert(actor->GetFormID()).second) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            if (ApplyActor(actor)) {
                ++queued;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return queued;
    }

    std::size_t Distribution::ResetLoadedNPCs()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* processes = RE::ProcessLists::GetSingleton();
        if (!player || !processes || !racemenu::IsReady()) return 0;

        std::size_t queued{};
        std::unordered_set<RE::FormID> seen;
        processes->ForAllActors([&](RE::Actor* actor) {
            if (!IsEligibleNPC(actor, player) || !seen.insert(actor->GetFormID()).second) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            racemenu::QueueClearBodyChangerMorphs(actor);
            ++queued;
            return RE::BSContainer::ForEachResult::kContinue;
        });
        // Keep manual skin locks: a texture override has no mod-owner field in
        // RaceMenu, so blindly removing it could erase another mod's override.
        // The reset action intentionally concerns Body Changer NG body morphs.
        ClearManualBodyAssignments();
        return queued;
    }

    bool Distribution::ApplyActor(RE::Actor* actor) const
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!IsEligibleNPC(actor, player)) return false;
        std::optional<ManualAssignment> manual;
        {
            std::scoped_lock lock(lock_);
            const auto found = std::ranges::find(manualAssignments_, actor->GetFormID(), &ManualAssignment::actorFormID);
            if (found != manualAssignments_.end()) manual = *found;
        }
        if (manual) {
            bool queued{};
            if (manual->useDefaultBody && racemenu::IsReady()) {
                racemenu::QueueClearBodyChangerMorphs(actor);
                queued = true;
            } else if (!manual->presetId.empty()) {
                queued = racemenu::QueueApply(actor, manual->presetId, racemenu::ApplyMode::commit) == racemenu::ApplyResult::queued;
            }
            if (manual->useDefaultSkin) {
                queued = skin_override::QueueClear(actor) == skin_override::ApplyResult::queued || queued;
            } else if (!manual->skinProfileId.empty()) {
                queued = skin_override::QueueApply(actor, manual->skinProfileId) == skin_override::ApplyResult::queued || queued;
            }
            return queued;
        }
        const auto rules = Snapshot();
        const auto selection = ChooseRuleSelection(rules, actor);
        if (!selection.HasSelection()) return false;
        bool queued{};
        if (selection.presetId) {
            queued = racemenu::QueueApply(actor, *selection.presetId, racemenu::ApplyMode::commit) == racemenu::ApplyResult::queued;
        }
        if (selection.skinProfileId) {
            queued = skin_override::QueueApply(actor, *selection.skinProfileId) == skin_override::ApplyResult::queued || queued;
        }
        return queued;
    }

    bool Distribution::ImportOBodyDefaults()
    {
        const auto legacyPath = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "OBody_presetDistributionConfig.json";
        try {
            std::ifstream stream(legacyPath);
            if (!stream) return false;
            const auto root = nlohmann::json::parse(stream);
            if (!root.is_object()) throw std::runtime_error("OBody configuration root is not an object");

            const auto catalog = PresetCatalog::Get().Snapshot();
            std::unordered_set<std::string> blacklisted;
            if (const auto found = root.find("blacklistedPresetsFromRandomDistribution"); found != root.end() && found->is_array()) {
                for (const auto& value : *found) if (value.is_string()) blacklisted.insert(value.get<std::string>());
            }
            std::vector<DistributionRule> imported;
            const auto matchingPresetIds = [&catalog](const std::vector<std::string>& names, const bool female) {
                std::vector<std::string> ids;
                for (const auto& name : names) {
                    const auto found = std::ranges::find_if(catalog, [&](const auto& preset) {
                        return preset.male == !female && preset.name == name;
                    });
                    if (found != catalog.end() && std::ranges::find(ids, found->PersistentId()) == ids.end()) {
                        ids.push_back(found->PersistentId());
                    }
                }
                return ids;
            };
            const auto addRule = [&](std::string name, const DistributionScope scope, const bool female,
                                     std::string target, const std::uint32_t baseFormID,
                                     const std::vector<std::string>& presetNames, const bool excluded = false) {
                DistributionRule rule{
                    .id = "obody-import-" + std::to_string(imported.size() + 1U),
                    .name = std::move(name),
                    .female = female,
                    .scope = scope,
                    .npcBaseFormID = baseFormID,
                    .target = std::move(target),
                    .presetIds = matchingPresetIds(presetNames, female),
                    .bodyExcluded = excluded
                };
                if (excluded || !rule.presetIds.empty()) imported.push_back(std::move(rule));
            };
            const auto addForBothSexes = [&](const std::string& name, const DistributionScope scope,
                                             const std::string& target, const std::uint32_t baseFormID,
                                             const bool excluded) {
                addRule(name + " (female)", scope, true, target, baseFormID, {}, excluded);
                addRule(name + " (male)", scope, false, target, baseFormID, {}, excluded);
            };
            const auto importObjectRules = [&](const char* key, const DistributionScope scope, const bool female) {
                const auto node = root.find(key);
                if (node == root.end() || !node->is_object()) return;
                for (auto entry = node->begin(); entry != node->end(); ++entry) {
                    addRule(std::string("OBody ") + key + ": " + entry.key(), scope, female,
                        entry.key(), 0U, StringsFromJsonArray(entry.value()));
                }
            };

            // OBody tests these global exclusions before it evaluates individual
            // distribution rules, so the imported exclusions must stay at the top.
            if (const auto node = root.find("blacklistedNpcs"); node != root.end()) {
                for (const auto& name : StringsFromJsonArray(*node)) {
                    addForBothSexes("OBody excluded NPC: " + name, DistributionScope::npcName, name, 0U, true);
                }
            }
            if (const auto node = root.find("blacklistedNpcsFormID"); node != root.end() && node->is_object()) {
                auto* dataHandler = const_cast<RE::TESDataHandler*>(RE::TESDataHandler::GetSingleton());
                for (auto plugin = node->begin(); plugin != node->end(); ++plugin) {
                    const auto* file = dataHandler ? dataHandler->LookupModByName(plugin.key()) : nullptr;
                    if (!file || !plugin.value().is_array()) continue;
                    for (const auto& raw : plugin.value()) {
                        if (!raw.is_string()) continue;
                        std::uint32_t localFormID{};
                        if (!ParseOBodyLocalFormID(raw.get_ref<const std::string&>(), file->IsLight(), localFormID)) continue;
                        if (const auto* npc = dataHandler->LookupForm<RE::TESNPC>(localFormID, plugin.key())) {
                            addForBothSexes("OBody excluded NPC FormID: " + plugin.key(), DistributionScope::npcBaseForm,
                                {}, npc->GetFormID(), true);
                        }
                    }
                }
            }

            // OBody first checks explicit FormIDs, then localized NPC-name rules.
            if (const auto node = root.find("npcFormID"); node != root.end() && node->is_object()) {
                auto* dataHandler = const_cast<RE::TESDataHandler*>(RE::TESDataHandler::GetSingleton());
                for (auto plugin = node->begin(); plugin != node->end(); ++plugin) {
                    const auto* file = dataHandler ? dataHandler->LookupModByName(plugin.key()) : nullptr;
                    if (!file || !plugin.value().is_object()) continue;
                    for (auto form = plugin.value().begin(); form != plugin.value().end(); ++form) {
                        std::uint32_t localFormID{};
                        if (!ParseOBodyLocalFormID(form.key(), file->IsLight(), localFormID)) continue;
                        const auto* npc = dataHandler->LookupForm<RE::TESNPC>(localFormID, plugin.key());
                        if (!npc) continue;
                        const auto presets = StringsFromJsonArray(form.value());
                        addRule("OBody NPC FormID: " + plugin.key() + ":" + form.key(), DistributionScope::npcBaseForm,
                            true, {}, npc->GetFormID(), presets);
                        addRule("OBody NPC FormID: " + plugin.key() + ":" + form.key(), DistributionScope::npcBaseForm,
                            false, {}, npc->GetFormID(), presets);
                    }
                }
            }
            if (const auto node = root.find("npc"); node != root.end() && node->is_object()) {
                for (auto entry = node->begin(); entry != node->end(); ++entry) {
                    const auto presets = StringsFromJsonArray(entry.value());
                    addRule("OBody NPC: " + entry.key(), DistributionScope::npcName, true, entry.key(), 0U, presets);
                    addRule("OBody NPC: " + entry.key(), DistributionScope::npcName, false, entry.key(), 0U, presets);
                }
            }

            // OBody's plugin/race blacklists occur after explicit NPC rules but
            // before faction, plugin, and race distribution.
            const auto importExclusionList = [&](const char* key, const DistributionScope scope, const bool female) {
                const auto node = root.find(key);
                if (node == root.end()) return;
                for (const auto& value : StringsFromJsonArray(*node)) {
                    addRule(std::string("OBody excluded ") + key + ": " + value, scope, female, value, 0U, {}, true);
                }
            };
            importExclusionList("blacklistedNpcsPluginFemale", DistributionScope::pluginFile, true);
            importExclusionList("blacklistedNpcsPluginMale", DistributionScope::pluginFile, false);
            importExclusionList("blacklistedRacesFemale", DistributionScope::raceEditorID, true);
            importExclusionList("blacklistedRacesMale", DistributionScope::raceEditorID, false);

            importObjectRules("factionFemale", DistributionScope::factionEditorID, true);
            importObjectRules("factionMale", DistributionScope::factionEditorID, false);
            importObjectRules("npcPluginFemale", DistributionScope::pluginFile, true);
            importObjectRules("npcPluginMale", DistributionScope::pluginFile, false);
            importObjectRules("raceFemale", DistributionScope::raceEditorID, true);
            importObjectRules("raceMale", DistributionScope::raceEditorID, false);

            std::vector<std::string> femaleDefaultNames;
            std::vector<std::string> maleDefaultNames;
            for (const auto& preset : catalog) {
                if (blacklisted.contains(preset.name)) continue;
                (preset.male ? maleDefaultNames : femaleDefaultNames).push_back(preset.name);
            }
            addRule("Imported OBody female default", DistributionScope::allNPCs, true, {}, 0U, femaleDefaultNames);
            addRule("Imported OBody male default", DistributionScope::allNPCs, false, {}, 0U, maleDefaultNames);
            PrependDefaultExclusionRules(imported);
            SetRules(std::move(imported));
            return true;
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Changer NG could not import OBody defaults: {}", exception.what());
            return false;
        }
    }
}
