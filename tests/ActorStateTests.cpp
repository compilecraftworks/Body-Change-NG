#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/Distribution.h"
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
            .selectedBodyId = "preset-a",
            .selectedSkinId = "skin-a",
            .manualBody = true,
            .manualSkin = true,
            .bodySignature = body,
            .skinSignature = StableStateSignature("skin", "skin-a", false)
        };
        Require(state.manualBody && state.manualSkin && state.selectedBodyId != state.selectedSkinId,
            "body and skin channels did not remain independent");
        state.bodyApplied = state.skinApplied = true;
        state.bodyVerifiedThisSession = state.skinVerifiedThisSession = true;
        bcn::PrepareRestoredState(state);
        Require(state.bodyApplied && state.skinApplied && !state.bodyVerifiedThisSession &&
                !state.skinVerifiedThisSession,
            "cosave restore did not separate persisted completion from live-session proof");
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
        std::cout << "ActorStateTests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ActorStateTests failed: " << error.what() << '\n';
        return 1;
    }
}
