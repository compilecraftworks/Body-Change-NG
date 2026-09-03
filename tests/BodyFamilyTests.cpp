#include "BodyChangeNG/BodyFamily.h"

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
    using namespace bcn::body_family;
    try {
        Require(DetectText("CBBE 3BA V2", Sex::female) == Bit(Family::cbbe), "CBBE/3BA");
        Require(DetectText("3BBB", Sex::female) == 0U, "3BBB must be ambiguous");
        Require(DetectText("BHUNP 3BBB", Sex::female) == Bit(Family::unp), "BHUNP");
        Require(DetectText("cube map", Sex::female) == 0U, "cube false positive");
        Require(DetectText("HIMBO V5", Sex::male) == Bit(Family::himbo), "HIMBO");
        Require(DetectText("SAM Light", Sex::male) == Bit(Family::sam), "SAM");
        Require(DetectText("samurai", Sex::male) == 0U, "samurai false positive");
        const auto cbbe = ClassifyPreset("3BBB Body Amazing", "irrelevant", "preset.xml");
        Require(cbbe.families == Bit(Family::cbbe) && !cbbe.male && !cbbe.conflict, "CBBE set classification");
        const auto unp = ClassifyPreset("BHUNP 3BBB", "irrelevant", "preset.xml");
        Require(unp.families == Bit(Family::unp) && !unp.conflict, "BHUNP set classification");
        const auto ube = ClassifyPreset("3BBB Body Amazing UBE Anus", "irrelevant", "preset.xml");
        Require(ube.families == Bit(Family::cbbe) && !ube.conflict, "UBE Anus must remain CBBE/3BA");
        const auto combined = ClassifyPreset("CBBE 3BA UBE Body", "irrelevant", "preset.xml");
        Require(combined.families == (Bit(Family::cbbe) | Bit(Family::ube)) && !combined.conflict,
            "real CBBE/UBE combined set must be visible in both families");
        Require(ClassifyPreset("HIMBO", "irrelevant", "preset.xml").families == Bit(Family::himbo), "HIMBO set");
        Require(ClassifyPreset("SAM Light", "irrelevant", "preset.xml").families == Bit(Family::sam), "SAM set");
        Require(ClassifyPreset("3BBB", "irrelevant", "preset.xml").families == 0U, "3BBB-only set");
        Require(ClassifyPreset("", "generic", "preset.xml", "UBE 2.0").families == Bit(Family::ube),
            "UBE BodySlide group classification");
        Require(ClassifyPreset("CBBE BHUNP", "irrelevant", "preset.xml").families ==
                (Bit(Family::cbbe) | Bit(Family::unp)), "combined female set must retain both families");
        const auto triple = ClassifyPreset("CBBE BHUNP UBE", "irrelevant", "preset.xml");
        Require(triple.families == (Bit(Family::cbbe) | Bit(Family::unp) | Bit(Family::ube)) &&
                PresetFamilyLabel(triple) == "CBBE 3BA / BHUNP / UNP / UBE",
            "combined female set must retain every family label");
        Require(ClassifyPreset("CBBE HIMBO", "irrelevant", "preset.xml").conflict, "cross-sex conflicting set");
        const auto fallback = ClassifyPreset("", "HIMBO Athletic", "preset.xml");
        Require(fallback.families == Bit(Family::himbo) && fallback.usedMetadataFallback, "metadata fallback");
        Require(ClassifyPreset("", "Samutchi Body", "preset.xml").families == 0U, "Samutchi false SAM match");
        Require(Matches(Bit(Family::femaleVanilla), Bit(Family::cbbe)), "unclassified female preset remains visible");
        Require(!Matches(Bit(Family::unp), Bit(Family::cbbe)), "UNP must not match CBBE");
        Require(Matches(Bit(Family::unp), 0U), "unknown actor must not filter");
        Require(DetectSkinTextureLayout("Textures\\!UBE\\Head\\femalehead_d.dds", Sex::female) ==
                SkinTextureLayout::ube, "UBE texture namespace");
        Require(DetectSkinTextureLayout("textures\\actors\\character\\female\\femalehead.dds", Sex::female) ==
                SkinTextureLayout::standard, "standard female texture namespace");
        Require(ResolveLoadedSkinTextureLayout(true, true, true, false) == SkinTextureLayout::standard,
            "standard live head must beat UBE outfit body geometry");
        Require(ResolveLoadedSkinTextureLayout(true, true, false, true) == SkinTextureLayout::ube,
            "UBE live head must beat standard outfit body geometry");
        Require(ResolveLoadedSkinTextureLayout(true, true, false, false) == SkinTextureLayout::unknown,
            "mixed outfit-only evidence must not classify the actor");
        const auto installedMixed = Bit(Family::cbbe) | Bit(Family::ube);
        Require(ResolveSkinTextureFamily(0U, installedMixed, SkinTextureLayout::ube, Sex::female) ==
                Bit(Family::ube), "mixed install UBE actor");
        Require(ResolveSkinTextureFamily(0U, installedMixed, SkinTextureLayout::standard, Sex::female) ==
                Bit(Family::cbbe), "mixed install standard CBBE actor");
        Require(ResolveSkinTextureFamily(0U, installedMixed | Bit(Family::unp),
                SkinTextureLayout::standard, Sex::female) == 0U,
            "ambiguous standard female frameworks must preserve fallback");
        std::cout << "BodyFamilyTests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BodyFamilyTests failed: " << error.what() << '\n';
        return 1;
    }
}
