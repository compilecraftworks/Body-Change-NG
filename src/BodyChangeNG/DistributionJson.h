#pragma once

#include <istream>
#include <nlohmann/json.hpp>

namespace bcn::distribution_json
{
    // Opt in only for the hand-authored distribution file. The library lexer
    // recognizes comments outside strings; it still rejects malformed JSON,
    // unfinished comments and trailing commas. Other config formats stay strict.
    [[nodiscard]] inline nlohmann::json Parse(std::istream& stream)
    {
        return nlohmann::json::parse(stream, nullptr, true, true);
    }
}
