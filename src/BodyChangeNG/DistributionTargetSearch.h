#pragma once

#include "BodyChangeNG/AssetIdentity.h"
#include <functional>
#include <numeric>
#include <string>
#include <vector>

namespace bcn::distribution_editor
{
    // One open target dropdown owns this cache. No engine pointers, disk I/O,
    // or per-frame conversion of the full catalog. Filter before clipping.
    struct TargetSearch
    {
        std::string query;
        std::string applied;
        std::vector<std::string> keys;
        std::vector<std::size_t> visible;

        static std::string Fold(std::string value)
        {
            for (auto& c : value) c = static_cast<char>(asset_identity::FoldCase(static_cast<unsigned char>(c)));
            return value;
        }
        template<class Range, class Key>
        void Build(const Range& options, Key key)
        {
            *this = {};
            keys.reserve(options.size());
            for (const auto& option : options) keys.push_back(Fold(std::invoke(key, option)));
            visible.resize(keys.size());
            std::iota(visible.begin(), visible.end(), std::size_t{});
        }
        bool Filter()
        {
            auto needle = Fold(query);
            if (needle == applied) return false;
            applied = std::move(needle);
            visible.clear();
            for (std::size_t i{}; i < keys.size(); ++i)
                if (keys[i].find(applied) != std::string::npos) visible.push_back(i);
            return true;
        }
    };
}
