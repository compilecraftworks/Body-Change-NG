#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace bcn
{
    enum class SkinSex : std::uint8_t
    {
        female,
        male
    };

    // RaceMenu's public skin-override API addresses texture resources by
    // shader texture index. Keeping the index explicit avoids assuming a
    // particular skin mod's file names or directory layout.
    struct SkinTextureLayer final
    {
        std::uint8_t shaderTextureIndex{};
        std::string path;
    };

    struct SkinProfile final
    {
        std::string id;
        std::string name;
        SkinSex sex{ SkinSex::female };
        std::vector<SkinTextureLayer> body;
        std::vector<SkinTextureLayer> hands;
        std::vector<SkinTextureLayer> feet;
        // FaceGen is addressed through the current actor's face-head node,
        // rather than by replacing the actor base, FaceGen NIF, or FaceTint
        // record.  The optional vampire layers take precedence for vampire
        // races and otherwise fall back to `face`.
        std::vector<SkinTextureLayer> face;
        std::vector<SkinTextureLayer> vampireFace;
        // Face detail DDS files all target texture slot 3. A pack may contain
        // several alternatives (freckles, rough, blank); runtime application
        // chooses the one matching the actor's current FaceGen detail name
        // instead of exposing each DDS as a separate skin row.
        std::vector<SkinTextureLayer> faceDetails;
        std::filesystem::path source;
    };

    class SkinProfiles final
    {
    public:
        static SkinProfiles& Get();

        // Loads BodySkin/<skin name>/profile.json and auto-detects the common
        // BodySkin/<skin name>/Textures/... layout when no profile JSON exists.
        // A usable skin always contains matching body, hands, and FaceGen face
        // textures for one sex. Missing feet fall back to the body layers,
        // which is how legacy BodyChange texture sets are authored.
        // Texture paths are game-relative and may point to any installed mod
        // folder, so player and NPC rule selection remain independent.
        void Refresh();
        [[nodiscard]] static std::vector<SkinProfile> ScanDirectory(const std::filesystem::path& a_root);
        [[nodiscard]] std::vector<SkinProfile> Snapshot() const;
        [[nodiscard]] std::optional<SkinProfile> Find(std::string_view a_id) const;

        [[nodiscard]] static std::filesystem::path RootPath();

    private:
        mutable std::mutex lock_;
        std::vector<SkinProfile> profiles_;
    };
}
