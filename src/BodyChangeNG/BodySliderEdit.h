#pragma once
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/SliderName.h"
#include <cmath>
#include <array>
#include <string_view>

namespace bcn::racemenu::editor
{
    // These exact keys are read/written by the installed body slider scripts.
    // Do not guess a key for an unknown or ambiguous framework.
    struct KeyInfo { body_family::Mask family; const char* key; const char* marker; };
    inline constexpr std::array kKeys{
        KeyInfo{body_family::Bit(body_family::Family::cbbe), "RaceMenuMorphsCBBE.esp", "BCNGEditorSession:CBBE"},
        KeyInfo{body_family::Bit(body_family::Family::himbo), "RaceMenuMorphsHIMBO.esp", "BCNGEditorSession:HIMBO"},
        KeyInfo{body_family::Bit(body_family::Family::ube), "UBE_RaceMenuMorphs.esp", "BCNGEditorSession:UBE"}
    };
    [[nodiscard]] constexpr const char* Key(body_family::Mask family) noexcept
    {
        for (const auto& info : kKeys) if (family == info.family) return info.key;
        return nullptr;
    }
    [[nodiscard]] constexpr const char* Marker(std::string_view key) noexcept
    {
        for (const auto& info : kKeys) if (key == info.key) return info.marker;
        return nullptr;
    }

    using Values = slider_name::Map<float>;
    [[nodiscard]] inline bool Finite(const Values& values)
    {
        for (const auto& [name, value] : values)
            if (name.empty() || !std::isfinite(value)) return false;
        return true;
    }

    struct Transfer final
    {
        Values body, originalEditor;

        [[nodiscard]] Values Editable() const
        {
            auto values = originalEditor;
            for (const auto& [name, value] : body) values[name] += value;
            return values;
        }

        // Preserve the pre-existing editor key as a foreign contribution.
        // The committed BCNG key takes only the difference. Include removed
        // and zero sliders so Reset does not resurrect the XML preset later.
        [[nodiscard]] Values Committed(const Values& edited) const
        {
            auto values = edited;
            for (const auto& [name, value] : body) values.try_emplace(name, 0.F);
            for (const auto& [name, value] : originalEditor) values[name] -= value;
            return values;
        }
    };
}
