#include "BodyChangeNG/BodyFamily.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <vector>

namespace
{
    [[nodiscard]] std::vector<std::string> Tokenize(const std::string_view text)
    {
        std::vector<std::string> tokens;
        std::string current;
        const auto flush = [&]() {
            if (current.empty()) return;
            tokens.push_back(std::move(current));
            current.clear();
        };
        for (const unsigned char character : text) {
            if (std::isalnum(character) != 0) current.push_back(static_cast<char>(std::tolower(character)));
            else flush();
        }
        flush();
        return tokens;
    }

    [[nodiscard]] bool HasToken(const std::vector<std::string>& tokens, const std::string_view value)
    {
        return std::ranges::find(tokens, value) != tokens.end();
    }

    [[nodiscard]] bool HasFragment(const std::vector<std::string>& tokens, const std::string_view value)
    {
        return std::ranges::any_of(tokens, [value](const auto& token) { return token.contains(value); });
    }

    [[nodiscard]] bool HasSequence(const std::vector<std::string>& tokens,
        const std::initializer_list<std::string_view> sequence)
    {
        if (sequence.size() == 0U || sequence.size() > tokens.size()) return false;
        for (std::size_t offset{}; offset + sequence.size() <= tokens.size(); ++offset) {
            auto token = tokens.begin() + static_cast<std::ptrdiff_t>(offset);
            auto expected = sequence.begin();
            while (expected != sequence.end() && *token == *expected) {
                ++token;
                ++expected;
            }
            if (expected == sequence.end()) return true;
        }
        return false;
    }

    [[nodiscard]] unsigned CountBits(std::uint32_t value) noexcept
    {
        unsigned result{};
        while (value != 0U) {
            result += value & 1U;
            value >>= 1U;
        }
        return result;
    }

    [[nodiscard]] std::string WithoutUbeAnusMarker(const std::string_view text)
    {
        const auto tokens = Tokenize(text);
        std::string result;
        for (std::size_t index = 0; index < tokens.size(); ++index) {
            if (tokens[index] == "ube" && index + 1U < tokens.size() && tokens[index + 1U] == "anus") {
                ++index;
                continue;
            }
            result += tokens[index];
            result.push_back(' ');
        }
        return result;
    }

    [[nodiscard]] bcn::body_family::PresetClassification ClassifyText(const std::string_view text)
    {
        using namespace bcn::body_family;
        auto female = DetectText(text, Sex::female) & NonVanillaFamilies(Sex::female);
        const auto male = DetectText(text, Sex::male) & NonVanillaFamilies(Sex::male);

        const auto femaleCount = CountBits(female);
        const auto maleCount = CountBits(male);
        if (femaleCount != 0U && maleCount != 0U) {
            return { .conflict = true };
        }
        // Multiple same-sex families are intentionally retained: a combined
        // BodySlide set is usable from either corresponding actor-family tab.
        if (femaleCount != 0U) return { .families = female, .male = false };
        if (maleCount != 0U) return { .families = male, .male = true };
        return {};
    }
}

namespace bcn::body_family
{
    Mask DetectText(const std::string_view text, const Sex sex)
    {
        const auto tokens = Tokenize(text);
        if (tokens.empty()) return 0U;

        Mask detected{};
        if (sex == Sex::female) {
            const auto cbbe = HasFragment(tokens, "cbbe");
            const auto threeBa = HasFragment(tokens, "3ba");
            // This is the canonical 3BA BodySlide set name even though it
            // contains neither the literal CBBE nor 3BA token.
            const auto threeBbbBodyAmazing = HasSequence(tokens, { "3bbb", "body", "amazing" });
            const auto unp = HasFragment(tokens, "bhunp") || HasFragment(tokens, "uunp") ||
                HasFragment(tokens, "unpb") || HasToken(tokens, "unp");
            const auto ube = HasToken(tokens, "ube") || HasToken(tokens, "ube2") ||
                HasFragment(tokens, "ubebody");
            if (cbbe || threeBa || threeBbbBodyAmazing) detected |= Bit(Family::cbbe);
            if (unp) detected |= Bit(Family::unp);
            if (ube) detected |= Bit(Family::ube);
            // 3BBB belongs to both CBBE and BHUNP ecosystems.  It may only
            // reinforce an already unambiguous signal, never decide alone.
            return detected;
        }

        if (HasFragment(tokens, "himbo")) detected |= Bit(Family::himbo);
        if (HasToken(tokens, "sam") || HasToken(tokens, "samlight") || HasToken(tokens, "samhighpoly")) {
            detected |= Bit(Family::sam);
        }
        return detected;
    }

    PresetClassification ClassifyPreset(const std::string_view bodySet, const std::string_view presetName,
        const std::string_view sourcePath)
    {
        // "UBE Anus" in 3BA set names denotes only the borrowed anus part;
        // it is not an UBE body-family declaration.
        auto result = ClassifyText(WithoutUbeAnusMarker(bodySet));
        if (result.families != 0U || result.conflict) return result;

        const auto metadata = std::string(presetName) + ' ' + std::string(sourcePath);
        result = ClassifyText(metadata);
        result.usedMetadataFallback = result.families != 0U || result.conflict;
        return result;
    }

    std::string PresetFamilyLabel(const PresetClassification& classification)
    {
        if (classification.conflict || classification.families == 0U) return "Unclassified";
        std::string label;
        const auto append = [&label](const std::string_view value) {
            if (!label.empty()) label += " / ";
            label += value;
        };
        if ((classification.families & Bit(Family::cbbe)) != 0U) append("CBBE 3BA");
        if ((classification.families & Bit(Family::unp)) != 0U) append("BHUNP / UNP");
        if ((classification.families & Bit(Family::ube)) != 0U) append("UBE");
        if ((classification.families & Bit(Family::himbo)) != 0U) append("HIMBO");
        if ((classification.families & Bit(Family::sam)) != 0U) append("SAM");
        return label.empty() ? "Unclassified" : label;
    }

    Mask PresetMask(const std::string_view family, const bool male)
    {
        const auto sex = male ? Sex::male : Sex::female;
        const auto explicitFamily = DetectText(family, sex) & NonVanillaFamilies(sex);
        return explicitFamily != 0U ? explicitFamily : VanillaFamily(sex);
    }
}
