#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace bcn::ui_text
{
    // One UTF-8 boundary pass and logarithmically many measurements. Do not
    // repeatedly allocate and measure every shorter suffix of a long DDS path.
    template<class Measure>
    [[nodiscard]] std::string Ellipsize(std::string_view value, float width, Measure&& measure)
    {
        if (value.empty() || width <= 0.0F) return {};
        if (measure(value) <= width) return std::string(value);
        constexpr std::string_view suffix = "...";
        const auto suffixWidth = measure(suffix);
        if (suffixWidth > width) return {};
        std::vector<std::size_t> boundaries{ 0U };
        for (std::size_t i = 1; i < value.size(); ++i) {
            if ((static_cast<unsigned char>(value[i]) & 0xC0U) != 0x80U) boundaries.push_back(i);
        }
        boundaries.push_back(value.size());
        std::size_t low = 0U, high = boundaries.size() - 1U;
        while (low + 1U < high) {
            const auto middle = low + (high - low) / 2U;
            if (measure(value.substr(0U, boundaries[middle])) + suffixWidth <= width) low = middle;
            else high = middle;
        }
        return std::string(value.substr(0U, boundaries[low])) + std::string(suffix);
    }
}
