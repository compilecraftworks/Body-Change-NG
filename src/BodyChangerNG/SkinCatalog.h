#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace bcn
{
    // A legacy BodyChange preset is a matched entry across its three public
    // FormLists.  It remains available for a future explicit importer only;
    // the normal Body Changer NG path uses shared, per-reference texture
    // profiles and never changes an ActorBase or a Skin Armor.
    struct SkinEntry final
    {
        std::string id;
        std::string name;
        std::uint32_t skinFormID{};
        std::uint32_t faceTextureSetFormID{};
        std::uint32_t headPartFormID{};
    };

    enum class SkinApplyResult : std::uint8_t
    {
        queued,
        unavailable,
        missingEntry,
        missingForm,
        noTaskInterface
    };

    class SkinCatalog final
    {
    public:
        static SkinCatalog& Get();

        // Reads BodyChange.esp's public FormLists when that optional mod is
        // installed. It never changes BodyChange's plugin or scripts.
        void Refresh();
        [[nodiscard]] std::vector<SkinEntry> Snapshot() const;
        [[nodiscard]] SkinApplyResult QueueApplyToPlayer(std::string a_entryId) const;

    private:
        mutable std::mutex lock_;
        std::vector<SkinEntry> entries_;
    };
}
