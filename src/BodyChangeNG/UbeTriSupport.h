#pragma once

#include "BodyChangeNG/UbeGenitalRandomization.h"

#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bcn::ube_anatomy
{
    // Read-only BodySlide body-TRI metadata, not RaceMenu's private interface.
    // Reader supplies exact Read(void*, uint32), Skip(uint32), Remaining().
    // No vertex buffers, counts from the file never control allocation.
    // Formats: RaceMenu MorphCache::CacheFile / BodySlide TriFile.cpp.
    template <class Reader>
    [[nodiscard]] std::optional<std::uint8_t> ReadExtraSupport(Reader& reader)
    {
        static_assert(std::endian::native == std::endian::little);
        if (reader.Remaining() > 128U * 1024U * 1024U) return std::nullopt;
        const auto number = [&](auto& value) { return reader.Read(&value, sizeof(value)); };
        std::array<char, 4> magic{};
        if (!reader.Read(magic.data(), 4U)) return std::nullopt;
        const bool packed = magic == std::array{'P', 'I', 'R', 'T'};
        if (!packed && magic != std::array{'\0', 'I', 'R', 'T'}) return std::nullopt;
        const auto count = [&](std::uint32_t& value) {
            if (!packed) return number(value);
            std::uint16_t shortCount{};
            if (!number(shortCount)) return false;
            value = shortCount;
            return true;
        };
        std::array<char, 255> name{};
        const auto readName = [&](std::string_view& result) {
            std::uint8_t length{};
            if (!number(length) || !reader.Read(name.data(), length)) return false;
            result = std::string_view(name.data(), length);
            return !result.empty() && result.find('\0') == std::string_view::npos;
        };
        std::uint8_t support{};
        std::uint32_t totalMorphs{};
        // UV metadata is validated, but can never enable a position morph.
        for (unsigned section{}; section < (packed ? 2U : 1U); ++section) {
            if (section == 1U && reader.Remaining() == 0U) break;
            std::uint32_t shapes{};
            if (!count(shapes) || shapes > 1024U) return std::nullopt;
            for (std::uint32_t s{}; s < shapes; ++s) {
                std::string_view shape;
                if (!readName(shape)) return std::nullopt;
                std::uint32_t unusedBlockSize{};
                if (!packed && !number(unusedBlockSize)) return std::nullopt;
                std::uint32_t morphs{};
                if (!count(morphs) || morphs > 4096U || totalMorphs + morphs > 65536U) return std::nullopt;
                totalMorphs += morphs;
                for (std::uint32_t m{}; m < morphs; ++m) {
                    std::string_view morph;
                    if (!readName(morph)) return std::nullopt;
                    const auto bit = section == 0U ? ube_genital::ExtraBit(morph) : 0U;
                    if (!packed && !number(unusedBlockSize)) return std::nullopt;
                    float multiplier = 1.F;
                    if (packed && (!number(multiplier) || !std::isfinite(multiplier))) return std::nullopt;
                    std::uint32_t vertices{};
                    if (!count(vertices) || vertices > 65535U) return std::nullopt;
                    const auto stride = packed ? (section == 0U ? 8U : 6U) : 16U;
                    if (vertices * stride > reader.Remaining()) return std::nullopt;
                    if (bit == 0U || multiplier == 0.F) {
                        if (!reader.Skip(vertices * stride)) return std::nullopt;
                        continue;
                    }
                    bool nonzero{};
                    for (std::uint32_t v{}; v < vertices;) {
                        const auto batch = (std::min)(vertices - v, 256U);
                        if (packed) {
                            struct Vertex { std::uint16_t index; std::array<std::int16_t, 3> delta; };
                            static_assert(sizeof(Vertex) == 8U);
                            std::array<Vertex, 256> buffer{};
                            if (!reader.Read(buffer.data(), batch * sizeof(Vertex))) return std::nullopt;
                            for (std::uint32_t i{}; i < batch; ++i) {
                                const auto& delta = buffer[i].delta;
                                nonzero |= delta[0] != 0 || delta[1] != 0 || delta[2] != 0;
                            }
                        } else {
                            struct Vertex { std::uint32_t index; std::array<float, 3> delta; };
                            static_assert(sizeof(Vertex) == 16U);
                            std::array<Vertex, 256> buffer{};
                            if (!reader.Read(buffer.data(), batch * sizeof(Vertex))) return std::nullopt;
                            for (std::uint32_t i{}; i < batch; ++i) {
                                if (buffer[i].index > 65535U) return std::nullopt;
                                for (const auto value : buffer[i].delta) {
                                    if (!std::isfinite(value)) return std::nullopt;
                                    nonzero |= value != 0.F;
                                }
                            }
                        }
                        v += batch;
                    }
                    if (nonzero) support |= static_cast<std::uint8_t>(bit);
                }
            }
        }
        if (reader.Remaining() != 0U) return std::nullopt;
        return support;
    }
}
