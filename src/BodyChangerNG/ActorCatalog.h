#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

    class ActorCatalog final
    {
    public:
        static ActorCatalog& Get();

        // The crosshair actor is opt-in. Normal refreshes enumerate every
        // currently loaded NPC with valid 3D, ordered by distance, so the UI
        // can search distant loaded actors without unsafe unloaded references.
        void Refresh(bool a_includeCrosshairActor = false);
        [[nodiscard]] std::vector<ActorEntry> Snapshot() const;
        [[nodiscard]] RE::Actor* Resolve(std::uint32_t a_formID) const;
        [[nodiscard]] std::uint32_t CrosshairActorFormID() const noexcept;

    private:
        std::vector<ActorEntry> entries_;
        std::uint32_t crosshairActorFormID_{};
    };
}
