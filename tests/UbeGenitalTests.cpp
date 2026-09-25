#include "BodyChangeNG/UbeGenitalRandomization.h"
#include "BodyChangeNG/UbeTriSupport.h"
#include "BodyChangeNG/BodyMorphPolicies.h"

#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(const bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    struct Reader
    {
        const std::vector<char>& bytes;
        std::size_t cursor{};
        std::uint32_t Remaining() const { return static_cast<std::uint32_t>(bytes.size() - cursor); }
        bool Read(void* target, std::uint32_t count) {
            if (count > Remaining()) return false;
            if (count) std::memcpy(target, bytes.data() + cursor, count);
            cursor += count;
            return true;
        }
        bool Skip(std::uint32_t count) {
            if (count > Remaining()) return false;
            cursor += count;
            return true;
        }
    };
    struct Fixture
    {
        bool packed;
        std::vector<char> bytes;
        template<class T> void Number(const T value) {
            const auto* raw = reinterpret_cast<const char*>(&value);
            bytes.insert(bytes.end(), raw, raw + sizeof(T));
        }
        void Count(std::uint32_t value) {
            if (packed) Number(static_cast<std::uint16_t>(value)); else Number(value);
        }
        void Name(std::string_view name) {
            Number(static_cast<std::uint8_t>(name.size()));
            bytes.insert(bytes.end(), name.begin(), name.end());
        }
        Fixture(bool isPacked, std::string_view name, unsigned vertices = 1, bool zero = false, float scale = 1.F) : packed(isPacked) {
            bytes = { packed ? 'P' : '\0', 'I', 'R', 'T' };
            Count(1); Name("BaseShape");
            if (!packed) Number(std::uint32_t{});
            Count(1); Name(name);
            if (!packed) Number(std::uint32_t{});
            if (packed) Number(scale);
            Count(vertices);
            for (unsigned i{}; i < vertices; ++i) {
                if (packed) {
                    Number(static_cast<std::uint16_t>(i));
                    Number(static_cast<std::int16_t>(zero ? 0 : 1));
                    Number(std::int16_t{}); Number(std::int16_t{});
                } else {
                    Number(i); Number(zero ? 0.F : 1.F); Number(0.F); Number(0.F);
                }
            }
        }
    };
    auto Parse(const std::vector<char>& bytes) {
        Reader reader{bytes};
        return bcn::ube_anatomy::ReadExtraSupport(reader);
    }
}

int main(int argc, char** argv)
try {
    using namespace bcn::ube_genital;
    std::array<unsigned, 3> branches{};
    for (unsigned roll{}; roll < 100; ++roll) ++branches[static_cast<unsigned>(SelectShape(roll))];
    Check(branches == std::array<unsigned, 3>{20, 60, 20}, "branch probabilities changed");
    unsigned recipes{};
    for (unsigned mask{}; mask < 32; ++mask) {
        for (unsigned shape{}; shape < 3; ++shape) {
            for (unsigned step{}; step <= 100; ++step) {
                std::map<std::string, float> values{{"AnusSpread", .27F}, {"NippleHeight n|p", .43F}, {"PenisLength", .1F}};
                const auto blend = static_cast<float>(step) / 100.F;
                Generate(static_cast<Shape>(shape), blend, static_cast<std::uint8_t>(mask),
                    [&](const char* name, float value) {
                        Check(std::isfinite(value), "nonfinite recipe");
                        values.insert_or_assign(name, value);
                    });
                Check(values.size() == 14U + std::popcount(mask), "unexpected sliders or unsupported extras");
                Check(values["AnusSpread"] == .27F && values["NippleHeight n|p"] == .43F && values["PenisLength"] == .1F,
                    "excluded anatomy was overwritten");
                for (const auto& slider : commonSliders) {
                    const auto r = slider.shapes[shape];
                    const auto expected = (r.start + (r.finish - r.start) * blend) / 100.F;
                    Check(std::abs(values.at(slider.name) - expected) < .000001F, "correlated recipe changed");
                }
                ++recipes;
            }
        }
    }
    constexpr std::array<Range,3> opening{{{-6,2},{-6,15},{0,35}}};
    for (unsigned i{}; i < 3; ++i) {
        Check(commonSliders[7].shapes[i].start == opening[i].start &&
            commonSliders[7].shapes[i].finish == opening[i].finish, "opening design range drift");
    }
    int invalidWrites{};
    const auto write = [&](const char*, float) { ++invalidWrites; };
    Generate(Shape::innie, std::numeric_limits<float>::quiet_NaN(), 31, write);
    Generate(Shape::innie, -1.F, 31, write);
    Generate(Shape::innie, 1.1F, 31, write);
    Generate(static_cast<Shape>(255), .5F, 31, write);
    Check(invalidWrites == 0, "invalid input wrote morphs");
    Check(!bcn::body_morph_policy::UsesConventionalAnatomyRecipe(
        bcn::body_morph_policy::FemaleFamily::ube), "UBE leaked into legacy nipple/genital policy");
    Check(ExtraBit("SHY_pUSSY") == 8U && ExtraBit("AnusSpread") == 0U && ExtraBit("PenisLength") == 0U,
        "case matching or allowlist changed");
    for (bool packed : {false, true}) {
        for (unsigned i{}; i < extraNames.size(); ++i) {
            Fixture valid(packed, extraNames[i], 513);
            Check(Parse(valid.bytes) == (1U << i), "valid extra missing");
            for (std::size_t size{}; size < valid.bytes.size(); ++size) {
                const std::vector<char> shortFile(valid.bytes.begin(), valid.bytes.begin() + size);
                Check(!Parse(shortFile), "truncated file accepted");
            }
        }
        Check(Parse(Fixture(packed, "Shy_Pussy", 0).bytes) == 0U, "empty morph accepted");
        Check(Parse(Fixture(packed, "Shy_Pussy", 3, true).bytes) == 0U, "zero deltas accepted");
        Check(Parse(Fixture(packed, "AnusSpread").bytes) == 0U, "excluded morph enabled");
    }
    Check(Parse(Fixture(true, "SHY_PUSSY").bytes) == 8U, "casefold TRI lookup failed");
    Check(Parse(Fixture(true, "Necoco_Overall_Nipple").bytes) == 0U, "unmapped nipple extra enabled");
    Check(Parse(Fixture(true, "Shy_Pussy", 1, false, 0.F).bytes) == 0U, "zero multiplier enabled morph");
    Check(!Parse(Fixture(true, "Shy_Pussy", 1, false, std::numeric_limits<float>::infinity()).bytes), "infinite multiplier accepted");
    auto uv = Fixture(true, "unused");
    uv.Count(1); uv.Name("BaseShape"); uv.Count(1); uv.Name("Shy_Pussy"); uv.Number(1.F);
    uv.Count(1); uv.Number(std::uint16_t{}); uv.Number(std::int16_t{1}); uv.Number(std::int16_t{});
    Check(Parse(uv.bytes) == 0U, "UV morph enabled position extra");
    uv.bytes.pop_back(); Check(!Parse(uv.bytes), "truncated UV accepted");
    auto many = Fixture(true, "unused");
    many.bytes[4] = char(-1); many.bytes[5] = char(-1);
    Check(!Parse(many.bytes), "excessive shape count accepted");
    // Optional real TRI files are read only, never game assets written.
    for (int i = 1; i < argc; ++i) {
        std::ifstream input(std::filesystem::path(argv[i]), std::ios::binary);
        Check(input.good(), "TRI input cannot be opened");
        std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
        const auto support = Parse(bytes);
        Check(support.has_value(), "real body TRI rejected");
        std::cout << "TRI extra mask=" << unsigned(*support) << " " << argv[i] << '\n';
    }
    std::cout << recipes << " correlated recipes; exact 20/60/20; TRI packed/legacy/truncation/exclusion tests passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
