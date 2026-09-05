#include "BodyChangeNG/OutfitRefit.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorState.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/OutfitRefitRules.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/Settings.h"

#include <SKSE/Logger.h>

#include <charconv>
#include <fstream>

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
        if (!armor || rules.blacklistedOutfitNames.contains(armor->GetName()) || rules.blacklistedFormIDs.contains(armor->GetFormID())) {
            return true;
        }
        const auto* file = armor->GetFile(0);
        return file && rules.blacklistedPlugins.contains(std::string(file->GetFilename()));
    }

    [[nodiscard]] bool IsForced(const RE::TESObjectARMO* armor, const bcn::OutfitRefit::Rules& rules)
    {
        return armor && (rules.forcedOutfitNames.contains(armor->GetName()) || rules.forcedFormIDs.contains(armor->GetFormID()));
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

    bool OutfitRefit::LoadOBodyRules()
    {
        const auto path = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "OBody_presetDistributionConfig.json";
        Rules loaded;
        try {
            std::ifstream stream(path);
            if (!stream) return false;
            const auto root = nlohmann::json::parse(stream);
            outfit_refit_rules::ImportedRules imported;
            if (!outfit_refit_rules::ParseOBodyRules(root, imported)) return false;

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
            return false;
        }
        std::scoped_lock lock(lock_);
        rules_ = std::move(loaded);
        evaluationRules_.reset();
        SKSE::log::info(
            "Body Change NG registered OBody outfit-correction rules from {} "
            "(excluded-names={}, excluded-plugins={}, excluded-forms={}, forced-names={}, forced-forms={}, "
            "female-mappings={}, male-mappings={})",
            path.string(), rules_.blacklistedOutfitNames.size(), rules_.blacklistedPlugins.size(),
            rules_.blacklistedFormIDs.size(), rules_.forcedOutfitNames.size(), rules_.forcedFormIDs.size(),
            rules_.femalePresetByOutfit.size(), rules_.malePresetByOutfit.size());
        return true;
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
        if (racemenu::HasActivePreview(actor)) return;
        const auto settings = Settings::Get().MorphOptions();
        if (!settings.outfitCorrection) {
            // A globally disabled feature has nothing to evaluate unless an
            // old BCNG/OBody outfit layer actually needs to be removed.
            if (!racemenu::HasOutfitCorrection(actor)) return;
            const auto signature = StableStateSignature("outfit", "disabled", true);
            if (ActorRegistry::Get().NeedsOutfitApply(actor, signature)) {
                racemenu::QueueClearOutfit(actor, signature);
            }
            return;
        }

        const auto snapshot = Snapshot();
        const auto& rules = *snapshot;
        constexpr std::array slots{
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary
        };
        std::array<RE::TESObjectARMO*, slots.size()> worn{};
        bool hasEligibleOutfit{};
        bool forceRefit{};
        for (std::size_t index{}; index < slots.size(); ++index) {
            worn[index] = actor->GetWornArmor(slots[index]);
            hasEligibleOutfit = hasEligibleOutfit || (worn[index] && !IsBlacklisted(worn[index], rules));
            forceRefit = forceRefit || IsForced(worn[index], rules);
        }
        forceRefit = forceRefit || HasAnyForcedWornArmor(actor, rules);
        if (!hasEligibleOutfit && !forceRefit) {
            const auto signature = StableStateSignature("outfit", "clear", true,
                settings.outfitNippleCorrection ? 1U : 0U);
            if (ActorRegistry::Get().NeedsOutfitApply(actor, signature)) {
                racemenu::QueueClearOutfit(actor, signature);
            }
            return;
        }

        const auto base = actor->GetActorBase();
        if (!base) return;
        const auto female = base->GetSex() == RE::SEX::kFemale;
        const auto& mapping = female ? rules.femalePresetByOutfit : rules.malePresetByOutfit;
        std::string presetName;
        for (const auto* armor : worn) {
            if (!armor) continue;
            if (const auto found = mapping.find(armor->GetName()); found != mapping.end()) {
                presetName = found->second;
                break;
            }
        }
        std::vector<std::string> candidates;
        std::string currentBodyId;
        body_family::Mask currentBodyFamily{};
        if (!presetName.empty()) candidates.push_back(std::move(presetName));
        if (const auto currentID = racemenu::CurrentPresetId(actor)) {
            currentBodyId = *currentID;
            if (const auto current = PresetCatalog::Get().Find(*currentID)) {
                candidates.push_back(current->name + "-Refit");
                currentBodyFamily = body_family::PresetMask(current->family, current->male);
            }
        }
        candidates.push_back(female ? "Female-Refit" : "Male-Refit");

        auto actorFamily = body_family::ResolveActor(actor);
        if (actorFamily == 0U) actorFamily = currentBodyFamily;
        const auto found = PresetCatalog::Get().FindRefit(candidates, !female, actorFamily);
        if (!found) {
            const auto signature = StableStateSignature("outfit", "procedural|" + currentBodyId, false,
                settings.outfitNippleCorrection ? 1U : 0U);
            if (ActorRegistry::Get().NeedsOutfitApply(actor, signature)) {
                racemenu::QueueApplyProceduralOutfit(actor, signature);
            }
            return;
        }
        const auto signature = StableStateSignature("outfit", found->PersistentId() + "|" + currentBodyId, false,
            settings.outfitNippleCorrection ? 1U : 0U, found->cachedContentHash);
        if (!ActorRegistry::Get().NeedsOutfitApply(actor, signature)) return;
        const auto result = racemenu::QueueApplyOutfit(actor, found->PersistentId(), signature);
        if (result != racemenu::ApplyResult::queued) {
            SKSE::log::debug("Body Change NG could not queue outfit correction '{}' for {:08X}", found->name, actor->GetFormID());
        }
    }
}
