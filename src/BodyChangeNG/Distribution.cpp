#include "BodyChangeNG/Distribution.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/PathMigration.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinOverrides.h"
#include "BodyChangeNG/SkinProfiles.h"

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
    constexpr auto kSchemaVersion = 3;

    [[nodiscard]] std::filesystem::path LegacyDistributionPath()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "BodyChangerNGdistribution.json";
    }
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

    [[nodiscard]] std::uint32_t LocalFormID(const RE::TESForm* form, const RE::TESFile* file)
    {
        return form && file ? form->GetFormID() & (file->IsLight() ? 0xFFFU : 0xFFFFFFU) : 0U;
    }

    [[nodiscard]] bool SetStableNPCIdentity(bcn::DistributionRule& rule, RE::TESNPC* base)
    {
        const auto* file = base ? base->GetFile(0) : nullptr;
        if (!base || !file || file->GetFilename().empty()) return false;
        rule.npcBaseFormID = base->GetFormID();
        rule.npcPlugin = std::string{ file->GetFilename() };
        rule.npcLocalFormID = LocalFormID(base, file);
        return rule.npcLocalFormID != 0U;
    }

    [[nodiscard]] bool ResolveStableNPCIdentity(bcn::DistributionRule& rule)
    {
        if (rule.scope != bcn::DistributionScope::npcBaseForm) return true;
        if (rule.npcPlugin.empty() || rule.npcLocalFormID == 0U) {
            rule.npcBaseFormID = 0U;
            return false;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* base = data ? data->LookupForm<RE::TESNPC>(rule.npcLocalFormID, rule.npcPlugin) : nullptr;
        rule.npcBaseFormID = base ? base->GetFormID() : 0U;
        return base != nullptr;
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
        const std::vector<std::string>& pool, RE::Actor* actor, const std::string_view kind,
        const std::string_view previous)
    {
        if (pool.empty()) return std::nullopt;
        if (!previous.empty() && std::ranges::find(pool, previous) != pool.end()) {
            return std::string{ previous };
        }
        const auto base = actor->GetActorBase();
        const auto key = rule.id + ":" + std::string{ kind };
        const auto index = static_cast<std::size_t>(StableHash(key, base->GetFormID(), actor->GetFormID()) % pool.size());
        return pool[index];
    }

    [[nodiscard]] std::vector<std::string> CompatibleSkinPool(
        const std::vector<std::string>& pool, RE::Actor* actor)
    {
        if (pool.empty() || !actor) return {};

        const auto actorFamily = bcn::body_family::ResolveActor(actor);
        if (actorFamily == 0U) return pool;

        const auto profiles = bcn::SkinProfiles::Get().Snapshot();
        std::vector<std::string> compatible;
        compatible.reserve(pool.size());
        for (const auto& id : pool) {
            const auto found = std::ranges::find(profiles, id, &bcn::SkinProfile::id);
            if (found != profiles.end() && bcn::SkinMatchesActor(found->bodyFamilies, actorFamily)) {
                compatible.push_back(id);
            }
        }
        return compatible;
    }

    [[nodiscard]] RuleSelection ChooseRuleSelection(const std::vector<bcn::DistributionRule>& rules,
        RE::Actor* actor, const std::optional<bcn::ActorState>& previous)
    {
        for (const auto& rule : rules) {
            if (!MatchesTarget(rule, actor)) continue;
            const auto compatibleSkins = rule.skinExcluded ? std::vector<std::string>{} :
                CompatibleSkinPool(rule.skinProfileIds, actor);
            return RuleSelection{
                .bodyExcluded = rule.bodyExcluded,
                .skinExcluded = rule.skinExcluded,
                .presetId = rule.bodyExcluded ? std::nullopt : ChooseFromPool(rule, rule.presetIds,
                    actor, "body", previous && !previous->manualBody ?
                        std::string_view{ previous->selectedBodyId } : std::string_view{}),
                .skinProfileId = rule.skinExcluded ? std::nullopt : ChooseFromPool(rule, compatibleSkins,
                    actor, "skin", previous && !previous->manualSkin ?
                        std::string_view{ previous->selectedSkinId } : std::string_view{})
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
            if (rule.scope == bcn::DistributionScope::npcBaseForm) {
                if (rule.npcPlugin.empty() && rule.npcBaseFormID != 0U) {
                    if (auto* form = RE::TESForm::LookupByID(rule.npcBaseFormID)) {
                        RE::TESNPC* base{};
                        if (auto* actor = form->As<RE::Actor>()) base = actor->GetActorBase();
                        else if (form->GetFormType() == RE::FormType::NPC) base = static_cast<RE::TESNPC*>(form);
                        [[maybe_unused]] const auto normalized = SetStableNPCIdentity(rule, base);
                    }
                }
                [[maybe_unused]] const auto resolved = ResolveStableNPCIdentity(rule);
            } else {
                rule.npcBaseFormID = 0U;
                rule.npcPlugin.clear();
                rule.npcLocalFormID = 0U;
            }
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
        const std::vector<bcn::DistributionRule>& rules)
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
                    { "npcPlugin", rule.npcPlugin },
                    { "npcLocalFormID", rule.npcLocalFormID },
                    { "target", rule.target },
                    { "bodyFamily", rule.bodyFamily },
                    { "presetIds", rule.presetIds },
                    { "skinProfileIds", rule.skinProfileIds },
                    { "excluded", rule.bodyExcluded && rule.skinExcluded },
                    { "bodyExcluded", rule.bodyExcluded },
                    { "skinExcluded", rule.skinExcluded },
                    { "source", rule.importedFromOBody ? "obody" : "bodychangeng" }
                });
            }
            const nlohmann::json root{
                { "schemaVersion", kSchemaVersion },
                { "rules", std::move(serializedRules) }
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
            SKSE::log::error("Body Change NG could not save distribution rules: {}", exception.what());
            return false;
        }
    }
}

namespace bcn
{
    bool SetDistributionRuleNPC(DistributionRule& rule, RE::TESForm* form)
    {
        RE::TESNPC* base{};
        if (auto* actor = form ? form->As<RE::Actor>() : nullptr) {
            base = actor->GetActorBase();
        } else if (form && form->GetFormType() == RE::FormType::NPC) {
            base = static_cast<RE::TESNPC*>(form);
        }
        return SetStableNPCIdentity(rule, base);
    }

    Distribution& Distribution::Get()
    {
        static Distribution distribution;
        return distribution;
    }

    std::filesystem::path Distribution::Path()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "BodyChangeNGdistribution.json";
    }

    bool Distribution::Load()
    {
        const auto path = Path();
        const auto sourcePath = path_migration::ResolveFile(path, LegacyDistributionPath());
        std::vector<DistributionRule> loaded;
        try {
            if (!std::filesystem::exists(sourcePath.path)) {
                std::scoped_lock lock(lock_);
                rules_ = DefaultExclusionRules();
                return false;
            }
            std::ifstream stream(sourcePath.path);
            const auto root = nlohmann::json::parse(stream);
            const auto schemaVersion = root.value("schemaVersion", 0);
            if (schemaVersion != kSchemaVersion ||
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
                    .npcPlugin = source.value("npcPlugin", std::string{}),
                    .npcLocalFormID = source.value("npcLocalFormID", 0U),
                    .target = source.value("target", std::string{}),
                    .bodyFamily = source.value("bodyFamily", std::string{}),
                    .presetIds = source.value("presetIds", std::vector<std::string>{}),
                    .skinProfileIds = source.value("skinProfileIds", std::vector<std::string>{}),
                    .excluded = source.value("excluded", false),
                    .bodyExcluded = source.value("bodyExcluded", false),
                    .skinExcluded = source.value("skinExcluded", false),
                    .importedFromOBody = source.value("source", std::string{}) == "obody"
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
            loaded = NormalizeRules(std::move(loaded));
            if (sourcePath.legacy) {
                if (WriteDistributionFile(path, loaded)) {
                    SKSE::log::info("Body Change NG migrated legacy distribution rules from {} to {}",
                        sourcePath.path.string(), path.string());
                } else {
                    SKSE::log::warn("Body Change NG loaded legacy distribution rules but could not migrate them to {}",
                        path.string());
                }
            }
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not load distribution rules from {}: {}",
                sourcePath.path.string(), exception.what());
            std::scoped_lock lock(lock_);
            rules_ = DefaultExclusionRules();
            return false;
        }
        {
            std::scoped_lock lock(lock_);
            rules_ = std::move(loaded);
        }
        return true;
    }

    bool Distribution::Save() const
    {
        const auto path = Path();
        std::vector<DistributionRule> rules;
        {
            std::scoped_lock lock(lock_);
            rules = rules_;
        }
        return WriteDistributionFile(path, rules);
    }

    bool Distribution::SaveRulesForNextGame(std::vector<DistributionRule> rules) const
    {
        rules = NormalizeRules(std::move(rules));
        return WriteDistributionFile(Path(), rules);
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
        ActorRegistry::Get().SetManualBody(actor, std::move(presetId), false);
    }

    void Distribution::SetManualSkinAssignment(RE::Actor* actor, std::string profileId)
    {
        ActorRegistry::Get().SetManualSkin(actor, std::move(profileId), false);
    }

    void Distribution::SetManualDefaultBody(RE::Actor* actor)
    {
        ActorRegistry::Get().SetManualBody(actor, {}, true);
    }

    void Distribution::SetManualDefaultSkin(RE::Actor* actor)
    {
        ActorRegistry::Get().SetManualSkin(actor, {}, true);
    }

    bool Distribution::RemoveManualAssignment(RE::Actor* actor)
    {
        return ActorRegistry::Get().RemoveManual(actor);
    }

    bool Distribution::RemoveManualBodyAssignment(RE::Actor* actor)
    {
        return ActorRegistry::Get().RemoveManualBody(actor);
    }

    void Distribution::ClearManualAssignments()
    {
        ActorRegistry::Get().ClearManualSelections();
    }

    void Distribution::ClearManualBodyAssignments()
    {
        ActorRegistry::Get().ClearManualBodySelections();
    }

    bool Distribution::HasManualAssignment(const RE::Actor* actor) const
    {
        return ActorRegistry::Get().HasManualSelection(actor);
    }

    std::size_t Distribution::ApplyLoadedNPCs()
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
            if (ActorWorkQueue::Get().Request(actor, ActorWorkReason::bulkLoad)) {
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
            racemenu::QueueClearBodyChangeMorphs(actor);
            ++queued;
            return RE::BSContainer::ForEachResult::kContinue;
        });
        // Keep manual skin locks: a texture override has no mod-owner field in
        // RaceMenu, so blindly removing it could erase another mod's override.
        // The reset action intentionally concerns Body Change NG body morphs.
        ClearManualBodyAssignments();
        ActorRegistry::Get().InvalidateAllBodyResults();
        return queued;
    }

    bool Distribution::ApplyActor(RE::Actor* actor) const
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!IsEligibleNPC(actor, player)) return false;
        const auto manual = ActorRegistry::Get().ManualSelection(actor);
        const auto previous = ActorRegistry::Get().Snapshot(actor);
        const auto rules = Snapshot();
        const auto selection = ChooseRuleSelection(rules, actor, previous);
        ActorRegistry::Get().SetRuleSelection(actor, selection.presetId, selection.skinProfileId);
        bool queued{};

        if (manual && manual->hasBody && manual->useDefaultBody && racemenu::IsReady() &&
            ActorRegistry::Get().NeedsBodyApply(actor, {}, true)) {
            racemenu::QueueClearBodyChangeMorphs(actor);
            queued = true;
        } else if (manual && manual->hasBody && !manual->bodyId.empty() &&
            ActorRegistry::Get().NeedsBodyApply(actor, manual->bodyId, false)) {
            queued = racemenu::QueueApply(actor, manual->bodyId,
                racemenu::ApplyMode::commit) == racemenu::ApplyResult::queued;
        } else if ((!manual || !manual->hasBody) && selection.presetId &&
            ActorRegistry::Get().NeedsBodyApply(actor, *selection.presetId, false)) {
            // Automatic distribution has at most one accepted body result per
            // actor. Let RaceMenu defer its expensive partition rebuild just
            // like OBody NG, while manual UI changes retain the synchronous
            // ordering needed for rapid preview/commit input.
            queued = racemenu::QueueApply(actor, *selection.presetId,
                racemenu::ApplyMode::commit, 0U,
                racemenu::UpdatePolicy::deferred) == racemenu::ApplyResult::queued;
        }

        if (manual && manual->hasSkin && manual->useDefaultSkin &&
            ActorRegistry::Get().NeedsSkinApply(actor, {}, true)) {
            queued = skin_override::QueueClear(actor) == skin_override::ApplyResult::queued || queued;
        } else if (manual && manual->hasSkin && !manual->skinId.empty() &&
            ActorRegistry::Get().NeedsSkinApply(actor, manual->skinId, false)) {
            queued = skin_override::QueueApply(actor, manual->skinId) == skin_override::ApplyResult::queued || queued;
        } else if ((!manual || !manual->hasSkin) && selection.skinProfileId &&
            ActorRegistry::Get().NeedsSkinApply(actor, *selection.skinProfileId, false)) {
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
            std::size_t requestedPresetNames{};
            std::unordered_set<std::string> missingPresetNames;
            const auto matchingPresetIds = [&catalog, &requestedPresetNames, &missingPresetNames](
                                               const std::vector<std::string>& names, const bool female) {
                std::vector<std::string> ids;
                for (const auto& name : names) {
                    ++requestedPresetNames;
                    const auto found = std::ranges::find_if(catalog, [&](const auto& preset) {
                        return preset.male == !female && preset.name == name;
                    });
                    if (found != catalog.end() && std::ranges::find(ids, found->PersistentId()) == ids.end()) {
                        ids.push_back(found->PersistentId());
                    } else if (found == catalog.end()) {
                        missingPresetNames.insert(name);
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
                    .bodyExcluded = excluded,
                    .importedFromOBody = true
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
            auto merged = Snapshot();
            std::erase_if(merged, [](const DistributionRule& rule) {
                return rule.importedFromOBody || rule.id.starts_with("obody-import-");
            });
            merged.insert(merged.end(), std::make_move_iterator(imported.begin()),
                std::make_move_iterator(imported.end()));
            SetRules(std::move(merged));
            SKSE::log::info(
                "Body Change NG imported {} OBody distribution rules; preset references={} unique-missing={}",
                imported.size(), requestedPresetNames, missingPresetNames.size());
            for (const auto& name : missingPresetNames) {
                SKSE::log::warn("Body Change NG could not match imported OBody preset '{}'", name);
            }
            return true;
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not import OBody defaults: {}", exception.what());
            return false;
        }
    }
}
