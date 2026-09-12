#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace bcn::native_addon
{
    enum class Channel : std::uint8_t { maleGenitals, futanari };
    using Paths = std::array<std::string, 8>;

    // BGSTextureSet's verified factory returns one construction reference.
    // Acquire a scoped intrusive owner FIRST, then relinquish that factory
    // reference. Do not apply this helper to refcount-zero NiObject factories.
    template<class Texture, class Reference>
    [[nodiscard]] Reference AdoptFactoryTexture(Texture* created)
    {
        Reference reference(created);
        if (created) created->DecRefCount();
        return reference;
    }

    [[nodiscard]] inline Paths ResolvePaths(Paths baseline, const Paths& overrides)
    {
        for (std::size_t i{}; i < baseline.size(); ++i)
            if (!overrides[i].empty()) baseline[i] = overrides[i];
        return baseline;
    }

    struct Target final
    {
        std::uint32_t armor{};
        std::uint32_t addon{};
        std::vector<std::string> nodes;
        bool operator==(const Target&) const = default;
    };

    struct Selection final
    {
        Channel channel{};
        std::vector<Target> targets;
        Paths overrides;
        bool operator==(const Selection&) const = default;
    };

    struct BaselineIdentity final
    {
        std::uint32_t actor{};
        std::uint32_t armor{};
        std::uint32_t addon{};
        Channel channel{};
        std::string node;
        bool operator==(const BaselineIdentity&) const = default;
    };

    struct BaselineIdentityHash final
    {
        [[nodiscard]] std::size_t operator()(const BaselineIdentity& value) const noexcept
        {
            auto hash = std::hash<std::uint32_t>{}(value.actor);
            const auto combine = [&hash](const std::size_t next) {
                hash ^= next + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
            };
            combine(std::hash<std::uint32_t>{}(value.armor));
            combine(std::hash<std::uint32_t>{}(value.addon));
            combine(std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(value.channel)));
            combine(std::hash<std::string>{}(value.node));
            return hash;
        }
    };

    // Baselines are value-only DDS path snapshots. Never retain a live actor,
    // geometry, material or TXST pointer across a 3D rebuild.
    class Baselines final
    {
    public:
        bool Put(BaselineIdentity identity, Paths paths)
        {
            std::scoped_lock lock(mutex_);
            const auto found = values_.find(identity);
            if (found != values_.end() && found->second == paths) return false;
            values_.insert_or_assign(std::move(identity), std::move(paths));
            return true;
        }
        [[nodiscard]] bool Get(const BaselineIdentity& identity, Paths& paths) const
        {
            std::scoped_lock lock(mutex_);
            const auto found = values_.find(identity);
            if (found == values_.end()) return false;
            paths = found->second;
            return true;
        }
        [[nodiscard]] std::string Diffuse(const BaselineIdentity& identity) const
        {
            Paths paths;
            return Get(identity, paths) ? paths[0] : std::string{};
        }
        void Retain(std::uint32_t actor, Channel channel,
            const std::vector<Target>& targets)
        {
            std::scoped_lock lock(mutex_);
            std::erase_if(values_, [&](const auto& item) {
                const auto& key = item.first;
                if (key.actor != actor || key.channel != channel) return false;
                return std::ranges::none_of(targets, [&](const Target& target) {
                    return target.armor == key.armor && target.addon == key.addon &&
                        std::ranges::find(target.nodes, key.node) != target.nodes.end();
                });
            });
        }
        void Forget(std::uint32_t actor)
        {
            std::scoped_lock lock(mutex_);
            std::erase_if(values_, [actor](const auto& item) {
                return item.first.actor == actor;
            });
        }
        void Reset()
        {
            std::scoped_lock lock(mutex_);
            values_.clear();
        }
    private:
        mutable std::mutex mutex_;
        std::unordered_map<BaselineIdentity, Paths, BaselineIdentityHash> values_;
    };

    // Only immutable value snapshots cross the native callback boundary.
    // No Actor, Biped, Geometry, Material or TESForm pointer is retained here.
    class Selections final
    {
    public:
        bool Publish(std::uint32_t actor, Selection selection)
        {
            std::scoped_lock lock(mutex_);
            const auto found = selections_.find(actor);
            if (found != selections_.end() && *found->second == selection) return false;
            auto next = std::make_shared<const Selection>(std::move(selection));
            selections_.insert_or_assign(actor, std::move(next));
            return true;
        }
        [[nodiscard]] std::shared_ptr<const Selection> Get(std::uint32_t actor) const
        {
            std::scoped_lock lock(mutex_);
            const auto found = selections_.find(actor);
            return found == selections_.end() ? nullptr : found->second;
        }
        bool Forget(std::uint32_t actor)
        {
            std::scoped_lock lock(mutex_);
            return selections_.erase(actor) != 0;
        }
        void Reset()
        {
            std::scoped_lock lock(mutex_);
            selections_.clear();
        }
    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint32_t, std::shared_ptr<const Selection>> selections_;
    };

    [[nodiscard]] constexpr bool AcceptSlot(std::int32_t index, std::uint32_t mask) noexcept
    {
        return index == 22 && (mask & (1U << 22U)) != 0U; // biped index 22 = equipment slot 52
    }

    [[nodiscard]] constexpr bool AcceptTargetView(bool hasSlot52, bool hasObject,
        bool hasGeometryNodes) noexcept
    { return hasSlot52 && hasObject && hasGeometryNodes; }

    // Exact SE/AE visitor ABI confirmed in the two captured game versions.
    // The engine reads a slot index, BIPOBJECT array and actor handle.
    struct VisitorContext final
    {
        std::int32_t index{};
        std::uint32_t pad{};
        const std::byte* parts{};
        std::uint32_t actorHandle{};
        std::uint32_t pad14{};
    };
    static_assert(offsetof(VisitorContext, parts) == 8);
    static_assert(offsetof(VisitorContext, actorHandle) == 0x10);
    static_assert(sizeof(VisitorContext) == 0x18);

    // A borrowed context for ONE synchronous native visitor call. It never
    // overwrites the engine's current or buffered BIPOBJECT and cannot leave
    // a private TXST pointer in either. The original visitor only reads these
    // fields; it does not own/destroy this byte copy. Slot is remapped to zero.
    class ScopedSupply final
    {
    public:
        ScopedSupply(const VisitorContext& source, const void* texture) : context(source)
        {
            std::memcpy(part.data(), source.parts + source.index * part.size(), part.size());
            std::memcpy(part.data() + 0x18, &texture, sizeof(texture));
            context.index = 0;
            context.parts = part.data();
        }
        ScopedSupply(const ScopedSupply&) = delete;
        ScopedSupply& operator=(const ScopedSupply&) = delete;
        alignas(8) std::array<std::byte, 0x78> part{};
        VisitorContext context;
    };
}
