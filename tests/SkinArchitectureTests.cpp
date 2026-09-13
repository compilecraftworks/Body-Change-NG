#include "BodyChangeNG/AppearanceWork.h"
#include "BodyChangeNG/NativeAddonPolicy.h"
#include "BodyChangeNG/AppearanceEventPolicy.h"
#include "BodyChangeNG/NativeSkinRouting.h"
#include "BodyChangeNG/NativeSkinOwnership.h"
#include "BodyChangeNG/NativeAddonCopy.h"
#include "BodyChangeNG/NativeTextureData.h"
#include "BodyChangeNG/NativeTexturePath.h"
#include "BodyChangeNG/SkinApplicationPlan.h"
#include "BodyChangeNG/SkinLayout.h"
#include "BodyChangeNG/SkinRefreshTargets.h"
#include "BodyChangeNG/UiCatalogPolicy.h"

#include <array>
#include <cstdint>
#include <cctype>
#include <iostream>

namespace NativeSlot = bcn::native_skin::slot_mask;

namespace
{
    bool Require(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
}

int main()
{
    using bcn::ui_catalog::ChoiceIntent;
    using bcn::ui_catalog::Tab;
    constexpr auto playerTabs = bcn::ui_catalog::ResolveAvailableTabs(true, true);
    if (!Require(playerTabs.size == 5U &&
            playerTabs.values[0] == Tab::body && playerTabs.values[1] == Tab::skin &&
            playerTabs.values[2] == Tab::tint && playerTabs.values[3] == Tab::futanari &&
            playerTabs.values[4] == Tab::overlay,
            "player catalog tab order changed")) return 1;
    constexpr auto npcTabs = bcn::ui_catalog::ResolveAvailableTabs(false, true);
    if (!Require(npcTabs.size == 4U && npcTabs.values[0] == Tab::body &&
            npcTabs.values[1] == Tab::skin && npcTabs.values[2] == Tab::futanari &&
            npcTabs.values[3] == Tab::overlay,
            "conditional NPC catalog tab order changed")) return 1;
    if (!Require(bcn::ui_catalog::MouseIntent(true, false) == ChoiceIntent::preview &&
            bcn::ui_catalog::MouseIntent(true, true) == ChoiceIntent::confirm &&
            bcn::ui_catalog::MouseIntent(false, true) == ChoiceIntent::confirm &&
            bcn::ui_catalog::MouseIntent(false, false) == ChoiceIntent::none,
            "single/double click selection policy regressed")) return 1;

    if (!Require(!bcn::ui_catalog::CommitsActorChoice(false, false) &&
            bcn::ui_catalog::CommitsActorChoice(true, false) &&
            !bcn::ui_catalog::CommitsActorChoice(false, true) &&
            !bcn::ui_catalog::CommitsActorChoice(true, true),
            "distribution confirmation committed an actor selection")) return 1;
    for (unsigned catalog{}; catalog < 3U; ++catalog) {
        std::optional<bcn::ui_catalog::PendingChoice> preview;
        const auto baseline = "committed-" + std::to_string(catalog);
        bcn::ui_catalog::RememberPreview(preview, 0U, "invalid", false, baseline);
        if (!Require(!preview, "invalid actor acquired preview ownership")) return 1;
        bcn::ui_catalog::RememberPreview(preview, 0x14U, "A", false, baseline);
        for (unsigned click{}; click < 1000U; ++click) {
            const auto candidate = click % 2U ? "B" : "A";
            bcn::ui_catalog::RememberPreview(preview, 0x14U, candidate, false, "live preview");
            if (!Require(preview->id == candidate && preview->originalId == baseline,
                    "checkbox/navigation preview accumulated or replaced its rollback baseline")) return 1;
        }
        bcn::ui_catalog::RememberPreview(preview, 0x14U, {}, true, "live preview");
        if (!Require(preview->useDefault && preview->id.empty() && preview->originalId == baseline,
                "default preview lost the committed baseline")) return 1;
        preview.reset(); // cancel, popup entry, tab change or UI close
        bcn::ui_catalog::RememberPreview(preview, 0x14U, "C", false, "new commit");
        if (!Require(preview->originalId == "new commit", "reopening retained an old baseline")) return 1;
        bcn::ui_catalog::RememberPreview(preview, 0x15U, "D", false, "NPC commit");
        if (!Require(preview->actorFormID == 0x15U && preview->originalId == "NPC commit",
                "preview baseline crossed actor boundaries")) return 1;
    }

    for (const auto* domain : { "skin", "skin-face" }) {
        for (const auto* file : { "body.dds", "hands.dds", "feet.dds", "head.dds", "body_msn.dds", "body_sk.dds", "body_s.dds" }) {
            const auto cache = std::string("textures\\BodyChangeNG\\Cache\\") + domain + "\\0123\\" + file;
            const auto paths = bcn::native_skin::PathsFromCache(cache);
            if (!Require(paths && paths->resource == cache &&
                    std::ranges::equal("Data\\Textures\\" + paths->native, "Data\\" + paths->resource,
                        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }) &&
                    !paths->native.starts_with("textures\\"),
                    "native TXST loader and DDS preflight resolve different files")) return 1;
        }
    }
    if (!Require(bcn::native_skin::PathsFromCache("Textures/BodyChangeNG/Cache/skin/123/body.dds").has_value() &&
            !bcn::native_skin::PathsFromCache("textures\\textures\\BodyChangeNG\\Cache\\skin\\x.dds") &&
            !bcn::native_skin::PathsFromCache("textures\\BodyChangeNG\\Cache\\..\\x.dds") &&
            !bcn::native_skin::PathsFromCache("C:\\textures\\x.dds") &&
            !bcn::native_skin::PathsFromCache("textures\\BodyChangeNG\\Cache\\" + std::string(260, 'a')) &&
            !bcn::native_skin::PathsFromCache("textures\\"),
            "native TXST cache path validation failed")) return 1;
    struct TextureData { std::string textureName; };
    struct ResourceID {
        std::string identity;
        void GenerateFromPath(const char* path) { identity = path; }
    };
    struct TextureForm {
        std::array<TextureData, 8> textures;
        std::array<ResourceID, 8> textureFileIDs;
        // A shader-level setter must not be assumed to write form components.
        void SetTexturePath(std::size_t, const char*) {}
    };
    TextureForm originalTextures;
    for (std::size_t index{}; index < 8U; ++index) {
        const auto path = "original-" + std::to_string(index) + ".dds";
        if (!Require(bcn::native_skin::WriteFormTexturePath(originalTextures, index, path.c_str()),
                "native form texture write failed")) return 1;
    }
    auto npcTextures = originalTextures;
    for (const auto index : { 0U, 1U, 2U, 7U }) {
        if (!Require(bcn::native_skin::WriteFormTexturePath(npcTextures, index, "cache.dds") &&
                npcTextures.textures[index].textureName == "cache.dds" &&
                npcTextures.textureFileIDs[index].identity == "cache.dds" &&
                originalTextures.textures[index].textureName != "cache.dds",
                "native DDS slot/resource identity was not isolated from original NPC")) return 1;
    }
    if (!Require(bcn::native_skin::WriteFormTexturePath(npcTextures, 2U, "") &&
            npcTextures.textureFileIDs[2].identity.empty() &&
            !bcn::native_skin::WriteFormTexturePath(npcTextures, 8U, "bad.dds") &&
            !bcn::native_skin::WriteFormTexturePath(npcTextures, 0U, nullptr),
            "native optional-map clear or bounds check failed")) return 1;

    // All 8! component permutations: the adapter must follow the getter, not
    // a guessed enum identity. This exercises production discovery/write
    // helpers; it does not pretend to execute Skyrim's getter offline.
    bcn::native_skin::FormTextureSlots permutation{ 0, 1, 2, 3, 4, 5, 6, 7 };
    do {
        auto privateForm = originalTextures;
        const auto readShader = [&](const std::size_t shader) {
            return privateForm.textures[permutation[shader]].textureName;
        };
        const auto discovered = bcn::native_skin::DiscoverFormTextureSlots(privateForm, readShader);
        if (!Require(discovered && *discovered == permutation,
                "shader-to-record mapping did not follow the runtime getter")) return 1;
        for (std::size_t record{}; record < 8U; ++record) {
            if (!Require(privateForm.textures[record].textureName == originalTextures.textures[record].textureName &&
                    privateForm.textureFileIDs[record].identity == originalTextures.textureFileIDs[record].identity,
                    "private slot discovery leaked probe names or changed resource IDs")) return 1;
        }
        for (std::size_t shader{}; shader < 8U; ++shader) {
            const auto path = "selected-shader-" + std::to_string(shader) + ".dds";
            if (!Require(bcn::native_skin::WriteFormTexturePath(privateForm, (*discovered)[shader], path.c_str()) &&
                    readShader(shader) == path,
                    "selected skin DDS reached the wrong shader channel")) return 1;
        }
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    auto invalidForm = originalTextures;
    if (!Require(!bcn::native_skin::DiscoverFormTextureSlots(invalidForm,
            [&](std::size_t) { return invalidForm.textures[0].textureName; }),
            "ambiguous getter mapping must fail without attaching a form")) return 1;
    for (std::size_t record{}; record < 8U; ++record) {
        if (!Require(invalidForm.textures[record].textureName == originalTextures.textures[record].textureName,
                "failed discovery did not restore every probe path")) return 1;
    }
    struct ModelData {
        std::string path;
        std::vector<int> alternateTextures;
        bool operator==(const ModelData&) const = default;
    };
    struct AddonData {
        int race{}, bipedModelData{}, data{};
        std::array<ModelData, 2> bipedModels, bipedModel1stPersons;
        std::array<int, 2> skinTextures{}, skinTextureSwapLists{};
        std::vector<int> additionalRaces;
        int footstepSet{}, artObject{};
        int GetSlotMask() const { return bipedModelData; }
    };
    AddonData source;
    source.race = 42;
    source.bipedModelData = NativeSlot::body | NativeSlot::hands | NativeSlot::feet;
    source.data = 77;
    source.bipedModels = { ModelData{ "male.nif", { 10, 11 } }, ModelData{ "female.nif", { 20 } } };
    source.bipedModel1stPersons = { ModelData{ "male1st.nif", { 30 } }, ModelData{ "female1st.nif", { 40 } } };
    source.skinTextures = { 101, 102 };
    source.skinTextureSwapLists = { 201, 202 };
    source.additionalRaces = { 42, 43, 44 };
    source.footstepSet = 50;
    source.artObject = 60;
    AddonData clone; // Actual TESForm::Copy leaves ARMA data blank like this.
    const auto sameModel = [](const auto& a, const auto& b) { return a == b; };
    if (!Require(!bcn::native_skin::SameAddonGeometry(clone, source, sameModel),
            "blank engine ARMA clone must be rejected before attaching")) return 1;
    bcn::native_skin::CopyAddonData(clone, source,
        [](auto& destination, auto& original) { destination = original; });
    if (!Require(bcn::native_skin::SameAddonGeometry(clone, source, sameModel) &&
            clone.data == source.data && clone.skinTextures == source.skinTextures &&
            clone.skinTextureSwapLists == source.skinTextureSwapLists,
            "ARMA copy lost model, race, priority, textures, or slot data")) return 1;
    clone.additionalRaces[0] = 99;
    clone.bipedModels[1].alternateTextures[0] = 99;
    clone.skinTextures[1] = 99;
    if (!Require(source.additionalRaces[0] == 42 &&
            source.bipedModels[1].alternateTextures[0] == 20 && source.skinTextures[1] == 102,
            "one NPC clone mutated source/shared addon storage")) return 1;
    if (!Require(!bcn::native_skin::SameAddonGeometry(clone, source, sameModel),
            "damaged clone geometry passed the pre-attach guard")) return 1;

    struct RefreshActor { int base; bool loaded; int refreshes{}; };
    RefreshActor player{ 7, true }, peer{ 7, true }, unrelated{ 8, true }, unloaded{ 7, false };
    const auto eligible = [](const RefreshActor* actor) { return actor->base == 7 && actor->loaded; };
    const auto refresh = [](RefreshActor* actor) { ++actor->refreshes; };
    // Reproduces Skyrim's NPC process enumeration, which omits the player.
    bcn::native_skin::VisitRefreshTargets(&player, [&](const auto& visit) {
        visit(&peer); visit(&unrelated); visit(&unloaded); visit(&peer);
    }, eligible, refresh);
    if (!Require(player.refreshes == 1 && peer.refreshes == 1 &&
            unrelated.refreshes == 0 && unloaded.refreshes == 0,
            "selected player was omitted or a peer received duplicate skin refreshes")) return 1;
    player.refreshes = peer.refreshes = 0;
    bcn::native_skin::VisitRefreshTargets(&peer, [&](const auto& visit) {
        visit(&peer); visit(&peer); visit(&player);
    }, eligible, refresh);
    if (!Require(peer.refreshes == 1 && player.refreshes == 1,
            "selected NPC also in the process list was refreshed twice")) return 1;
    player.refreshes = 0;
    bcn::native_skin::VisitRefreshTargets(&player, [](const auto&) {}, eligible, refresh);
    if (!Require(player.refreshes == 1,
            "missing process lists suppressed the selected player's refresh")) return 1;

    using bcn::SkinCompatibilityStatus;
    using bcn::SkinLayout;
    using bcn::SkinRace;
    using bcn::SkinSex;
    using bcn::SkinUvLayout;
    using bcn::appearance::WorkChannel;
    using bcn::body_family::Bit;
    using bcn::body_family::Family;

    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::cbbe)).Compatible(),
            "Legacy was rejected for a CBBE actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::unp)).Compatible(),
            "Legacy was rejected for a UNP actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::female,
            SkinRace::humanoid, SkinSex::female, SkinRace::humanoid,
            Bit(Family::ube)).status == SkinCompatibilityStatus::incompatibleLayout,
            "Legacy leaked into a UBE actor")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::unknown, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo)).status == SkinCompatibilityStatus::unknownProfileLayout,
            "an ambiguous skin profile did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo) | Bit(Family::sam)).status ==
                SkinCompatibilityStatus::unknownActorLayout,
            "an actor with conflicting layout evidence did not fail closed")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::legacy, SkinSex::male,
            SkinRace::humanoid, SkinSex::male, SkinRace::humanoid,
            Bit(Family::himbo)).Compatible() &&
            bcn::ResolveRuntimeSkinUvLayout(SkinLayout::legacy,
                Bit(Family::himbo)) == SkinUvLayout::himbo &&
            bcn::ResolveRuntimeSkinUvLayout(SkinLayout::legacy,
                Bit(Family::sam)) == SkinUvLayout::sam,
            "Legacy male classification lost exact runtime BodyFamily routing")) return 1;
    if (!Require(bcn::EvaluateSkinCompatibility(SkinLayout::argonian, SkinSex::female,
            SkinRace::argonian, SkinSex::female, SkinRace::argonian, 0U).Compatible(),
            "an exact Argonian layout incorrectly depended on a humanoid body family")) return 1;

    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::legacy, SkinRace::humanoid,
            4U, 0U) == bcn::FeetLayerSource::bodyAtlas,
            "a verified humanoid layout lost its shared feet atlas")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::argonian, SkinRace::argonian,
            4U, 0U) == bcn::FeetLayerSource::none,
            "Argonian feet incorrectly inherited the body DDS")) return 1;
    if (!Require(bcn::ResolveFeetLayerSource(SkinLayout::khajiit, SkinRace::khajiit,
            4U, 1U) == bcn::FeetLayerSource::explicitFeet,
            "an explicit Khajiit feet atlas was not authoritative")) return 1;

    if (!Require(bcn::AllowsBroadSkinSlotFallback(SkinLayout::ube) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinLayout::legacy) &&
            !bcn::AllowsBroadSkinSlotFallback(SkinLayout::unknown),
            "broad skin-slot routing escaped the UBE shared-atlas boundary")) return 1;
    using NativeRole = bcn::native_skin::TextureRole;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::body, "actors\\character\\female\\femalebody_1.dds",
            SkinUvLayout::cbbe) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::hands, "femalehands_1.dds",
                SkinUvLayout::cbbe) == NativeRole::hands &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::feet, "femalebody_1.dds",
                SkinUvLayout::unp) == NativeRole::feet,
            "native ARMA slot routing crossed body, hand, or foot TXST roles")) return 1;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::body,
            "textures\\actors\\character\\female\\femalebody_etc_v2_1.dds",
            SkinUvLayout::cbbe) == NativeRole::cbbeGenitalAnal &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::body,
                "textures\\BakaUNP\\VaginalAnalCanal2.dds",
                SkinUvLayout::unp) == NativeRole::unpGenitalAnal,
            "native skin texture-swap lists lost the 3BA or BHUNP auxiliary atlas")) return 1;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            NativeSlot::ubeBody, "!UBE\\Body\\femalebody_1_d.dds",
            SkinUvLayout::ube) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                0x00800174U, {}, SkinUvLayout::ube) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                NativeSlot::ubeBody, "unknown.dds",
                SkinUvLayout::cbbe) == NativeRole::unmanaged,
            "UBE slot 53 escaped its explicit layout boundary")) return 1;
    if (!Require(bcn::native_skin::ShouldSynthesizeUbeTextureSet(
            SkinUvLayout::ube, NativeRole::body,
            "!UBE\\Body\\femalebody_tangent_1.nif", false, false) &&
            bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::hands,
                "!UBE/Hands/femalehands_tangent_1.nif", false, false) &&
            bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::feet,
                "!UBE\\Feet\\femalefeet_tangent_1.nif", false, false) &&
            !bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::body,
                "Custom\\Body\\body.nif", false, false) &&
            !bcn::native_skin::ShouldSynthesizeUbeTextureSet(
                SkinUvLayout::ube, NativeRole::body,
                "!UBE\\Body\\femalebody_tangent_1.nif", true, false) &&
            bcn::native_skin::UbeBaselineTexturePath(0U) ==
                "!UBE\\Body\\femalebody_1_d.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(1U) ==
                "!UBE\\Body\\femalebody_1_n.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(2U) ==
                "!UBE\\Body\\femalebody_1_sk.dds" &&
            bcn::native_skin::UbeBaselineTexturePath(3U).empty(),
            "UBE's missing NAM1 synthesis escaped the verified naked graph or guessed an undeclared channel")) return 1;
    constexpr auto multiPartMask = NativeSlot::body | NativeSlot::hands | NativeSlot::feet;
    if (!Require(bcn::native_skin::ResolveTextureRole(
            multiPartMask, "femalehands_1.dds", SkinUvLayout::cbbe) == NativeRole::hands &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "femalefeet_1.dds", SkinUvLayout::cbbe) == NativeRole::feet &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "femalebody_1.dds", SkinUvLayout::cbbe) == NativeRole::body &&
            bcn::native_skin::ResolveTextureRole(
                multiPartMask, "renamed.dds", SkinUvLayout::cbbe) == NativeRole::unmanaged,
            "a multi-slot ARMA ignored exact source TXST evidence or guessed an ambiguous role")) return 1;
    const auto conventionalRequired = bcn::native_skin::RequiredRoleMask(
        false, true, true, true, false, false);
    const auto conventionalAvailable = bcn::native_skin::RoleBit(NativeRole::body) |
        bcn::native_skin::RoleBit(NativeRole::hands) |
        bcn::native_skin::RoleBit(NativeRole::feet);
    const auto missingHands = conventionalAvailable &
        static_cast<bcn::native_skin::TextureRoleMask>(~bcn::native_skin::RoleBit(NativeRole::hands));
    const auto ubeRequired = bcn::native_skin::RequiredRoleMask(
        true, true, true, true, false, false);
    const auto bodyOnlyRequired = bcn::native_skin::RequiredRoleMask(
        false, true, false, false, false, false);
    const auto partialApplicable = bcn::native_skin::ApplicableRoleMask(
        missingHands, conventionalRequired);
    const auto cbbePackWithOptionalAtlas = bcn::native_skin::RequiredRoleMask(
        false, true, true, true, true, false);
    const auto cbbeApplicableWithoutSeparateAtlas = bcn::native_skin::ApplicableRoleMask(
        conventionalAvailable, cbbePackWithOptionalAtlas);
    if (!Require(bcn::native_skin::CoversRequiredRoles(
            conventionalAvailable, conventionalRequired) &&
            !bcn::native_skin::CoversRequiredRoles(missingHands, conventionalRequired) &&
            partialApplicable == missingHands &&
            cbbeApplicableWithoutSeparateAtlas == conventionalAvailable &&
            bcn::native_skin::CoversRequiredRoles(
                bcn::native_skin::RoleBit(NativeRole::body), bodyOnlyRequired) &&
            bcn::native_skin::CoversRequiredRoles(
                bcn::native_skin::RoleBit(NativeRole::body), ubeRequired),
            "native TXST partial routing borrowed an absent part, lost an available part, rejected a CBBE pack with optional genital DDS, or rejected UBE's shared atlas")) return 1;
    using GraphAction = bcn::native_skin::GraphAction;
    if (!Require(bcn::native_skin::ResolveGraphAction(
            true, true, true, true, false) == GraphAction::reuse &&
            bcn::native_skin::ResolveGraphAction(
                true, false, true, false, false) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, false, false, true) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, true, false, false, true) == GraphAction::rebuildFromCurrentSource &&
            bcn::native_skin::ResolveGraphAction(
                true, true, false, false, false) == GraphAction::rejectForeignOwner,
            "native skin ownership failed to isolate RSV/unowned-source rebasing from arbitrary providers")) return 1;
    using SharedBaseAction = bcn::native_skin::SharedBaseAction;
    if (!Require(bcn::native_skin::ResolveSharedBaseAction(
            0U, 0x20U, {}, "skin-a") == SharedBaseAction::keepOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, {}, "skin-a") == SharedBaseAction::transferOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", "skin-a") == SharedBaseAction::keepOwner &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", {}) == SharedBaseAction::rejectConflict &&
            bcn::native_skin::ResolveSharedBaseAction(
                0x10U, 0x20U, "skin-a", "skin-b") == SharedBaseAction::rejectConflict,
            "ActorBase ownership could not transfer at Default or admitted contradictory active requests")) return 1;

    if (!Require(
            bcn::native_addon::AcceptTargetView(true, true, true) &&
            !bcn::native_addon::AcceptTargetView(false, true, true) &&
            !bcn::native_addon::AcceptTargetView(true, false, true) &&
            !bcn::native_addon::AcceptTargetView(true, true, false),
            "SOS/TNG native TXST did not share the same exact slot-52 admission rule")) return 1;

    using bcn::appearance::Event;
    using bcn::appearance::Feature;
    if (!Require(!bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::equipmentChanged) &&
            !bcn::appearance::NeedsReconcile(Feature::faceTint, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::maleGenitalAddon, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::equipmentChanged) &&
            bcn::appearance::NeedsReconcile(Feature::outfitMorph, Event::equipmentChanged),
            "equipment events regained ownership of native base skin or lost external-addon handling")) return 1;
    if (!Require(!bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::maleGenitalAddon, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::actor3DAttached) &&
            bcn::appearance::NeedsReconcile(Feature::outfitMorph, Event::actor3DAttached),
            "3D attach handling lost an external addon or started repainting native base skin")) return 1;
    if (!Require(!bcn::appearance::NeedsReconcile(Feature::baseSkin, Event::niNodeUpdated) &&
            !bcn::appearance::NeedsReconcile(Feature::faceTint, Event::niNodeUpdated) &&
            bcn::appearance::NeedsReconcile(Feature::maleGenitalAddon, Event::niNodeUpdated) &&
            bcn::appearance::NeedsReconcile(Feature::futanariAddon, Event::niNodeUpdated),
            "3D rebuild lost SOS addon restoration or regained native skin repainting")) return 1;

    constexpr std::array channels{
        WorkChannel::actorReconcile,
        WorkChannel::initialDistribution,
        WorkChannel::equipmentReconcile,
        WorkChannel::raceMenuRestore,
        WorkChannel::equipmentVerify,
        WorkChannel::bodyPreview,
        WorkChannel::bodyCommit,
        WorkChannel::outfitRefit,
        WorkChannel::bodyPreviewCleanup,
        WorkChannel::skinApply,
        WorkChannel::skinFaceRefresh,
        WorkChannel::maleGenitalSkinApply,
        WorkChannel::futanariSkinApply,
        WorkChannel::tintApply
    };
    for (std::size_t left{}; left < channels.size(); ++left) {
        for (std::size_t right = left + 1U; right < channels.size(); ++right) {
            if (!Require(bcn::appearance::ChannelValue(channels[left]) !=
                    bcn::appearance::ChannelValue(channels[right]),
                    "two appearance operations share a replacement channel")) return 1;
        }
    }
    if (!Require(WorkChannel::tintApply != WorkChannel::futanariSkinApply &&
            bcn::appearance::IsInteractiveChannel(WorkChannel::tintApply) &&
            !bcn::appearance::IsInteractiveChannel(WorkChannel::equipmentVerify),
            "appearance channel semantics regressed")) return 1;

    bcn::SkinProfile cbbeProfile;
    cbbeProfile.layout = SkinLayout::legacy;
    cbbeProfile.race = SkinRace::humanoid;
    cbbeProfile.body = { { 0U, "base-body.dds" }, { 1U, "base-body_n.dds" } };
    cbbeProfile.hands = { { 0U, "base-hands.dds" } };
    cbbeProfile.face = { { 0U, "base-face.dds" } };
    cbbeProfile.elderBody = { { 1U, "elder-body_n.dds" } };
    cbbeProfile.cbbeGenitalAnal = { { 0U, "cbbe-genital.dds" } };
    cbbeProfile.unpGenitalAnal = { { 0U, "unp-genital.dds" } };
    const auto elderPlan = bcn::skin_plan::Build(cbbeProfile, {
        .elder = true,
        .bodyFamily = Bit(Family::cbbe)
    });
    if (!Require(elderPlan.body.size() == 2U &&
            elderPlan.body[1].path == "elder-body_n.dds" &&
            elderPlan.hands.size() == 1U && elderPlan.face.size() == 1U &&
            elderPlan.feet.size() == 2U && elderPlan.feet[1].path == "elder-body_n.dds" &&
            elderPlan.runtimeUvLayout == SkinUvLayout::cbbe &&
            elderPlan.cbbeGenitalAnal.size() == 1U && elderPlan.unpGenitalAnal.size() == 1U &&
            bcn::skin_plan::RoutesGenitalAnalAtlas(elderPlan, SkinUvLayout::cbbe) &&
            !bcn::skin_plan::RoutesGenitalAnalAtlas(elderPlan, SkinUvLayout::unp),
            "the equipment-independent plan lost a body, hand, foot, or face layer")) return 1;

    // Characterize female conditional-channel precedence independently of
    // installed assets: race -> vampire -> elder, preserving other channels.
    {
        auto conditional = cbbeProfile;
        conditional.sex = bcn::SkinSex::female;
        conditional.face = { { 0U, "pack/base-face.dds" }, { 1U, "pack/base-face_n.dds" } };
        conditional.raceFace[static_cast<std::size_t>(bcn::HumanoidSkinRace::breton)] =
            { { 1U, "pack/breton-face_n.dds" } };
        conditional.vampireFace = { { 1U, "pack/vampire-face_n.dds" } };
        conditional.elderFace = { { 1U, "pack/elder-face_n.dds" } };
        conditional.elderHands = { { 1U, "pack/elder-hands_n.dds" } };
        for (const auto family : { Family::cbbe, Family::unp }) {
            for (const bool vampire : { false, true }) {
                for (const bool elder : { false, true }) {
                    const auto plan = bcn::skin_plan::Build(conditional, {
                        .elder = elder, .vampire = vampire,
                        .humanoidRace = bcn::HumanoidSkinRace::breton,
                        .bodyFamily = Bit(family)
                    });
                    const auto expected = elder ? "pack/elder-face_n.dds" :
                        vampire ? "pack/vampire-face_n.dds" : "pack/breton-face_n.dds";
                    if (!Require(plan.face.size() == 2U &&
                            plan.face[0].path == "pack/base-face.dds" &&
                            plan.face[1].path == expected &&
                            plan.body[1].path == (elder ? "elder-body_n.dds" : "base-body_n.dds") &&
                            plan.feet[1].path == plan.body[1].path &&
                            plan.hands.size() == (elder ? 2U : 1U),
                            "female race/vampire/elder precedence or part isolation changed")) return 1;
                }
            }
        }
    }

    // Male Native plans use the same conditional/fallback policy, independent
    // of HIMBO/SAM meshes. A missing variant keeps this pack's generic DDS.
    for (const auto family : { Family::maleVanilla, Family::himbo, Family::sam }) {
        for (const bool variants : { false, true }) {
            bcn::SkinProfile male;
            male.sex = bcn::SkinSex::male;
            male.layout = SkinLayout::legacy;
            male.race = SkinRace::humanoid;
            male.body = { { 0U, "male/body.dds" }, { 1U, "male/body_n.dds" } };
            male.hands = { { 0U, "male/hands.dds" }, { 1U, "male/hands_n.dds" } };
            male.face = { { 0U, "male/head.dds" }, { 1U, "male/head_n.dds" } };
            if (variants) {
                male.elderBody = { { 1U, "maleold/body_n.dds" } };
                male.elderHands = { { 1U, "maleold/hands_n.dds" } };
                male.elderFace = { { 1U, "maleold/head_n.dds" } };
                male.raceFace[static_cast<std::size_t>(bcn::HumanoidSkinRace::breton)] =
                    { { 1U, "bretonmale/head_n.dds" } };
                male.vampireFace = { { 1U, "male/vampire_n.dds" } };
            }
            for (const bool elder : { false, true }) {
                for (const bool vampire : { false, true }) {
                    const auto plan = bcn::skin_plan::Build(male, {
                        .elder = elder, .vampire = vampire,
                        .humanoidRace = bcn::HumanoidSkinRace::breton, .bodyFamily = Bit(family)
                    });
                    const auto faceNormal = !variants ? "male/head_n.dds" : elder ?
                        "maleold/head_n.dds" : vampire ? "male/vampire_n.dds" : "bretonmale/head_n.dds";
                    if (!Require(plan.body.size() == 2U && plan.hands.size() == 2U &&
                            plan.face.size() == 2U && plan.feet.size() == 2U &&
                            plan.body[0].path == "male/body.dds" && plan.hands[0].path == "male/hands.dds" &&
                            plan.face[0].path == "male/head.dds" &&
                            plan.body[1].path == (variants && elder ? "maleold/body_n.dds" : "male/body_n.dds") &&
                            plan.hands[1].path == (variants && elder ? "maleold/hands_n.dds" : "male/hands_n.dds") &&
                            plan.face[1].path == faceNormal && plan.feet[1].path == plan.body[1].path,
                            "male conditional channels did not fall back to the selected pack")) return 1;
                }
            }
        }
    }

    const auto unpPlan = bcn::skin_plan::Build(cbbeProfile, {
        .bodyFamily = Bit(Family::unp)
    });
    if (!Require(unpPlan.runtimeUvLayout == SkinUvLayout::unp &&
            unpPlan.cbbeGenitalAnal.size() == 1U && unpPlan.unpGenitalAnal.size() == 1U &&
            !bcn::skin_plan::RoutesGenitalAnalAtlas(unpPlan, SkinUvLayout::cbbe) &&
            bcn::skin_plan::RoutesGenitalAnalAtlas(unpPlan, SkinUvLayout::unp),
            "Legacy genital/anal assets were incorrectly used as catalog classification")) return 1;

    bcn::SkinProfile beastProfile;
    beastProfile.layout = SkinLayout::argonian;
    beastProfile.race = SkinRace::argonian;
    beastProfile.body = { { 0U, "argonian-body.dds" } };
    const auto beastPlan = bcn::skin_plan::Build(beastProfile, {});
    if (!Require(beastPlan.beastTail && beastPlan.feet.empty(),
            "the planner leaked an Argonian body DDS into feet")) return 1;

    bcn::SkinProfile ubeProfile;
    ubeProfile.layout = SkinLayout::ube;
    ubeProfile.race = SkinRace::humanoid;
    ubeProfile.body = { { 0U, "ube-body.dds" } };
    ubeProfile.hands = { { 0U, "wrong-hands.dds" } };
    const auto ubePlan = bcn::skin_plan::Build(ubeProfile, {
        .bodyFamily = Bit(Family::ube)
    });
    if (!Require(ubePlan.broadSharedAtlas && ubePlan.hands.size() == 1U &&
            ubePlan.hands.front().path == "ube-body.dds" &&
            ubePlan.feet.size() == 1U && ubePlan.feet.front().path == "ube-body.dds",
            "UBE did not remain one explicit shared-atlas plan")) return 1;

    cbbeProfile.vampireFace = { { 0U, "vampire-face.dds" } };
    cbbeProfile.faceDetails = {
        { 3U, "BodySkin\\린아 ver2\\textures\\femalehead_frek.dds" },
        { 3U, "BodySkin\\린아 ver2\\textures\\femalehead_rough.dds" }
    };
    const auto facePlan = bcn::skin_plan::Build(cbbeProfile, {
        .vampire = true,
        .faceDetailFilename = "textures\\actors\\character\\femalehead_rough.dds",
        .bodyFamily = Bit(Family::cbbe)
    });
    if (!Require(facePlan.requiresFaceGeometry && facePlan.face.size() == 2U &&
            facePlan.face[0].path == "vampire-face.dds" &&
            facePlan.face[1].path ==
                "BodySkin\\린아 ver2\\textures\\femalehead_rough.dds",
            "UTF-8 face-detail paths crashed or escaped filename matching")) return 1;

    cbbeProfile.faceDetails = {
        { 3U, "BodySkin\\린아 ver2\\textures\\femaleheaddetail_age40.dds" }
    };
    const auto observedCrashPlan = bcn::skin_plan::Build(cbbeProfile, {
        .faceDetailFilename =
            "Actors\\Character\\Female\\FemaleHeadDetail_Age40.dds",
        .bodyFamily = Bit(Family::cbbe)
    });
    if (!Require(observedCrashPlan.face.size() == 2U &&
            observedCrashPlan.face[1].path ==
                "BodySkin\\린아 ver2\\textures\\femaleheaddetail_age40.dds",
            "the TuLED Unicode Age40 face-detail crash regressed")) return 1;

    // Pack A has one complexion alternative (frek), so it can replace the
    // original rough channel. Selecting B afterwards must still choose rough,
    // exactly as selecting B directly does; A must not become the baseline.
    bcn::SkinProfile packA;
    packA.face = { { 0U, "pack-a/head.dds" } };
    packA.faceDetails = { { 3U, "pack-a/femalehead_frek.dds" } };
    bcn::SkinProfile packB;
    packB.face = { { 0U, "pack-b/head.dds" } };
    packB.faceDetails = {
        { 3U, "pack-b/femalehead_frek.dds" },
        { 3U, "pack-b/femalehead_rough.dds" }
    };
    const std::string originalDetail = "original/femalehead_rough.dds";
    const auto firstPlan = bcn::skin_plan::Build(packA, { .faceDetailFilename = originalDetail });
    if (!Require(firstPlan.face.back().path == "pack-a/femalehead_frek.dds",
            "single pack detail fallback changed")) return 1;
    const auto directB = bcn::skin_plan::Build(packB, { .faceDetailFilename = originalDetail });
    const auto afterA = bcn::skin_plan::Build(packB, {
        // Face TXST remains untouched by pack A; only its node DDS changes.
        .faceDetailFilename = originalDetail
    });
    if (!Require(afterA.face.back().path == directB.face.back().path &&
            afterA.face.back().path == "pack-b/femalehead_rough.dds" &&
            afterA.face.front().path == "pack-b/head.dds",
            "face detail selection depends on the previously selected pack")) return 1;

    // Production planner, repeated A/B selection across installed Legacy
    // runtimes: no prior pack state may leak between body/hands/feet/face.
    for (const auto family : { Family::cbbe, Family::unp }) {
        for (const auto* pack : { "A", "B", "A", "B" }) {
            bcn::SkinProfile profile;
            profile.layout = SkinLayout::legacy;
            profile.race = SkinRace::humanoid;
            const std::string prefix = std::string(pack) + "/";
            profile.body = { { 0U, prefix + "body.dds" }, { 1U, prefix + "body_n.dds" } };
            profile.hands = { { 0U, prefix + "hands.dds" } };
            profile.face = { { 0U, prefix + "face.dds" } };
            auto plan = bcn::skin_plan::Build(profile, { .bodyFamily = Bit(family) });
            if (!Require(plan.body.front().path == prefix + "body.dds" &&
                    plan.hands.front().path == prefix + "hands.dds" &&
                    plan.feet.front().path == prefix + "body.dds" &&
                    plan.feet.back().path == prefix + "body_n.dds" &&
                    plan.face.front().path == prefix + "face.dds",
                    "repeated selection mixed a Legacy body/hand/foot/face DDS")) return 1;
            profile.feet = { { 0U, prefix + "feet.dds" } };
            plan = bcn::skin_plan::Build(profile, { .bodyFamily = Bit(family) });
            if (!Require(plan.feet.size() == 1U && plan.feet.front().path == prefix + "feet.dds" &&
                    plan.hands.front().path == prefix + "hands.dds" &&
                    plan.body.front().path == prefix + "body.dds",
                    "explicit feet DDS was replaced by body or hands")) return 1;
            profile.face.clear();
            plan = bcn::skin_plan::Build(profile, { .bodyFamily = Bit(family) });
            if (!Require(plan.face.empty() && !plan.requiresFaceGeometry &&
                    plan.body.front().path == prefix + "body.dds" &&
                    plan.feet.front().path == prefix + "feet.dds" &&
                    plan.hands.front().path == prefix + "hands.dds",
                    "body-only selection gained a face dependency")) return 1;
        }
    }

    std::cout << "Skin architecture tests passed\n";
    return 0;
}
