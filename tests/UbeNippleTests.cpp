#include "BodyChangeNG/UbeNippleRandomization.h"
#include "BodyChangeNG/UbeGenitalRandomization.h"
#include "BodyChangeNG/SliderName.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Check(const bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    bool Near(const float a, const float b) { return std::abs(a - b) < .00001F; }
    using Values = bcn::slider_name::Map<float>;
    struct Event {
        bool chance;
        float low, high;
        bool operator==(const Event&) const = default;
    };
    struct Draws {
        unsigned mask, step, cursor{}, randomIndex{};
        std::vector<Event> events;
        bool Chance(int threshold) {
            events.push_back({true, float(threshold), 0});
            return (mask & (1U << cursor++)) != 0U;
        }
        float Range(float low, float high) {
            events.push_back({false, low, high});
            // Different fraction per draw: detects accidentally correlated
            // range calls while sweeping every branch and range endpoint.
            const auto unit = float((step + 17U * randomIndex++) % 101U) / 100.F;
            return low + (high - low) * unit;
        }
    };
}

int main()
try {
    const Values base{{"nippleLENGTH", 1.3F}, {"NippleHeight n|p", -.4F},
        {"NippleDispertionSides n|p", .25F}, {"NippleHorizontal n|p", -.1F},
        {"AreolaHorizontal", .05F}, {"AreolaVertical", -.05F}, {"BreastsSize", .7F},
        {"Vagina_spread", .09F}, {"AnusSpread", .4F}, {"PenisLength", .2F},
        {"NippleInverted_PuffyAreola", .38F}, {"NippleInverted_PuffyAreola_UV_fix", .07F},
        {"NippleCircularCrease", .2F}, {"NEW_NippleCircularCrease_Uv_fix", .2F},
        {"NippleCreaseVertical", .65F}, {"Necoco_Overall_Nipple", -.5F},
        {"AreolaeSizeSmall", .4F}, {"AreolaeSizeBig", .5F}, {"NipplesPerkiness", .8F},
        {"NipplesShowUp", .9F}, {"AreolaErection", .6F}, {"Nippleinverted", .7F}};
    const std::array untouched{"NippleHeight n|p", "NippleDispertionSides n|p", "NippleHorizontal n|p",
        "AreolaHorizontal", "AreolaVertical", "BreastsSize", "Vagina_spread", "AnusSpread", "PenisLength",
        "NippleInverted_PuffyAreola", "NippleInverted_PuffyAreola_UV_fix", "NippleCircularCrease",
        "NEW_NippleCircularCrease_Uv_fix", "NippleCreaseVertical", "Necoco_Overall_Nipple"};
    unsigned cases{};
    for (unsigned mask{}; mask < 512; ++mask) {
        for (unsigned step{}; step <= 100; ++step) {
            Draws expectedDraws{mask, step}, actualDraws{mask, step};
            Values source, actual;
            bcn::body_morph_policy::GenerateNippleMorphs(
                [&](int p) { return expectedDraws.Chance(p); },
                [&](float low, float high) { return expectedDraws.Range(low, high); },
                [&](const char* name, float value) { source.emplace(name, value); });
            bcn::ube_nipple::Generate(
                [&](int p) { return actualDraws.Chance(p); },
                [&](float low, float high) { return actualDraws.Range(low, high); },
                [&](const char* name, float value) {
                    Check(std::isfinite(value) && value >= -.15001F && value <= .60001F, "unbounded UBE target");
                    Check(actual.emplace(name, value).second, "duplicate target write");
                });
            Check(actualDraws.events == expectedDraws.events, "chance/range order diverged from 3BA");
            Values expected;
            const auto areola = source.at("AreolaSize");
            expected["AreolaeSizeSmall_UV_fix"] = areola < 0 ? -areola * .3F : 0.F;
            expected["AreolaeSizeBig_UV_fix"] = areola > 0 ? areola * .35F : 0.F;
            if (source.contains("AreolaPull_v2")) {
                const auto pull = source.at("AreolaPull_v2");
                expected["AreolaeSizeSmall"] = pull < 0 ? -pull * 1.2F : 0.F;
                expected["AreolaeSizeBig"] = pull > 0 ? pull * .35F : 0.F;
            }
            const auto length = source.at("NippleLength");
            const auto thick = source.contains("NippleThicc_v2") ? source.at("NippleThicc_v2") / .9F * .1F : 0.F;
            expected["NippleLength"] = (length < .2F ? .05F + length * 2.F : .25F + (length - .2F) * 1.5F) + thick;
            expected["Nipples_Fantasy"] = (source.at("NippleManga") + .3F) / 1.1F * .45F;
            expected["NEW_Nipples_Fantasy_UV_fix"] = expected["Nipples_Fantasy"];
            if (source.contains("NipplePerkManga")) expected["NipplesPerkiness"] = -.1F + (source.at("NipplePerkManga") + .3F) / 1.5F * .5F;
            if (source.contains("NipBGone")) expected["NipplesShowUp"] = (1.F - source.at("NipBGone")) * .25F;
            expected["NippleDiameter n|p"] = -.15F + (source.at("NippleSize") + .5F) * .5F + thick;
            expected["NEW_NippleDiameter_neg_value_Uv_fix"] = (std::max)(0.F, -expected["NippleDiameter n|p"]);
            expected["NippleCreaseCentral"] = source.at("NippleDip") * .3F;
            expected["NippleCreaseHorizontal"] = (source.at("NippleCrease_v2") + .4F) / 1.4F * .3F;
            if (source.contains("NipplePuffy_v2")) expected["AreolaErection"] = .25F + (source.at("NipplePuffy_v2") - .4F) / .3F * .2F;
            if (source.contains("NippleInvert_v2")) {
                const auto inverted = source.at("NippleInvert_v2");
                expected["Nippleinverted"] = inverted == 1.F ? .6F : .35F + (inverted - .65F);
            }
            Check(actual.size() == expected.size(), "UBE optional writes differ");
            for (const auto& [name, value] : expected) Check(actual.contains(name) && Near(actual.at(name), value), "UBE mapping changed");
            auto values = base;
            for (const auto& [name, value] : actual) values.insert_or_assign(name, value);
            for (const auto* name : untouched) Check(values.at(name) == base.at(name), "unrelated slider changed");
            for (const auto* name : {"AreolaeSizeSmall", "AreolaeSizeBig", "NipplesPerkiness",
                "NipplesShowUp", "AreolaErection", "Nippleinverted"}) {
                if (!actual.contains(name)) Check(values.at(name) == base.at(name), "failed optional branch cleared XML");
            }
            Check(values.at("NIPPLElength") == actual.at("NippleLength"), "casefold write failed");
            const auto once = values;
            for (const auto& [name, value] : actual) values.insert_or_assign(name, value);
            Check(values == once, "reapplying accumulated targets");
            bcn::ube_genital::Generate(bcn::ube_genital::Shape::average, .5F, 31,
                [&](const char* name, float) { Check(!actual.contains(name), "genital recipe overlaps nipple writes"); });
            if (mask == 511) {
                Check(actual.contains("AreolaErection") && actual.contains("Nippleinverted") &&
                    actual.contains("NipplesShowUp") && actual.contains("NipplesPerkiness"),
                    "independent branches became mutually exclusive");
                Check(actual.at("Nippleinverted") == .6F, "50% fixed inversion branch changed");
            }
            ++cases;
        }
    }
    std::cout << cases << " UBE nipple mappings; original independent draws, optional preservation, casefold, no accumulation passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
