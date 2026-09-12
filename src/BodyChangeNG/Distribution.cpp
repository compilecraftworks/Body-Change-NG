#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/DistributionOverlayColors.h"
#include "BodyChangeNG/DistributionRuleNames.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariSupport.h"
#include "BodyChangeNG/OverlayPolicy.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/PathMigration.h"
#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinApplication.h"
#include "BodyChangeNG/SkinProfiles.h"

#include <SKSE/Logger.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESClass.h>
#include <RE/T/TESCombatStyle.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace
{
    constexpr auto kSchemaVersion = 7;

    [[nodiscard]] constexpr std::string_view BodyApplyResultLabel(
        const bcn::racemenu::ApplyResult result) noexcept
    {
        using Result = bcn::racemenu::ApplyResult;
        switch (result) {
        case Result::queued: return "queued";
        case Result::unavailable: return "racemenu-unavailable";
        case Result::invalidActor: return "invalid-actor";
        case Result::actor3DUnavailable: return "actor-3d-unavailable";
        case Result::missingPreset: return "missing-preset";
        case Result::emptyPreset: return "empty-preset";
        case Result::incompatibleSex: return "incompatible-sex";
        case Result::incompatibleBodyFamily: return "incompatible-body-family";
        case Result::noTaskInterface: return "task-interface-unavailable";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view SkinApplyResultLabel(
        const bcn::skin_application::ApplyResult result) noexcept
    {
        using Result = bcn::skin_application::ApplyResult;
        switch (result) {
        case Result::queued: return "queued";
        case Result::invalidActor: return "invalid-actor";
        case Result::missingProfile: return "missing-profile";
        case Result::incompatibleSex: return "incompatible-sex";
        case Result::incompatibleRace: return "incompatible-race";
        case Result::incompatibleBodyFamily: return "incompatible-body-family";
        case Result::ambiguousProfileLayout: return "ambiguous-profile-layout";
        case Result::ambiguousActorLayout: return "ambiguous-actor-layout";
        case Result::incompatibleFutanariType: return "incompatible-futanari-type";
        case Result::noTaskInterface: return "task-interface-unavailable";
        case Result::unsupportedRuntime: return "unsupported-runtime";
        case Result::actorBaseUnavailable: return "actor-base-unavailable";
        case Result::sharedActorBaseConflict: return "shared-actor-base-conflict";
        case Result::ownershipConflict: return "ownership-conflict";
        }
        return "unknown";
    }

    [[nodiscard]] std::filesystem::path LegacyDistributionPath()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "BodyChangerNGdistribution.json";
    }
    [[nodiscard]] bool IsEligibleNPC(RE::Actor* actor, RE::Actor* player)
    {
        // Loaded corpses still own a live RaceMenu morph target.  Excluding
        // them leaves actors that were already dead when a cell was scanned
        // on the generated Zeroed Sliders mesh forever.  Keep every other
        // safety boundary, and let ActorRegistry's applied signatures make
        // repeated attach/init events idempotent.
        return actor && bcn::IsDistributionActorStateEligible(actor == player, actor->IsDisabled(),
            actor->IsDead(), actor->Is3DLoaded(), actor->HasKeywordString("ActorTypeNPC"));
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

    [[nodiscard]] bool IsValidScope(const bcn::DistributionScope scope)
    {
        using enum bcn::DistributionScope;
        return scope == allNPCs || scope == npcBaseForm || scope == npcName || scope == factionEditorID ||
            scope == pluginFile || scope == raceEditorID || scope == modInstalledFollower || scope == elderNPC ||
            scope == keyword || scope == npcClass || scope == combatStyle;
    }

    [[nodiscard]] bool IsStableTargetFormScope(const bcn::DistributionScope scope)
    {
        using enum bcn::DistributionScope;
        return scope == factionEditorID || scope == raceEditorID || scope == keyword ||
            scope == npcClass || scope == combatStyle;
    }

    [[nodiscard]] bool IsExpectedTargetFormType(const bcn::DistributionScope scope, const RE::TESForm* form)
    {
        if (!form) return false;
        switch (scope) {
        case bcn::DistributionScope::factionEditorID:
            return form->GetFormType() == RE::FormType::Faction;
        case bcn::DistributionScope::raceEditorID:
            return form->GetFormType() == RE::FormType::Race;
        case bcn::DistributionScope::keyword:
            return form->GetFormType() == RE::FormType::Keyword;
        case bcn::DistributionScope::npcClass:
            return form->GetFormType() == RE::FormType::Class;
        case bcn::DistributionScope::combatStyle:
            return form->GetFormType() == RE::FormType::CombatStyle;
        default:
            return false;
        }
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

    [[nodiscard]] bool SetStableTargetIdentity(bcn::DistributionRule& rule, RE::TESForm* form)
    {
        if (!IsStableTargetFormScope(rule.scope) || !IsExpectedTargetFormType(rule.scope, form)) return false;
        const auto* file = form->GetFile(0);
        if (!file || file->GetFilename().empty()) return false;
        const auto localFormID = LocalFormID(form, file);
        if (localFormID == 0U) return false;
        rule.targetFormID = form->GetFormID();
        rule.targetPlugin = std::string{ file->GetFilename() };
        rule.targetLocalFormID = localFormID;
        if (const auto* editorID = form->GetFormEditorID(); editorID && editorID[0] != '\0') {
            rule.target = editorID;
        }
        return true;
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

    [[nodiscard]] bool ResolveStableTargetIdentity(bcn::DistributionRule& rule)
    {
        if (!IsStableTargetFormScope(rule.scope)) return true;
        auto* data = RE::TESDataHandler::GetSingleton();
        RE::TESForm* form{};
        if (data && !rule.targetPlugin.empty() && rule.targetLocalFormID != 0U) {
            form = data->LookupForm(rule.targetLocalFormID, rule.targetPlugin);
        }
        // Schema-3 and imported OBody rules stored only an EditorID. Resolve
        // that legacy representation once, then persist the stable identity.
        if (!IsExpectedTargetFormType(rule.scope, form) && !rule.target.empty()) {
            auto* legacy = RE::TESForm::LookupByEditorID(rule.target);
            if (IsExpectedTargetFormType(rule.scope, legacy)) {
                form = legacy;
                [[maybe_unused]] const auto converted = SetStableTargetIdentity(rule, form);
            }
        }
        rule.targetFormID = IsExpectedTargetFormType(rule.scope, form) ? form->GetFormID() : 0U;
        return rule.targetFormID != 0U;
    }

    [[nodiscard]] bool MatchesTarget(const bcn::DistributionRule& rule, RE::Actor* actor)
    {
        if (!rule.enabled || !actor) return false;
        const auto base = actor->GetActorBase();
        if (!base || (base->GetSex() == RE::SEX::kFemale) != rule.female) return false;
        if (!rule.includeCustomFollowers && IsCustomFollower(actor, base)) return false;
        if (!rule.includeElderNPCs && bcn::IsElderActor(base)) return false;
        switch (rule.scope) {
        case bcn::DistributionScope::allNPCs:
            return true;
        case bcn::DistributionScope::npcBaseForm:
            return base->GetFormID() == rule.npcBaseFormID;
        case bcn::DistributionScope::npcName:
            return EqualIgnoreCase(base->GetName(), rule.target);
        case bcn::DistributionScope::factionEditorID: {
            auto* form = RE::TESForm::LookupByID(rule.targetFormID);
            auto* faction = form && form->GetFormType() == RE::FormType::Faction ?
                static_cast<RE::TESFaction*>(form) : nullptr;
            // CommonLib exposes this query as a non-const member even though it
            // does not mutate the NPC.  The faction form comes from the loaded
            // data handler and is not modified here.
            return faction && base->IsInFaction(faction);
        }
        case bcn::DistributionScope::pluginFile: {
            const auto* file = base->GetFile(0);
            return file && EqualIgnoreCase(file->GetFilename(), rule.target);
        }
        case bcn::DistributionScope::raceEditorID: {
            const auto* race = base->GetRace();
            return race && race->GetFormID() == rule.targetFormID;
        }
        case bcn::DistributionScope::modInstalledFollower:
            return IsCustomFollower(actor, base);
        case bcn::DistributionScope::elderNPC:
            return bcn::IsElderActor(base);
        case bcn::DistributionScope::keyword: {
            auto* form = RE::TESForm::LookupByID(rule.targetFormID);
            const auto* target = form && form->GetFormType() == RE::FormType::Keyword ?
                static_cast<RE::BGSKeyword*>(form) : nullptr;
            return target && base->HasKeyword(target);
        }
        case bcn::DistributionScope::npcClass: {
            auto* form = RE::TESForm::LookupByID(rule.targetFormID);
            auto* target = form && form->GetFormType() == RE::FormType::Class ?
                static_cast<RE::TESClass*>(form) : nullptr;
            return target && base->IsInClass(target);
        }
        case bcn::DistributionScope::combatStyle: {
            auto* form = RE::TESForm::LookupByID(rule.targetFormID);
            const auto* target = form && form->GetFormType() == RE::FormType::CombatStyle ?
                static_cast<RE::TESCombatStyle*>(form) : nullptr;
            return target && base->GetCombatStyle() == target;
        }
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
        bool matched{};
        bool defaultBodyRequested{};
        std::string ruleId;
        std::optional<std::string> presetId;
        std::optional<std::string> skinProfileId;
        std::optional<std::string> futanariSkinId;
        std::array<std::optional<std::string>,
            bcn::overlay::Index(bcn::overlay::Area::count)> overlayIds;
        std::array<std::uint32_t, bcn::overlay::Index(bcn::overlay::Area::count)> overlayColors{};

        [[nodiscard]] bool HasSelection() const noexcept
        {
            return presetId.has_value() || skinProfileId.has_value() ||
                futanariSkinId.has_value() ||
                std::ranges::any_of(overlayIds, [](const auto& id) { return id.has_value(); });
        }
    };

    [[nodiscard]] bcn::DistributionFeature OverlayFeature(const bcn::overlay::Area area)
    {
        switch (area) {
        case bcn::overlay::Area::face: return bcn::DistributionFeature::overlayFace;
        case bcn::overlay::Area::hands: return bcn::DistributionFeature::overlayHands;
        case bcn::overlay::Area::feet: return bcn::DistributionFeature::overlayFeet;
        default: return bcn::DistributionFeature::overlayBody;
        }
    }

    [[nodiscard]] std::optional<std::string> ChooseFromPool(const bcn::DistributionRule& rule,
        const std::vector<std::string>& pool, RE::Actor* actor, const std::string_view kind,
        const std::string_view previous, const bcn::DistributionFeature feature)
    {
        if (pool.empty()) return std::nullopt;
        if (bcn::MayRetainPreviousDistributionSelection(feature) &&
            !previous.empty() && std::ranges::find(pool, previous) != pool.end()) {
            return std::string{ previous };
        }
        const auto base = actor->GetActorBase();
        const auto key = rule.id + ":" + std::string{ kind };
        const auto index = static_cast<std::size_t>(StableHash(key, base->GetFormID(),
            bcn::DistributionReferenceSeed(feature, actor->GetFormID())) % pool.size());
        return pool[index];
    }

    [[nodiscard]] std::vector<std::string> CompatibleSkinPool(
        const std::vector<std::string>& pool, RE::Actor* actor,
        const bcn::body_family::Mask distributionFamily)
    {
        if (pool.empty() || !actor) return {};

        const auto* base = actor->GetActorBase();
        if (!base) return {};
        const auto actorFemale = base->GetSex() == RE::SEX::kFemale;
        std::vector<std::string> legacyPool;
        legacyPool.reserve(pool.size());
        for (const auto& id : pool) {
            const auto profile = bcn::SkinProfiles::Get().Find(id);
            if (!profile || profile->layout != bcn::SkinLayout::legacy ||
                profile->race != bcn::SkinRace::humanoid ||
                (actorFemale ? profile->sex != bcn::SkinSex::female :
                    profile->sex != bcn::SkinSex::male)) continue;
            legacyPool.push_back(id);
        }
        return bcn::SkinProfiles::Get().CompatibleIds(legacyPool,
            actorFemale ? bcn::SkinSex::female : bcn::SkinSex::male, distributionFamily,
            bcn::ResolveActorSkinRace(actor));
    }

    [[nodiscard]] std::vector<std::string> CompatibleOverlayPool(
        const std::vector<std::string>& pool, RE::Actor* actor,
        const bcn::overlay::Area area)
    {
        if (!actor) return {};
        const auto* base = actor->GetActorBase();
        if (!base) return {};
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto family = bcn::body_family::ResolveActor(actor);
        std::vector<std::string> result;
        result.reserve(pool.size());
        for (const auto& id : pool) {
            if (const auto entry = bcn::overlay::Find(id); entry && entry->area == area &&
                entry->layout == bcn::overlay::Layout::legacy &&
                bcn::overlay::EntryMatchesActor(
                    entry->layout, entry->sex, family, female)) {
                result.push_back(id);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<std::string> CompatibleFutanariPool(
        const std::vector<std::string>& pool, RE::Actor* actor)
    {
        if (pool.empty() || !actor) return {};
        const auto type = bcn::futanari_support::RegisteredType(actor);
        if (!type || *type == bcn::FutanariSkinType::ubeTrx) return {};
        std::vector<std::string> result;
        result.reserve(pool.size());
        for (const auto& id : pool) {
            const auto profile = bcn::FutanariSkinProfiles::Get().Find(id);
            if (profile && profile->type == *type) result.push_back(id);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::string> CompatiblePresetPool(
        const std::vector<std::string>& pool, RE::Actor* actor,
        const bcn::body_family::Mask distributionFamily)
    {
        if (pool.empty() || !actor) return {};
        const auto* base = actor->GetActorBase();
        if (!base) return {};
        const auto actorMale = base->GetSex() != RE::SEX::kFemale;
        std::vector<std::string> legacyPool;
        legacyPool.reserve(pool.size());
        for (const auto& id : pool) {
            const auto preset = bcn::PresetCatalog::Get().Find(id);
            if (!preset || preset->male != actorMale ||
                (bcn::body_family::PresetMask(preset->family, preset->male) &
                    bcn::body_family::Bit(bcn::body_family::Family::ube)) != 0U) continue;
            legacyPool.push_back(id);
        }
        return bcn::PresetCatalog::Get().CompatibleIds(
            legacyPool, actorMale, distributionFamily);
    }

    [[nodiscard]] RuleSelection ChooseRuleSelection(const std::vector<bcn::DistributionRule>& rules,
        RE::Actor* actor, const std::optional<bcn::ActorState>& previous,
        const bcn::body_family::Mask distributionFamily, const bool useBodyPreset)
    {
        RuleSelection result;
        const auto rememberRule = [&](const bcn::DistributionRule& rule) {
            result.matched = true;
            if (result.ruleId.empty()) result.ruleId = rule.id;
        };

        // Each feature owns its own ordered rule stream. A body-only rule must
        // never shadow a later skin, overlay, or futanari rule merely because
        // all channels used to share one editor row in 1.1.x.
        for (const auto& rule : rules) {
            if (!MatchesTarget(rule, actor)) continue;
            if (rule.presetIds.empty()) continue;
            if (!useBodyPreset) {
                result.defaultBodyRequested = true;
                rememberRule(rule);
                break;
            }
            const auto compatible = CompatiblePresetPool(
                rule.presetIds, actor, distributionFamily);
            result.presetId = ChooseFromPool(rule, compatible, actor, "body",
                previous && !previous->body.selection.manual ?
                    std::string_view{ previous->body.selection.selectedId } : std::string_view{},
                bcn::DistributionFeature::body);
            if (result.presetId) {
                rememberRule(rule);
                break;
            }
        }

        for (const auto& rule : rules) {
            if (!MatchesTarget(rule, actor)) continue;
            if (rule.skinProfileIds.empty()) continue;
            const auto compatible = CompatibleSkinPool(
                rule.skinProfileIds, actor, distributionFamily);
            result.skinProfileId = ChooseFromPool(rule, compatible, actor, "skin",
                previous && !previous->skin.selection.manual ?
                    std::string_view{ previous->skin.selection.selectedId } : std::string_view{},
                bcn::DistributionFeature::skin);
            if (result.skinProfileId) {
                rememberRule(rule);
                break;
            }
        }

        for (const auto& rule : rules) {
            if (!rule.female || rule.futanariSkinIds.empty() || !MatchesTarget(rule, actor)) continue;
            const auto compatible = CompatibleFutanariPool(rule.futanariSkinIds, actor);
            result.futanariSkinId = ChooseFromPool(rule, compatible, actor, "futanari",
                previous && !previous->futanari.manual ?
                    std::string_view{ previous->futanari.selectedSkinId } : std::string_view{},
                bcn::DistributionFeature::futanari);
            if (result.futanariSkinId) {
                rememberRule(rule);
                break;
            }
        }

        for (const auto area : bcn::overlay::kAreas) {
            const auto index = bcn::overlay::Index(area);
            for (const auto& rule : rules) {
                if (!MatchesTarget(rule, actor)) continue;
                if (rule.overlayIds[index].empty()) continue;
                const auto compatible = CompatibleOverlayPool(rule.overlayIds[index], actor, area);
                const auto& previousArea = previous ?
                    previous->overlay.areas[index] : bcn::OverlayAreaState{};
                const auto previousId = previous && !previousArea.manual &&
                    !previousArea.items.empty() ?
                    std::string_view{ previousArea.items.front().selectedId } :
                    std::string_view{};
                result.overlayIds[index] = ChooseFromPool(rule, compatible, actor,
                    std::string{ "overlay-" } + std::string{ bcn::overlay::StableName(area) },
                    previousId, OverlayFeature(area));
                if (result.overlayIds[index]) {
                    result.overlayColors[index] = bcn::DistributionOverlayColor(rule, area, *result.overlayIds[index]);
                    rememberRule(rule);
                    break;
                }
            }
        }
        return result;
    }

    [[nodiscard]] std::string GenerateRuleId(const std::size_t index)
    {
        return "rule-" + std::to_string(index + 1U);
    }

    [[nodiscard]] std::vector<bcn::DistributionRule> DefaultDistributionRules()
    {
        // Distribution is opt-in: no hidden blacklist or default random pool
        // exists until the user selects catalog rows and creates a rule.
        return {};
    }

    [[nodiscard]] std::vector<bcn::DistributionRule> SplitRulesByFeature(
        std::vector<bcn::DistributionRule> rules)
    {
        std::vector<bcn::DistributionRule> expanded;
        expanded.reserve(rules.size());
        for (const auto& source : rules) {
            const auto append = [&](const std::string_view suffix, auto&& assign) {
                auto rule = source;
                rule.presetIds.clear();
                rule.skinProfileIds.clear();
                rule.futanariSkinIds.clear();
                for (auto& ids : rule.overlayIds) ids.clear();
                for (auto& colors : rule.overlayColors) colors.clear();
                assign(rule);
                if (!expanded.empty() && expanded.back().id == source.id) {
                    rule.id += suffix;
                } else if (std::ranges::any_of(expanded, [&](const auto& existing) {
                               return existing.id == source.id;
                           })) {
                    rule.id += suffix;
                }
                expanded.push_back(std::move(rule));
            };

            auto channelCount = static_cast<unsigned>(!source.presetIds.empty()) +
                static_cast<unsigned>(!source.skinProfileIds.empty()) +
                static_cast<unsigned>(!source.futanariSkinIds.empty()) +
                static_cast<unsigned>(std::ranges::any_of(source.overlayIds,
                    [](const auto& ids) { return !ids.empty(); }));
            if (channelCount == 0U) continue;
            if (channelCount == 1U) {
                expanded.push_back(source);
                continue;
            }
            auto first = true;
            const auto suffix = [&](const std::string_view value) {
                if (first) {
                    first = false;
                    return std::string_view{};
                }
                return value;
            };
            if (!source.presetIds.empty()) append(suffix("-body"), [&](auto& rule) {
                rule.presetIds = source.presetIds;
            });
            if (!source.skinProfileIds.empty()) append(suffix("-skin"), [&](auto& rule) {
                rule.skinProfileIds = source.skinProfileIds;
            });
            if (!source.futanariSkinIds.empty()) append(suffix("-futanari"), [&](auto& rule) {
                rule.futanariSkinIds = source.futanariSkinIds;
            });
            if (std::ranges::any_of(source.overlayIds,
                    [](const auto& ids) { return !ids.empty(); })) {
                append(suffix("-overlay"), [&](auto& rule) {
                    rule.overlayIds = source.overlayIds;
                    rule.overlayColors = source.overlayColors;
                });
            }
        }
        return expanded;
    }

    [[nodiscard]] std::vector<bcn::DistributionRule> NormalizeRules(std::vector<bcn::DistributionRule> rules)
    {
        rules = SplitRulesByFeature(std::move(rules));
        if (rules.size() > 256U) rules.resize(256U);
        std::unordered_set<std::string> known;
        for (std::size_t index{}; index < rules.size(); ++index) {
            auto& rule = rules[index];
            // Every persisted row is a positive, opt-in distribution rule.
            rule.enabled = true;
            if (rule.id.empty() || !known.insert(rule.id).second) {
                std::size_t suffix = index;
                do {
                    rule.id = GenerateRuleId(suffix++);
                } while (!known.insert(rule.id).second);
            }
            if (!rule.nameKey.empty() &&
                (!bcn::distribution_names::Find(rule.nameKey) ||
                    (!rule.name.empty() && !bcn::distribution_names::IsLocalizedValue(rule.nameKey, rule.name)))) {
                rule.nameKey.clear();
            }
            if (rule.nameKey.empty()) {
                rule.nameKey = bcn::distribution_names::RecognizeKey(rule.id, rule.name, rule.female);
            }
            if (rule.name.empty()) {
                if (rule.nameKey.empty()) rule.nameKey = bcn::distribution_names::DefaultRuleKey(rule.female);
                rule.name = bcn::distribution_names::Localized(rule.nameKey, bcn::UiLanguage::english);
            }
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
            if (IsStableTargetFormScope(rule.scope)) {
                [[maybe_unused]] const auto resolved = ResolveStableTargetIdentity(rule);
            } else {
                rule.targetFormID = 0U;
                rule.targetPlugin.clear();
                rule.targetLocalFormID = 0U;
            }
            if (rule.target.size() > 512U) rule.target.clear();
            rule.bodyFamily.clear();
            std::erase_if(rule.presetIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
            std::erase_if(rule.skinProfileIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
            std::erase_if(rule.futanariSkinIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
            for (auto& pool : rule.overlayIds) {
                std::erase_if(pool, [](const auto& id) { return id.empty() || id.size() > 1024U; });
            }
            bcn::PruneDistributionOverlayColors(rule);
        }
        return rules;
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

    [[nodiscard]] std::array<std::vector<std::string>,
        bcn::overlay::Index(bcn::overlay::Area::count)> OverlayPoolsFromJson(
        const nlohmann::json& source)
    {
        std::array<std::vector<std::string>,
            bcn::overlay::Index(bcn::overlay::Area::count)> result;
        if (!source.is_array()) return result;
        for (std::size_t index{}; index < result.size() && index < source.size(); ++index) {
            result[index] = StringsFromJsonArray(source[index]);
        }
        return result;
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
                    { "nameKey", rule.nameKey },
                    { "enabled", rule.enabled },
                    { "female", rule.female },
                    { "scope", static_cast<std::uint8_t>(rule.scope) },
                    { "npcPlugin", rule.npcPlugin },
                    { "npcLocalFormID", rule.npcLocalFormID },
                    { "targetPlugin", rule.targetPlugin },
                    { "targetLocalFormID", rule.targetLocalFormID },
                    { "target", rule.target },
                    { "bodyFamily", rule.bodyFamily },
                    { "presetIds", rule.presetIds },
                    { "skinProfileIds", rule.skinProfileIds },
                    { "futanariSkinIds", rule.futanariSkinIds },
                    { "overlayIds", rule.overlayIds },
                    { "overlayColors", rule.overlayColors },
                    { "includeCustomFollowers", rule.includeCustomFollowers },
                    { "includeElderNPCs", rule.includeElderNPCs }
                });
            }
            const nlohmann::json root{
                { "schemaVersion", kSchemaVersion },
                { "rules", std::move(serializedRules) }
            };
            auto temporary = path;
            temporary += ".new";
            {
                std::ofstream stream(temporary, std::ios::trunc | std::ios::binary);
                stream << root.dump(2) << '\n';
                stream.flush();
                if (!stream.good()) throw std::runtime_error("write failed");
            }
            // Never discard the last valid rule file before its replacement
            // has been flushed and parsed successfully.
            {
                std::ifstream verification(temporary, std::ios::binary);
                const auto parsed = nlohmann::json::parse(verification);
                if (!parsed.is_object() || parsed.value("schemaVersion", 0) != kSchemaVersion ||
                    !parsed.contains("rules") || !parsed["rules"].is_array()) {
                    throw std::runtime_error("temporary distribution verification failed");
                }
            }
            std::error_code error;
            if (!MoveFileExW(temporary.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const auto code = GetLastError();
                std::filesystem::remove(temporary, error);
                throw std::system_error(static_cast<int>(code), std::system_category(),
                    "atomic distribution replace");
            }
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

    bool SetDistributionRuleTargetForm(DistributionRule& rule, RE::TESForm* form)
    {
        return SetStableTargetIdentity(rule, form);
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
        {
            std::scoped_lock lock(lock_);
            savedRules_.reset();
        }
        const auto path = Path();
        const auto sourcePath = path_migration::ResolveFile(path, LegacyDistributionPath());
        std::vector<DistributionRule> loaded;
        try {
            if (!std::filesystem::exists(sourcePath.path)) {
                std::scoped_lock lock(lock_);
                rules_ = DefaultDistributionRules();
                evaluationRules_.reset();
                return false;
            }
            std::ifstream stream(sourcePath.path);
            const auto root = nlohmann::json::parse(stream);
            const auto schemaVersion = root.value("schemaVersion", 0);
            if ((schemaVersion < 3 || schemaVersion > kSchemaVersion) ||
                !root.contains("rules") || !root["rules"].is_array()) {
                throw std::runtime_error("unsupported distribution schema");
            }
            for (const auto& source : root["rules"]) {
                if (!source.is_object() || loaded.size() >= 256U) continue;
                // Versions that supported OBody distribution import tagged
                // generated rows. Retire those rows once and never turn them
                // into editable BCNG-owned rules.
                const auto sourceId = source.value("id", std::string{});
                if (source.value("source", std::string{}) == "obody" ||
                    sourceId.starts_with("obody-import-")) {
                    continue;
                }
                const auto legacyBoolean = [&](const std::string_view key) {
                    const auto value = source.find(key);
                    return value != source.end() && value->is_boolean() && value->get<bool>();
                };
                bool legacyOverlayExcluded{};
                if (const auto values = source.find("overlayExcluded");
                    values != source.end() && values->is_array()) {
                    legacyOverlayExcluded = std::ranges::any_of(*values, [](const auto& value) {
                        return value.is_boolean() && value.template get<bool>();
                    });
                }
                const auto legacyExcluded = legacyBoolean("excluded") ||
                    legacyBoolean("bodyExcluded") || legacyBoolean("skinExcluded") ||
                    legacyOverlayExcluded;
                const auto overlayPools = source.contains("overlayIds") ?
                    OverlayPoolsFromJson(source["overlayIds"]) :
                    decltype(DistributionRule::overlayIds){};
                const auto hasPositivePool = !source.value("presetIds", std::vector<std::string>{}).empty() ||
                    !source.value("skinProfileIds", std::vector<std::string>{}).empty() ||
                    !source.value("futanariSkinIds", std::vector<std::string>{}).empty() ||
                    std::ranges::any_of(overlayPools, [](const auto& pool) { return !pool.empty(); });
                if (legacyExcluded && !hasPositivePool) continue;
                DistributionRule rule{
                    .id = source.value("id", std::string{}),
                    .name = source.value("name", std::string{}),
                    .nameKey = source.value("nameKey", std::string{}),
                    .enabled = source.value("enabled", true),
                    .female = source.value("female", true),
                    .scope = static_cast<DistributionScope>(source.value("scope", 0)),
                    .npcPlugin = source.value("npcPlugin", std::string{}),
                    .npcLocalFormID = source.value("npcLocalFormID", 0U),
                    .targetPlugin = source.value("targetPlugin", std::string{}),
                    .targetLocalFormID = source.value("targetLocalFormID", 0U),
                    .target = source.value("target", std::string{}),
                    .bodyFamily = source.value("bodyFamily", std::string{}),
                    .presetIds = source.value("presetIds", std::vector<std::string>{}),
                    .skinProfileIds = source.value("skinProfileIds", std::vector<std::string>{}),
                    .futanariSkinIds = source.value("futanariSkinIds", std::vector<std::string>{}),
                    .overlayIds = overlayPools,
                    .overlayColors = source.contains("overlayColors") ?
                        ReadDistributionOverlayColors(source["overlayColors"], overlayPools) :
                        decltype(DistributionRule::overlayColors){},
                    .includeCustomFollowers = source.value("includeCustomFollowers", false),
                    .includeElderNPCs = source.value("includeElderNPCs", false)
                };
                if (rule.id.empty()) rule.id = GenerateRuleId(loaded.size());
                if (!IsValidScope(rule.scope)) continue;
                if (rule.target.size() > 512U) rule.target.clear();
                // Legacy exclusion-only rows were discarded above. Positive
                // pools survive migration without retaining exclusion state.
                std::erase_if(rule.presetIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                std::erase_if(rule.skinProfileIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                std::erase_if(rule.futanariSkinIds, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                for (auto& pool : rule.overlayIds) {
                    std::erase_if(pool, [](const auto& id) { return id.empty() || id.size() > 1024U; });
                }
                loaded.push_back(std::move(rule));
            }
            loaded = NormalizeRules(std::move(loaded));
            if (sourcePath.legacy) {
                if (WriteDistributionFile(path, loaded)) {
                    SKSE::log::info("Body Change NG migrated legacy distribution rules from {} to {}",
                        bcn::path_text::Utf8(sourcePath.path), bcn::path_text::Utf8(path));
                } else {
                    SKSE::log::warn("Body Change NG loaded legacy distribution rules but could not migrate them to {}",
                        bcn::path_text::Utf8(path));
                }
            }
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not load distribution rules from {}: {}",
                bcn::path_text::Utf8(sourcePath.path), exception.what());
            std::scoped_lock lock(lock_);
            rules_ = DefaultDistributionRules();
            evaluationRules_.reset();
            return false;
        }
        {
            std::scoped_lock lock(lock_);
            rules_ = std::move(loaded);
            evaluationRules_.reset();
        }
        return true;
    }

    bool Distribution::SaveRulesForNextGame(std::vector<DistributionRule> rules) const
    {
        rules = NormalizeRules(std::move(rules));
        if (!WriteDistributionFile(Path(), rules)) return false;
        std::scoped_lock lock(lock_);
        savedRules_ = std::move(rules);
        return true;
    }

    std::shared_ptr<const std::vector<DistributionRule>> Distribution::EvaluationRules() const
    {
        std::scoped_lock lock(lock_);
        if (!evaluationRules_) evaluationRules_ = std::make_shared<const std::vector<DistributionRule>>(rules_);
        return evaluationRules_;
    }

    std::vector<DistributionRule> Distribution::SavedRulesSnapshot() const
    {
        std::scoped_lock lock(lock_);
        return savedRules_ ? *savedRules_ : rules_;
    }

    void Distribution::SetRules(std::vector<DistributionRule> rules)
    {
        rules = NormalizeRules(std::move(rules));
        std::scoped_lock lock(lock_);
        rules_ = std::move(rules);
        evaluationRules_.reset();
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

    bool Distribution::ApplyActor(RE::Actor* actor) const
    {
        // User preview owns this actor until confirm/close. Automatic
        // distribution must not replace the interactive body or skin.
        if (actor && (frame_tasks::HasPreview(actor->GetFormID()) ||
            racemenu::HasActivePreview(actor) || overlay::HasActivePreview(actor))) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!IsEligibleNPC(actor, player)) return false;
        const auto manual = ActorRegistry::Get().ManualSelection(actor);
        const auto previous = ActorRegistry::Get().Snapshot(actor);
        const auto rules = EvaluationRules();
        const auto actorFemale = actor->GetActorBase() &&
            actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        const auto settings = Settings::Get().Snapshot();
        const auto distributionFamily = actorFemale ?
            NpcDistributionFamily(settings.femaleNpcBodyType) :
            NpcDistributionFamily(settings.maleNpcBodyType);
        const auto useBodyPreset = actorFemale ? UsesNpcBodyPreset(settings.femaleNpcBodyType) :
            UsesNpcBodyPreset(settings.maleNpcBodyType);
        const auto selection = ChooseRuleSelection(
            *rules, actor, previous, distributionFamily, useBodyPreset);
        ActorRegistry::Get().SetRuleSelection(actor, selection.presetId,
            selection.skinProfileId, selection.defaultBodyRequested,
            selection.futanariSkinId);
        // The absence of a matching positive rule means "unchanged", not
        // "forget the co-save choice". Resolve
        // the effective state after the tri-state update so a native skin
        // graph (which is intentionally detached at a load boundary) and an
        // unverified BodyMorph choice are actually restored in the new
        // session even when the current rule contributes no new ID.
        const auto effective = ActorRegistry::Get().Snapshot(actor);
        const auto* automaticBody = effective && (!manual || !manual->hasBody) ?
            &effective->body.selection : nullptr;
        const auto* automaticSkin = effective && (!manual || !manual->hasSkin) ?
            &effective->skin.selection : nullptr;
        const auto* automaticFutanari = effective && (!manual || !manual->hasFutanari) ?
            &effective->futanari : nullptr;
        bool queued{};
        const auto ruleSource =
            (selection.ruleId.empty() ? std::string_view{ "none" } :
                std::string_view{ selection.ruleId });
        const auto bodySource = manual && manual->hasBody ?
            std::string_view{ "manual" } : ruleSource;
        const auto skinSource = manual && manual->hasSkin ?
            std::string_view{ "manual" } : ruleSource;
        const auto queueBody = [&](const std::string_view presetId,
                                   const racemenu::ApplyResult result) {
            if (result != racemenu::ApplyResult::queued) {
                SKSE::log::warn(
                    "Body Change NG rejected NPC body distribution actor={:08X} source='{}' preset='{}' result={}",
                    actor->GetFormID(), bodySource, presetId, BodyApplyResultLabel(result));
                return false;
            }
            return true;
        };
        const auto queueSkin = [&](const std::string_view profileId,
                                   const skin_application::ApplyResult result) {
            if (result != skin_application::ApplyResult::queued) {
                SKSE::log::warn(
                    "Body Change NG rejected NPC skin distribution actor={:08X} base={:08X} source='{}' profile='{}' result={}",
                    actor->GetFormID(), actor->GetActorBase()->GetFormID(), skinSource,
                    profileId, SkinApplyResultLabel(result));
                return false;
            }
            return true;
        };

        if (manual && manual->hasBody && manual->useDefaultBody && racemenu::IsReady() &&
            ActorRegistry::Get().NeedsBodyApply(actor, {}, true)) {
            racemenu::QueueClearBodyChangeMorphs(actor);
            queued = true;
        } else if (manual && manual->hasBody && !manual->bodyId.empty() &&
            ActorRegistry::Get().NeedsBodyApply(actor, manual->bodyId, false)) {
            const auto bodyQueued = queueBody(manual->bodyId,
                racemenu::QueueApply(actor, manual->bodyId, racemenu::ApplyMode::commit));
            queued = bodyQueued;
        } else if (automaticBody && !automaticBody->useDefault &&
            !automaticBody->selectedId.empty() &&
            ActorRegistry::Get().NeedsBodyApply(actor, automaticBody->selectedId, false)) {
            // Automatic distribution has at most one accepted body result per
            // actor. Let RaceMenu defer its expensive partition rebuild just
            // like OBody NG, while manual UI changes retain the synchronous
            // ordering needed for rapid preview/commit input.
            const auto bodyQueued = queueBody(automaticBody->selectedId,
                racemenu::QueueApply(actor, automaticBody->selectedId,
                    racemenu::ApplyMode::commit, 0U,
                    racemenu::UpdatePolicy::deferred));
            queued = bodyQueued;
        } else if (automaticBody && automaticBody->useDefault &&
            ActorRegistry::Get().NeedsBodyApply(actor, {}, true)) {
            racemenu::QueueClearBodyChangeMorphs(actor);
            queued = true;
        }

        if (manual && manual->hasSkin && manual->useDefaultSkin &&
            ActorRegistry::Get().NeedsSkinApply(actor, {}, true)) {
            queued = queueSkin("<default>", skin_application::QueueClear(actor)) || queued;
        } else if (manual && manual->hasSkin && !manual->skinId.empty() &&
            ActorRegistry::Get().NeedsSkinApply(actor, manual->skinId, false)) {
            queued = queueSkin(manual->skinId,
                skin_application::QueueApply(actor, manual->skinId)) || queued;
        } else if (automaticSkin && !automaticSkin->useDefault &&
            !automaticSkin->selectedId.empty() &&
            ActorRegistry::Get().NeedsSkinApply(actor, automaticSkin->selectedId, false)) {
            queued = queueSkin(automaticSkin->selectedId,
                skin_application::QueueApply(actor, automaticSkin->selectedId)) || queued;
        }
        // ActorWorkQueue immediately follows this selection pass with the
        // provider-rebuild reconciler. Keep futanari state selection here and
        // let that single path perform the TXST mutation, avoiding two jobs
        // for the same slot-52 ArmorAddon on one attach event.
        queued = (selection.futanariSkinId.has_value() && automaticFutanari &&
            !automaticFutanari->manual) || queued;
        // Newly selected distribution overlays are applied independently by
        // anatomical area. Existing saved selections are restored by the
        // actor reconcile boundary even when this rule contributes no new ID.
        for (const auto area : overlay::kAreas) {
            const auto index = overlay::Index(area);
            const auto effectiveArea = effective ?
                std::addressof(effective->overlay.areas[index]) : nullptr;
            if (!selection.overlayIds[index] || (effectiveArea && effectiveArea->manual)) continue;
            const auto result = overlay::QueueApply(actor, area,
                *selection.overlayIds[index], overlay::ApplyMode::automatic, selection.overlayColors[index]);
            queued = result == overlay::ApplyResult::queued || queued;
            if (result != overlay::ApplyResult::queued &&
                result != overlay::ApplyResult::missingEntry) {
                SKSE::log::warn("Body Change NG rejected NPC overlay distribution actor={:08X} source='{}' area={} id='{}' result={}",
                    actor->GetFormID(), ruleSource, overlay::StableName(area),
                    *selection.overlayIds[index], static_cast<std::uint32_t>(result));
            }
        }
        return queued;
    }

}
