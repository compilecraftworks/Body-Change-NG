#pragma once

#include "BodyChangeNG/ActorStateSerialization.h"
#include <span>
#include <unordered_map>

namespace bcn::actor_serialization
{
    inline constexpr std::uint32_t kMaxRecordActors = 16384U;
    inline constexpr std::uint32_t kMaxRecordStrings = 131072U;
    struct EncodedActor final {
        SerializedActorStateV5 state;
        std::vector<SerializedOverlayItemV5> overlays;
    };

    // Multiple ordinary ASTR v6 records are already understood by the loader.
    // Bound each record, not the entire save; never truncate actor choices.
    template<class WriteRecord>
    bool WriteActorRecords(std::span<const ActorState> states, WriteRecord writeRecord)
    {
        std::vector<std::string> strings{ std::string{} };
        std::unordered_map<std::string, std::uint32_t> indices{ { {}, 0U } };
        std::vector<EncodedActor> actors;
        const auto flush = [&] {
            if (!writeRecord(strings, actors)) return false;
            actors.clear();
            strings = { std::string{} };
            indices.clear();
            indices.emplace(std::string{}, 0U);
            return true;
        };
        for (const auto& state : states) {
            std::size_t stringBudget = 7U; // base identity + feature IDs
            for (const auto& area : state.overlay.areas) stringBudget += 2U * area.items.size();
            if (stringBudget >= kMaxRecordStrings) return false;
            if (!actors.empty() && (actors.size() >= kMaxRecordActors ||
                    strings.size() + stringBudget > kMaxRecordStrings)) {
                if (!flush()) return false;
            }
            const auto indexFor = [&](const std::string& value) {
                const auto [entry, inserted] = indices.try_emplace(value, static_cast<std::uint32_t>(strings.size()));
                if (inserted) strings.push_back(value);
                return entry->second;
            };
            auto& actor = actors.emplace_back();
            actor.state = EncodeV6(state, indexFor, actor.overlays);
        }
        return actors.empty() ? (states.empty() ? flush() : true) : flush();
    }
}
