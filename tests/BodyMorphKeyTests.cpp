#include "BodyChangeNG/BodyMorphKeys.h"
#include "BodyChangeNG/BodyMorphPolicies.h"

#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace
{
    unsigned checks{};
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }
    bool Near(float a, float b) { return std::abs(a - b) < 0.00001F; }
    struct Actor {};
    struct Morphs {
        std::map<std::string, std::map<std::string, float>> values;
        void ClearBodyMorphKeys(Actor*, const char* key) {
            for (auto it = values.begin(); it != values.end();) {
                it->second.erase(key);
                if (it->second.empty()) it = values.erase(it);
                else ++it;
            }
        }
        void ClearMorphs(Actor*) { values.clear(); }
        float Sum(const std::string& name) const {
            float result{};
            if (const auto found = values.find(name); found != values.end()) {
                for (const auto& [key, value] : found->second) result += value;
            }
            return result;
        }
    };
}

int main()
{
    try {
        namespace keys = bcn::racemenu::keys;
        using bcn::body_morph_policy::FemaleFamily;
        Actor actor;
        for (bool preserve : {false, true}) {
            Morphs morphs;
            morphs.values["Breasts"] = {{keys::body, .7F}, {keys::legacyBody, .1F},
                {"Foreign", .2F}, {keys::outfit, -.05F}};
            morphs.values["Breasts"][keys::obody] = 1.2F;
            morphs.values["Breasts"][keys::oclothe] = -.1F;
            morphs.values["OBodyOnly"] = {{keys::obody, .6F}, {keys::oclothe, -.1F}};
            morphs.values["ForeignOnly"] = {{"Foreign", .5F}};
            const auto original = morphs.values;
            for (unsigned frame{}; frame < 128U; ++frame) {
                keys::PreviewBase baseline(preserve);
                for (const auto& [name, values] : morphs.values) {
                    for (const auto& [key, value] : values) baseline.Visit(name.c_str(), key.c_str(), value);
                }
                // A/B/A includes negatives and values above BodySlide 100%.
                const float desired = frame % 3U == 0U ? -.5F : frame % 3U == 1U ? 1.75F : .4F;
                morphs.values["Breasts"][keys::preview] =
                    bcn::racemenu::PreviewPresetCorrection(desired, baseline.values["Breasts"]);
                if (baseline.values.contains("ForeignOnly")) {
                    morphs.values["ForeignOnly"][keys::preview] = -baseline.values["ForeignOnly"];
                }
                if (baseline.values.contains("OBodyOnly")) {
                    morphs.values["OBodyOnly"][keys::preview] = -baseline.values["OBodyOnly"];
                }
                Check(Near(morphs.Sum("OBodyOnly"), 0.0F),
                    "preserving preview did not neutralize the OBody-only slider");
                Check(Near(morphs.Sum("Breasts"), desired + (preserve ? .2F : 0.0F)),
                    "preview did not follow the selected preservation policy");
                Check(Near(morphs.Sum("ForeignOnly"), preserve ? .5F : 0.0F),
                    "foreign-only slider was lost or omitted by non-preserving preview");
                morphs.ClearBodyMorphKeys(&actor, keys::preview);
                Check(morphs.values == original, "preview cancel changed persistent or foreign keys");
            }
            keys::BeginPresetCommit(morphs, &actor, preserve);
            Check(morphs.values["Breasts"].contains("Foreign") == preserve,
                "commit clearing scope ignored preservation");
            Check(!morphs.values["Breasts"].contains(keys::obody) &&
                !morphs.values["Breasts"].contains(keys::oclothe) && Near(morphs.Sum("OBodyOnly"), 0.0F),
                "commit preserved OBody/OClothe when other-morph preservation was enabled");
            for (float desired : {-.5F, .4F, 1.75F}) {
                keys::BeginPresetCommit(morphs, &actor, preserve);
                morphs.values["Breasts"][keys::body] = desired;
                Check(morphs.values["Breasts"][keys::body] == desired &&
                    Near(morphs.Sum("Breasts"), desired + (preserve ? .2F : 0.0F)),
                    "preset value was clamped, accumulated, or compensated against a foreign value");
            }
            keys::ClearReplacedBody(morphs, &actor); // Default is not a ClearMorphs request.
            Check(Near(morphs.Sum("Breasts"), preserve ? .2F : 0.0F) &&
                Near(morphs.Sum("ForeignOnly"), preserve ? .5F : 0.0F),
                "Default reset removed another mod's keys or resurrected cleared values");
        }
        // Default preview/reset without a BCNG preset: both old provider keys
        // are removed on confirmation only; exact similarly named keys survive.
        {
            Morphs morphs;
            morphs.values["Breasts"] = {{keys::obody, 1.5F}, {keys::oclothe, -.2F},
                {"RaceMenu", .25F}, {"OBodyOtherMod", .1F}};
            morphs.values["obody_processed"] = {{keys::obody, 1.0F}};
            const auto original = morphs.values;
            keys::PreviewBase baseline(true);
            for (const auto& [name, values] : morphs.values) {
                for (const auto& [key, value] : values) baseline.Visit(name.c_str(), key.c_str(), value);
            }
            for (const auto& [name, value] : baseline.values) morphs.values[name][keys::preview] = -value;
            Check(Near(morphs.Sum("Breasts"), .35F), "default preview retained OBody or suppressed other mods");
            Check(morphs.values["obody_processed"][keys::obody] == 1.0F,
                "preview deleted OBody's persistent processed marker");
            morphs.ClearBodyMorphKeys(&actor, keys::preview);
            Check(morphs.values == original, "default preview cancel changed the original keys");
            keys::ClearOwned(morphs, &actor);
            Check(morphs.values == original, "BCNG-only ownership cleanup deleted OBody keys");
            keys::ClearReplacedBody(morphs, &actor);
            Check(Near(morphs.Sum("Breasts"), .35F) && !morphs.values.contains("obody_processed"),
                "default confirmation failed to remove exactly OBody/OClothe");
            const auto once = morphs.values;
            keys::ClearReplacedBody(morphs, &actor);
            Check(morphs.values == once, "repeated default reset accumulated changes");
        }
        // SFS NotReady must leave the old clothing layer alone, but a ready
        // plan replaces it using the exact same new-preset refit generator.
        for (bool preserve : {false, true}) {
            for (bool ready : {false, true}) {
                for (bool clothed : {false, true}) {
                    for (bool nipples : {false, true}) {
                        Morphs morphs;
                        morphs.values["NipBGone"] = {{keys::body, .7F}, {keys::outfit, .3F}, {"Foreign", .2F}};
                        morphs.values["Breasts"] = {{keys::body, .7F}, {keys::outfit, -.05F}, {"Foreign", .2F}};
                        const auto original = morphs.values;
                        for (const auto own : {-.5F, .4F, 1.75F}) {
                            keys::PreviewBase baseline(preserve, ready);
                            for (const auto& [name, values] : morphs.values) {
                                for (const auto& [key, value] : values) baseline.Visit(name.c_str(), key.c_str(), value);
                            }
                            std::map<std::string, float> desired{{"NipBGone", own}, {"Breasts", own}};
                            std::map<std::string, float> outfit;
                            if (ready && clothed) bcn::body_morph_policy::GenerateOutfitMorphs(
                                FemaleFamily::cbbe3ba, .5F, nipples,
                                [&](const char* name) { return desired[name]; },
                                [&](const char* name, float value) { outfit[name] = value; });
                            for (const auto& [name, value] : outfit) desired[name] += value;
                            for (const auto& [name, value] : baseline.values) desired.try_emplace(name, 0.0F);
                            for (const auto& [name, value] : desired) morphs.values[name][keys::preview] =
                                bcn::racemenu::PreviewPresetCorrection(value, baseline.values[name]);
                            const auto foreign = preserve ? .2F : 0.0F;
                            Check(Near(morphs.Sum("NipBGone"), foreign +
                                (ready ? (clothed && nipples ? 1.0F : own) : own + .3F)),
                                "preview target refit used an old/foreign body value or guessed unready SFS state");
                            Check(Near(morphs.Sum("Breasts"), foreign + own +
                                (!ready || clothed ? -.05F : 0.0F)), "preview additive/hidden refit changed");
                            morphs.ClearBodyMorphKeys(&actor, keys::preview);
                            Check(morphs.values == original, "preview outfit cancellation changed committed state");
                        }
                    }
                }
            }
        }
        Check(!keys::IsOwned("OBody") && !keys::IsOwned("OClothe") &&
            !keys::IsOwned("BodyChangeNGOtherMod"), "foreign morph was mistaken for a BCNG legacy key");
        Check(keys::IsReplacedBody("OBody") && keys::IsReplacedBody("OClothe") &&
            !keys::IsReplacedBody("OBodyOtherMod") && !keys::IsReplacedBody("OClotheOtherMod"),
            "OBody replacement was missing or matched an unrelated key prefix");

        for (auto family : {FemaleFamily::cbbe3ba, FemaleFamily::bhunpUnp}) {
            for (bool nipples : {false, true}) {
                for (float weight : {0.0F, .5F, 1.0F}) {
                    for (float own : {-.5F, .4F, 1.75F}) {
                        std::map<std::string, float> output;
                        std::set<std::string> reads;
                        bcn::body_morph_policy::GenerateOutfitMorphs(family, weight, nipples,
                            [&](const char* name) { reads.insert(name); return own; },
                            [&](const char* name, float value) { output[name] = value; });
                        const auto cbbe = family == FemaleFamily::cbbe3ba;
                        Check(output.size() == (cbbe ? 15U : 13U) + (nipples ? 8U : 0U),
                            "procedural refit slider coverage changed");
                        Check(reads.size() == 8U + (nipples ? 6U : 0U),
                            "fixed offsets incorrectly started subtracting preset values");
                        for (const auto& name : reads) {
                            float target{};
                            if (name == "BreastCleavage" || name == "NavelEven" || name == "NipBGone") target = 1.0F;
                            if (name == "AreolaSize" || name == "NippleAreola") target = -.3F;
                            if (name == "NipplePerkManga") target = -.25F;
                            for (float foreign : {-.4F, 0.0F, .2F, 1.2F}) {
                                Check(Near(own + output[name] + foreign, target + foreign),
                                    "target correction counteracted foreign morphs");
                            }
                        }
                        Check(Near(output["Breasts"], -.05F) && Near(output["BreastHeight"], .15F),
                            "OBody additive breast offsets changed");
                        if (nipples) Check(Near(output["NippleDistance"], .05F + .03F * weight) &&
                            Near(output["NippleDown"], -.1F * weight), "nipple offsets changed");
                        Check(!output.contains(cbbe ? "NippleAreola" : "NipBGone"),
                            "CBBE and BHUNP refit dialects were mixed");
                    }
                }
            }
        }
        for (auto family : {FemaleFamily::none, FemaleFamily::ube}) {
            unsigned writes{};
            bcn::body_morph_policy::GenerateOutfitMorphs(family, .5F, true,
                [](const char*) { return .4F; }, [&](const char*, float) { ++writes; });
            Check(writes == 0U, "unsupported family received procedural refit");
        }
        std::cout << "BodyMorphKeyTests passed: " << checks << " checks\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
