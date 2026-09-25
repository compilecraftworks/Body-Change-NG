#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/BodyRandomizationPolicy.h"
#include "BodyChangeNG/UbeNippleRandomization.h"
#include "BodyChangeNG/PresetCatalog.h"

#include <pugixml.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>

namespace
{
    using Values = std::map<std::string, float>;
    void Check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
    bool Near(float a, float b) { return std::abs(a - b) < .00001F; }
    struct Draws
    {
        unsigned mask{}, cursor{};
        float fraction{};
        std::vector<int> probabilities;
        std::vector<std::pair<float, float>> ranges;
        bool Chance(int threshold) {
            probabilities.push_back(threshold);
            return (mask & (1U << cursor++)) != 0;
        }
        float Random(float low, float high) {
            Check(low < high, "invalid upstream random range");
            ranges.emplace_back(low, high);
            return low + (high - low) * fraction;
        }
    };
    Draws* activeDraws;
}

// Minimal engine-free bindings for the *unmodified* upstream function bodies.
namespace RE { class Actor { public: Values own; }; }
namespace stl
{
    bool chance(int threshold) { return activeDraws->Chance(threshold); }
    float random(float low, float high) { return activeDraws->Random(low, high); }
    bool cmp(const char* a, const char* b) { return _stricmp(a, b) == 0; }
    template <class Needles>
    bool contains(std::string_view text, const Needles& needles) {
        std::string lower(text);
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return std::ranges::any_of(needles, [&](auto needle) { return lower.contains(needle); });
    }
}
namespace PresetManager
{
    struct Slider
    {
        std::string name;
        float min{}, max{};
        Slider() = default;
        Slider(const char* name, float value) : name(name), min(value), max(value) {}
        Slider(const char* name, float low, float high) : name(name), min(low), max(high) {}
    };
    using SliderSet = std::map<std::string, Slider>;
    void AddSliderToSet(SliderSet&, Slider&&, bool = false);
    enum class BodyType { CBBE, UNP };
    using namespace std::literals;
    constexpr std::array DefaultSliders{"Breasts"sv, "BreastsSmall"sv, "NippleDistance"sv,
        "NippleSize"sv, "ButtCrack"sv, "Butt"sv, "ButtSmall"sv, "Legs"sv, "Arms"sv, "ShoulderWidth"sv};
}
#include "reference/OBodyNG443Parser.inl"
namespace Reference
{
    struct OBody
    {
        bool setNippleSlidersRefitEnabled{};
        static PresetManager::SliderSet GenerateRandomNippleSliders();
        static PresetManager::SliderSet GenerateRandomGenitalSliders();
        PresetManager::SliderSet GenerateClotheSliders(RE::Actor*) const;
        PresetManager::Slider DeriveSlider(RE::Actor*, const char*, float) const;
        float GetMorph(RE::Actor* actor, const char* name) const {
            const auto found = actor->own.find(name);
            return found == actor->own.end() ? 0.F : found->second;
        }
    };
}
#include "reference/OBodyNG443Morphs.inl"

namespace {
    void CheckXml() {
        const auto path = std::filesystem::temp_directory_path() /
            ("BCNG-OBodyParity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(path);
        struct Cleanup { std::filesystem::path path; ~Cleanup() {
            std::error_code ec; std::filesystem::remove_all(path, ec);
        }} cleanup{path};
        pugi::xml_document xml;
        auto root = xml.append_child("SliderPresets");
        unsigned cases{};
        for (const auto* body : {"CBBE 3BA", "BHUNP", "UBE SE 2.0 Release Body", "UBE SE 2.0 - Necoco Body"}) {
            for (const auto* name : {"Breasts", "NippleSize", "Butt", "NipplesShowUp", "SkinnyMorph", "Necoco_Boob_Base"}) {
                for (unsigned endpoints{}; endpoints < 4U; ++endpoints) {
                    for (float low : {-150.F, 0.F, 1.F, 100.F, 250.F}) {
                        for (float high : {-80.F, 0.F, 1.F, 100.F, 300.F}) {
                            auto preset = root.append_child("Preset");
                            preset.append_attribute("name") = ("Case " + std::to_string(cases++)).c_str();
                            preset.append_attribute("set") = body;
                            for (bool big : {false, true}) {
                                if (!(endpoints & (big ? 2U : 1U))) continue;
                                for (float value : {big ? high : low, 0.F, 200.F, -300.F}) {
                                    auto slider = preset.append_child("setslider");
                                    slider.append_attribute("name") = name;
                                    slider.append_attribute("size") = big ? "BIG" : "small";
                                    slider.append_attribute("value") = value;
                                }
                            }
                        }
                    }
                }
            }
        }
        Check(xml.save_file((path / "parity.xml").c_str()), "cannot create XML parity fixture");
        const auto catalog = bcn::PresetCatalog::ScanDirectory(path);
        Check(catalog.size() == cases, "production lost parity presets");
        for (auto node : root.children("Preset")) {
            const auto expected = PresetManager::SliderSetFromNode(node,
                PresetManager::GetBodyType(node.attribute("set").value()));
            const auto actual = std::ranges::find(catalog, node.attribute("name").value(), &bcn::BodyPreset::name);
            Check(actual != catalog.end() && actual->sliders.size() == expected.size(), "XML slider coverage differs");
            for (const auto& slider : actual->sliders) {
                const auto& reference = expected.at(slider.name);
                Check(slider.lowWeight == reference.min && slider.highWeight == reference.max,
                    "zero/negative/over-100/missing/duplicate XML endpoint differs from OBody");
            }
        }
        std::cout << "OBody NG 4.4.3 unchanged-source XML parity: " << cases << " cases passed\n";
    }
}

int main()
{
    try {
        CheckXml();
        using namespace bcn::body_morph_policy;
        unsigned cases{};
        for (unsigned mask{}; mask < 1024U; ++mask) {
            for (float fraction : {0.F, .37F, .999F}) {
                for (bool nipples : {false, true}) {
                    Draws expectedDraws{mask, 0U, fraction};
                    activeDraws = &expectedDraws;
                    const auto expected = nipples ? Reference::OBody::GenerateRandomNippleSliders() :
                        Reference::OBody::GenerateRandomGenitalSliders();
                    Draws actualDraws{mask, 0U, fraction};
                    Values actual;
                    const auto chance = [&](int value) { return actualDraws.Chance(value); };
                    const auto range = [&](float low, float high) { return actualDraws.Random(low, high); };
                    const auto write = [&](const char* name, float value) { actual.insert_or_assign(name, value); };
                    if (nipples) GenerateNippleMorphs(chance, range, write);
                    else GenerateGenitalMorphs(chance, range, write);
                    if (actualDraws.probabilities != expectedDraws.probabilities) {
                        std::cerr << "mask=" << mask << " nipples=" << nipples << " expected:";
                        for (auto p : expectedDraws.probabilities) std::cerr << ' ' << p;
                        std::cerr << " actual:";
                        for (auto p : actualDraws.probabilities) std::cerr << ' ' << p;
                        std::cerr << '\n';
                    }
                    Check(actualDraws.probabilities == expectedDraws.probabilities,
                        "randomization probability/branch order differs from upstream");
                    Check(actualDraws.ranges == expectedDraws.ranges,
                        "randomization ranges/order differ from upstream");
                    if (nipples) {
                        Draws ubeDraws{mask, 0U, fraction};
                        Values ube;
                        bcn::ube_nipple::Generate(
                            [&](int p) { return ubeDraws.Chance(p); },
                            [&](float low, float high) { return ubeDraws.Random(low, high); },
                            [&](const char* name, float value) { ube.emplace(name, value); });
                        Check(ubeDraws.probabilities == expectedDraws.probabilities &&
                            ubeDraws.ranges == expectedDraws.ranges,
                            "UBE draw program differs from unmodified OBody source");
                        Check(ube.contains("Nippleinverted") == expected.contains("NippleInvert_v2") &&
                            ube.contains("AreolaErection") == expected.contains("NipplePuffy_v2") &&
                            ube.contains("NipplesShowUp") == expected.contains("NipBGone"),
                            "UBE independent optional branch differs from upstream");
                    }
                    Check(actual.size() == expected.size(), "optional writes differ from upstream");
                    for (const auto& [name, value] : expected) {
                        Check(actual.contains(name) && Near(actual.at(name), value.min) && Near(value.min, value.max),
                            "random morph name/value differs from upstream");
                    }
                    // A missing optional write retains the preset value, not an
                    // invented zero. The apply path overlays only returned names.
                    Values xml{{"Labiapuffyness", 1.75F}, {"NippleInvert_v2", -.5F}};
                    for (const auto& [name, value] : actual) xml.insert_or_assign(name, value);
                    for (const auto& [name, value] : Values{{"Labiapuffyness", 1.75F}, {"NippleInvert_v2", -.5F}})
                        if (!expected.contains(name)) Check(xml.at(name) == value, "absent optional write cleared XML");
                    ++cases;
                }
            }
        }
        for (const auto family : {FemaleFamily::cbbe3ba, FemaleFamily::bhunpUnp}) {
            for (bool nipples : {false, true}) {
                for (float own : {-1.5F, 0.F, .7F, 2.5F}) {
                    Reference::OBody original{nipples};
                    RE::Actor actor;
                    const auto targets = original.GenerateClotheSliders(&actor);
                    for (const auto& [name, value] : targets) actor.own[name] = own;
                    const auto expected = original.GenerateClotheSliders(&actor);
                    for (float weight : {0.F, .25F, .5F, .73F, 1.F}) {
                        Values actual;
                        GenerateOutfitMorphs(family, weight, nipples,
                            [&](const char* name) { return original.GetMorph(&actor, name); },
                            [&](const char* name, float value) { actual[name] = value; });
                        Check(actual.size() == expected.size(), "refit coverage differs from upstream");
                        for (const auto& [name, value] : expected) {
                            const float target = ((value.max - value.min) * weight) + value.min;
                            Check(actual.contains(name) && Near(actual.at(name), target),
                                "refit value differs from upstream");
                        }
                        ++cases;
                    }
                }
            }
        }
        for (auto family : {FemaleFamily::none}) {
            Check(!SupportsOutfitCorrection(family) && !UsesConventionalAnatomyRecipe(family),
                "unsupported actor became eligible");
        }
        Check(UsesConventionalAnatomyRecipe(FemaleFamily::cbbe3ba), "conventional anatomy disabled");
        Check(SupportsOutfitCorrection(FemaleFamily::ube) &&
            !UsesConventionalAnatomyRecipe(FemaleFamily::ube), "UBE entered conventional anatomy recipe");
        for (unsigned i{}; i < 1000U; ++i) {
            const float value = OBodyRandom(-1.5F, 2.5F);
            Check(value >= -1.5F && value < 2.5F, "random draw escaped upstream half-open bounds");
        }
        std::cout << "OBody NG 4.4.3 unchanged-source numeric parity: " << cases << " cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
