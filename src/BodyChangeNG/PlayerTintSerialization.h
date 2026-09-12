#pragma once

#include "BodyChangeNG/PlayerTint.h"
#include <algorithm>
#include <cmath>

namespace bcn::player_tint
{
    inline constexpr std::uint32_t kStateVersion = 2U;
    inline constexpr std::uint32_t kMaxSavedLayers = 32U;

    template<class Write, class WriteString>
    [[nodiscard]] bool WriteState(const PersistedState& state, Write&& write, WriteString&& string)
    {
        if (state.layers.size() > kMaxSavedLayers || state.backups.size() > 15U) return false;
        if (!string(state.pack.value_or(std::string{})) ||
            !write(static_cast<std::uint32_t>(state.layers.size()))) return false;
        for (const auto& layer : state.layers) {
            if (!write(static_cast<std::uint8_t>(layer.layer)) || !write(static_cast<std::uint8_t>(layer.restored)) ||
                !write(layer.color.red) || !write(layer.color.green) || !write(layer.color.blue) ||
                !write(layer.color.alpha) || !string(layer.assetID)) return false;
        }
        if (!write(static_cast<std::uint32_t>(state.backups.size()))) return false;
        for (const auto& backup : state.backups) {
            if (!write(backup.type) || !string(backup.texturePath) || !write(backup.color[0]) ||
                !write(backup.color[1]) || !write(backup.color[2]) || !write(backup.alpha)) return false;
        }
        return true;
    }

    template<class Read, class ReadString>
    [[nodiscard]] std::optional<PersistedState> ReadState(
        const std::uint32_t version, Read&& read, ReadString&& string)
    {
        if (version != 1U && version != kStateVersion) return std::nullopt;
        PersistedState state;
        std::string pack;
        std::uint32_t count{};
        if (!string(pack) || pack.size() > 1024U || !read(count) || count > kMaxSavedLayers) return std::nullopt;
        if (!pack.empty()) state.pack = std::move(pack);
        for (std::uint32_t i{}; i < count; ++i) {
            PersistedLayerState layer;
            std::uint8_t raw{}, restored{};
            if (!read(raw) || !read(restored) || !read(layer.color.red) || !read(layer.color.green) ||
                !read(layer.color.blue) || !read(layer.color.alpha) || !string(layer.assetID)) return std::nullopt;
            if (raw > static_cast<std::uint8_t>(Layer::dirt) || restored > 1U || layer.assetID.size() > 1024U ||
                !std::isfinite(layer.color.red) || !std::isfinite(layer.color.green) ||
                !std::isfinite(layer.color.blue) || !std::isfinite(layer.color.alpha)) continue;
            layer.layer = static_cast<Layer>(raw);
            layer.restored = restored != 0U;
            layer.color.red = std::clamp(layer.color.red, 0.0F, 1.0F);
            layer.color.green = std::clamp(layer.color.green, 0.0F, 1.0F);
            layer.color.blue = std::clamp(layer.color.blue, 0.0F, 1.0F);
            layer.color.alpha = std::clamp(layer.color.alpha, 0.0F, 1.0F);
            state.layers.push_back(std::move(layer));
        }
        // v1 had no per-save original masks. Do not import another character's global JSON backup.
        if (version == 1U) return state;
        if (!read(count) || count > 15U) return std::nullopt;
        for (std::uint32_t i{}; i < count; ++i) {
            OriginalBackup backup;
            if (!read(backup.type) || !string(backup.texturePath) || !read(backup.color[0]) ||
                !read(backup.color[1]) || !read(backup.color[2]) || !read(backup.alpha)) return std::nullopt;
            if (backup.type > static_cast<std::uint8_t>(Layer::dirt) || backup.texturePath.empty() ||
                backup.texturePath.size() > 1024U || !std::isfinite(backup.alpha)) continue;
            backup.alpha = std::clamp(backup.alpha, 0.0F, 1.0F);
            if (std::ranges::find(state.backups, backup.type, &OriginalBackup::type) == state.backups.end()) {
                state.backups.push_back(std::move(backup));
            }
        }
        return state;
    }
}
