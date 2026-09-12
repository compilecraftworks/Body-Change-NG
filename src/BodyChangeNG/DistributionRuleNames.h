#pragma once

#include "BodyChangeNG/Settings.h"

#include <array>
#include <string_view>

namespace bcn::distribution_names
{
    struct Entry final
    {
        std::string_view key;
        std::string_view korean;
        std::string_view english;
        std::string_view chinese;
    };

    inline constexpr std::array kEntries{
        Entry{ "rule-new-female", "새 여성 NPC 규칙", "New female NPC rule", "新的女性 NPC 规则" },
        Entry{ "rule-new-male", "새 남성 NPC 규칙", "New male NPC rule", "新的男性 NPC 规则" },
        Entry{ "rule-all-female", "모든 여성 NPC", "All female NPCs", "所有女性 NPC" },
        Entry{ "rule-all-male", "모든 남성 NPC", "All male NPCs", "所有男性 NPC" }
    };

    [[nodiscard]] constexpr const Entry* Find(const std::string_view key) noexcept
    {
        for (const auto& entry : kEntries) {
            if (entry.key == key) return &entry;
        }
        return nullptr;
    }

    [[nodiscard]] constexpr std::string_view Localized(const std::string_view key,
        const UiLanguage language) noexcept
    {
        const auto* entry = Find(key);
        if (!entry) return {};
        switch (language) {
        case UiLanguage::korean: return entry->korean;
        case UiLanguage::chineseSimplified: return entry->chinese;
        default: return entry->english;
        }
    }

    [[nodiscard]] constexpr bool IsLocalizedValue(const std::string_view key,
        const std::string_view value) noexcept
    {
        const auto* entry = Find(key);
        return entry && (value == entry->korean || value == entry->english || value == entry->chinese);
    }

    [[nodiscard]] constexpr std::string_view DefaultRuleKey(const bool female) noexcept
    {
        return female ? "rule-all-female" : "rule-all-male";
    }

    [[nodiscard]] constexpr std::string_view NewRuleKey(const bool female) noexcept
    {
        return female ? "rule-new-female" : "rule-new-male";
    }

    [[nodiscard]] constexpr bool IsGeneratedRuleKey(const std::string_view key) noexcept
    {
        return key == "rule-new-female" || key == "rule-new-male" ||
            key == "rule-all-female" || key == "rule-all-male";
    }

    [[nodiscard]] constexpr std::string_view RetargetGeneratedRuleKey(
        const std::string_view key, const bool female) noexcept
    {
        if (key == "rule-new-female" || key == "rule-new-male") return NewRuleKey(female);
        if (key == "rule-all-female" || key == "rule-all-male") return DefaultRuleKey(female);
        return {};
    }

    [[nodiscard]] constexpr std::string_view RecognizeKey(const std::string_view,
        const std::string_view name, const bool female) noexcept
    {
        for (const auto key : { NewRuleKey(female), DefaultRuleKey(female) }) {
            if (!name.empty() && IsLocalizedValue(key, name)) return key;
        }
        // Older editors could change the stored sex without retargeting the
        // generated label.  Recover that exact built-in text as a generated
        // name for the current sex; arbitrary custom names remain untouched.
        for (const auto key : { NewRuleKey(!female), DefaultRuleKey(!female) }) {
            if (!name.empty() && IsLocalizedValue(key, name)) {
                return key == NewRuleKey(!female) ? NewRuleKey(female) :
                    DefaultRuleKey(female);
            }
        }
        return name.empty() ? DefaultRuleKey(female) : std::string_view{};
    }
}
