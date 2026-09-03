#include "BodyChangeNG/PresetCatalog.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main(const int argc, char** argv)
{
    if (argc == 2) {
        const auto presets = bcn::PresetCatalog::ScanDirectory(std::filesystem::path{ argv[1] });
        if (!Require(!presets.empty(), "real BodySlide preset folder produced no presets")) return 1;
        std::cout << "real presets=" << presets.size() << '\n';
        for (const auto& preset : presets) {
            std::cout << preset.name << " | set=" << preset.bodySet << " | family=" << preset.family
                      << " | sex=" << (preset.male ? "male" : "female") << '\n';
        }
        const auto dongTan = std::ranges::find(presets, std::string{ "DongTan Style" }, &bcn::BodyPreset::name);
        if (dongTan != presets.end()) {
            if (!Require(dongTan->family == "CBBE 3BA" && !dongTan->male,
                    "DongTan Style was classified into the wrong body family or sex")) return 1;
            const auto slider = [&](const std::string_view name) {
                return std::ranges::find(dongTan->sliders, name, &bcn::BodySlider::name);
            };
            const auto arms = slider("Arms");
            if (!Require(arms != dongTan->sliders.end() && arms->lowWeight == 1.0F && arms->highWeight == .41F,
                    "DongTan Arms low/high values do not match OBody NG's /100 parser")) return 1;
            const auto breastWidth = slider("BreastWidth");
            if (!Require(breastWidth != dongTan->sliders.end() && breastWidth->lowWeight == 0.0F &&
                    breastWidth->highWeight == -1.5F,
                    "DongTan negative BodySlide values were clamped or scaled incorrectly")) return 1;
            const auto nippleSize = slider("NippleSize");
            if (!Require(nippleSize != dongTan->sliders.end() && nippleSize->lowWeight == .3F &&
                    nippleSize->highWeight == -1.3F,
                    "DongTan NippleSize low/high values do not match OBody NG")) return 1;
        }
        return 0;
    }
    const auto root = std::filesystem::temp_directory_path() / "BodyChangeNGPresetCatalogTests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Nested");
    {
        std::ofstream preset(root / "Nested" / "example.xml");
        preset << "<SliderPresets>"
               << "<Preset name=\"3BA &amp; Natural\" set=\"CBBE 3BA\">"
               << "<SetSlider name=\"Breasts\" value=\"25\" size=\"small\"/>"
               << "<SetSlider name=\"Breasts\" value=\"75\" size=\"big\"/>"
               << "</Preset>"
               << "<Preset name='CBBE Classic' set='CBBE'>"
               << "<SetSlider name='Waist' value='20' size='small'/>"
               << "</Preset>"
               << "<Preset name='HIMBO Strong' set='HIMBO'/>"
               << "<Preset name='HIMBO Daddy (Clothes)' set='HIMBO'/>"
               << "<Preset name='Reusable Outfit Body' set='CBBE Bikini Armor Cuirass Dress Panty Overalls NeverNude Feet Hands Push Cleavage'/>"
               << "</SliderPresets>";
    }
    {
        std::ofstream fallback(root / "single.xml");
        fallback << "<NotAPreset/>";
    }

    {
        std::ofstream preset(root / "Nested" / "classification.xml");
        preset << "<SliderPresets>"
               << "<Preset name='Common Only' set=''><SetSlider name='Breasts'/><SetSlider name='Waist'/><SetSlider name='Butt'/></Preset>"
               << "<Preset name='3BBB Only' set='3BBB'><SetSlider name='3BBB'/></Preset>"
               << "<Preset name='UBE Anus 3BA' set='3BBB Body Amazing UBE Anus'/>>"
               << "<Preset name='Dual Female' set='CBBE BHUNP'/></SliderPresets>";
    }

    {
        std::ofstream preset(root / "Nested" / "ube-group.xml");
        preset << "<SliderPresets><Preset name='Generic UBE Preset' set=''>"
               << "<Group name='UBE 2.0'/><SetSlider name='ClaviclesAngle' value='25' size='small'/>"
               << "</Preset></SliderPresets>";
    }

    {
        std::ofstream preset(root / "Nested" / "Clothes Outfit Bikini Armor Cuirass Dress Panty Overalls.xml");
        preset << "<SliderPresets><Preset name='Reusable Preset From Outfit Mod' set='CBBE'/></SliderPresets>";
    }

    const auto presets = bcn::PresetCatalog::ScanDirectory(root);
    if (!Require(presets.size() == 11U, "preset scanner accepted an invalid XML or lost a valid preset")) return 1;
    const auto find = [&](const std::string_view name) {
        return std::ranges::find(presets, name, &bcn::BodyPreset::name);
    };
    const auto threeBa = find("3BA & Natural");
    if (!Require(threeBa != presets.end() && threeBa->family == "CBBE 3BA" && !threeBa->male,
            "3BA preset classification failed")) return 1;
    if (!Require(threeBa->sliders.size() == 1U && threeBa->sliders[0].name == "Breasts" &&
            threeBa->sliders[0].lowWeight == 0.25F && threeBa->sliders[0].highWeight == 0.75F,
            "BodySlide low/high slider parsing failed")) return 1;
    const auto cbbe = find("CBBE Classic");
    if (!Require(cbbe != presets.end() && cbbe->family == "CBBE 3BA" && !cbbe->male,
            "CBBE was not merged into the displayed CBBE 3BA family")) return 1;
    const auto himbo = find("HIMBO Strong");
    if (!Require(himbo != presets.end() && himbo->family == "HIMBO" && himbo->male, "HIMBO preset classification failed")) return 1;
    const auto himboClothes = find("HIMBO Daddy (Clothes)");
    if (!Require(himboClothes != presets.end() && himboClothes->family == "HIMBO" && himboClothes->male &&
            !himboClothes->isRefit, "HIMBO (Clothes) preset was hidden or moved out of the main catalog")) return 1;
    if (!Require(find("Reusable Outfit Body") != presets.end(),
            "clothing words in a BodySlide set hid a reusable preset")) return 1;
    if (!Require(find("Reusable Preset From Outfit Mod") != presets.end(),
            "clothing words in an XML source path hid a reusable preset")) return 1;
    if (!Require(find("Common Only")->family == "Unclassified", "common sliders incorrectly classified a body family")) return 1;
    if (!Require(find("3BBB Only")->family == "Unclassified", "3BBB-only preset incorrectly classified a body family")) return 1;
    if (!Require(find("UBE Anus 3BA")->family == "CBBE 3BA", "UBE Anus incorrectly replaced the 3BA family")) return 1;
    if (!Require(find("Dual Female")->family == "CBBE 3BA / BHUNP / UNP", "combined female set lost either family")) return 1;
    if (!Require(find("Generic UBE Preset")->family == "UBE", "UBE Group metadata was not used")) return 1;
    if (!Require(!threeBa->PersistentId().empty(), "persistent preset id is empty")) return 1;

    std::filesystem::remove_all(root);
    return 0;
}
