#include "BodyChangeNG/ActorStateSerialization.h"
#include "BodyChangeNG/NativeAddonPolicy.h"
#include "BodyChangeNG/AppearanceEventPolicy.h"
#include "BodyChangeNG/FrameTaskQueue.h"
#include "BodyChangeNG/FutanariRouting.h"
#include "BodyChangeNG/FutanariSupport.h"
#include "BodyChangeNG/SkinGeometryRouting.h"
#include "BodyChangeNG/SkinSessionState.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void Check(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
    }

    // Exercise the codec used by ActorRegistry's actual ASTR v6 writer/reader.
    // No Skyrim geometry, SKSE serialization callbacks or provider is mocked
    // here: this proves persistent data and work policy, not in-game rendering.
    bcn::ActorState RoundTrip(const bcn::ActorState& state)
    {
        std::vector<std::string> strings{ "" };
        std::vector<bcn::actor_serialization::SerializedOverlayItemV5> overlayItems;
        auto wire = bcn::actor_serialization::EncodeV6(state, [&](const std::string& value) {
            for (std::size_t i{}; i < strings.size(); ++i) {
                if (strings[i] == value) return static_cast<std::uint32_t>(i);
            }
            strings.push_back(value);
            return static_cast<std::uint32_t>(strings.size() - 1U);
        }, overlayItems);
        std::array<std::byte, sizeof(wire)> bytes{};
        std::memcpy(bytes.data(), &wire, sizeof(wire));
        decltype(wire) read{};
        std::memcpy(&read, bytes.data(), sizeof(read));
        auto restored = bcn::actor_serialization::DecodeV6(read, overlayItems, strings);
        Check(restored.has_value(), "ASTR v6 round-trip failed");
        bcn::PrepareRestoredState(*restored);
        return *restored;
    }
}

int main()
{
    try {
        namespace session = bcn::skin_session;
        using Channel = session::AddonTextureChannel;
        bcn::futanari_support::Availability installedWithoutActors{
            .providers = static_cast<std::uint8_t>(bcn::futanari_support::Provider::sos),
            .trx = true
        };
        Check(installedWithoutActors.Any(),
            "installed female addon was hidden merely because no actor was assigned");
        Check(!bcn::futanari_support::Availability{}.Any(),
            "missing female addon unexpectedly enabled the futanari feature");
        // Provider classification must survive a native TXST selection without
        // treating the new private cache namespace as UBE/ERF/TRX evidence.
        const std::string cache = R"(textures\BodyChangeNG\Cache\futanari\1234\skin.dds)";
        Check(bcn::futanari::ClassifyEvidence({}, "Penis", cache) ==
                bcn::futanari::AddonKind::none,
            "arbitrary BCNG genital cache was classified as UBE");
        for (const auto& [original, kind] : std::array{
                 std::pair{R"(textures\[TRX] Futa addon\Regular\Schlong.dds)", bcn::futanari::AddonKind::trx},
                 std::pair{R"(textures\ERF_Futanari\fairskincbbe\futanari_schlong.dds)", bcn::futanari::AddonKind::erf},
                 std::pair{R"(textures\!UBE\Body\malebody_1_d.dds)", bcn::futanari::AddonKind::ube}}) {
            for (int i{}; i < 8; ++i) {
                Check(bcn::futanari::ClassifyEvidence({}, "Penis", original) == kind,
                    "captured provider diffuse no longer identifies its UV family");
            }
        }
        Check(!bcn::skin_geometry::IsMaleGenital("BaseShape", cache) &&
                bcn::skin_geometry::IsMaleGenital("BaseShape",
                    R"(textures\actors\character\SOS\VectorPlexus Regular\malegenitals_1.dds)") &&
                !bcn::skin_geometry::IsMaleGenital("BaseShape",
                    R"(textures\actors\character\male\malebody_1.dds)"),
            "male original diffuse lost genital routing or admitted torso DDS");
        bcn::ActorState male{ .actorFormID = 0x1234, .basePlugin = "Skyrim.esm" };
        male.skin.selection = { "HIMBO/Skin_A", true, false };
        male.body.selection = { "Body_A", true, false };
        male.skin.application = { "HIMBO/Skin_A", false, true, true, 123 };
        const auto skinGeneration = session::BeginSkinChange(male.actorFormID);
        session::MarkAddonReconciled(male.actorFormID, Channel::maleGenitals, 777);
        Check(!session::NeedsAddonReconcile(777, 777, true),
            "identical native source requested another refresh");
        Check(session::NeedsAddonReconcile(777, 777, false),
            "missing native selection suppressed rebuild restoration");
        // Detach invalidates only live work. The saved desired state survives.
        session::Forget(male.actorFormID);
        Check(!session::IsCurrentSkinChange(male.actorFormID, skinGeneration),
            "old male callback survived detach");
        auto loadedMale = RoundTrip(male);
        Check(loadedMale.skin.selection.selectedId == "HIMBO/Skin_A" &&
                loadedMale.body.selection.selectedId == "Body_A" &&
                !loadedMale.skin.application.verifiedThisSession,
            "male choice/body isolation lost across load");
        Check(session::NeedsAddonReconcile(session::ReconciledAddonSignature(male.actorFormID,
                Channel::maleGenitals), 777), "same SOS armor did not require attach restore");

        bcn::ActorState female{ .actorFormID = 0x2345 };
        female.futanari = {
            .selectedSkinId = "FutaSkin_04", .manual = true, .useDefault = false };
        female.skin.selection = { "CBBE/Skin_B", true, false };
        const auto futaGeneration = session::BeginFutanariChange(female.actorFormID);
        session::MarkAddonReconciled(female.actorFormID, Channel::futanari, 888);
        session::ClearAddonReconciled(female.actorFormID, Channel::futanari); // unequip
        auto loadedFemale = RoundTrip(female); // save while unequipped
        session::Reset(); // game exit/load
        Check(!session::IsCurrentFutanariChange(female.actorFormID, futaGeneration) &&
                loadedFemale.futanari.selectedSkinId == "FutaSkin_04" &&
                loadedFemale.futanari.manual && !loadedFemale.futanari.useDefault &&
                loadedFemale.skin.selection.selectedId == "CBBE/Skin_B",
            "unequipped futanari choice lost or old callback survived load");
        Check(session::NeedsAddonReconcile(session::ReconciledAddonSignature(female.actorFormID,
                Channel::futanari), 888), "same futanari armor did not require re-equip restore");
        loadedFemale.futanari = { .manual = true, .useDefault = true }; // explicit Default only
        const auto loadedFutanariDefault = RoundTrip(loadedFemale).futanari;
        Check(loadedFutanariDefault.selectedSkinId.empty() && loadedFutanariDefault.manual &&
                loadedFutanariDefault.useDefault,
            "explicit futanari Default resurrected the last choice");

        // The player uses the same ASTR actor record as manually edited NPCs.
        bcn::ActorState pendingReset;
        pendingReset.actorFormID = 0x14;
        pendingReset.overlay.areas[0].items.push_back({ "owned", "owned.dds", 3U });
        bcn::ResetActorSelectionsToDefaults(pendingReset);
        const auto restoredReset = RoundTrip(pendingReset);
        Check(restoredReset.overlay.areas[0].useDefault &&
            restoredReset.overlay.areas[0].items.size() == 1U &&
            restoredReset.overlay.areas[0].items.front().ownedSlot == 3U,
            "save/load discarded pending overlay removal ownership");

        // Explicit Default is a real saved choice too; it must not collapse
        // into an absent selection and reveal a stale automatic/applied value.
        bcn::ActorState manualPlayer{
            .actorFormID = 0x14,
            .baseLocalFormID = 0x7,
            .basePlugin = "Skyrim.esm",
            .body = { .selection = { "PlayerBody", true, false } },
            .skin = { .selection = { {}, true, true } },
            .overlay = { .areas = {
                bcn::OverlayAreaState{ .items = {
                    { "PlayerFacePaint", "textures\\paint\\player-face.dds", 1U, 0xA0B0C0D0U },
                    { "PlayerFacePaint2", "textures\\paint\\player-face-2.dds", 2U, 0x90FFFFFFU }
                }, .manual = true },
                bcn::OverlayAreaState{}, bcn::OverlayAreaState{}, bcn::OverlayAreaState{}
            } },
            .futanari = { .selectedSkinId = "PlayerFutanari", .manual = true }
        };
        const auto loadedManualPlayer = RoundTrip(manualPlayer);
        Check(loadedManualPlayer.body.selection.manual &&
                loadedManualPlayer.body.selection.selectedId == "PlayerBody" &&
                loadedManualPlayer.skin.selection.manual &&
                loadedManualPlayer.skin.selection.useDefault &&
                loadedManualPlayer.skin.selection.selectedId.empty() &&
                loadedManualPlayer.overlay.areas[0].manual &&
                loadedManualPlayer.overlay.areas[0].items.size() == 2U &&
                loadedManualPlayer.overlay.areas[0].items[0].selectedId == "PlayerFacePaint" &&
                loadedManualPlayer.overlay.areas[0].items[0].color == 0xA0B0C0D0U &&
                loadedManualPlayer.overlay.areas[0].items[1].selectedId == "PlayerFacePaint2" &&
                loadedManualPlayer.futanari.selectedSkinId == "PlayerFutanari",
            "manual player body/skin/multiple overlays/futanari choices did not survive ASTR v6");

        auto otherMale = male;
        otherMale.actorFormID = 0x3456;
        otherMale.skin.selection.selectedId = "SAM/Skin_C";
        Check(RoundTrip(otherMale).skin.selection.selectedId != loadedMale.skin.selection.selectedId,
            "two actor choices were merged");

        bcn::ActorState distributedNpc{
            .actorFormID = 0x4567,
            .baseLocalFormID = 0xABC,
            .basePlugin = "DistributedNPCs.esp",
            .body = {
                .selection = { "Rule/Body_07", false, false },
                .application = { "Rule/Body_07", false, true, true, 0x1111 },
                .outfitSignature = 0x2222
            },
            .skin = {
                .selection = { "Rule/Skin_03", false, false },
                .application = { "Rule/Skin_03", false, true, true, 0x3333 }
            },
            .overlay = { .areas = {
                bcn::OverlayAreaState{ .items = { { "Overlay/Face_02", "textures\\paint\\face02.dds", 2U } } },
                bcn::OverlayAreaState{ .items = { { "Overlay/Body_05", "textures\\paint\\body05.dds", 5U } }, .manual = true },
                bcn::OverlayAreaState{ .manual = true, .useDefault = true },
                bcn::OverlayAreaState{}
            } },
            .futanari = { .selectedSkinId = "FutaSkin_02" }
        };
        const auto loadedDistributedNpc = RoundTrip(distributedNpc);
        Check(loadedDistributedNpc.actorFormID == distributedNpc.actorFormID &&
                loadedDistributedNpc.baseLocalFormID == distributedNpc.baseLocalFormID &&
                loadedDistributedNpc.basePlugin == distributedNpc.basePlugin &&
                loadedDistributedNpc.body.selection.selectedId == "Rule/Body_07" &&
                !loadedDistributedNpc.body.selection.manual &&
                loadedDistributedNpc.body.application.appliedId == "Rule/Body_07" &&
                loadedDistributedNpc.body.application.signature == 0x1111 &&
                loadedDistributedNpc.body.outfitSignature == 0x2222 &&
                loadedDistributedNpc.skin.selection.selectedId == "Rule/Skin_03" &&
                !loadedDistributedNpc.skin.selection.manual &&
                loadedDistributedNpc.skin.application.appliedId == "Rule/Skin_03" &&
                loadedDistributedNpc.skin.application.signature == 0x3333 &&
                loadedDistributedNpc.futanari.selectedSkinId == "FutaSkin_02" &&
                !loadedDistributedNpc.futanari.manual &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::face)].items.front().selectedId ==
                    "Overlay/Face_02" &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::face)].items.front().ownedSlot == 2U &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::body)].manual &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::body)].items.front().texturePath ==
                    "textures\\paint\\body05.dds" &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::hands)].useDefault &&
                loadedDistributedNpc.overlay.areas[bcn::overlay::Index(bcn::overlay::Area::hands)].items.empty() &&
                !loadedDistributedNpc.body.application.verifiedThisSession &&
                !loadedDistributedNpc.skin.application.verifiedThisSession,
            "distributed NPC body/skin/addon state did not survive the real cosave codec");

        // Actual selection reducer + ASTR codec: initial absence, late female
        // registration, provider family switch, no candidate, and manual lock.
        // Provider resolution itself still requires in-game validation.
        auto lateRegistered = loadedDistributedNpc;
        lateRegistered.futanari = {};
        Check(!bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::nullopt),
            "an unregistered NPC acquired an automatic futanari choice");
        Check(bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::string{"ERF/A"}),
            "late registration did not accept its first automatic selection");
        lateRegistered = RoundTrip(lateRegistered);
        Check(!lateRegistered.futanari.manual && lateRegistered.futanari.selectedSkinId == "ERF/A",
            "late automatic futanari selection became manual or disappeared after load");
        Check(bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::string{"TRX/B"}),
            "addon family switch did not replace the automatic selection");
        Check(lateRegistered.body.selection.selectedId == loadedDistributedNpc.body.selection.selectedId &&
                lateRegistered.skin.selection.selectedId == loadedDistributedNpc.skin.selection.selectedId &&
                lateRegistered.overlay.areas[0].items.front().selectedId ==
                    loadedDistributedNpc.overlay.areas[0].items.front().selectedId,
            "futanari-only re-evaluation changed another distribution channel");
        Check(!bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::nullopt) &&
                lateRegistered.futanari.selectedSkinId == "TRX/B",
            "temporarily absent provider erased a saved selection");
        Check(!bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::string{"TRX/B"}),
            "identical provider event changed its selection repeatedly");
        lateRegistered.futanari.manual = true;
        Check(!bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::string{"ERF/A"}) &&
                lateRegistered.futanari.selectedSkinId == "TRX/B",
            "provider re-evaluation overwrote a manual futanari skin");
        lateRegistered.futanari = { .manual = true, .useDefault = true };
        Check(!bcn::UpdateAutomaticFutanariSelection(lateRegistered.futanari, std::string{"ERF/A"}) &&
                RoundTrip(lateRegistered).futanari.useDefault,
            "provider re-evaluation undid a saved manual Default");

        const auto erfA = session::FutanariSelectionSignature(888U, "ERF/A", 1U);
        const auto erfB = session::FutanariSelectionSignature(888U, "ERF/B", 1U);
        Check(erfA != erfB && session::NeedsAddonReconcile(erfA, erfB),
            "new rule candidate on unchanged geometry was incorrectly treated as already applied");
        Check(erfA != session::FutanariSelectionSignature(888U, "ERF/A", 2U) &&
                erfA != session::FutanariSelectionSignature(999U, "ERF/A", 1U),
            "DDS refresh or provider replacement was hidden by the reconcile signature");
        Check(session::FutanariSelectionSignature(0U, "ERF/A", 1U) == 0U,
            "missing geometry acquired a non-empty reconcile signature");
        for (unsigned i{}; i < 1000U; ++i) {
            Check(!session::NeedsAddonReconcile(erfA,
                    session::FutanariSelectionSignature(888U, "ERF/A", 1U)),
                "identical rebuild event requested another native refresh");
        }

        session::MarkAddonReconciled(male.actorFormID, Channel::maleGenitals, 777);
        session::MarkAddonReconciled(otherMale.actorFormID, Channel::maleGenitals, 777);
        session::Forget(male.actorFormID);
        Check(session::ReconciledAddonSignature(otherMale.actorFormID, Channel::maleGenitals) == 777,
            "one actor detach invalidated another actor using the same armor");

        using Queue = bcn::async_work::FrameTaskQueue;
        Queue queue;
        int applied{};
        const auto channel = bcn::appearance::ChannelValue(
            bcn::appearance::WorkChannel::equipmentVerify);
        for (int i{}; i < 1000; ++i) queue.Submit(0x1234, channel, [&] { ++applied; }, 2);
        Check(queue.Pending() == 1, "NiNode event burst grew duplicate skin work");
        queue.Advance(); Check(!queue.Take(), "rebuild ran before the settled boundary");
        queue.Advance();
        auto job = queue.Take();
        Check(job.has_value(), "coalesced skin job disappeared");
        job->run();
        Check(applied == 1 && !queue.Take(), "rebuild burst executed duplicate skin work");
        auto lease = job->lease;
        queue.Reset(false);
        Check(!Queue::ValidLease(lease), "load retained a stale material callback lease");

        Check(bcn::native_addon::AcceptTargetView(true, true, true),
            "current slot-52 geometry was not admitted to native TXST");
        Check(!bcn::native_addon::AcceptTargetView(false, true, true) &&
                !bcn::native_addon::AcceptTargetView(true, false, true) &&
                !bcn::native_addon::AcceptTargetView(true, true, false),
            "invalid SOS/TNG target was admitted to native TXST");

        bcn::actor_serialization::SerializedActorStateV2 bad{};
        bad.selectedFutanariSkinIndex = 1;
        std::vector<std::string> emptyTable{ "" };
        Check(!bcn::actor_serialization::Decode(bad, emptyTable),
            "corrupt futanari string index was accepted");

        bcn::actor_serialization::SerializedActorStateV2 legacy{};
        const auto legacyDecoded = bcn::actor_serialization::Decode(legacy, emptyTable);
        Check(legacyDecoded.has_value() &&
                std::ranges::all_of(legacyDecoded->overlay.areas, [](const auto& area) {
                    return area.items.empty() && !area.manual;
                }),
            "ASTR v2 compatibility synthesized stale overlay state");

        // ASTR v5 had a futanari ID but no source flags. It can only have come
        // from the old direct UI, so v6 migration must protect it as manual.
        std::vector<std::string> legacyStrings{ "", "LegacyFuta" };
        std::vector<bcn::actor_serialization::SerializedOverlayItemV5> noOverlays;
        bcn::ActorState legacyFutaState{ .futanari = { .selectedSkinId = "LegacyFuta" } };
        const auto legacyFutaWire = bcn::actor_serialization::EncodeV5(legacyFutaState,
            [&](const std::string& value) {
                return value.empty() ? 0U : 1U;
            }, noOverlays);
        const auto migratedFuta = bcn::actor_serialization::Decode(
            legacyFutaWire, noOverlays, legacyStrings);
        Check(migratedFuta && migratedFuta->futanari.manual &&
                migratedFuta->futanari.selectedSkinId == "LegacyFuta",
            "ASTR v5 futanari selection was not protected during migration");
        std::cout << "GenitalPersistenceTests passed (codec/session/common queue; engine integration not exercised)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GenitalPersistenceTests failed: " << error.what() << '\n';
        return 1;
    }
}
