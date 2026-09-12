#pragma once

#include "BodyChangeNG/SFSRenderedOutfitAPI.h"
#include "BodyChangeNG/OutfitRefitEvaluation.h"
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace bcn::rendered_outfit
{
    namespace abi = sfs::rendered_outfit_api;
    enum class Route { worn, rendered, defer, invalidActor };

    struct Stamp final
    {
        std::uint64_t epoch{}, revision{}, scene{};
        abi::Status status{};
        bool operator==(const Stamp&) const = default;
    };
    inline Stamp VersionOf(const abi::Snapshot& snapshot)
    {
        return {snapshot.epoch, snapshot.revision, snapshot.sceneGeneration, snapshot.status};
    }
    inline Stamp VersionOf(const abi::Changed& change)
    {
        return {change.epoch, change.revision, change.sceneGeneration, change.status};
    }
    struct View final
    {
        Route route{Route::defer};
        abi::Snapshot snapshot{};
        std::span<const abi::Item> items;
    };

    // One reusable caller-owned buffer, not one allocation per tracked NPC.
    // The returned span lasts only until the next Read; never store it in jobs.
    class Reader final
    {
    public:
        View Read(abi::Query query, const std::uint32_t actor)
        {
            if (!query) return {Route::worn};
            if (!actor) return {Route::invalidActor};
            if (items_.size() > 256U) std::vector<abi::Item>{}.swap(items_);
            if (items_.empty()) items_.resize(16U);
            View result;
            // Retry changed capacity, never accept a partial array. No outfit
            // count cap: unusually large valid outfits are not truncated.
            for (unsigned attempt{}; attempt < 3U; ++attempt) {
                result.snapshot = {};
                result.snapshot.structSize = sizeof(abi::Snapshot);
                const auto status = query(actor, &result.snapshot, items_.data(),
                    static_cast<std::uint32_t>(items_.size()), sizeof(abi::Item));
                const auto& header = result.snapshot;
                if (header.structSize < sizeof(abi::Snapshot) || header.apiVersion != abi::kVersion ||
                    header.actorFormID != actor || header.status != status) return result;
                switch (status) {
                case abi::Status::NotManaged: result.route = Route::worn; return result;
                case abi::Status::InvalidActor: result.route = Route::invalidActor; return result;
                case abi::Status::Ready:
                    if (header.requiredCount > items_.size()) return result;
                    result.route = Route::rendered;
                    result.items = {items_.data(), header.requiredCount};
                    return result;
                case abi::Status::BufferTooSmall:
                    if (header.requiredCount <= items_.size()) return result;
                    items_.resize(header.requiredCount);
                    break;
                default: return result; // NotReady / wrong thread / unknown ABI error != naked
                }
            }
            return result;
        }
    private:
        std::vector<abi::Item> items_;
    };

    inline std::optional<abi::Changed> DecodeChange(const void* data, std::size_t size)
    {
        if (!data || size < sizeof(abi::Changed)) return {};
        abi::Changed result;
        std::memcpy(&result, data, sizeof(result));
        if (result.structSize < sizeof(result) || result.structSize > size ||
            result.apiVersion != abi::kVersion) return {};
        return result;
    }

    inline bool CanApply(const std::optional<Stamp>& planned, const View& latest)
    {
        return planned && (latest.route == Route::worn || latest.route == Route::rendered) &&
            *planned == VersionOf(latest.snapshot);
    }

    // Preserve BCNG/ORefit's 32 -> 46 -> 56 mapping precedence. API order is
    // only an identity sort and must not make a ring or a lower FormID win.
    inline constexpr std::uint32_t kChestSlots[]{1U << 2U, 1U << 16U, 1U << 26U};
    inline unsigned ChestPriority(const std::uint32_t visibleSlots)
    {
        for (unsigned i{}; i < 3U; ++i) if (visibleSlots & kChestSlots[i]) return i;
        return 3U;
    }
    inline std::uint32_t RuleForm(const abi::Item& item)
    {
        return item.originalFormID && !(item.flags & abi::OriginalUnknown) ?
            item.originalFormID : item.formID;
    }

    struct OutfitDecision final
    {
        bool eligible{}, forced{};
        std::string preset;
    };
    template <class Rules, class Mapping, class Resolve>
    OutfitDecision EvaluateVisible(std::span<const abi::Item> items,
        const Rules& rules, const Mapping& mapping, Resolve&& resolve)
    {
        OutfitDecision result;
        unsigned bestPriority = 3U;
        std::uint32_t bestForm = UINT32_MAX;
        for (const auto& item : items) {
            const auto armor = resolve(item);
            if (!armor) continue; // one unresolved addition cannot block other clothing
            const auto priority = ChestPriority(item.visibleSlots);
            result.eligible |= priority < 3U && !outfit_refit_evaluation::IsBlacklisted(*armor, rules);
            result.forced |= outfit_refit_evaluation::IsForced(*armor, rules);
            if (priority < 3U && (priority < bestPriority ||
                    (priority == bestPriority && armor->formID < bestForm))) {
                if (const auto found = mapping.find(std::string(armor->name)); found != mapping.end()) {
                    result.preset = found->second;
                    bestPriority = priority;
                    bestForm = armor->formID;
                }
            }
        }
        return result;
    }
}
