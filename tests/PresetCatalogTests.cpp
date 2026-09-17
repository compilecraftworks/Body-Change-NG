#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SliderName.h"

#include <cmath>
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
    bcn::BodyPreset contentA{.name = "same", .source = "same.xml", .family = "CBBE 3BA",
        .sliders = {{"Breasts", 0.2F, 0.7F}}};
    auto contentB = contentA;
    contentB.sliders.front().highWeight = 0.8F;
    if (!Require(contentA.PersistentId() == contentB.PersistentId() &&
            contentA.ContentHash() != contentB.ContentHash(), "same-ID XML edit was not detected")) return 1;
    contentB = contentA;
    contentB.sliders.clear();
    if (!Require(contentA.ContentHash() != contentB.ContentHash(), "removed slider was not detected")) return 1;
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
               << "<SetSlider name='CBBEOnlyTestSlider' value='10' size='small'/>"
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
               << "<Preset name='Unknown Extension' set=''><SetSlider name='ForeignUnknownSlider'/></Preset>"
               << "<Preset name='3BBB Only' set='3BBB'><SetSlider name='3BBB'/></Preset>"
               << "<Preset name='UBE Anus 3BA' set='3BBB Body Amazing UBE Anus'/>>"
               << "<Preset name='Dual Female' set='CBBE BHUNP'/>"
               << "<Preset name='Shared-Refit' set='CBBE 3BA'><SetSlider name='Breasts'/></Preset>"
               << "</SliderPresets>";
    }

    {
        std::ofstream preset(root / "Nested" / "ube-refit.xml");
        preset << "<SliderPresets><Preset name='Shared-Refit' set='UBE 2.0'>"
               << "<SetSlider name='ClaviclesAngle'/></Preset></SliderPresets>";
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
    const auto displayEntries = bcn::BuildPresetList(presets);
    if (!Require(displayEntries.size() == presets.size(), "metadata list lost a preset")) return 1;
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const auto& p = presets[i]; const auto& entry = displayEntries[i];
        if (!Require(entry.id == p.PersistentId() && entry.name == p.name &&
                entry.family == p.family && entry.source == p.source && entry.male == p.male,
                "metadata projection changed ID, label, sex, family, or ordering")) return 1;
    }
    {
        const auto originalDirectory = std::filesystem::current_path();
        const auto catalogDirectory = root / "Data/CalienteTools/BodySlide/SliderPresets";
        std::filesystem::create_directories(catalogDirectory);
        std::filesystem::copy_file(root / "Nested/example.xml", catalogDirectory / "example.xml");
        std::filesystem::current_path(root);
        auto& catalog = bcn::PresetCatalog::Get();
        catalog.Refresh();
        const auto first = catalog.ListSnapshot();
        const auto again = catalog.ListSnapshot();
        if (!Require(first == again && !first->empty(), "UI read rebuilt or copied the metadata snapshot")) return 1;
        const auto full = catalog.Find(first->front().id);
        if (!Require(full && full->name == first->front().name, "metadata ID no longer resolves a complete preset")) return 1;
        const auto oldSize = first->size();
        std::filesystem::remove(catalogDirectory / "example.xml");
        catalog.Refresh();
        if (!Require(catalog.ListSnapshot()->empty() && first->size() == oldSize,
                "refresh mutated an in-flight UI snapshot or retained removed entries")) return 1;
        std::filesystem::current_path(originalDirectory);
    }
    if (!Require(presets.size() == 14U, "preset scanner accepted an invalid XML or lost a valid preset")) return 1;
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

    const std::vector refitNames{ std::string{ "Shared-Refit" } };
    const auto cbbeRefit = bcn::PresetCatalog::SelectRefit(presets, refitNames, false,
        bcn::body_family::Bit(bcn::body_family::Family::cbbe));
    const auto ubeRefit = bcn::PresetCatalog::SelectRefit(presets, refitNames, false,
        bcn::body_family::Bit(bcn::body_family::Family::ube));
    if (!Require(cbbeRefit && cbbeRefit->family == "CBBE 3BA",
            "named refit selected the wrong family for a CBBE/3BA actor")) return 1;
    if (!Require(ubeRefit && ubeRefit->family == "UBE",
            "named refit selected the wrong family for a UBE actor")) return 1;

    // Distinct presets must never supply one another's zero values. Within
    // one preset, differently cased low/high entries are one RaceMenu morph.
    const auto caseRoot = root / "CaseNames";
    std::filesystem::create_directories(caseRoot);
    {
        std::ofstream preset(caseRoot / "case.xml");
        preset << R"(<SliderPresets>
<Preset name="Target" set="CBBE 3BBB Body Amazing">
  <SetSlider name="Breasts" size="small" value="120"/>
  <SetSlider name="Breasts" size="big" value="120"/>
</Preset>
<Preset name="Other" set="CBBE 3BBB Body Amazing">
  <SetSlider name="breasts" size="small" value="0"/>
  <SetSlider name="breasts" size="big" value="0"/>
</Preset>
<Preset name="Mixed" set="CBBE 3BA">
  <SetSlider name="Breasts" size="small" value="120"/>
  <SetSlider name="breasts" size="big" value="-150"/>
  <SetSlider name="Waist" size="small" value="-150"/>
  <SetSlider name="waist" size="big" value="120"/>
  <SetSlider name="HipBone" size="small" value="20"/>
  <SetSlider name="Hipbone" size="big" value="80"/>
</Preset>
<Preset name="Mixed Reversed" set="CBBE 3BA">
  <SetSlider name="Hipbone" size="big" value="80"/>
  <SetSlider name="HipBone" size="small" value="20"/>
  <SetSlider name="waist" size="big" value="120"/>
  <SetSlider name="Waist" size="small" value="-150"/>
  <SetSlider name="breasts" size="big" value="-150"/>
  <SetSlider name="Breasts" size="small" value="120"/>
</Preset>
<Preset name="UNP Mixed" set="BHUNP">
  <SetSlider name="breasts" size="small" value="120"/>
  <SetSlider name="BREASTS" size="big" value="-150"/>
  <SetSlider name="nippleSIZE" size="small" value="25"/>
  <SetSlider name="NippleSize" size="big" value="75"/>
  <SetSlider name="Hipbone" size="small" value="-150"/>
  <SetSlider name="HipBone" size="big" value="120"/>
</Preset>
<Preset name="Duplicates-Refit" set="CBBE 3BA">
  <SetSlider name="Breasts" size="small" value="10"/>
  <SetSlider name="breasts" size="small" value="120"/>
  <SetSlider name="BREASTS" size="big" value="-150"/>
</Preset>
</SliderPresets>)";
    }
    const auto casePresets = bcn::PresetCatalog::ScanDirectory(caseRoot);
    if (!Require(casePresets.size() == 6U, "case fixture presets were lost")) return 1;
    const auto caseFind = [&](const std::string_view name) -> const bcn::BodyPreset& {
        return *std::ranges::find(casePresets, name, &bcn::BodyPreset::name);
    };
    const auto& target = caseFind("Target");
    const auto& other = caseFind("Other");
    if (!Require(target.sliders.size() == 1U && target.sliders[0].lowWeight == 1.2F &&
            target.sliders[0].highWeight == 1.2F && other.sliders.size() == 1U &&
            other.sliders[0].lowWeight == 0.0F && other.sliders[0].highWeight == 0.0F,
            "another preset's zero slider changed the target's +120 value")) return 1;
    const auto& mixed = caseFind("Mixed");
    const auto& reversed = caseFind("Mixed Reversed");
    if (!Require(mixed.sliders.size() == 3U && reversed.sliders.size() == 3U &&
            mixed.sliders[0].name == "Breasts" && reversed.sliders.back().name == "breasts",
            "case variants were not merged or first authored spelling was lost")) return 1;
    const auto valuesAt = [](const bcn::BodyPreset& preset, const float weight) {
        bcn::slider_name::Map<float> result;
        for (const auto& slider : preset.sliders) result.insert_or_assign(slider.name,
            slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight);
        return result;
    };
    for (const auto weight : {0.0F, .25F, .5F, 1.0F}) {
        const auto forward = valuesAt(mixed, weight);
        const auto backward = valuesAt(reversed, weight);
        if (!Require(forward.size() == backward.size(), "case merging changed with XML order")) return 1;
        for (const auto& [name, value] : forward) {
            if (!Require(std::abs(value - backward.at(name)) < .00001F,
                    "interpolated morph changed when XML endpoint order was reversed")) return 1;
        }
        if (!Require(std::abs(forward.at("BREASTS") - (1.2F - 2.7F * weight)) < .00001F &&
                std::abs(forward.at("WAIST") - (-1.5F + 2.7F * weight)) < .00001F,
                "negative or over-100 morph value was clamped")) return 1;
    }
    const auto& unp = caseFind("UNP Mixed");
    const auto unpLow = valuesAt(unp, 0.0F);
    const auto unpHigh = valuesAt(unp, 1.0F);
    if (!Require(unp.sliders.size() == 3U && std::abs(unpLow.at("Breasts") + .2F) < .00001F &&
            unpHigh.at("breasts") == 2.5F && unpLow.at("NIPPLEsize") == .75F &&
            unpHigh.at("NippleSize") == .25F && unpLow.at("HipBone") == -1.5F &&
            unpHigh.at("HipBone") == 1.2F, "UNP inversion was casing-dependent or inverted a non-default slider")) return 1;
    const auto& duplicate = caseFind("Duplicates-Refit");
    if (!Require(duplicate.isRefit && duplicate.sliders.size() == 1U &&
            duplicate.sliders[0].name == "Breasts" && duplicate.sliders[0].lowWeight == 1.2F &&
            duplicate.sliders[0].highWeight == -1.5F,
            "named refit duplicate did not retain last value per endpoint")) return 1;
    std::filesystem::remove_all(root);
    std::cout << "PresetCatalogTests passed: catalog, case variants, endpoint order, range and UNP inversion\n";
    return 0;
}
