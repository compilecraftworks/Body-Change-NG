#pragma once

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/SkinLayout.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace RE
{
    class TESNPC;
}

namespace bcn
{
    // Humanoid face normals can have race-specific variants inside one skin
    // pack. This is deliberately separate from SkinRace: the latter protects
    // incompatible human/Argonian/Khajiit UV layouts, while this enum selects
    // an optional face layer without splitting one pack into many UI rows.
    enum class HumanoidSkinRace : std::uint8_t
    {
        generic,
        nord,
        breton,
        darkElf,
        highElf,
        imperial,
        orc,
        redguard,
        woodElf,
        count
    };

    // RaceMenu's public skin-override API addresses texture resources by
    // shader texture index. Keeping the index explicit avoids assuming a
    // particular skin mod's file names or directory layout.
    struct SkinTextureLayer final
    {
        std::uint8_t shaderTextureIndex{};
        std::string path;
    };

    struct MaleGenitalTextureVariant final
    {
        // Matches the SOS ArmorAddon model directory (for example
        // "VectorPlexus Regular") so several installed addon atlases can
        // coexist in one BodySkin pack without one overwriting another.
        std::string addonDirectory;
        std::vector<SkinTextureLayer> humanoid;
        std::vector<SkinTextureLayer> argonian;
        std::vector<SkinTextureLayer> khajiit;
        std::vector<SkinTextureLayer> elder;
    };

    struct SkinProfile final
    {
        std::string id;
        std::string name;
        SkinSex sex{ SkinSex::female };
        SkinRace race{ SkinRace::humanoid };
        // Catalog identity is only Legacy vs UBE for humanoid skins. The
        // actor's runtime BodyFamily is deliberately not stored on the pack
        // and is consulted only when compatibility/routing is needed.
        SkinLayout layout{ SkinLayout::unknown };
        std::vector<SkinTextureLayer> body;
        // Optional family-specific genital/anal atlases. CBBE 3BA shares
        // femalebody_etc_v2_1 across its vagina and anus geometries. BHUNP
        // shares BakaUNP/VaginalAnalCanal2 across vagina, anus, and canal.
        // Neither atlas may fall back to the regular body or another part.
        std::vector<SkinTextureLayer> cbbeGenitalAnal;
        std::vector<SkinTextureLayer> unpGenitalAnal;
        // SOS uses a separate slot-52 ArmorAddon. Variants are selected from
        // that addon's immutable model path; missing race/channel files leave
        // the underlying genital material untouched.
        std::vector<MaleGenitalTextureVariant> maleGenitals;
        std::vector<SkinTextureLayer> hands;
        std::vector<SkinTextureLayer> feet;
        // FaceGen is addressed through the current actor's face-head node,
        // rather than by replacing the actor base, FaceGen NIF, or FaceTint
        // record.  The optional vampire layers take precedence for vampire
        // races and otherwise fall back to `face`.
        std::vector<SkinTextureLayer> face;
        std::vector<SkinTextureLayer> vampireFace;
        // Optional original-path variants from the same conventional female
        // pack. At runtime only matching elder/race layers replace the same
        // material channel; missing files fall back to the base pack layer or
        // ultimately leave the actor's original texture untouched.
        std::vector<SkinTextureLayer> elderBody;
        std::vector<SkinTextureLayer> elderHands;
        std::vector<SkinTextureLayer> elderFace;
        std::array<std::vector<SkinTextureLayer>,
            static_cast<std::size_t>(HumanoidSkinRace::count)> raceFace;
        // Face detail DDS files all target texture slot 3. A pack may contain
        // several alternatives (freckles, rough, blank); runtime application
        // chooses the one matching the actor's current FaceGen detail name
        // instead of exposing each DDS as a separate skin row.
        std::vector<SkinTextureLayer> faceDetails;
        std::filesystem::path source;
        std::uint64_t contentHash{};
    };

    enum class FutanariSkinType : std::uint8_t
    {
        ubeTrx,
        cbbeTrx,
        erf
    };

    struct FutanariSkinProfile final
    {
        std::string id;
        std::string name;
        FutanariSkinType type{ FutanariSkinType::cbbeTrx };
        std::vector<SkinTextureLayer> layers;
        std::filesystem::path source;
        std::uint64_t contentHash{};
    };

    [[nodiscard]] std::string FutanariSkinTypeLabel(FutanariSkinType a_type);

    [[nodiscard]] constexpr bool FutanariSkinTypeMatchesActor(
        const FutanariSkinType type, const body_family::Mask actorFamily) noexcept
    {
        const auto ube = body_family::Bit(body_family::Family::ube);
        const auto legacy = body_family::kFemaleFamilies & ~ube;
        const auto hasUbe = (actorFamily & ube) != 0U;
        const auto hasLegacy = (actorFamily & legacy) != 0U;
        if (hasUbe && !hasLegacy) return type == FutanariSkinType::ubeTrx;
        if (hasLegacy && !hasUbe) return type != FutanariSkinType::ubeTrx;
        return true;
    }

    [[nodiscard]] constexpr body_family::Mask StandardSkinFamilies(const SkinSex sex) noexcept
    {
        return sex == SkinSex::female ?
            body_family::Bit(body_family::Family::femaleVanilla) |
                body_family::Bit(body_family::Family::cbbe) |
                body_family::Bit(body_family::Family::unp) :
            body_family::kMaleFamilies;
    }

    [[nodiscard]] constexpr bool SkinLayoutMatchesActor(
        const SkinLayout layout, const body_family::Mask actorFamily) noexcept
    {
        return ResolveRuntimeSkinUvLayout(layout, actorFamily) != SkinUvLayout::unknown;
    }

    [[nodiscard]] constexpr bool SkinRaceMatchesActor(
        const SkinRace skinRace, const SkinRace actorRace) noexcept
    {
        return skinRace == actorRace;
    }

    [[nodiscard]] constexpr SkinCompatibility SkinProfileCompatibility(
        const SkinProfile& profile, const SkinSex actorSex,
        const SkinRace actorRace, const body_family::Mask actorFamilies) noexcept
    {
        return EvaluateSkinCompatibility(profile.layout, profile.sex,
            profile.race, actorSex, actorRace, actorFamilies);
    }

    [[nodiscard]] std::string SkinFamilyLabel(SkinLayout a_layout, SkinSex a_sex);
    [[nodiscard]] std::string SkinRaceLabel(SkinRace a_race);
    [[nodiscard]] SkinRace SkinRaceFromEditorID(std::string_view a_editorID);
    [[nodiscard]] SkinRace ResolveActorSkinRace(const RE::Actor* a_actor);
    [[nodiscard]] HumanoidSkinRace HumanoidSkinRaceFromEditorID(std::string_view a_editorID);
    [[nodiscard]] bool IsElderSkinVariant(
        std::string_view a_raceEditorID, std::string_view a_voiceEditorID);
    [[nodiscard]] bool IsElderActor(RE::TESNPC* a_base);

    class SkinProfiles final
    {
    public:
        static SkinProfiles& Get();

        // Auto-detects the common BodySkin/<skin name>/Textures/... layout.
        // Skin-pack profile JSON is deliberately unsupported: catalog identity
        // and compatibility come only from the stable relative folder path,
        // texture namespace, actor sex/race, and runtime BodyFamily.
        // Conventional and UBE packs may be partial. Each available body part
        // and material channel becomes an override; absent parts/channels keep
        // the actor's underlying texture instead of being synthesized from a
        // different part. UBE uses its !UBE/Body and !UBE/Head atlases, with
        // the body atlas targeting the slot-53 UBE body geometry.
        // Texture paths are game-relative and may point to any installed mod
        // folder, so player and NPC rule selection remain independent.
        void Refresh();
        // UI refreshes must not hash every installed DDS on Skyrim's render
        // thread.  The previous snapshot remains readable until the worker
        // publishes one complete replacement.
        [[nodiscard]] bool RefreshAsync();
        [[nodiscard]] bool Refreshing() const noexcept { return refreshing_.load(std::memory_order_acquire); }
        [[nodiscard]] static std::vector<SkinProfile> ScanDirectory(const std::filesystem::path& a_root);
        [[nodiscard]] std::vector<SkinProfile> Snapshot() const;
        [[nodiscard]] std::optional<SkinProfile> Find(std::string_view a_id) const;
        [[nodiscard]] std::uint64_t ContentHash(std::string_view a_id) const;
        [[nodiscard]] std::vector<std::string> CompatibleIds(
            const std::vector<std::string>& a_ids, SkinSex a_sex,
            body_family::Mask a_actorFamily, SkinRace a_actorRace) const;

        [[nodiscard]] static std::filesystem::path RootPath();

    private:
        mutable std::mutex lock_;
        std::atomic_bool refreshing_{};
        std::vector<SkinProfile> profiles_;
        std::unordered_map<std::string, std::uint64_t> contentHashes_;
    };

    class FutanariSkinProfiles final
    {
    public:
        static FutanariSkinProfiles& Get();

        // Each top-level Futanari folder is one user-facing pack. A pack may
        // expose more than one supported atlas and receives one typed row for
        // each. Missing material channels are intentional and stay untouched.
        void Refresh();
        [[nodiscard]] static std::vector<FutanariSkinProfile> ScanDirectory(
            const std::filesystem::path& a_root);
        [[nodiscard]] std::vector<FutanariSkinProfile> Snapshot() const;
        [[nodiscard]] std::optional<FutanariSkinProfile> Find(std::string_view a_id) const;
        [[nodiscard]] std::uint64_t ContentHash(std::string_view a_id) const;

        [[nodiscard]] static std::filesystem::path RootPath();

    private:
        mutable std::mutex lock_;
        std::vector<FutanariSkinProfile> profiles_;
        std::unordered_map<std::string, std::uint64_t> contentHashes_;
    };
}
