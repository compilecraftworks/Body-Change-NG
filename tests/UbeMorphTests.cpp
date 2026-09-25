#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/BodyMorphKeys.h"

#include <pugixml.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool pass, const char* message)
    {
        if (!pass) throw std::runtime_error(message);
    }
    bool Near(float a, float b) { return std::abs(a - b) < 0.00001F; }
    struct Scratch
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("BCNG-UbeMorphTests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Scratch() { std::filesystem::create_directory(path); }
        ~Scratch() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    };
    const bcn::BodySlider& Slider(const bcn::BodyPreset& preset, std::string_view name)
    {
        for (const auto& slider : preset.sliders)
            if (bcn::slider_name::Equal{}(slider.name, name)) return slider;
        throw std::runtime_error("authored slider lost (name/space/polarity/case)");
    }
    void Basic(const Scratch& scratch)
    {
        pugi::xml_document xml;
        Check(xml.load_string(R"(<SliderPresets>
<Preset name="Zeroed" set="UBE SE 2.0 Release Preview"><Group name="UBE"/></Preset>
<Preset name="Unknown" set=""/>
<Preset name="Legacy Empty" set="CBBE"/>
<Preset name="UBE Values" set="UBE SE 2.0 Release Preview">
  <SetSlider name="NipplesShowUp" size="small" value="0"/>
  <SetSlider name="nipplesshowup" size="big" value="150"/>
  <SetSlider name="SkinnyMorph" size="small" value="0"/>
  <SetSlider name="SkinnyMorph" size="big" value="100"/>
  <SetSlider name="HipSize p|n" size="small" value="-150"/>
  <SetSlider name="HipSize p|n" size="big" value="250"/>
  <SetSlider name="Breasts" size="small" value="20"/>
</Preset>
<Preset name="Only Small" set="UBE"><SetSlider name="NipplesShowUp" size="small" value="64"/></Preset>
<Preset name="Only Big" set="UBE"><SetSlider name="SkinnyMorph" size="big" value="40"/></Preset>
<Preset name="Nude-Refit" set="UBE"><SetSlider name="NipplesShowUp" size="small" value="-30"/></Preset>
<Preset name="Legacy" set="CBBE"><SetSlider name="NipplesShowUp" size="small" value="64"/></Preset>
</SliderPresets>)"), "fixture XML invalid");
        Check(xml.save_file((scratch.path / "test.xml").c_str()), "cannot write test fixture");
        const auto presets = bcn::PresetCatalog::ScanDirectory(scratch.path);
        const auto find = [&](std::string_view name) -> const bcn::BodyPreset& {
            for (const auto& preset : presets) if (preset.name == name) return preset;
            throw std::runtime_error("valid UBE preset missing from catalog");
        };
        Check(find("Zeroed").UsesBuildDefaults(), "empty UBE Zeroed must be applicable");
        Check(!find("Unknown").UsesBuildDefaults() && !find("Legacy Empty").UsesBuildDefaults(),
            "UBE exception widened to arbitrary empty/unknown presets");
        const auto& values = find("UBE Values");
        Check(values.sliders.size() == 4, "differently cased endpoints became separate morphs");
        Check(Near(Slider(values, "NipplesShowUp").lowWeight, 0) &&
            Near(Slider(values, "NipplesShowUp").highWeight, 1.5F), "UBE XML scale differs from OBody NG");
        Check(Near(Slider(values, "SkinnyMorph").lowWeight, 0) &&
            Near(Slider(values, "SkinnyMorph").highWeight, 1), "UBE retained a build-default subtraction");
        Check(Near(Slider(values, "HipSize p|n").lowWeight, -1.5F) &&
            Near(Slider(values, "HipSize p|n").highWeight, 2.5F), "authored range or n|p polarity altered");
        Check(Near(Slider(values, "Breasts").lowWeight, .2F), "UBE received UNP inversion");
        Check(Near(Slider(find("Only Small"), "NipplesShowUp").lowWeight, .64F) &&
            Near(Slider(find("Only Small"), "NipplesShowUp").highWeight, 0), "omitted high endpoint must be zero");
        Check(Near(Slider(find("Only Big"), "SkinnyMorph").lowWeight, 0) &&
            Near(Slider(find("Only Big"), "SkinnyMorph").highWeight, .4F), "omitted low endpoint became -1");
        Check(Near(Slider(find("Nude-Refit"), "NipplesShowUp").lowWeight, -.3F),
            "additive named refit was normalized as an absolute preset");
        Check(Near(Slider(find("Legacy"), "NipplesShowUp").lowWeight, .64F), "non-UBE behavior changed");

        // Empty preview cancels replaced values through a temporary delta;
        // it never removes a foreign UBE RaceMenu customization key.
        bcn::racemenu::keys::PreviewBase preview(true);
        preview.Visit("HipSize p|n", "BodyChangeNG", 1.2F);
        preview.Visit("HipSize p|n", "OBody", .3F);
        preview.Visit("HipSize p|n", "UBE_RaceMenuMorphs.esp", .4F);
        Check(Near(preview.values["HipSize p|n"], 1.5F), "Zeroed preview swallowed foreign morphs");
        const float zeroedPreviewDelta = -preview.values["HipSize p|n"];
        Check(Near(1.2F + .3F + .4F + zeroedPreviewDelta, .4F), "Zeroed preview failed to cancel old base keys");
        std::filesystem::remove(scratch.path / "test.xml");
    }

    // Independent reference for OBody NG 4.4.3/4.4.4's non-UNP math:
    // PresetManager.cpp SliderSetFromNode + Body.cpp ApplySlider. Exercise the
    // production catalog, not just a second copy of BCNG's removed policy.
    void OBodyParity(const Scratch& scratch)
    {
        std::size_t cases{};
        for (const auto* body : { "CBBE 3BA", "UBE SE 2.0 Release Body", "UBE SE 2.0 - Necoco Body" }) {
            for (const auto* name : { "BreastsBigger", "NipplesShowUp", "SkinnyMorph",
                     "BreastFlat", "Necoco_Boob_Base", "HipSize p|n" }) {
                for (const unsigned endpoints : { 0U, 1U, 2U, 3U }) {
                    for (const float low : { -150.F, 0.F, 64.F, 100.F, 250.F }) {
                        for (const float high : { -80.F, 0.F, 20.F, 100.F, 300.F }) {
                            pugi::xml_document xml;
                            auto node = xml.append_child("SliderPresets").append_child("Preset");
                            node.append_attribute("name") = "Parity";
                            node.append_attribute("set") = body;
                            for (const bool large : { false, true }) {
                                if (!(endpoints & (large ? 2U : 1U))) continue;
                                auto value = node.append_child("SetSlider");
                                value.append_attribute("name") = name;
                                value.append_attribute("size") = large ? "big" : "small";
                                value.append_attribute("value") = large ? high : low;
                            }
                            Check(xml.save_file((scratch.path / "parity.xml").c_str()), "cannot save parity fixture");
                            const auto parsed = bcn::PresetCatalog::ScanDirectory(scratch.path);
                            Check(parsed.size() == 1, "parity preset lost");
                            if (endpoints == 0U) {
                                Check(parsed.front().sliders.empty(), "absent XML sliders were synthesized");
                                ++cases;
                                continue;
                            }
                            const auto& actual = Slider(parsed.front(), name);
                            const float min = (endpoints & 1U) ? low / 100.F : 0.F;
                            const float max = (endpoints & 2U) ? high / 100.F : 0.F;
                            Check(Near(actual.lowWeight, min) && Near(actual.highWeight, max),
                                "OBody endpoint parity failed");
                            for (const float weight : { 0.F, .25F, .5F, .73F, 1.F }) {
                                const float expected = ((max - min) * weight) + min;
                                const float applied = actual.lowWeight + (actual.highWeight - actual.lowWeight) * weight;
                                Check(Near(applied, expected), "OBody weight parity failed");
                                ++cases;
                            }
                        }
                    }
                }
            }
        }
        std::filesystem::remove(scratch.path / "parity.xml");
        std::cout << "CBBE/3BA + UBE OBody NG parity: " << cases << " cases passed\n";
    }

    void InstalledOsp(const Scratch& scratch, const std::filesystem::path& root)
    {
        std::size_t cases{}, entries{};
        // Use the installed body/hand/foot slider names, but do not convert
        // their OSP defaults into runtime morphs (OBody does not do so).
        for (const auto* file : { "UBE SE 2.0 Release Body.osp", "UBE SE 2.0 Release Hands.osp",
                 "UBE SE 2.0 Release Feet.osp", "UBE SE 2.0 Release Preview.osp" }) {
            pugi::xml_document osp;
            Check(osp.load_file((root / file).c_str()), "installed UBE OSP not readable");
            const auto set = osp.child("SliderSetInfo").child("SliderSet");
            Check(set, "missing OSP slider set");
            for (const auto slider : set.children("Slider")) {
                ++entries;
                Check(!slider.attribute("invert").as_bool(), "UBE gained an inverted slider: audit policy before use");
                const auto name = std::string(slider.attribute("name").as_string());
                if (slider.attribute("zap").as_bool() && !slider.attribute("uv").as_bool()) continue;
                for (float target : { -1.5F, -.5F, 0.F, .25F, 1.F, 1.5F, 2.5F }) {
                    pugi::xml_document xml;
                    auto preset = xml.append_child("SliderPresets").append_child("Preset");
                    preset.append_attribute("name") = "Every UBE Slider";
                    preset.append_attribute("set") = "UBE SE 2.0 Release Preview";
                    for (bool large : { false, true }) {
                        auto value = preset.append_child("SetSlider");
                        value.append_attribute("name") = name.c_str();
                        value.append_attribute("size") = large ? "big" : "small";
                        value.append_attribute("value") = (target + (large ? .3F : 0.F)) * 100.F;
                    }
                    Check(xml.save_file((scratch.path / "all.xml").c_str()), "cannot save generated OSP test");
                    const auto parsed = bcn::PresetCatalog::ScanDirectory(scratch.path);
                    Check(parsed.size() == 1, "generated preset did not survive the production catalog");
                    const auto& morph = Slider(parsed.front(), name);
                    for (float weight : { 0.F, .25F, .5F, .73F, 1.F }) {
                        const float applied = morph.lowWeight + (morph.highWeight - morph.lowWeight) * weight;
                        const float expected = target + .3F * weight;
                        Check(Near(applied, expected), "installed UBE slider differs from OBody XML math");
                        ++cases;
                    }
                }
            }
        }
        std::cout << "Installed UBE OSP: " << entries << " slider entries; " << cases << " XML interpolation cases passed\n";
    }
}

int main(int argc, char** argv)
{
    try {
        Scratch scratch;
        Basic(scratch);
        OBodyParity(scratch);
        if (argc == 2) InstalledOsp(scratch, std::filesystem::path(argv[1]));
        std::cout << "UBE XML scale / Zeroed / partial endpoints / key preservation passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
