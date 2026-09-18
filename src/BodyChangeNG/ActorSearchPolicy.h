#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace bcn::actor_search
{
    inline std::string Normalize(std::string_view text)
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
        std::string result(text);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }
    inline bool Matches(std::string_view query, std::string_view name, std::uint32_t reference)
    {
        if (query.empty()) return false;
        const auto id = std::format("{:08x}", reference);
        if (query.starts_with("0x")) return query.size() > 2 && id.find(query.substr(2)) != std::string::npos;
        return Normalize(name).find(query) != std::string::npos || id.find(query) != std::string::npos;
    }
}
