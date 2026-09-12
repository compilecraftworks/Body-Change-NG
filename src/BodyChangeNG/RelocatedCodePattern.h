#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

namespace bcn::native_addon
{
    // Imported OS calls are identified by their resolved IAT value, not an
    // Address Library data ID that is absent in some GOG libraries.
    inline constexpr std::uint64_t kSleepImport = UINT64_MAX;

    [[nodiscard]] inline std::optional<std::uint32_t> FindUniqueImport(
        std::span<const std::uint8_t> iat, std::uint32_t iatRva, std::uint64_t function)
    {
        if (!function || iat.empty() || iat.size() % 8 || iat.size() > 0x100000 ||
            iat.size() > UINT32_MAX - iatRva) return {};
        std::optional<std::uint32_t> found;
        for (std::size_t i{}; i < iat.size(); i += 8) {
            std::uint64_t pointer{};
            std::memcpy(&pointer, iat.data()+i, 8);
            if (pointer != function) continue;
            if (found) return {};
            found = iatRva + static_cast<std::uint32_t>(i);
        }
        return found;
    }

    struct RelativeOperand final {
        std::uint32_t offset{}, instructionEnd{};
        std::uint64_t targetID{};
        std::uint32_t addend{};
    };
    struct RelocatedPattern final {
        std::uint64_t functionID{};
        std::string_view bytes;
        std::span<const RelativeOperand> operands;
    };

    [[nodiscard]] inline std::optional<std::uint32_t> RelativeTarget(
        std::span<const std::uint8_t> bytes, std::size_t operand,
        std::uint32_t instructionEndRva)
    {
        if (operand > bytes.size() || bytes.size() - operand < 4) return {};
        std::int32_t displacement{};
        std::memcpy(&displacement, bytes.data() + operand, 4);
        const auto target = std::int64_t{ instructionEndRva } + displacement;
        if (target < 0 || target > UINT32_MAX) return {};
        return static_cast<std::uint32_t>(target);
    }

    // No opcode/field/constant wildcard. A displacement is accepted only when
    // its decoded target is the same runtime Address Library ID and addend.
    template<class Resolve>
    [[nodiscard]] bool MatchRelocatedCode(const RelocatedPattern& pattern,
        std::span<const std::uint8_t> actual, std::uint32_t functionRva, Resolve resolve)
    {
        if (pattern.bytes.empty() || actual.size() < pattern.bytes.size() ||
            pattern.bytes.size() > UINT32_MAX - functionRva) return false;
        std::size_t compared{};
        for (const auto& operand : pattern.operands) {
            if (operand.offset < compared || operand.offset > pattern.bytes.size() ||
                pattern.bytes.size() - operand.offset < 4 ||
                operand.instructionEnd < operand.offset + 4 ||
                operand.instructionEnd > pattern.bytes.size()) return false;
            if (std::memcmp(actual.data() + compared, pattern.bytes.data() + compared,
                    operand.offset - compared) != 0) return false;
            const auto expected = resolve(operand.targetID);
            const auto target = RelativeTarget(actual, operand.offset,
                functionRva + operand.instructionEnd);
            if (!expected || *expected > UINT32_MAX - operand.addend || !target ||
                *target != *expected + operand.addend) return false;
            compared = operand.offset + 4;
        }
        return std::memcmp(actual.data() + compared, pattern.bytes.data() + compared,
            pattern.bytes.size() - compared) == 0;
    }

    // Search only an Address-Library-identified, unwind-bounded caller. The
    // preceding complete instruction sequence and original branch target must
    // match uniquely. This does not search the whole executable for E8 bytes.
    [[nodiscard]] inline std::optional<std::uint32_t> FindVisitorBranch(
        std::span<const std::uint8_t> caller, std::uint32_t callerRva,
        std::string_view prefix, std::uint8_t opcode, std::uint32_t visitorRva)
    {
        if (prefix.empty() || caller.size() > 0x4000 || caller.size() < prefix.size() + 5 ||
            caller.size() > UINT32_MAX - callerRva) return {};
        std::optional<std::uint32_t> found;
        for (std::size_t i{}; i <= caller.size() - prefix.size() - 5; ++i) {
            const auto site = i + prefix.size();
            if (caller[site] != opcode ||
                std::memcmp(caller.data() + i, prefix.data(), prefix.size()) != 0) continue;
            if (RelativeTarget(caller, site + 1, callerRva + static_cast<std::uint32_t>(site) + 5) != visitorRva) continue;
            if (found) return {}; // Ambiguous means no patch, not first-match wins.
            found = callerRva + static_cast<std::uint32_t>(site);
        }
        return found;
    }

    inline constexpr std::string_view directVisitorPrefix{
        "\x49\x8B\x04\x24\x49\x8B\xCC\xFF\x50\x38\x48\x85\xC0\x74\x0E\x48\x8B\xD0\x48\x8D\x4D\xDF" };
    inline constexpr std::string_view seRecursiveVisitorPrefix{
        "\x48\x8B\x01\xFF\x50\x38\x48\x85\xC0\x74\x12\x48\x8B\xD0\x48\x8B\xCE\x48\x83\xC4\x20\x5F\x5E\x5B" };
    inline constexpr std::string_view aeRecursiveVisitorPrefix{
        "\x48\x8B\x01\xFF\x50\x38\x48\x85\xC0\x74\x13\x48\x8B\xD0\x49\x8B\xCF\x48\x83\xC4\x20\x41\x5F\x5F\x5B" };
}
