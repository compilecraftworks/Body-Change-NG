#pragma once

#include "BodyChangeNG/SliderName.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace bcn::ube_genital
{
    // Independent of the OBody-compatible 3BA/BHUNP policy. Percent values
    // below are absolute BCNG-owned targets, never additive deltas.
    enum class Shape : std::uint8_t { innie, average, outie };
    [[nodiscard]] constexpr Shape SelectShape(const unsigned roll) noexcept
    {
        return roll < 20U ? Shape::innie : roll < 80U ? Shape::average : Shape::outie;
    }

    inline constexpr std::array<const char*, 5> extraNames{
        "Anus_creases", "Anus_creases_1", "BiggerAnus_2", "Shy_Pussy", "Necoco_Body_Addon_Lewd_Zone"
    };

    [[nodiscard]] constexpr std::uint8_t ExtraBit(const std::string_view name) noexcept
    {
        for (std::size_t i{}; i < extraNames.size(); ++i) {
            if (slider_name::Equal{}(name, extraNames[i])) return static_cast<std::uint8_t>(1U << i);
        }
        return 0U;
    }

    struct Range { float start; float finish; };
    struct Slider {
        const char* name;
        std::array<Range, 3> shapes;
    };

    // Conservative, correlated recipes informed by the installed TOFU UBE
    // preset survey. These are BCNG design limits, not author-recommended or
    // visually certified limits. See docs/UBE-GENITAL-RANDOMIZATION-20260926-KO.md.
    // One shared blend controls a whole recipe: never combine independently
    // rolled extreme shape, width, protrusion and opening values.
    inline constexpr std::array<Slider, 11> commonSliders{{
        { "PubicAreaSize p|n", {{{10, 25}, {5, 30}, {5, 25}}} },
        { "PubicAreaHilly", {{{25, 45}, {10, 35}, {10, 30}}} },
        { "PussyCute", {{{50, 80}, {50, 15}, {20, 0}}} },
        { "Vagina_shape", {{{0, 10}, {5, 25}, {20, 45}}} },
        { "Vagina_shape_wider", {{{0, 5}, {0, 10}, {5, 20}}} },
        { "Vagina_Fantasy", {{{0, 20}, {20, 50}, {40, 70}}} },
        { "ClitorisErection", {{{0, 15}, {10, 30}, {20, 45}}} },
        // 3BA branch ranges inform, but do not directly scale to, this mesh.
        // Positive limits use the outer-mesh displacement as a design guide;
        // -6 is the observed authored lower endpoint, not a safety guarantee.
        { "Vagina_spread", {{{-6, 2}, {-6, 15}, {0, 35}}} },
        { "AnusSize", {{{15, 30}, {20, 40}, {25, 50}}} },
        { "BiggerAnus", {{{10, 20}, {15, 30}, {20, 35}}} },
        { "AnusTriangular", {{{20, 40}, {30, 60}, {40, 80}}} }
    }};

    // Necoco-only detail is small and correlated with the common recipe.
    // The two crease variants share a fixed budget instead of stacking maxima.
    // No extra is written without a non-empty morph in a loaded BODYTRI.
    inline constexpr std::array<Slider, 5> extraSliders{{
        { extraNames[0], {{{5, 0}, {5, 0}, {5, 0}}} },
        { extraNames[1], {{{0, 5}, {0, 5}, {0, 5}}} },
        { extraNames[2], {{{0, 3}, {0, 5}, {5, 10}}} },
        { extraNames[3], {{{5, 10}, {5, 0}, {0, 0}}} },
        { extraNames[4], {{{0, 5}, {5, 10}, {10, 20}}} }
    }};

    template <class Write>
    void Generate(const Shape shape, const float blend, const std::uint8_t supportedExtras, Write&& write)
    {
        const auto index = static_cast<std::size_t>(shape);
        if (index >= 3U || !std::isfinite(blend) || blend < 0.F || blend > 1.F) return;
        const auto apply = [&](const Slider& slider) {
            const auto range = slider.shapes[index];
            write(slider.name, (range.start + (range.finish - range.start) * blend) / 100.F);
        };
        for (const auto& slider : commonSliders) apply(slider);
        for (std::size_t i{}; i < extraSliders.size(); ++i) {
            if ((supportedExtras & (1U << i)) != 0U) apply(extraSliders[i]);
        }
        // AnusSpread is deliberately absent, NOT forced to zero. Authored
        // preset values and other mods' independently owned morphs survive.
    }
}
