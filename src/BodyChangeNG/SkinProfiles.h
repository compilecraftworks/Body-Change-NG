#pragma once

#include "BodyChangeNG/BodyFamily.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace bcn
{
    enum class SkinSex : std::uint8_t
    {
        female,
        male
    };

    // Beast-race textures use different UVs and file namespaces even when
    // their body mesh belongs to the same broad CBBE/UNP or male family.
    // Keep that compatibility axis independent from BodyFamily so a human,
    // Argonian, and Khajiit skin can never be offered interchangeably.
    enum class SkinRace : std::uint8_t
    {
        humanoid,
        argonian,
        khajiit
    };

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
        // UBE uses its own body/head topology, UVs, texture namespace, and
        // naked-body slot. Keep compatibility on the catalog item so the UI,
        // distribution backend, and RaceMenu reapply path all make the same
        // decision instead of relying on the user-facing name.
        body_family::Mask bodyFamilies{};
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

    [[nodiscard]] constexpr body_family::Mask StandardSkinFamilies(const SkinSex sex) noexcept
    {
        return sex == SkinSex::female ?
            body_family::Bit(body_family::Family::femaleVanilla) |
                body_family::Bit(body_family::Family::cbbe) |
                body_family::Bit(body_family::Family::unp) :
            body_family::kMaleFamilies;
    }

    [[nodiscard]] constexpr bool SkinMatchesActor(
        const body_family::Mask skinFamilies, const body_family::Mask actorFamily) noexcept
    {
        // Unlike an unclassified BodySlide preset, a conventional skin cannot
        // safely be treated as a generic female fallback for UBE's different
        // UV topology. Unknown evidence still preserves the show-all fallback.
        return skinFamilies == 0U || actorFamily == 0U ||
            (skinFamilies & actorFamily) != 0U;
    }

    [[nodiscard]] constexpr bool SkinRaceMatchesActor(
        const SkinRace skinRace, const SkinRace actorRace) noexcept
    {
        return skinRace == actorRace;
    }

    [[nodiscard]] std::string SkinFamilyLabel(body_family::Mask a_families, SkinSex a_sex);
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

        // Loads BodySkin/<skin name>/profile.json and auto-detects the common
        // BodySkin/<skin name>/Textures/... layout when no profile JSON exists.
        // Conventional and UBE packs may be partial. Each available body part
        // and material channel becomes an override; absent parts/channels keep
        // the actor's underlying texture instead of being synthesized from a
        // different part. UBE uses its !UBE/Body and !UBE/Head atlases, with
        // the body atlas targeting the slot-53 UBE body geometry.
        // Texture paths are game-relative and may point to any installed mod
        // folder, so player and NPC rule selection remain independent.
        void Refresh();
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
        std::vector<SkinProfile> profiles_;
        std::unordered_map<std::string, std::uint64_t> contentHashes_;
    };
}
