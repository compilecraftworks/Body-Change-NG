// OBody NG 4.4.3, GPL-3.0, Aietos / Sairion350 and contributors.
// Unmodified function bodies retained only as an independent test oracle.
// https://github.com/Aietos/OBody-NG/tree/4.4.3
// Production adapts engine bindings/eligibility, not these numeric recipes.
namespace Reference {
using namespace PresetManager;
PresetManager::SliderSet OBody::GenerateRandomNippleSliders() {
        PresetManager::SliderSet set;

        if (stl::chance(15))
            AddSliderToSet(set, Slider{"AreolaSize", stl::random(-1.0f, 0.0f)});
        else
            AddSliderToSet(set, Slider{"AreolaSize", stl::random(0.0f, 1.0f)});

        if (stl::chance(75)) AddSliderToSet(set, Slider{"AreolaPull_v2", stl::random(-0.25f, 1.0f)});

        if (stl::chance(15))
            AddSliderToSet(set, Slider{"NippleLength", stl::random(0.2f, 0.3f)});
        else
            AddSliderToSet(set, Slider{"NippleLength", stl::random(0.0f, 0.1f)});

        AddSliderToSet(set, Slider{"NippleManga", stl::random(-0.3f, 0.8f)});

        if (stl::chance(25)) AddSliderToSet(set, Slider{"NipplePerkManga", stl::random(-0.3f, 1.2f)});

        if (stl::chance(15)) AddSliderToSet(set, Slider{"NipBGone", stl::random(0.6f, 1.0f)});

        AddSliderToSet(set, Slider{"NippleSize", stl::random(-0.5f, 0.3f)});
        AddSliderToSet(set, Slider{"NippleDip", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"NippleCrease_v2", stl::random(-0.4f, 1.0f)});

        if (stl::chance(6)) AddSliderToSet(set, Slider{"NipplePuffy_v2", stl::random(0.4f, 0.7f)});

        if (stl::chance(35)) AddSliderToSet(set, Slider{"NippleThicc_v2", stl::random(0.0f, 0.9f)});

        if (stl::chance(2)) {
            if (stl::chance(50))
                AddSliderToSet(set, Slider{"NippleInvert_v2", 1.0f});
            else
                AddSliderToSet(set, Slider{"NippleInvert_v2", stl::random(0.65f, 0.8f)});
        }

        return set;
    }

PresetManager::SliderSet OBody::GenerateRandomGenitalSliders() {
        PresetManager::SliderSet set;

        if (stl::chance(20)) {
            // innie
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(0.95f, 1.1f)});

            if (stl::chance(50)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.75f, 1.25f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 1.0f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 0.5f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 0.1f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 0.1f)});
            AddSliderToSet(set, Slider{"Labiaspread", 0.0F});
            AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.3f)});
            AddSliderToSet(set, Slider{"LabiaBulgogi_v2", 0.0F});
            AddSliderToSet(set, Slider{"LabiaNeat_v2", 0.0F});
            AddSliderToSet(set, Slider{"VaginaHole", stl::random(-0.2f, 0.05f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.4f, 0.25f)});
        } else if (stl::chance(75)) {
            // average
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(0.4f, 0.75f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.5f, 1.0f)});

            if (stl::chance(30)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 0.75f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 0.5f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 0.75f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 1.0f)});

            if (stl::chance(50)) {
                AddSliderToSet(set, Slider{"Labiaspread", stl::random(0.0f, 1.0f)});
                AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.7f)});

                if (stl::chance(60)) AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 0.1f)});
            } else {
                AddSliderToSet(set, Slider{"Labiaspread", 0.0F});
                AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.2f)});

                if (stl::chance(45)) AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 0.3f)});
            }

            AddSliderToSet(set, Slider{"LabiaNeat_v2", 0.0F});
            AddSliderToSet(set, Slider{"VaginaHole", stl::random(-0.2f, 0.40f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.2f, 0.25f)});
        } else {
            // outie
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(-0.25f, 0.30f)});

            if (stl::chance(30)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.20f, 0.50f)});

            if (stl::chance(10)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 0.35f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaspread", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 1.0f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"LabiaNeat_v2", stl::random(0.0f, 0.25f)});

            AddSliderToSet(set, Slider{"VaginaHole", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.4f, 0.25f)});
        }

        AddSliderToSet(set, Slider{"Vaginasize", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"ClitSwell_v2", stl::random(-0.3f, 1.1f)});
        AddSliderToSet(set, Slider{"Cutepuffyness", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"LabiaTightUp", stl::random(0.0f, 1.0f)});

        if (stl::chance(60))
            AddSliderToSet(set, Slider{"CBPC", stl::random(-0.25f, 0.25f)});
        else
            AddSliderToSet(set, Slider{"CBPC", stl::random(0.6f, 1.0f)});

        AddSliderToSet(set, Slider{"AnalPosition_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalTexPos_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalTexPosRe_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalLoose_v2", -0.1F});

        return set;
    }

PresetManager::SliderSet OBody::GenerateClotheSliders(RE::Actor* a_actor) const {
        PresetManager::SliderSet set;
        // breasts
        // make area on sides behind breasts not sink in
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastSideShape", 0.0F));
        // make area under breasts not sink in
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastUnderDepth", 0.0F));
        // push breasts together
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastCleavage", 1.0F));
        // push up smaller breasts more
        AddSliderToSet(set, Slider{"BreastGravity2", -0.1F, -0.05F});
        // Make top of breast rise higher
        AddSliderToSet(set, Slider{"BreastTopSlope", -0.2F, -0.35F});
        // push breasts together
        AddSliderToSet(set, Slider{"BreastsTogether", 0.3F, 0.35F});
        // push breasts up
        // AddSliderToSet(set, Slider{ "PushUp", 0.6f, 0.4f });
        // Shrink breasts slightly
        AddSliderToSet(set, Slider{"Breasts", -0.05F});
        // Move breasts up on body slightly
        AddSliderToSet(set, Slider{"BreastHeight", 0.15F});

        // butt
        // remove butt impressions
        AddSliderToSet(set, DeriveSlider(a_actor, "ButtDimples", 0.0F));
        AddSliderToSet(set, DeriveSlider(a_actor, "ButtUnderFold", 0.0F));
        // shrink ass slightly
        AddSliderToSet(set, Slider{"AppleCheeks", -0.05F});
        AddSliderToSet(set, Slider{"Butt", -0.05F});

        // Torso
        // remove definition on clavical bone
        AddSliderToSet(set, DeriveSlider(a_actor, "Clavicle_v2", 0.0F));
        // Push out navel
        AddSliderToSet(set, DeriveSlider(a_actor, "NavelEven", 1.0F));

        // hip
        // remove defintion on hip bone
        AddSliderToSet(set, DeriveSlider(a_actor, "HipCarved", 0.0F));

        if (setNippleSlidersRefitEnabled) {
            // nipple
            // sublte change to tip shape
            AddSliderToSet(set, DeriveSlider(a_actor, "NippleDip", 0.0F));
            AddSliderToSet(set, DeriveSlider(a_actor, "NippleTip", 0.0F));
            // flatten areola
            AddSliderToSet(set, DeriveSlider(a_actor, "NipplePuffy_v2", 0.0F));
            // shrink areola
            AddSliderToSet(set, DeriveSlider(a_actor, "AreolaSize", -0.3F));
            // flatten nipple
            AddSliderToSet(set, DeriveSlider(a_actor, "NipBGone", 1.0F));
            // AddSliderToSet(set, DeriveSlider(a_actor, "NippleManga", -0.75f));
            //  push nipples together
            AddSliderToSet(set, Slider{"NippleDistance", 0.05F, 0.08F});
            // Lift large breasts up
            AddSliderToSet(set, Slider{"NippleDown", 0.0F, -0.1F});
            // Flatten nipple + areola
            AddSliderToSet(set, DeriveSlider(a_actor, "NipplePerkManga", -0.25F));
            // Flatten nipple
            // AddSliderToSet(set, DeriveSlider(a_actor, "NipplePerkiness", 0.0f));
        }

        return set;
    }

Slider OBody::DeriveSlider(RE::Actor* a_actor, const char* a_morph, float a_target) const {
        return Slider{a_morph, a_target - GetMorph(a_actor, a_morph)};
    }
}
