#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace RE
{
    class Actor;
}

namespace bcn
{
    struct ActorEntry final
    {
        std::uint32_t formID{};
        std::string name;
        bool female{};
        bool player{};
    };

    struct ActorSearchResult final
    {
        std::string query;
        std::shared_ptr<const std::vector<ActorEntry>> entries;
        bool running{};
        bool failed{};
    };

    class ActorCatalog final
    {
    public:
        static ActorCatalog& Get();

        // The crosshair actor is opt-in. Normal refreshes list up to 32 valid,
        // living NPCs within 4096 units, ordered by distance. Hostility does not
        // affect selection; disabled/unloaded references remain excluded.
        void Refresh(bool a_includeCrosshairActor = false);
        [[nodiscard]] std::vector<ActorEntry> Snapshot() const;
        [[nodiscard]] RE::Actor* Resolve(std::uint32_t a_formID) const;
        [[nodiscard]] std::uint32_t CrosshairActorFormID() const noexcept;
        // Explicit, distance-independent search of existing actor references.
        // Never instantiate an NPC base, load a cell, or retain actor pointers.
        void Search(std::string query);
        void CancelSearch();
        [[nodiscard]] ActorSearchResult SearchSnapshot() const;

    private:
        std::vector<ActorEntry> entries_;
        std::uint32_t crosshairActorFormID_{};
    };
}
