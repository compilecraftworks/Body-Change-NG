// Execute the production UI projection and distribution-pool function.
// Only actor/provider boundaries are fakes; XML parsing and family filtering
// use the real PresetCatalog and BodyFamilyRules implementations.
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/Settings.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace RE {
enum class SEX { kMale, kFemale };
struct NPC { SEX sex{ SEX::kFemale }; SEX GetSex() const { return sex; } };
class Actor {
public:
    NPC base;
    bcn::body_family::Mask family{};
    bool hasBase{ true };
    const NPC* GetActorBase() const { return hasBase ? &base : nullptr; }
};
}
RE::Actor* selectedActor{};
unsigned familyReads{};
namespace bcn::body_family {
Mask ResolveActor(RE::Actor* actor) { ++familyReads; return actor ? actor->family : 0U; }
}
namespace bcn {
Settings& Settings::Get() { static Settings value; return value; }
SettingsData Settings::Snapshot() const { return data_; }
void Settings::Update(const SettingsData& data) { data_ = data; }
struct ActorCatalog {
    static ActorCatalog& Get() { static ActorCatalog value; return value; }
    RE::Actor* Resolve(unsigned) const { return selectedActor; }
};
}
namespace bcn::racemenu {
std::optional<std::string> CurrentPresetId(RE::Actor*) { return {}; }
}
enum class DistributionPool { body, skin, futanari, overlay };
bool g_distributionFemale{ true }, g_distributionSelectionMode{};
unsigned g_selectedActorFormID{ 1U };
bool IsDistributionSelectionFor(DistributionPool pool)
{ return g_distributionSelectionMode && pool == DistributionPool::body; }
struct CatalogItem {
    std::string id, name, family, source;
    bool favorite{}, current{}, compatible{ true }, body{};
};
#include "distribution_body_items.inc"
#include "distribution_preset_pool.inc"

void Check(bool condition, const char* message)
{ if (!condition) throw std::runtime_error(message); }

struct Fixture {
    std::filesystem::path previous{ std::filesystem::current_path() };
    std::filesystem::path root{ std::filesystem::temp_directory_path() /
        ("BCNG-DistributionPresetTests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count())) };
    Fixture()
    {
        Check(std::filesystem::create_directory(root), "fixture must be a newly created directory");
        std::filesystem::current_path(root);
    }
    ~Fixture()
    {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        if (root.parent_path() == std::filesystem::temp_directory_path() &&
            root.filename().string().starts_with("BCNG-DistributionPresetTests-"))
            std::filesystem::remove_all(root, error);
    }
};

int main()
{
    try {
        Fixture fixture;
        const auto directory = bcn::PresetCatalog::BodySlidePresetDirectory();
        std::filesystem::create_directories(directory);
        {
            std::ofstream xml(directory / "families.xml");
            xml << "<SliderPresets>";
            for (const auto& [name, body] : std::vector<std::pair<std::string, std::string>>{
                     {"Preset3BA", "CBBE 3BA"}, {"PresetUBE", "UBE 2.0"},
                     {"PresetUNP", "BHUNP"}, {"PresetHIMBO", "HIMBO"},
                     {"PresetSAM", "SAM"}, {"Generic", ""} })
                xml << "<Preset name='" << name << "' set='" << body <<
                    "'><SetSlider name='TestOnlySlider' size='small' value='25'/></Preset>";
            xml << "<Preset name='Zeroed UBE' set='UBE 2.0'/></SliderPresets>";
        }
        auto& catalog = bcn::PresetCatalog::Get();
        catalog.Refresh();
        const auto originalList = catalog.ListSnapshot();
        Check(originalList->size() == 7U, "fixture catalog classification failed");
        const auto id = [&](const char* name) {
            const auto found = std::ranges::find(*originalList, name, &bcn::PresetListEntry::name);
            Check(found != originalList->end(), "missing fixture preset");
            return found->id;
        };
        const auto cbbeId = id("Preset3BA"), ubeId = id("PresetUBE"), zeroId = id("Zeroed UBE");
        std::vector<std::string> pool;
        for (const auto& entry : *originalList) pool.push_back(entry.id);
        pool.push_back("missing-preset");
        using namespace bcn::body_family;
        RE::Actor npc{ .family = Bit(Family::cbbe) };
        selectedActor = &npc;
        auto settings = bcn::Settings::Get().Snapshot();
        settings.femaleNpcBodyType = bcn::FemaleNpcBodyType::ube;
        settings.favoriteBodyPresets = { ubeId };
        bcn::Settings::Get().Update(settings);
        const auto contains = [](const auto& values, const auto& value) {
            return std::ranges::find(values, value) != values.end();
        };
        const auto row = [](const auto& items, const auto& value) {
            return std::ranges::find(items, value, &CatalogItem::id);
        };
        g_distributionSelectionMode = false;
        auto items = BodyItems();
        Check(row(items, cbbeId) != items.end() && row(items, ubeId) == items.end(),
            "normal CBBE actor list incorrectly follows UBE distribution settings");
        g_distributionSelectionMode = true;
        items = BodyItems();
        Check(row(items, cbbeId) == items.end() && row(items, ubeId) != items.end() &&
                row(items, zeroId) != items.end() && !row(items, ubeId)->compatible &&
                row(items, ubeId)->favorite,
            "UBE distribution candidates must show independently of the CBBE preview actor");
        auto result = CompatiblePresetPoolIds(pool, &npc, Bit(Family::ube));
        Check(!contains(result, ubeId) && !contains(result, cbbeId),
            "UBE setting overrode the actual CBBE actor at automatic selection");
        npc.family = Bit(Family::ube);
        result = CompatiblePresetPoolIds(pool, &npc, Bit(Family::ube));
        Check(contains(result, ubeId) && contains(result, zeroId) && !contains(result, cbbeId),
            "UBE NPC did not receive compatible UBE/Zeroed candidates");
        items = BodyItems();
        Check(row(items, ubeId) != items.end() && row(items, ubeId)->compatible,
            "UBE preview actor lost compatible candidate");

        // Both sides of every family/sex boundary, repeated list mode switches
        // and a fresh immutable snapshot after each iteration (no state growth).
        for (unsigned iteration{}; iteration < 1000U; ++iteration) {
            for (const bool male : { false, true }) {
                npc.base.sex = male ? RE::SEX::kMale : RE::SEX::kFemale;
                g_distributionFemale = !male;
                for (const auto actorFamily : { 0U, Bit(Family::cbbe), Bit(Family::unp),
                         Bit(Family::ube), Bit(Family::himbo), Bit(Family::sam) }) {
                    npc.family = actorFamily;
                    for (const auto configured : { Bit(Family::cbbe), Bit(Family::unp),
                             Bit(Family::ube), Bit(Family::himbo), Bit(Family::sam) }) {
                        familyReads = 0;
                        result = CompatiblePresetPoolIds(pool, &npc, configured);
                        Check(familyReads == 1U, "family detection repeated for each candidate");
                        for (const auto& entry : *originalList) {
                            const auto mask = PresetMask(entry.family, entry.male);
                            const bool expected = entry.male == male &&
                                Matches(mask, configured) && Matches(mask, actorFamily);
                            Check(contains(result, entry.id) == expected,
                                "automatic family/sex/unknown-family compatibility mismatch");
                        }
                        Check(!contains(result, std::string("missing-preset")), "missing asset accepted");
                    }
                    g_distributionSelectionMode = iteration % 2U != 0U;
                    items = BodyItems();
                    const auto expectedFamily = g_distributionSelectionMode ?
                        DistributionCatalogFamily(settings) : actorFamily;
                    for (const auto& entry : *originalList) {
                        const bool expected = entry.male == male &&
                            Matches(PresetMask(entry.family, entry.male), expectedFamily);
                        Check((row(items, entry.id) != items.end()) == expected,
                            "switching normal/distribution mode changed the wrong candidate list");
                    }
                }
            }
            Check(catalog.ListSnapshot() == originalList, "mode switches mutated/rebuilt the preset catalog");
        }
        Check(CompatiblePresetPoolIds(pool, nullptr, Bit(Family::ube)).empty(), "null actor accepted");
        npc.hasBase = false;
        Check(CompatiblePresetPoolIds(pool, &npc, Bit(Family::ube)).empty(), "missing actor base accepted");
        std::cout << "Distribution presets: real UI/pool functions, UBE/3BA/UNP/HIMBO/SAM, Zeroed, "
            "preview compatibility, unknown families and 1000 mode-switch cycles PASS\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
