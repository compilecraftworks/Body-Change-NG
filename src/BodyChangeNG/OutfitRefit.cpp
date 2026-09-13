#include "BodyChangeNG/OutfitRefit.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/OutfitRefitEvaluation.h"
#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/OutfitRefitRules.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/RenderedOutfit.h"

#include <SKSE/Logger.h>
#include <RE/P/ProcessLists.h>

#include <charconv>
#include <fstream>
#include <unordered_set>

namespace
{
    [[nodiscard]] bool ParseHex(std::string_view text, std::uint32_t& value)
    {
        if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    }

    [[nodiscard]] bool ParseLocalFormID(std::string_view text, const bool lightPlugin, std::uint32_t& value)
    {
        if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
        const auto digits = lightPlugin ? 3U : 6U;
        if (text.size() > digits) text.remove_prefix(text.size() - digits);
        return !text.empty() && ParseHex(text, value);
    }

    [[nodiscard]] bool IsBlacklisted(const RE::TESObjectARMO* armor, const bcn::OutfitRefit::Rules& rules)
    {
        if (!armor) return true;
        const auto* file = armor->GetFile(0);
        return bcn::outfit_refit_evaluation::IsBlacklisted({
            armor->GetName(), file ? file->GetFilename() : "", armor->GetFormID()
        }, rules);
    }

    [[nodiscard]] bool IsForced(const RE::TESObjectARMO* armor, const bcn::OutfitRefit::Rules& rules)
    {
        if (!armor) return false;
        const auto* file = armor->GetFile(0);
        return bcn::outfit_refit_evaluation::IsForced({
            armor->GetName(), file ? file->GetFilename() : "", armor->GetFormID()
        }, rules);
    }

    // OBody NG deliberately treats a force-refit item in any worn slot as an
    // override for the usual body/chest clothing test.  Looking only at the
    // three nakedness slots would miss rings, accessories, or modded slots
    // explicitly listed in outfitsForceRefit.
    [[nodiscard]] bool HasAnyForcedWornArmor(RE::Actor* actor, const bcn::OutfitRefit::Rules& rules)
    {
        if (!actor) return false;
        // This guard skips ONLY the extra all-slot force scan. Ordinary
        // chest eligibility and per-outfit mappings below are unaffected.
        if (rules.forcedOutfitNames.empty() && rules.forcedFormIDs.empty()) return false;
        for (const auto& [boundObject, inventoryData] : actor->GetInventory()) {
            const auto& entry = inventoryData.second;
            if (!boundObject || !entry || !entry->IsWorn()) continue;
            if (IsForced(boundObject->As<RE::TESObjectARMO>(), rules)) return true;
        }
        return false;
    }
}

namespace bcn
{
    OutfitRefit& OutfitRefit::Get()
    {
        static OutfitRefit refit;
        return refit;
    }

    std::shared_ptr<const OutfitRefit::Rules> OutfitRefit::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        if (!evaluationRules_) evaluationRules_ = std::make_shared<const Rules>(rules_);
        return evaluationRules_;
    }

    OBodyOutfitImportReport OutfitRefit::LoadOBodyRules()
    {
        const auto path = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "OBody_presetDistributionConfig.json";
        Rules loaded;
        try {
            std::ifstream stream(path);
            if (!stream) return {};
            const auto root = nlohmann::json::parse(stream);
            outfit_refit_rules::ImportedRules imported;
            if (!outfit_refit_rules::ParseOBodyRules(root, imported)) return {};

            auto* dataHandler = const_cast<RE::TESDataHandler*>(RE::TESDataHandler::GetSingleton());
            const auto resolveFormIDs = [&](const auto& references, auto& destination) {
                if (!dataHandler) return;
                for (const auto& reference : references) {
                    const auto* file = dataHandler->LookupModByName(reference.plugin);
                    if (!file) continue;
                    std::uint32_t local{};
                    if (!ParseLocalFormID(reference.formID, file->IsLight(), local)) continue;
                    if (const auto* armor = dataHandler->LookupForm<RE::TESObjectARMO>(local, reference.plugin)) {
                        destination.insert(armor->GetFormID());
                    }
                }
            };

            resolveFormIDs(imported.blacklistedFormIDs, loaded.blacklistedFormIDs);
            resolveFormIDs(imported.forcedFormIDs, loaded.forcedFormIDs);
            loaded.blacklistedOutfitNames = std::move(imported.blacklistedOutfitNames);
            loaded.blacklistedPlugins = std::move(imported.blacklistedPlugins);
            loaded.forcedOutfitNames = std::move(imported.forcedOutfitNames);
            loaded.femalePresetByOutfit = std::move(imported.femalePresetByOutfit);
            loaded.malePresetByOutfit = std::move(imported.malePresetByOutfit);
        } catch (const std::exception& exception) {
            SKSE::log::error("Body Change NG could not register OBody outfit-correction rules: {}", exception.what());
            return {};
        }
        const OBodyOutfitImportReport report{
            .loaded = true,
            .excludedNames = loaded.blacklistedOutfitNames.size(),
            .excludedPlugins = loaded.blacklistedPlugins.size(),
            .excludedFormIDs = loaded.blacklistedFormIDs.size(),
            .forcedNames = loaded.forcedOutfitNames.size(),
            .forcedFormIDs = loaded.forcedFormIDs.size(),
            .femaleMappings = loaded.femalePresetByOutfit.size(),
            .maleMappings = loaded.malePresetByOutfit.size()
        };
        std::scoped_lock lock(lock_);
        rules_ = std::move(loaded);
        evaluationRules_.reset();
        SKSE::log::info(
            "Body Change NG registered OBody outfit-correction rules from {} "
            "(excluded-names={}, excluded-plugins={}, excluded-forms={}, forced-names={}, forced-forms={}, "
            "female-mappings={}, male-mappings={})",
            bcn::path_text::Utf8(path), rules_.blacklistedOutfitNames.size(), rules_.blacklistedPlugins.size(),
            rules_.blacklistedFormIDs.size(), rules_.forcedOutfitNames.size(), rules_.forcedFormIDs.size(),
            rules_.femalePresetByOutfit.size(), rules_.malePresetByOutfit.size());
        return report;
    }

    void OutfitRefit::ClearLegacyRules()
    {
        std::scoped_lock lock(lock_);
        rules_ = {};
        evaluationRules_.reset();
    }

    void OutfitRefit::ProcessActor(RE::Actor* actor) const
    {
        if (!actor || !actor->Is3DLoaded()) return;
        const auto useSFS = rendered_outfit::Available();
        if (useSFS && !frame_tasks::InGameTask()) {
            rendered_outfit::Request(actor);
            return;
        }
        // A new decision can equal the already-applied signature while an
        // older, different decision is still queued (A -> B -> A). Cancel
        // that pending generation even when no replacement morph is needed.
        if (useSFS) racemenu::CancelPendingOutfit(actor);
        if (racemenu::HasActivePreview(actor)) return;
        // Keep the disabled/no-owned-layer fast path: do not create registry
        // entries for every untouched NPC just to record an empty correction.
        if (!Settings::Get().OutfitCorrectionEnabled() && !racemenu::HasOutfitCorrection(actor)) return;
        const auto plan = Evaluate(actor);
        if (plan.action == Action::defer || !ActorRegistry::Get().NeedsOutfitApply(actor, plan.signature)) return;
        if (plan.action == Action::clear) {
            if (racemenu::HasOutfitCorrection(actor)) racemenu::QueueClearOutfit(actor, plan.signature);
            else ActorRegistry::Get().MarkOutfitApplied(actor, plan.signature);
        } else if (plan.action == Action::procedural) {
            racemenu::QueueApplyProceduralOutfit(actor, plan.signature);
        } else if (plan.preset) {
            [[maybe_unused]] const auto result = racemenu::QueueApplyOutfit(
                actor, plan.preset->PersistentId(), plan.signature);
        }
    }

    OutfitRefit::Plan OutfitRefit::Evaluate(RE::Actor* actor, const BodyPreset* previewBody) const
    {
        if (!actor || !actor->Is3DLoaded()) return {};
        if (rendered_outfit::Available() && !frame_tasks::InGameTask()) return {};
        const auto settings = Settings::Get().MorphOptions();
        if (!settings.outfitCorrection) {
            return { Action::clear, StableStateSignature("outfit", "disabled", true) };
        }

        const auto snapshot = Snapshot();
        const auto& rules = *snapshot;
        const auto base = actor->GetActorBase();
        if (!base) return {};
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto& mapping = female ? rules.femalePresetByOutfit : rules.malePresetByOutfit;
        std::string presetName;
        const auto rendered = rendered_outfit::Read(actor);
        if (rendered.route == rendered_outfit::Route::defer ||
            rendered.route == rendered_outfit::Route::invalidActor) return {};
        constexpr std::array slots{
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary
        };
        std::array<RE::TESObjectARMO*, slots.size()> worn{};
        bool hasEligibleOutfit{};
        bool forceRefit{};
        if (rendered.route == rendered_outfit::Route::rendered) {
            const auto decision = rendered_outfit::EvaluateVisible(rendered.items, rules, mapping,
                [](const auto& item) -> std::optional<outfit_refit_evaluation::ArmorIdentity> {
                    auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(rendered_outfit::RuleForm(item));
                    if (!armor) armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(item.formID);
                    if (!armor) return {};
                    // Unknown dynamic provenance is not an invented source
                    // plugin. Names can still match; known originals retain
                    // all original name/FormID/plugin rules and mappings.
                    const auto* file = (armor->GetFormID() >> 24U) == 0xFFU ? nullptr : armor->GetFile(0);
                    return outfit_refit_evaluation::ArmorIdentity{
                        armor->GetName(), file ? file->GetFilename() : "", armor->GetFormID()};
                });
            hasEligibleOutfit = decision.eligible;
            forceRefit = decision.forced;
            presetName = decision.preset;
        } else {
            // API absent/unsupported or explicit NotManaged: preserve the
            // original inventory-based path, including any-slot force rules.
            for (std::size_t index{}; index < slots.size(); ++index) {
                worn[index] = actor->GetWornArmor(slots[index]);
                hasEligibleOutfit = hasEligibleOutfit || (worn[index] && !IsBlacklisted(worn[index], rules));
                forceRefit = forceRefit || IsForced(worn[index], rules);
            }
            forceRefit = forceRefit || HasAnyForcedWornArmor(actor, rules);
            for (const auto* armor : worn) {
                if (!armor) continue;
                if (const auto found = mapping.find(armor->GetName()); found != mapping.end()) {
                    presetName = found->second;
                    break;
                }
            }
        }
        if (!outfit_refit_evaluation::ShouldApply(hasEligibleOutfit, forceRefit)) {
            const auto signature = StableStateSignature("outfit", "clear", true,
                settings.outfitNippleCorrection ? 1U : 0U);
            return { Action::clear, signature };
        }

        std::vector<std::string> candidates;
        std::string currentBodyId;
        body_family::Mask currentBodyFamily{};
        if (!presetName.empty()) candidates.push_back(std::move(presetName));
        if (previewBody) {
            currentBodyId = previewBody->PersistentId();
            candidates.push_back(previewBody->name + "-Refit");
            currentBodyFamily = body_family::PresetMask(previewBody->family, previewBody->male);
        } else if (const auto currentID = racemenu::CurrentPresetId(actor)) {
            currentBodyId = *currentID;
            if (const auto current = PresetCatalog::Get().Find(*currentID)) {
                candidates.push_back(current->name + "-Refit");
                currentBodyFamily = body_family::PresetMask(current->family, current->male);
            }
        }
        candidates.push_back(female ? "Female-Refit" : "Male-Refit");

        auto actorFamily = body_family::ResolveActor(actor);
        if (actorFamily == 0U) actorFamily = currentBodyFamily;
        if (female && !body_morph_policy::SupportsOutfitCorrection(
                body_morph_policy::ResolveFemaleFamily(actorFamily, currentBodyFamily))) {
            // CBBE/3BA and BHUNP/UNP each use an explicitly supported slider
            // dialect. UBE (and an ambiguous family) must not receive either
            // a guessed procedural layer or an imported/named refit layer.
            // Clear a layer written by an earlier BCNG build once, then cache
            // the no-op signature so ordinary equip events stay inexpensive.
            const auto signature = StableStateSignature("outfit", "unsupported-female-family", true,
                static_cast<std::uint32_t>(actorFamily));
            return { Action::clear, signature };
        }
        auto found = PresetCatalog::Get().FindRefit(candidates, !female, actorFamily);
        if (!found) {
            const auto signature = StableStateSignature("outfit", "procedural|" + currentBodyId, false,
                settings.outfitNippleCorrection ? 1U : 0U);
            return { Action::procedural, signature };
        }
        const auto signature = StableStateSignature("outfit", found->PersistentId() + "|" + currentBodyId, false,
            settings.outfitNippleCorrection ? 1U : 0U, found->cachedContentHash);
        return { Action::named, signature, std::move(found) };
    }

    std::size_t OutfitRefit::ProcessLoadedActors() const
    {
        std::unordered_set<RE::FormID> seen;
        std::size_t processed{};
        const auto process = [&](RE::Actor* actor) {
            if (!actor || !actor->Is3DLoaded() || !seen.insert(actor->GetFormID()).second) return;
            ProcessActor(actor);
            ++processed;
        };

        process(RE::PlayerCharacter::GetSingleton());
        if (auto* processes = RE::ProcessLists::GetSingleton()) {
            processes->ForAllActors([&](RE::Actor* actor) {
                process(actor);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        return processed;
    }
}
