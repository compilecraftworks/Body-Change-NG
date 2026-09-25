#pragma once

#include <cmath>
#include <random>
#include <stdexcept>

namespace bcn::body_morph_policy
{
    // Same generator construction and half-open float range as OBody's STL.h.
    [[nodiscard]] inline float OBodyRandom(float low, float high)
    {
        if (low >= high) throw std::invalid_argument("Random minimum must be below maximum");
        std::random_device device;
        std::mt19937 generator(device());
        std::uniform_real_distribution<float> distribution(low, std::nextafter(high, low));
        return distribution(generator);
    }

    [[nodiscard]] inline bool OBodyChance(int threshold)
    {
        // Deliberately retain the upstream 0..99 draw and <= comparison.
        return OBodyRandom(0.F, 99.F) <= static_cast<float>(threshold);
    }

    // OBody NG 4.4.3 Body.cpp GenerateRandomNippleSliders and
    // GenerateRandomGenitalSliders: same slider names, conditional writes,
    // probabilities and ranges. Caller owns actor eligibility and RNG state.
    // A failed optional branch must leave its XML value alone.
    template <class Chance, class Range, class Write>
    void GenerateNippleMorphs(Chance&& chance, Range&& range, Write&& write)
    {
        const auto random = [&](const char* name, float low, float high) {
            write(name, range(low, high));
        };
        const bool smallAreola = chance(15);
        random("AreolaSize", smallAreola ? -1.F : 0.F, smallAreola ? 0.F : 1.F);
        if (chance(75)) random("AreolaPull_v2", -.25F, 1.F);
        const bool longer = chance(15);
        random("NippleLength", longer ? .2F : 0.F, longer ? .3F : .1F);
        random("NippleManga", -.3F, .8F);
        if (chance(25)) random("NipplePerkManga", -.3F, 1.2F);
        if (chance(15)) random("NipBGone", .6F, 1.F);
        random("NippleSize", -.5F, .3F);
        random("NippleDip", 0.F, 1.F);
        random("NippleCrease_v2", -.4F, 1.F);
        if (chance(6)) random("NipplePuffy_v2", .4F, .7F);
        if (chance(35)) random("NippleThicc_v2", 0.F, .9F);
        if (chance(2)) {
            if (chance(50)) write("NippleInvert_v2", 1.F);
            else random("NippleInvert_v2", .65F, .8F);
        }
    }

    template <class Chance, class Range, class Write>
    void GenerateGenitalMorphs(Chance&& chance, Range&& range, Write&& write)
    {
        const auto random = [&](const char* name, float low, float high) {
            write(name, range(low, high));
        };
        if (chance(20)) {
            random("Innieoutie", .95F, 1.1F);
            if (chance(50)) random("Labiapuffyness", .75F, 1.25F);
            if (chance(40)) random("LabiaMorePuffyness_v2", 0.F, 1.F);
            random("Labiaprotrude", 0.F, .5F);
            random("Labiaprotrude2", 0.F, .1F);
            random("Labiaprotrudeback", 0.F, .1F);
            write("Labiaspread", 0.F);
            random("LabiaCrumpled_v2", 0.F, .3F);
            write("LabiaBulgogi_v2", 0.F);
            write("LabiaNeat_v2", 0.F);
            random("VaginaHole", -.2F, .05F);
            random("Clit", -.4F, .25F);
        } else if (chance(75)) {
            random("Innieoutie", .4F, .75F);
            if (chance(40)) random("Labiapuffyness", .5F, 1.F);
            if (chance(30)) random("LabiaMorePuffyness_v2", 0.F, .75F);
            random("Labiaprotrude", 0.F, .5F);
            random("Labiaprotrude2", 0.F, .75F);
            random("Labiaprotrudeback", 0.F, 1.F);
            if (chance(50)) {
                random("Labiaspread", 0.F, 1.F);
                random("LabiaCrumpled_v2", 0.F, .7F);
                if (chance(60)) random("LabiaBulgogi_v2", 0.F, .1F);
            } else {
                write("Labiaspread", 0.F);
                random("LabiaCrumpled_v2", 0.F, .2F);
                if (chance(45)) random("LabiaBulgogi_v2", 0.F, .3F);
            }
            write("LabiaNeat_v2", 0.F);
            random("VaginaHole", -.2F, .4F);
            random("Clit", -.2F, .25F);
        } else {
            random("Innieoutie", -.25F, .3F);
            if (chance(30)) random("Labiapuffyness", .2F, .5F);
            if (chance(10)) random("LabiaMorePuffyness_v2", 0.F, .35F);
            random("Labiaprotrude", 0.F, 1.F);
            random("Labiaprotrude2", 0.F, 1.F);
            random("Labiaprotrudeback", 0.F, 1.F);
            random("Labiaspread", 0.F, 1.F);
            random("LabiaCrumpled_v2", 0.F, 1.F);
            random("LabiaBulgogi_v2", 0.F, 1.F);
            if (chance(40)) random("LabiaNeat_v2", 0.F, .25F);
            random("VaginaHole", 0.F, 1.F);
            random("Clit", -.4F, .25F);
        }
        random("Vaginasize", 0.F, 1.F);
        random("ClitSwell_v2", -.3F, 1.1F);
        random("Cutepuffyness", 0.F, 1.F);
        random("LabiaTightUp", 0.F, 1.F);
        const bool soft = chance(60);
        random("CBPC", soft ? -.25F : .6F, soft ? .25F : 1.F);
        random("AnalPosition_v2", 0.F, 1.F);
        random("AnalTexPos_v2", 0.F, 1.F);
        random("AnalTexPosRe_v2", 0.F, 1.F);
        write("AnalLoose_v2", -.1F);
    }
}
