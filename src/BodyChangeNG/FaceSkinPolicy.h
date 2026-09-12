#pragma once

#include "BodyChangeNG/NativeTexturePath.h"
#include "BodyChangeNG/SkinTextureOwnership.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace bcn::face_skin
{
    // Shader texture indices, not BGSTextureSet's Papyrus ordinal mapping.
    // Deliberately excludes tint (6), eyes, mouth, and overlay nodes.
    inline constexpr std::array<std::uint8_t, 5> kChannels{ 0, 1, 2, 3, 7 };
    using Paths = std::array<std::string, kChannels.size()>;

    // NiOverride loads the supplied resource directly; it is not the native
    // BGSTextureSet loader which adds Data/Textures itself.
    [[nodiscard]] inline std::optional<std::string> CacheOverridePath(std::string_view cached)
    {
        const auto paths = native_skin::PathsFromCache(cached);
        return paths ? std::optional<std::string>{ paths->resource } : std::nullopt;
    }

    [[nodiscard]] constexpr bool CanRestoreChannel(std::uint8_t channel, bool hasPath) noexcept
    { return hasPath || (channel != 0 && channel != 1); }

    [[nodiscard]] constexpr bool RequiredTextureReady(std::uint8_t channel, bool hasPath,
        bool texturePresent, bool rendererPresent) noexcept
    { return !hasPath || (channel != 0 && channel != 1) || (texturePresent && rendererPresent); }

    [[nodiscard]] constexpr bool OwnsActiveBatch(std::uint64_t epoch, std::uint64_t currentEpoch,
        std::uint64_t generation, std::uint64_t activeGeneration) noexcept
    {
        return epoch == currentEpoch && generation == activeGeneration;
    }

    [[nodiscard]] constexpr bool Persistent(const bool player) noexcept { return player; }

    // One engine rebuild at a time. A selection arriving during that rebuild
    // requests one more pass, not an early face write or an unbounded queue.
    struct RebuildGate
    {
        bool requested{};
        bool inFlight{};
        constexpr void Request() noexcept { requested = true; }
        [[nodiscard]] constexpr bool Blocked() const noexcept { return requested || inFlight; }
        [[nodiscard]] constexpr bool CanApply(bool faceRunning, bool hasSelection, bool complete) const noexcept
        { return !Blocked() && !faceRunning && hasSelection && !complete; }
        [[nodiscard]] constexpr bool Begin(bool faceRunning, bool hasDispatch) noexcept
        {
            if (faceRunning || inFlight || !requested || !hasDispatch) return false;
            requested = false;
            inFlight = true;
            return true;
        }
        constexpr void Complete() noexcept { inFlight = false; }
    };

    enum class ReapplyAction { complete, wait, applyFace };
    [[nodiscard]] constexpr ReapplyAction ResolveReapply(bool faceComplete, bool facePending) noexcept
    {
        return faceComplete ? ReapplyAction::complete :
            facePending ? ReapplyAction::wait : ReapplyAction::applyFace;
    }

    struct Baseline final
    {
        std::uint32_t actor{};
        std::uint32_t base{};
        std::string node;
        bool female{};
        Paths visible;
        Paths saved;
        // Keep both the last observed key and the next in-flight key. A save
        // can happen on either side of a Papyrus dispatch.
        Paths owned;
        Paths pending;
        std::uint8_t touched{};
    };

    [[nodiscard]] inline std::string PathIdentity(std::string_view path)
    {
        std::string result;
        result.reserve(path.size());
        for (const auto c : path) result.push_back(skin_texture::ownership::Normalize(c));
        if (result.starts_with("data\\")) result.erase(0, 5);
        if (result.starts_with("textures\\")) result.erase(0, 9);
        return result;
    }

    // A NiSourceTexture name can include Data/Textures, unlike the TXST
    // string returned by NiOverride. Engine fallback names are NOT filenames.
    [[nodiscard]] inline std::string ReloadableTexturePath(std::string_view name)
    {
        auto relative = PathIdentity(name);
        if (!relative.ends_with(".dds") || relative.find_first_of(":|\0", 0, 3) != std::string::npos) return {};
        for (std::size_t begin{}; begin <= relative.size();) {
            const auto end = relative.find('\\', begin);
            const auto part = std::string_view(relative).substr(begin,
                end == std::string::npos ? relative.size() - begin : end - begin);
            if (part.empty() || part == "." || part == "..") return {};
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        return "textures\\" + relative;
    }

    [[nodiscard]] inline std::string CaptureVisiblePath(std::string_view property, std::string_view textureName)
    {
        // The game can assign complexion directly to detailTexture while its
        // TXST path stays empty. Prefer the actual, reloadable texture name.
        auto actual = ReloadableTexturePath(textureName);
        return actual.empty() ? std::string(property) : actual;
    }

    [[nodiscard]] inline bool Owns(const std::string_view current,
        const std::string_view written) noexcept
    {
        // RaceMenu interns strings case-insensitively. A returned string can
        // keep an earlier spelling; that is not a new provider taking over.
        // Compare the complete resource identity, never just the filename.
        // No allocation: ownership checks also run during Default cleanup.
        const auto trim = [](std::string_view value) {
            const auto prefix = [](std::string_view value, std::string_view key) {
                return value.size() >= key.size() &&
                    skin_texture::ownership::ContainsNormalized(value.substr(0, key.size()), key);
            };
            if (prefix(value, "data\\")) value.remove_prefix(5);
            if (prefix(value, "textures\\")) value.remove_prefix(9);
            return value;
        };
        const auto left = trim(current);
        const auto right = trim(written);
        return !right.empty() && left.size() == right.size() &&
            skin_texture::ownership::ContainsNormalized(left, right);
    }

    [[nodiscard]] inline std::string_view OwnedValue(const Baseline& value,
        std::size_t index, std::string_view current) noexcept
    {
        return Owns(current, value.pending[index]) ? std::string_view(value.pending[index]) :
            std::string_view(value.owned[index]);
    }

    template<class WriteLive, class ReadInterned, class SaveInterned>
    bool WriteLegacyString(const std::string& path, bool persistent,
        WriteLive write, ReadInterned read, SaveInterned save)
    {
        if (path.empty()) return false;
        write();
        if (!persistent) return true;
        if (!Owns(read(), path)) return false;
        save();
        return true;
    }

    [[nodiscard]] inline std::string RestoreVisible(const std::string& currentSaved,
        const std::string& owned, const std::string& saved, const std::string& visible)
    {
        // A later provider owns the key. Do not erase or paint over it on Default.
        if (!currentSaved.empty() && !Owns(currentSaved, owned)) return currentSaved;
        return saved.empty() ? visible : saved;
    }
}
