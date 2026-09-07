#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/DistributionRuleNames.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void Require(const bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
}

int main()
{
    try {
        // Schema 4 appends new scopes. Existing schema-3 numeric values must
        // never move, or a saved rule could silently change meaning.
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::allNPCs) == 0U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::npcBaseForm) == 1U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::npcName) == 2U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::factionEditorID) == 3U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::pluginFile) == 4U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::raceEditorID) == 5U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::modInstalledFollower) == 6U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::elderNPC) == 7U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::keyword) == 8U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::npcClass) == 9U);
        static_assert(static_cast<std::uint8_t>(bcn::DistributionScope::combatStyle) == 10U);
        using bcn::StableStateSignature;
        const auto body = StableStateSignature("body", "preset-a", false, 0U);
        Require(body == StableStateSignature("body", "preset-a", false, 0U),
            "identical body state was not stable");
        Require(body != StableStateSignature("body", "preset-b", false, 0U),
            "different preset IDs collided");
        Require(body != StableStateSignature("skin", "preset-a", false, 0U),
            "channel identity was not included");
        Require(body != StableStateSignature("body", "preset-a", true, 0U),
            "default and selected state collided");
        Require(body != StableStateSignature("body", "preset-a", false, 1U),
            "randomization options were not included");
        Require(bcn::UsesQueuedAutomaticPath(false) && bcn::UsesQueuedAutomaticPath(true),
            "an automatic actor mode bypassed the coalescing queue");
        Require(bcn::AutomaticActorBudget(false) == 4U,
            "normal mode actor budget changed");
        Require(bcn::AutomaticActorBudget(true) == 2U,
            "performance mode actor budget changed");
        Require(bcn::InitialDistributionDelayTicks() == 2U,
            "initial distribution no longer yields to save-load listeners");
        Require(bcn::DistributionReferenceSeed(bcn::DistributionFeature::skin, 0x1234U) == 0U &&
                bcn::DistributionReferenceSeed(bcn::DistributionFeature::body, 0x1234U) == 0x1234U,
            "native skin distribution escaped ActorBase scope or body distribution lost reference scope");
        Require(!bcn::MayRetainPreviousDistributionSelection(bcn::DistributionFeature::skin) &&
                bcn::MayRetainPreviousDistributionSelection(bcn::DistributionFeature::body),
            "a legacy reference-scoped skin choice could prevent ActorBase convergence");
        Require(bcn::IsDistributionActorStateEligible(false, false, false, true, true) &&
                bcn::IsDistributionActorStateEligible(false, false, true, true, true),
            "loaded corpses were excluded from NPC distribution");
        Require(!bcn::IsDistributionActorStateEligible(true, false, false, true, true) &&
                !bcn::IsDistributionActorStateEligible(false, true, false, true, true) &&
                !bcn::IsDistributionActorStateEligible(false, false, false, false, true) &&
                !bcn::IsDistributionActorStateEligible(false, false, false, true, false),
            "NPC distribution eligibility lost a player/disabled/3D/type safety boundary");
        Require(StableStateSignature("body", "same", false, 0, 1ULL) !=
            StableStateSignature("body", "same", false, 0, 0x100000001ULL), "upper content hash bits lost");
        const auto omittedCorrection = bcn::racemenu::AbsolutePresetCorrection(0.0F, 0.4F);
        Require(std::abs(omittedCorrection + 0.4F) < 0.00001F &&
                std::abs(0.4F + omittedCorrection) < 0.00001F,
            "an XML-omitted body slider did not normalize to zero");
        const auto firstCorrection = bcn::racemenu::AbsolutePresetCorrection(0.8F, 0.35F);
        const auto repeatedCorrection = bcn::racemenu::AbsolutePresetCorrection(0.8F, 0.35F);
        Require(firstCorrection == repeatedCorrection &&
                std::abs(0.35F + repeatedCorrection - 0.8F) < 0.00001F,
            "repeated distribution accumulated body morph values");
        const auto previewCorrection = bcn::racemenu::AbsolutePresetCorrection(0.8F, 1.05F, 0.1F);
        Require(std::abs(1.05F + previewCorrection - 0.9F) < 0.00001F,
            "preview normalization did not preserve the current outfit correction");
        const auto externalMorph = 0.35F;
        const auto committedMorph = bcn::racemenu::AbsolutePresetCorrection(0.8F, externalMorph);
        const auto outfitCorrection = bcn::racemenu::OutfitTargetCorrection(
            0.0F, externalMorph + committedMorph);
        Require(std::abs(externalMorph + committedMorph + outfitCorrection) < 0.00001F,
            "procedural outfit correction retained an external RaceMenu morph contribution");

        bcn::ActorState state{
            .actorFormID = 0x1234U,
            .baseLocalFormID = 0x5678U,
            .basePlugin = "Example.esp",
            .body = {
                .selection = { .selectedId = "preset-a", .manual = true },
                .application = { .signature = body }
            },
            .skin = {
                .selection = { .selectedId = "skin-a", .manual = true },
                .application = { .signature = StableStateSignature("skin", "skin-a", false) }
            },
            .futanari = { .selectedSkinId = "futanari:skin-a:cbbe-trx" }
        };
        Require(state.body.selection.manual && state.skin.selection.manual &&
                state.body.selection.selectedId != state.skin.selection.selectedId,
            "body and skin channels did not remain independent");
        state.body.application.applied = state.skin.application.applied = true;
        state.body.application.verifiedThisSession = state.skin.application.verifiedThisSession = true;
        bcn::PrepareRestoredState(state);
        Require(state.body.application.applied && state.skin.application.applied &&
                state.futanari.selectedSkinId == "futanari:skin-a:cbbe-trx" &&
                !state.body.application.verifiedThisSession &&
                !state.skin.application.verifiedThisSession,
            "cosave restore lost the futanari selection or live-session proof boundary");
        const auto preservedBodyBeforeReset = state.body;
        const auto preservedSkin = state.skin;
        const auto preservedFutanari = state.futanari;
        state.body = {};
        Require(state.skin.selection.selectedId == preservedSkin.selection.selectedId &&
                state.skin.application.signature == preservedSkin.application.signature &&
                state.futanari.selectedSkinId == preservedFutanari.selectedSkinId,
            "resetting body state crossed the skin or futanari feature boundary");
        state.body = preservedBodyBeforeReset;
        const auto preservedBody = state.body;
        state.skin = {};
        Require(state.body.selection.selectedId == preservedBody.selection.selectedId &&
                state.body.application.signature == preservedBody.application.signature &&
                state.futanari.selectedSkinId == preservedFutanari.selectedSkinId,
            "resetting skin state crossed the body or futanari feature boundary");
        using Decision = bcn::RestoredApplicationDecision;
        Require(bcn::EvaluateRestoredApplication(true, false, true, true) == Decision::acceptLive,
            "a matching restored live state was not accepted");
        Require(bcn::EvaluateRestoredApplication(true, false, true, false) == Decision::apply &&
                bcn::EvaluateRestoredApplication(true, false, true, std::nullopt) == Decision::apply,
            "a missing or unverifiable restored live state was incorrectly skipped");
        Require(bcn::EvaluateRestoredApplication(true, true, true, std::nullopt) == Decision::skipVerified,
            "current-session verification was not cached");
        Require(bcn::EvaluateRestoredApplication(true, false, false, true) == Decision::apply,
            "a stale persisted signature was accepted from live state alone");
        bcn::DistributionRule rule{
            .female = true,
            .bodyFamily = "CBBE 3BA",
            .presetIds = { "female-body" },
            .skinProfileIds = { "female-skin" }
        };
        Require(bcn::SetDistributionRuleSex(rule, false), "rule sex change was not detected");
        Require(!rule.female && rule.bodyFamily.empty() && rule.presetIds.empty() && rule.skinProfileIds.empty(),
            "rule sex change retained hidden selections from the previous sex");
        rule.presetIds = { "male-body" };
        Require(!bcn::SetDistributionRuleSex(rule, false) && rule.presetIds.size() == 1U,
            "unchanged rule sex unnecessarily destroyed compatible selections");
        Require(bcn::NpcDistributionFamily(bcn::FemaleNpcBodyType::cbbe3ba) ==
                bcn::body_family::Bit(bcn::body_family::Family::cbbe) &&
                bcn::NpcDistributionFamily(bcn::FemaleNpcBodyType::bhunpUnp) ==
                bcn::body_family::Bit(bcn::body_family::Family::unp) &&
                bcn::NpcDistributionFamily(bcn::FemaleNpcBodyType::ube) ==
                bcn::body_family::Bit(bcn::body_family::Family::ube) &&
                bcn::NpcDistributionFamily(bcn::MaleNpcBodyType::himbo) ==
                bcn::body_family::Bit(bcn::body_family::Family::himbo) &&
                bcn::NpcDistributionFamily(bcn::MaleNpcBodyType::sam) ==
                bcn::body_family::Bit(bcn::body_family::Family::sam),
            "NPC distribution body-type settings crossed family boundaries");
        Require(!bcn::UsesNpcBodyPreset(bcn::FemaleNpcBodyType::vanilla) &&
                !bcn::UsesNpcBodyPreset(bcn::MaleNpcBodyType::vanilla) &&
                bcn::UsesNpcBodyPreset(bcn::FemaleNpcBodyType::cbbe3ba) &&
                bcn::UsesNpcBodyPreset(bcn::MaleNpcBodyType::himbo),
            "Vanilla NPC body settings did not disable only automatic BodySlide morph distribution");
        const bcn::SettingsData defaultSettings;
        Require(defaultSettings.femaleNpcBodyType == bcn::FemaleNpcBodyType::cbbe3ba &&
                defaultSettings.maleNpcBodyType == bcn::MaleNpcBodyType::himbo,
            "new-install NPC body-type defaults were not CBBE 3BA and HIMBO");
        namespace names = bcn::distribution_names;
        for (const auto& entry : names::kEntries) {
            Require(!names::Localized(entry.key, bcn::UiLanguage::korean).empty() &&
                    !names::Localized(entry.key, bcn::UiLanguage::english).empty() &&
                    !names::Localized(entry.key, bcn::UiLanguage::chineseSimplified).empty(),
                "a distribution-rule name is missing a Korean, English, or Chinese translation");
            Require(names::IsLocalizedValue(entry.key, entry.korean) &&
                    names::IsLocalizedValue(entry.key, entry.english) &&
                    names::IsLocalizedValue(entry.key, entry.chinese),
                "a localized distribution-rule name could not be recognized after loading");
        }
        Require(names::RecognizeKey("default-exclude-elder-female",
                    "Exclude Body Distribution for Elder NPCs (Female)", true) ==
                "default-exclude-elder-female" &&
                names::RecognizeKey("user-rule-1", "새 여성 NPC 규칙", true) ==
                "rule-new-female",
            "legacy sample/generated rule names did not migrate to language-neutral keys");
        std::vector<bcn::DistributionRule> editableSamples{
            { .id = "default-exclude-elder-female", .name = "renamed sample" },
            { .id = "user-rule-1", .name = "user rule" }
        };
        editableSamples.front().scope = bcn::DistributionScope::allNPCs;
        Require(editableSamples.front().name == "renamed sample" &&
                editableSamples.front().scope == bcn::DistributionScope::allNPCs,
            "a built-in sample rule was not editable");
        std::size_t selectedSample{};
        Require(bcn::MoveDistributionRule(editableSamples, selectedSample, 1) &&
                selectedSample == 1U && editableSamples[1].id == "default-exclude-elder-female",
            "a built-in sample rule could not be reordered");
        Require(bcn::EraseDistributionRule(editableSamples, selectedSample) &&
                editableSamples.size() == 1U && selectedSample == 0U,
            "a built-in sample rule could not be deleted");
        editableSamples.push_back({ .id = "user-rule-1", .female = true });
        std::uint32_t nextRuleId{ 1U };
        Require(bcn::GenerateUniqueUserRuleId(editableSamples, nextRuleId) == "user-rule-2" &&
                nextRuleId == 3U,
            "a newly created distribution rule reused an existing ImGui/persistent ID");
        std::vector<bcn::DistributionRule> priorityRules{
            { .id = "female-all", .female = true, .scope = bcn::DistributionScope::allNPCs },
            { .id = "male-all", .female = false, .scope = bcn::DistributionScope::allNPCs },
            { .id = "female-lower", .female = true, .scope = bcn::DistributionScope::npcName }
        };
        Require(bcn::EarlierCatchAllRule(priorityRules, 2U) == 0U &&
                !bcn::EarlierCatchAllRule(priorityRules, 1U),
            "rule-priority diagnostics missed a same-sex catch-all or crossed sex boundaries");
        std::cout << "ActorStateTests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ActorStateTests failed: " << error.what() << '\n';
        return 1;
    }
}
