#pragma once
#include "BodyChangeNG/FaceSkinPolicy.h"
#include <optional>
#include <vector>

namespace bcn::face_skin
{
    inline constexpr std::uint32_t kMaxBaselines = 16384;
    template<class WriteValue, class WriteString>
    bool WriteBaselines(const std::vector<Baseline>& values, WriteValue value, WriteString string)
    {
        if (values.size() > kMaxBaselines || !value(static_cast<std::uint32_t>(values.size()))) return false;
        for (const auto& row : values) {
            if (!value(row.actor) || !value(row.base) || !string(row.node) ||
                !value(static_cast<std::uint8_t>(row.female)) || !value(row.touched)) return false;
            for (const auto* paths : { &row.visible, &row.saved, &row.owned, &row.pending }) {
                for (const auto& path : *paths) if (!string(path)) return false;
            }
        }
        return true;
    }
    template<class ReadValue, class ReadString>
    std::optional<std::vector<Baseline>> ReadBaselines(ReadValue value, ReadString string)
    {
        std::uint32_t count{};
        if (!value(count) || count > kMaxBaselines) return std::nullopt;
        std::vector<Baseline> rows;
        rows.reserve(count);
        for (std::uint32_t i{}; i < count; ++i) {
            Baseline row;
            std::uint8_t female{};
            if (!value(row.actor) || !value(row.base) || !string(row.node) ||
                !value(female) || !value(row.touched) || female > 1 || row.touched > 31 ||
                row.actor == 0 || row.base == 0 || row.node.empty() || row.node.size() > 1024) return std::nullopt;
            row.female = female != 0;
            for (auto* paths : { &row.visible, &row.saved, &row.owned, &row.pending }) {
                for (auto& path : *paths) if (!string(path) || path.size() > 1024) return std::nullopt;
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }
}
