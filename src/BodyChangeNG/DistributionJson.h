#pragma once

#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
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

    // Move comment tokens, not old JSON values, below the newly saved root.
    // Quoted/escaped strings and whole block comments must be skipped so that
    // paths, URLs and disabled example objects cannot become active rules.
    // Canonical separators make repeated saves byte-stable (no growing footer).
    [[nodiscard]] inline std::string Comments(std::string_view text)
    {
        std::string comments;
        for (std::size_t position{}; position < text.size();) {
            if (text[position] == '"') {
                ++position;
                while (position < text.size()) {
                    const auto value = text[position++];
                    if (value == '\\' && position < text.size()) ++position;
                    else if (value == '"') break;
                }
                continue;
            }
            if (text[position] != '/' || position + 1 == text.size()) {
                ++position;
                continue;
            }
            const auto begin = position;
            if (text[position + 1] == '/') {
                const auto end = text.find_first_of("\r\n", position + 2);
                position = end == std::string_view::npos ? text.size() : end;
            } else if (text[position + 1] == '*') {
                const auto end = text.find("*/", position + 2);
                if (end == std::string_view::npos) {
                    throw std::runtime_error("unfinished distribution comment; original file retained");
                }
                position = end + 2;
            } else {
                ++position;
                continue;
            }
            comments.append(text.substr(begin, position - begin));
            comments.push_back('\n');
        }
        return comments;
    }

    [[nodiscard]] inline std::string ReadComments(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path)) return {};
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) throw std::runtime_error("could not read distribution comments");
        const std::string text((std::istreambuf_iterator<char>(stream)), {});
        if (stream.bad()) throw std::runtime_error("distribution comment read failed");
        return Comments(text);
    }

    [[nodiscard]] inline bool BackupForMigration(const std::filesystem::path& source, int schema)
    {
        // Preserve the exact input first. Never overwrite an earlier backup.
        for (unsigned attempt{}; attempt < 100U; ++attempt) {
            auto backup = source;
            backup += ".schema" + std::to_string(schema) + (attempt ? "." + std::to_string(attempt) : "") + ".bak";
            std::error_code error;
            if (std::filesystem::copy_file(source, backup, std::filesystem::copy_options::none, error)) return true;
            if (error != std::errc::file_exists) return false;
        }
        return false;
    }
}
