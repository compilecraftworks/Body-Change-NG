#include "BodyChangeNG/DistributionAuthoringRuntime.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/PathText.h"
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESClass.h>
#include <RE/T/TESCombatStyle.h>
#include <RE/B/BGSKeyword.h>
#include <RE/RTTI.h>
#include "BodyChangeNG/DistributionTargetRead.h"

namespace bcn::distribution_authoring
{
    namespace
    {
        using Target = TargetEntry;
        std::mutex targetLock;
        std::shared_ptr<const std::vector<Target>> targets = std::make_shared<const std::vector<Target>>();
        [[nodiscard]] bool Equal(std::string_view left, std::string_view right) {
            if (left.size() != right.size()) return false;
            for (std::size_t i{}; i < left.size(); ++i) {
                const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
                if (lower(left[i]) != lower(right[i])) return false;
            }
            return true;
        }
        struct Builtin { DistributionScope scope; std::uint32_t local; std::string_view editor; };
        constexpr Builtin builtins[]{
            {DistributionScope::raceEditorID,79680,"ArgonianRace"}, {DistributionScope::raceEditorID,79681,"BretonRace"},
            {DistributionScope::raceEditorID,79682,"DarkElfRace"}, {DistributionScope::raceEditorID,79683,"HighElfRace"},
            {DistributionScope::raceEditorID,79684,"ImperialRace"}, {DistributionScope::raceEditorID,79685,"KhajiitRace"},
            {DistributionScope::raceEditorID,79686,"NordRace"}, {DistributionScope::raceEditorID,79687,"OrcRace"},
            {DistributionScope::raceEditorID,79688,"RedguardRace"}, {DistributionScope::raceEditorID,79689,"WoodElfRace"},
            {DistributionScope::raceEditorID,559162,"ArgonianRaceVampire"}, {DistributionScope::raceEditorID,559164,"BretonRaceVampire"},
            {DistributionScope::raceEditorID,559165,"DarkElfRaceVampire"}, {DistributionScope::raceEditorID,559168,"HighElfRaceVampire"},
            {DistributionScope::raceEditorID,559172,"ImperialRaceVampire"}, {DistributionScope::raceEditorID,559173,"KhajiitRaceVampire"},
            {DistributionScope::raceEditorID,558996,"NordRaceVampire"}, {DistributionScope::raceEditorID,688825,"OrcRaceVampire"},
            {DistributionScope::raceEditorID,559174,"RedguardRaceVampire"}, {DistributionScope::raceEditorID,559236,"WoodElfRaceVampire"},
            {DistributionScope::factionEditorID,113856,"BanditFaction"}, {DistributionScope::factionEditorID,378957,"PotentialFollowerFaction"},
            {DistributionScope::factionEditorID,378958,"CurrentFollowerFaction"},
            {DistributionScope::keyword,79764,"ActorTypeNPC"}, {DistributionScope::keyword,79766,"ActorTypeUndead"}, {DistributionScope::keyword,688827,"Vampire"},
            {DistributionScope::npcClass,78198,"CombatWarrior1H"}, {DistributionScope::npcClass,118293,"CombatWarrior2H"},
            {DistributionScope::npcClass,124880,"CombatMageDestruction"}, {DistributionScope::npcClass,118295,"EncClassBanditMelee"},
            {DistributionScope::npcClass,89063,"EncClassBanditMissile"}, {DistributionScope::npcClass,236848,"EncClassBanditWizard"}, {DistributionScope::npcClass,78443,"Citizen"},
            {DistributionScope::combatStyle,245275,"csHumanMeleeLvl1"}, {DistributionScope::combatStyle,428104,"csHumanMeleeLvl2"},
            {DistributionScope::combatStyle,245276,"csHumanMagic"}, {DistributionScope::combatStyle,245277,"csHumanMissile"},
            {DistributionScope::combatStyle,249690,"csHumanTankLvl1"}, {DistributionScope::combatStyle,250568,"csHumanBerserkerLvl1"}
        };
        template<class T>
        void Collect(std::vector<Target>& result, RE::TESDataHandler* data, DistributionScope scope) {
            for (auto* candidate : data->GetFormArray<T>()) {
                distribution_target::Read<T>(candidate, [&](T* form, const char* name, const char* editor) {
                    const auto* file = form->GetFile(0);
                    if (!file || file->GetFilename().empty()) return;
                    Target value{scope, form->GetFormID(), form->GetFormID() & (file->IsLight() ? 0xFFFU : 0xFFFFFFU),
                        std::string(file->GetFilename()), name ? name : "", editor ? editor : ""};
                    if (!value.local) return;
                    // These verified names remain usable even when Skyrim does not retain an EditorID.
                    if (Equal(value.plugin, "Skyrim.esm")) for (const auto& builtin : builtins) {
                        if (builtin.scope == scope && builtin.local == value.local) { value.editor = builtin.editor; break; }
                    }
                    result.push_back(std::move(value));
                });
            }
        }
        [[nodiscard]] std::shared_ptr<const std::vector<Target>> TargetSnapshot() {
            std::scoped_lock lock(targetLock); return targets;
        }
    }

    std::shared_ptr<const AssetIndex> Catalog(const Kind kind)
    {
        // Store lightweight names/IDs only. Rebuild on catalog publication, not per NPC or frame.
        static std::mutex lock;
        static std::shared_ptr<const PresetList> previousPresets;
        static std::shared_ptr<const std::vector<SkinProfile>> previousSkins;
        static std::shared_ptr<const std::vector<FutanariSkinProfile>> previousFuta;
        static std::array<std::uint64_t, 4> overlayRevisions{};
        static std::array<std::shared_ptr<const AssetIndex>, 7> indexes;
        const auto index = static_cast<std::size_t>(kind);
        std::scoped_lock guard(lock);
        std::vector<AssetEntry> entries;
        if (kind == Kind::preset) {
            const auto snapshot = PresetCatalog::Get().ListSnapshot();
            if (indexes[index] && previousPresets == snapshot) return indexes[index];
            for (const auto& p : *snapshot) entries.push_back({p.id, p.name, p.source, {}, {}});
            previousPresets = snapshot;
        } else if (kind == Kind::skin) {
            const auto snapshot = SkinProfiles::Get().SharedSnapshot();
            if (indexes[index] && previousSkins == snapshot) return indexes[index];
            for (const auto& p : *snapshot) entries.push_back({p.id, p.name,
                path_text::GenericUtf8(p.source.lexically_relative(SkinProfiles::RootPath())),
                std::string(p.sex == SkinSex::female ? "female:" : "male:") + SkinFamilyLabel(p.layout, p.sex) + ":" + SkinRaceLabel(p.race), {}});
            previousSkins = snapshot;
        } else if (kind == Kind::futa) {
            const auto snapshot = FutanariSkinProfiles::Get().SharedSnapshot();
            if (indexes[index] && previousFuta == snapshot) return indexes[index];
            for (const auto& p : *snapshot) entries.push_back({p.id, p.name,
                path_text::GenericUtf8(p.source.lexically_relative(FutanariSkinProfiles::RootPath())),
                p.type == FutanariSkinType::erf ? "ERF" : p.type == FutanariSkinType::cbbeTrx ? "TRX" : "UBE-TRX", {}});
            previousFuta = snapshot;
        } else {
            const auto area = index - static_cast<std::size_t>(Kind::face);
            const auto revision = overlay::CatalogRevision();
            if (indexes[index] && overlayRevisions[area] == revision) return indexes[index];
            for (const auto& p : overlay::Snapshot(static_cast<overlay::Area>(area))) entries.push_back({p.id, p.name, {}, {}, p.texturePath});
            overlayRevisions[area] = revision;
        }
        auto replacement = std::make_shared<AssetIndex>();
        replacement->Assign(std::move(entries));
        indexes[index] = replacement;
        return replacement;
    }

    void RefreshTargets(const bool includeNPCs)
    {
        auto replacement = std::make_shared<std::vector<Target>>();
        if (auto* data = RE::TESDataHandler::GetSingleton()) {
            Collect<RE::TESRace>(*replacement, data, DistributionScope::raceEditorID);
            Collect<RE::TESFaction>(*replacement, data, DistributionScope::factionEditorID);
            Collect<RE::BGSKeyword>(*replacement, data, DistributionScope::keyword);
            Collect<RE::TESClass>(*replacement, data, DistributionScope::npcClass);
            Collect<RE::TESCombatStyle>(*replacement, data, DistributionScope::combatStyle);
            if (includeNPCs) Collect<RE::TESNPC>(*replacement, data, DistributionScope::npcBaseForm);
        }
        std::scoped_lock lock(targetLock); targets = std::move(replacement);
    }

    bool ResolveNamedTarget(DistributionRule& rule)
    {
        const auto snapshot = TargetSnapshot();
        const auto* found = FindTarget(*snapshot, rule.scope, TargetValue(rule.target));
        if (rule.scope == DistributionScope::npcBaseForm) {
            rule.npcBaseFormID = found ? found->id : 0U;
            rule.npcPlugin = found ? found->plugin : "";
            rule.npcLocalFormID = found ? found->local : 0U;
        } else {
            rule.targetFormID = found ? found->id : 0U;
            rule.targetPlugin = found ? found->plugin : "";
            rule.targetLocalFormID = found ? found->local : 0U;
        }
        return found != nullptr;
    }

    std::vector<DistributionRule> ReadableRules(std::vector<DistributionRule> rules)
    {
        const auto targetSnapshot = TargetSnapshot();
        for (auto& rule : rules) {
            const auto describePool = [](auto& pool, Kind kind) {
                if (pool.empty()) return;
                const auto index = Catalog(kind);
                for (auto& id : pool) id = index->Describe(id);
            };
            describePool(rule.presetIds, Kind::preset);
            describePool(rule.skinProfileIds, Kind::skin);
            describePool(rule.futanariSkinIds, Kind::futa);
            for (const auto area : overlay::kAreas) {
                const auto i = overlay::Index(area);
                if (rule.overlayIds[i].empty()) continue;
                const auto index = Catalog(static_cast<Kind>(static_cast<unsigned>(Kind::face) + i));
                auto colors = decltype(DistributionRule::overlayColors)::value_type{};
                for (auto& id : rule.overlayIds[i]) {
                    const auto color = DistributionOverlayColor(rule, area, id);
                    id = index->Describe(id); colors[id] = color;
                }
                rule.overlayColors[i] = std::move(colors);
            }
            if (NamedTarget(rule.target)) continue;
            const auto npc = rule.scope == DistributionScope::npcBaseForm;
            const auto& plugin = npc ? rule.npcPlugin : rule.targetPlugin;
            const auto local = npc ? rule.npcLocalFormID : rule.targetLocalFormID;
            for (const auto& option : *targetSnapshot) {
                if (option.scope != rule.scope || option.local != local || !Equal(option.plugin, plugin)) continue;
                const auto name = !option.editor.empty() ? option.editor : option.name;
                if (name.empty()) break;
                const Json reference{{"name", name}, {"plugin", option.plugin}};
                if (const auto* found = FindTarget(*targetSnapshot, rule.scope, reference); found && found->id == option.id) {
                    try {
                        (void)Text(reference.at("name"));
                        (void)Text(reference.at("plugin"));
                        const auto encoded = std::string(kTargetPrefix) + reference.dump();
                        if (encoded.size() <= 4096U) rule.target = encoded;
                    } catch (const std::exception&) {
                        // Keep stable plugin/local identity if an engine label is
                        // not representable; conversion must not lose the rule.
                    }
                }
                break;
            }
        }
        return rules;
    }
}
