#pragma once

#include "BodyChangeNG/BodyRandomizationPolicy.h"

#include <string_view>

namespace bcn::ube_nipple
{
    // Reuse the original 3BA draw program verbatim: same independent chance
    // calls, thresholds, order, conditional range calls and optional writes.
    // Only translate the resulting anatomical roles to UBE's morph dialect.
    // UBE bounds are BCNG design values, not a 1:1 visual conversion.
    // See docs/UBE-NIPPLE-RANDOMIZATION-20260926-KO.md.
    template <class Chance, class Range, class Write>
    void Generate(Chance&& chance, Range&& range, Write&& write)
    {
        float length{}, diameter{}, thickness{};
        const auto pair = [&](const char* name, const char* uv, const float value) {
            write(name, value);
            write(uv, value);
        };
        body_morph_policy::GenerateNippleMorphs(chance, range,
            [&](const char* rawName, const float value) {
                const std::string_view name(rawName);
                if (name == "AreolaSize") {
                    // 3BA AreolaSize is UV-only. Keep this independent of the
                    // optional mesh pull below, just as in the original.
                    write("AreolaeSizeSmall_UV_fix", value < 0.F ? -value * .3F : 0.F);
                    write("AreolaeSizeBig_UV_fix", value > 0.F ? value * .35F : 0.F);
                } else if (name == "AreolaPull_v2") {
                    write("AreolaeSizeSmall", value < 0.F ? -value * 1.2F : 0.F);
                    write("AreolaeSizeBig", value > 0.F ? value * .35F : 0.F);
                } else if (name == "NippleLength") {
                    // Original short [0,.1) / long [.2,.3) branches are disjoint.
                    length = value < .2F ? .05F + value * 2.F : .25F + (value - .2F) * 1.5F;
                } else if (name == "NippleManga") {
                    pair("Nipples_Fantasy", "NEW_Nipples_Fantasy_UV_fix", (value + .3F) / 1.1F * .45F);
                } else if (name == "NipplePerkManga") {
                    write("NipplesPerkiness", -.1F + (value + .3F) / 1.5F * .5F);
                } else if (name == "NipBGone") {
                    write("NipplesShowUp", (1.F - value) * .25F);
                } else if (name == "NippleSize") {
                    diameter = -.15F + (value + .5F) * .5F;
                } else if (name == "NippleDip") {
                    write("NippleCreaseCentral", value * .3F);
                } else if (name == "NippleCrease_v2") {
                    write("NippleCreaseHorizontal", (value + .4F) / 1.4F * .3F);
                } else if (name == "NipplePuffy_v2") {
                    write("AreolaErection", .25F + (value - .4F) / .3F * .2F);
                } else if (name == "NippleThicc_v2") {
                    // UBE has no single matching thick morph. Reuse this one
                    // draw for a small length+diameter component; no new draw.
                    thickness = value / .9F * .1F;
                } else if (name == "NippleInvert_v2") {
                    // Preserve the nested 50% fixed-value / ranged-value
                    // branch, not a new choice of two different UBE presets.
                    write("Nippleinverted", value == 1.F ? .6F : .35F + (value - .65F));
                }
            });
        // Compose independent size/length/thickness effects once in the owned
        // target. Reapplying never adds to a previous committed result.
        write("NippleLength", length + thickness);
        diameter += thickness;
        write("NippleDiameter n|p", diameter);
        write("NEW_NippleDiameter_neg_value_Uv_fix", diameter < 0.F ? -diameter : 0.F);
        // Failed optional branches deliberately leave their XML values alone.
        // Position/spacing, NippleInverted_PuffyAreola and Necoco's compound
        // Overall_Nipple have no additional random draw or reset here.
    }
}
