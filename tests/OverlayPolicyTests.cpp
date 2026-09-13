#include "BodyChangeNG/OverlayPolicy.h"
#include "BodyChangeNG/AppearanceColorDrafts.h"
#include "BodyChangeNG/AppearancePreviewState.h"
#include "BodyChangeNG/OverlayProviderConfig.h"
#include "BodyChangeNG/OverlaySlotAccounting.h"
#include "BodyChangeNG/UiText.h"
#include "BodyChangeNG/OverlayReplacementState.h"
#include "BodyChangeNG/OverlayColor.h"
#include "BodyChangeNG/ActorStateSerialization.h"
#include "BodyChangeNG/RaceMenuLegacyStringABI.h"
#include "BodyChangeNG/FrameTaskQueue.h"

#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
    void Check(const bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try {
        bcn::AppearancePreviewState<std::string> selection;
        std::string live = "A";
        selection.Begin(live);
        live = "preview B";
        Check(selection.Saved(live) == "A", "saving a preview serialized its live value");
        selection.Commit("B"); // render task for B has not run yet
        selection.Begin(live);
        live = "preview C"; // latest-wins work superseded B's render task
        Check(selection.Saved(live) == "B", "fast commit/preview lost the committed selection");
        selection.Commit("");
        live = "preview D";
        Check(selection.Saved(live).empty(), "explicit default was overwritten by a preview");
        selection.Reset();
        live = "other save";
        Check(selection.Saved(live) == "other save", "loading another save retained the previous intent");
        bcn::ui::AppearanceColorDrafts<std::uint32_t> drafts;
        drafts.Set(0x14U, 0U, "paint-A", 0x80FF0000U);
        drafts.Set(0x14U, 0U, "paint-B", 0x4000FF00U);
        drafts.Set(0x14U, 1U, "paint-A", 0x200000FFU);
        drafts.Set(0x15U, 0U, "paint-A", 0xFFFFFFFFU);
        Check(drafts.Find(0x14U, 0U, "paint-A") == 0x80FF0000U &&
                drafts.Find(0x14U, 0U, "paint-B") == 0x4000FF00U &&
                drafts.Find(0x14U, 1U, "paint-A") == 0x200000FFU &&
                drafts.Find(0x15U, 0U, "paint-A") == 0xFFFFFFFFU,
            "A/B/A preview colors crossed item, actor, or area boundaries");
        for (unsigned i{}; i < 10000U; ++i) drafts.Set(0x14U, 0U, "paint-A", i);
        Check(drafts.Entries(0x14U).size() == 3U,
            "color dragging accumulated one record per edit");
        auto distributionSnapshot = drafts.Entries(0x14U);
        drafts.EraseActor(0x14U);
        Check(drafts.Entries(0x14U).empty() && drafts.Entries(0x15U).size() == 1U &&
                distributionSnapshot.size() == 3U,
            "reset leaked actors or invalidated a copied distribution snapshot");
        drafts.Clear();
        Check(drafts.Entries(0x15U).empty(), "UI close retained color drafts");
        unsigned measurements{};
        const auto measure = [&](std::string_view text) {
            ++measurements;
            float width{};
            for (const unsigned char ch : text) if ((ch & 0xC0U) != 0x80U) width += 1.0F;
            return width;
        };
        Check(bcn::ui_text::Ellipsize("얼굴 텍스처 경로", 6.0F, measure) == "얼굴 ...",
            "ellipsis split a UTF-8 character or exceeded the row");
        Check(bcn::ui_text::Ellipsize("short", 5.0F, measure) == "short" &&
            bcn::ui_text::Ellipsize("long text", 2.0F, measure).empty(),
            "ellipsis clipped fitting text or overflowed a narrow row");
        measurements = 0U;
        Check(bcn::ui_text::Ellipsize(std::string(10000, 'x'), 60.0F, measure).size() == 60U &&
            measurements < 20U, "long-path clipping regressed to repeated linear truncation");
        struct OwningString {
            std::string value;
            const char* c_str() const { return value.c_str(); }
            ~OwningString() {} // Same ABI distinction as CommonLib BSFixedString.
        };
        using bcn::racemenu_compat::LegacyNodeName;
        static_assert(!std::is_trivially_destructible_v<OwningString>);
        static_assert(std::is_trivially_destructible_v<LegacyNodeName>);
        OwningString owner{ "Body [Ovl7]" };
        const LegacyNodeName borrowed(owner);
        Check(borrowed.data == owner.c_str() &&
                std::string_view(borrowed.data) == "Body [Ovl7]",
            "legacy DLL boundary must receive the string pointer, not the owning object address");
        using namespace bcn::overlay;
        const auto provider = ReadProviderCounts(true, [](const char* path) -> std::optional<double> {
            return std::string_view(path).contains("face") ? 0.0 : 32.0;
        });
        Check(provider && (*provider)[Index(Area::face)] == 0U &&
            (*provider)[Index(Area::body)] == 32U, "actual RaceMenu slot metadata lost");
        for (const double invalid : { -1.0, 128.0, 1.5, double(INFINITY), double(NAN) }) {
            Check(!ReadProviderCounts(true, [=](const char*) { return std::optional<double>(invalid); }),
                "invalid provider count accepted");
        }
        Check(!ReadProviderCounts({}, [](const char*) { return std::optional<double>(3.0); }) &&
            ReadProviderCounts(false, [](const char*) -> std::optional<double> { return {}; }) ==
                ProviderCounts{}, "missing or disabled provider misreported capacity");
        SlotAccounting accounting;
        accounting.Observe(true, false, false); // Another mod, also alpha-zero reservations.
        accounting.Observe(true, false, false);
        accounting.Observe(true, true, false); // BCNG confirmed.
        accounting.Observe(true, true, false);
        Check(accounting.Result(16, 2).capacity == 14 && accounting.Result(16, 2).applied == 2,
            "BCNG applied / available excludes foreign slots incorrectly");
        accounting.Observe(true, false, true); // Temporary preview must NOT lower the denominator.
        Check(accounting.Result(16, 2).capacity == 14 && accounting.Result(16, 2).applied == 2,
            "normal preview changed BCNG count or available capacity");
        SlotAccounting defaultPreview;
        defaultPreview.Observe(true, false, false);
        defaultPreview.Observe(true, false, false);
        Check(defaultPreview.Result(16, 2).capacity == 14 && defaultPreview.Result(16, 2).applied == 2,
            "default preview changed confirmed selections");
        Check(accounting.Result(1, 2).capacity == 0 && accounting.Result(1, 2).applied == 0,
            "slot accounting underflowed a reduced provider capacity");
        const bcn::OverlayItemState saved{ .selectedId = "old", .texturePath = "old.dds",
            .ownedSlot = 2U, .color = 0x8044AA22U };
        PreviewState preview{ .original = { saved },
            .live = bcn::OverlayItemState{ "A", "A.dds", 3U, saved.color } };
        for (const auto* id : { "B", "C", "D" }) {
            Check(preview.original.size() == 1U && preview.original.front().ownedSlot == 2U &&
                    preview.live && preview.live->ownedSlot == 3U &&
                    preview.original.front().color == saved.color,
                "additive preview mutated committed overlay ownership");
            preview.live->selectedId = id;
            preview.live->texturePath = std::string(id) + ".dds";
        }
        preview.liveDefault = true;
        preview.live.reset();
        Check(preview.original.size() == 1U && preview.original.front().selectedId == "old",
            "Default preview discarded the committed restoration snapshot");
        std::vector<bcn::OverlayItemState> committedPaints{ saved };
        PreviewState borrowedPreview{ .original = committedPaints, .live = saved };
        for (std::uint32_t color{}; color < 10000U; ++color) {
            borrowedPreview.live->color = color;
            const auto* restoreColor = borrowedPreview.BorrowedSelection(committedPaints);
            Check(restoreColor && restoreColor->color == saved.color &&
                    restoreColor->ownedSlot == saved.ownedSlot && committedPaints.size() == 1U,
                "batch color preview duplicated/deleted a committed slot or mutated its saved color");
            SlotAccounting borrowedAccounting;
            borrowedAccounting.Observe(true, true, true);
            const auto usage = borrowedAccounting.Result(16U, committedPaints.size());
            Check(usage.applied == 1U && usage.capacity == 16U,
                "borrowed batch preview changed either overlay counter");
        }
        // Restoration follows the latest committed state, never an obsolete
        // snapshot after reset, slot reassignment, or another confirmed edit.
        committedPaints.front().color = 0xFFFFFFFFU;
        Check(borrowedPreview.BorrowedSelection(committedPaints)->color == 0xFFFFFFFFU,
            "cancel restored a stale committed color");
        committedPaints.front().ownedSlot = 7U;
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "cancel borrowed a reassigned slot");
        committedPaints.front() = saved;
        committedPaints.front().texturePath = "foreign.dds";
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "cancel borrowed a foreign texture");
        committedPaints.clear();
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "cancel resurrected a reset selection");
        committedPaints = { saved };
        borrowedPreview.original.clear();
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "new transient paint was mistaken for borrowed paint");
        borrowedPreview.original = committedPaints;
        borrowedPreview.liveDefault = true;
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "default preview borrowed an individual paint");
        borrowedPreview.liveDefault = false;
        borrowedPreview.live.reset();
        Check(!borrowedPreview.BorrowedSelection(committedPaints), "pending preview borrowed a nonexistent node");
        Check(PackColor(UnpackColor(0x0044AA22U)) == 0x0044AA22U &&
                PackColor(UnpackColor(0x8044AA22U)) == 0x8044AA22U &&
                PackColor(UnpackColor(0xFFFFFFFFU)) == 0xFFFFFFFFU,
            "color or alpha zero did not survive the UI conversion");

        bcn::ActorState colored{ .actorFormID = 0x14U };
        for (const auto area : kAreas) {
            colored.overlay.areas[Index(area)].items = { saved,
                bcn::OverlayItemState{ "second", "second.dds", 4U, 0xFFFFFFFFU } };
            colored.overlay.areas[Index(area)].items.front().color =
                0x0044AA22U + static_cast<std::uint32_t>(Index(area)) * 0x40000000U;
        }
        std::vector<std::string> strings{ "" };
        const auto indexFor = [&](const std::string& value) {
            const auto found = std::ranges::find(strings, value);
            if (found != strings.end()) return static_cast<std::uint32_t>(found - strings.begin());
            strings.push_back(value);
            return static_cast<std::uint32_t>(strings.size() - 1U);
        };
        std::vector<bcn::actor_serialization::SerializedOverlayItemV5> overlayItems;
        const auto wire = bcn::actor_serialization::EncodeV5(colored, indexFor, overlayItems);
        const auto restored = bcn::actor_serialization::Decode(wire, overlayItems, strings);
        Check(restored.has_value(), "ASTR v5 multiple-overlay record rejected");
        for (const auto area : kAreas) {
            const auto& items = restored->overlay.areas[Index(area)].items;
            Check(items.size() == 2U &&
                    items.front().color == colored.overlay.areas[Index(area)].items.front().color &&
                    items.front().ownedSlot == 2U && items.back().ownedSlot == 4U,
                "per-item color/opacity/ownership changed across save-load");
        }
        const auto oldWire = bcn::actor_serialization::Encode(colored, indexFor);
        const auto oldRestored = bcn::actor_serialization::Decode(oldWire, strings);
        Check(oldRestored && oldRestored->overlay.areas[0].items.size() == 1U &&
                oldRestored->overlay.areas[0].items.front().color == 0xFFFFFFFFU,
            "ASTR v3 migration must default to white opaque without discarding selections");
        auto corrupt = wire;
        auto corruptItems = overlayItems;
        corruptItems.front().pathIndex = 0xFFFFFFFFU;
        Check(!bcn::actor_serialization::Decode(corrupt, corruptItems, strings),
            "invalid v5 string index accepted");
        Check(CanClaimNode(false, "") && CanClaimNode(false,
                "textures\\actors\\character\\overlays\\default.dds") &&
                !CanClaimNode(true, "") &&
                !CanClaimNode(false, "textures\\dse-soulgem-oven\\veins_cbbe.dds"),
            "a reserved foreign overlay must not be stolen even when alpha is zero");
        Check(OwnsRegisteredOrLive(true, false) &&
                OwnsRegisteredOrLive(false, true) &&
                !OwnsRegisteredOrLive(false, false),
            "a RaceMenu-consumed key lost exact live-node ownership or a foreign node was accepted");
        Check(LegacyOverlayDefaultCount(Area::body) == 3U &&
                LegacyOverlayDefaultCount(Area::hands) == 3U &&
                LegacyOverlayDefaultCount(Area::feet) == 3U &&
                LegacyOverlayDefaultCount(Area::face) == 3U &&
                LegacyOverlayNodePrefix(Area::body) == "Body" &&
                LegacyOverlayNodePrefix(Area::hands) == "Hands" &&
                LegacyOverlayNodePrefix(Area::feet) == "Feet" &&
                LegacyOverlayNodePrefix(Area::face) == "Face",
            "RaceMenu v1 overlay defaults or exact node prefixes changed");
        Check(NormalizeTexturePath("  ./Textures/Actors/Paint.DDS  ") ==
                "textures\\actors\\paint.dds",
            "texture path normalization is not stable");
        Check(IsRaceMenuDefaultTexture(
                "textures/actors/character/overlays/default.dds") &&
                IsRaceMenuDefaultTexture("IGNORE") &&
                !IsRaceMenuDefaultTexture("textures/paint/body.dds"),
            "RaceMenu default/ignore filtering regressed");
        Check(ClassifyLayout("LDD Makeup UBE", "textures/paint.dds") == Layout::ube &&
                ClassifyLayout("Community", "textures/paint.dds",
                    R"(C:\mods\!Community Overlays UBE RaceMenu\paint.dds)") == Layout::ube &&
                ClassifyLayout("Cube tattoo", "textures/cube.dds") == Layout::legacy,
            "overlay UBE token/provider classification regressed");
        Check(ClassifySex("Female paint", "paint.dds") == Sex::female &&
                ClassifySex("Male paint", "paint.dds") == Sex::male &&
                ClassifySex("General paint", "paint.dds") == Sex::unisex &&
                ClassifySex("UBE paint", "paint.dds") == Sex::female,
            "overlay sex classification regressed");
        const auto cbbe = bcn::body_family::Bit(bcn::body_family::Family::cbbe);
        const auto ube = bcn::body_family::Bit(bcn::body_family::Family::ube);
        const auto himbo = bcn::body_family::Bit(bcn::body_family::Family::himbo);
        Check(EntryMatchesActor(Layout::legacy, Sex::female, cbbe, true) &&
                !EntryMatchesActor(Layout::ube, Sex::female, cbbe, true) &&
                EntryMatchesActor(Layout::ube, Sex::female, ube, true) &&
                !EntryMatchesActor(Layout::legacy, Sex::female, ube, true) &&
                EntryMatchesActor(Layout::legacy, Sex::unisex, himbo, false) &&
                !EntryMatchesActor(Layout::ube, Sex::female, himbo, false) &&
                EntryMatchesActor(Layout::legacy, Sex::unisex, 0U, true) &&
                EntryMatchesActor(Layout::ube, Sex::unisex, 0U, true),
            "actor-specific overlay sex/layout filtering regressed");
        Check(StableId(Area::body, "Textures/Paint/A.dds") ==
                StableId(Area::body, "textures\\paint\\a.dds") &&
                StableId(Area::body, "paint.dds") != StableId(Area::face, "paint.dds"),
            "existing overlay relative-path ID stability regressed");

        const auto layers = TextureLayers(
            "Textures/Paint/Diffuse.dds|ignore|Textures/Paint/Normal.dds||"
            "Textures/Paint/Env.dds");
        Check(layers.size() == 3U &&
                layers[0].index == 0U && layers[0].path == "textures\\paint\\diffuse.dds" &&
                layers[1].index == 2U && layers[1].path == "textures\\paint\\normal.dds" &&
                layers[2].index == 4U && layers[2].path == "textures\\paint\\env.dds",
            "packed RaceMenu texture layers crossed indices");
        Check(TextureLayers("|||||||").empty(),
            "an empty packed overlay was accepted");

        std::array<std::optional<std::string>, 8U> storedPaths;
        const auto readStored = [&](const std::uint8_t index) { return storedPaths[index]; };
        for (const auto* prefix : { "", "textures\\", "Data/Textures/" }) {
            storedPaths[0] = std::string(prefix) + "TouchedbyDibella/HeartPubeM.dds";
            Check(StoredPathsMatch("touchedbydibella\\heartpubem.dds", readStored),
                "same BCNG overlay resource with a loader prefix was rejected");
            Check(!StoredPathsMatch("otherpack\\heartpubem.dds", readStored),
                "different mod's same filename was treated as owned");
        }
        storedPaths[1] = "Foreign/Normal.dds";
        Check(!StoredPathsMatch("TouchedbyDibella/HeartPubeM.dds", readStored),
            "foreign companion map protection was weakened");
        storedPaths[1].reset();
        // Repeated replacements/color ownership checks keep the same node;
        // only that node's exact current resource is accepted each time.
        for (const auto* name : { "A", "B", "C", "A" }) {
            const auto selected = std::string("Paint/") + name + ".dds";
            storedPaths[0] = "textures/" + selected;
            Check(StoredPathsMatch(selected, readStored), "replacement/color ownership drifted");
        }
        storedPaths[0] = "Textures/Textures/Paint/A.dds";
        Check(!StoredPathsMatch("Paint/A.dds", readStored),
            "duplicate texture roots were silently treated as equivalent");

        // Exercise the production replacement ordering against a stored-key
        // table: ownership/scalars must never disappear between operations.
        storedPaths.fill(std::nullopt);
        storedPaths[0] = "old.dds";
        storedPaths[1] = "old_normal.dds";
        std::uint32_t tint = 0x8044AA22U;
        std::vector<std::string> operations;
        ReplaceRegisteredPaint("old.dds|old_normal.dds", "new.dds||new_mask.dds",
            [&](const TextureLayer& layer) {
                Check(storedPaths[0].has_value() && tint == 0x8044AA22U,
                    "replacement erased ownership or color before upserting");
                storedPaths[layer.index] = layer.path;
                operations.push_back("write" + std::to_string(layer.index));
            }, [&] {
                Check(storedPaths[0] == "new.dds" && storedPaths[2] == "new_mask.dds",
                    "scalar commit preceded texture registration");
                tint = 0x00FF0000U;
                operations.push_back("color");
            }, [&](std::uint8_t index) {
                Check(tint == 0x00FF0000U && storedPaths[0] == "new.dds",
                    "obsolete map removed before new registration completed");
                storedPaths[index].reset();
                operations.push_back("remove" + std::to_string(index));
            });
        Check(operations == std::vector<std::string>{"write0", "write2", "color", "remove1"} &&
                StoredPathsMatch("new.dds||new_mask.dds", readStored),
            "companion maps accumulated or shared keys were deleted");
        for (unsigned i{}; i < 64U; ++i) {
            const auto next = "paint" + std::to_string(i) + ".dds||new_mask.dds";
            ReplaceRegisteredPaint("new.dds||new_mask.dds", next,
                [&](const TextureLayer& layer) { storedPaths[layer.index] = layer.path; },
                [&] { tint = i << 24U; },
                [&](std::uint8_t) { Check(false, "same layout replacement removed a key"); });
            Check(StoredPathsMatch(next, readStored) && tint == (i << 24U),
                "repeated paint/opacity replacement drifted");
        }
        Check(IsFaceSourceFeature(4U, false) && IsFaceSourceFeature(5U, false) &&
                !IsFaceSourceFeature(5U, true) && !IsFaceSourceFeature(0U, false),
            "native FaceGen head rejected or paint mistaken for a face source");

        // Checkbox snapshots accumulate across areas and reconcile against the
        // same production policy, including deferred node-slot reservations.
        {
            using Item = bcn::OverlayItemState;
            constexpr std::size_t areaCount = 4U, slotCount = 6U;
            std::array<std::vector<Item>, areaCount> checked, shown;
            std::array<std::array<std::optional<Item>, slotCount>, areaCount> nodes;
            for (auto& area : nodes) {
                area[0] = Item{ "foreign", "foreign.dds", 0U, 0xFF112233U };
                area[1] = Item{ "committed", "committed.dds", 1U, 0xFF445566U };
            }
            const auto initialNodes = nodes;
            const auto reconcile = [&](const std::size_t area, const std::vector<Item>& wanted) {
                PreviewState stackOwner{ .original = { *initialNodes[area][1] }, .batchMode = true,
                    .batch = shown[area] };
                std::vector<Item> deferred;
                shown[area] = ReconcilePreviewItems(shown[area], wanted,
                    [&](const Item& removed) {
                        const std::vector<Item> committed{ *initialNodes[area][1] };
                        if (const auto* borrowed = stackOwner.BorrowedSelection(removed, committed)) {
                            nodes[area][removed.ownedSlot] = *borrowed;
                        } else {
                            const auto& node = nodes[area][removed.ownedSlot];
                            if (node && node->texturePath == removed.texturePath)
                                nodes[area][removed.ownedSlot].reset();
                        }
                    },
                    [&](const Item& incoming, const Item* previous,
                        std::span<const Item> reserved) -> std::optional<Item> {
                        std::optional<std::uint8_t> slot;
                        if (previous) slot = previous->ownedSlot;
                        else if (incoming.selectedId == "committed") slot = std::uint8_t{ 1U };
                        else {
                            for (std::uint8_t i{}; i < slotCount; ++i) {
                                if (!nodes[area][i] && std::ranges::none_of(reserved,
                                    [i](const auto& item) { return item.ownedSlot == i; })) {
                                    slot = i;
                                    break;
                                }
                            }
                        }
                        if (!slot) return {};
                        auto item = incoming;
                        item.ownedSlot = *slot;
                        deferred.push_back(item);
                        return item;
                    });
                // RaceMenu's writes become visible only after the entire batch
                // has chosen slots. A free-slot scan alone would collide here.
                std::set<std::uint8_t> slots;
                for (const auto& item : shown[area]) Check(slots.insert(item.ownedSlot).second,
                    "deferred checkbox previews allocated a duplicate slot");
                for (const auto& item : deferred) nodes[area][item.ownedSlot] = item;
                Check(nodes[area][0]->texturePath == "foreign.dds" &&
                        nodes[area][0]->color == 0xFF112233U,
                    "checkbox preview touched a foreign node");
            };
            const auto item = [](const char* id, const std::uint32_t color = 0xFFFFFFFFU) {
                return Item{ id, std::string(id) + ".dds", kNoOwnedSlot, color };
            };
            checked[0] = { item("A"), item("B"), item("C") };
            reconcile(0U, checked[0]);
            Check(shown[0].size() == 3U, "checkboxes did not accumulate in one area");
            const auto retainedSlot = shown[0][0].ownedSlot;
            checked[0] = { item("A", 0x4000FF00U), item("C"), item("D") };
            reconcile(0U, checked[0]);
            Check(shown[0].size() == 3U && shown[0][0].ownedSlot == retainedSlot &&
                    shown[0][0].color == 0x4000FF00U &&
                    std::ranges::none_of(shown[0], [](const auto& x) { return x.selectedId == "B"; }),
                "uncheck/color edit removed a retained preview or left an unchecked one");
            checked[0] = { item("committed", 0x80445566U), item("A"), item("C"), item("D"),
                item("E"), item("F") };
            reconcile(0U, checked[0]);
            Check(shown[0].size() == 5U && checked[0].size() == 6U,
                "capacity overflow overwrote a foreign slot or lost checked candidates");
            SlotAccounting stackUsage;
            PreviewState stackOwner{ .original = { *initialNodes[0][1] }, .batchMode = true, .batch = shown[0] };
            for (std::uint8_t i{}; i < slotCount; ++i) {
                bool ownsPreview{};
                stackOwner.VisitLive([&](const auto& x) { ownsPreview |= x.ownedSlot == i; });
                stackUsage.Observe(nodes[0][i].has_value(), i == 1U, ownsPreview);
            }
            Check(stackUsage.Result(slotCount, 1U).applied == 1U &&
                    stackUsage.Result(slotCount, 1U).capacity == 5U,
                "checkbox stack changed committed slot counts");
            reconcile(0U, {});
            Check(shown[0].empty() && nodes[0][1]->color == 0xFF445566U &&
                    std::ranges::count_if(nodes[0], [](const auto& n) { return n.has_value(); }) == 2,
                "clear selection failed to restore the committed stack/color");

            bcn::async_work::FrameTaskQueue batch;
            const auto submit = [&](const std::size_t area) {
                const auto snapshot = checked[area];
                Check(batch.Submit(0x14U, static_cast<std::uint32_t>(area + 1U),
                    [&, area, snapshot] { reconcile(area, snapshot); }), "checkbox snapshot queue failed");
            };
            const auto settle = [&] {
                for (unsigned tick{}; tick < 24U; ++tick) {
                    batch.Advance();
                    if (auto work = batch.Take()) work->run();
                }
                Check(batch.Pending() == 0U, "checkbox preview queue did not settle");
            };
            for (std::uint32_t click{}; click < 1000U; ++click) {
                const auto area = click % areaCount;
                checked[area] = { item("A", click), item("B", click + 1U) };
                if (click % 3U == 0U) checked[area].erase(checked[area].begin());
                submit(area);
                Check(batch.Pending() <= 4U, "checkbox edits grew the per-area work queue");
                if (click % 7U == 0U) settle();
            }
            settle();
            for (std::size_t area{}; area < areaCount; ++area) {
                Check(shown[area].size() == checked[area].size(),
                    "cross-area selection erased another area's checked previews");
                for (const auto& desired : checked[area]) {
                    const auto found = std::ranges::find(shown[area], desired.selectedId, &Item::selectedId);
                    Check(found != shown[area].end() && found->color == desired.color,
                        "latest checkbox/color snapshot was not rendered");
                }
                checked[area].clear();
                submit(area); // close/uncheck must supersede a not-yet-run add
            }
            settle();
            for (std::size_t area{}; area < areaCount; ++area) {
                Check(shown[area].empty() && nodes[area][1]->color == 0xFF445566U &&
                        std::ranges::count_if(nodes[area], [](const auto& n) { return n.has_value(); }) == 2,
                    "closing distribution leaked preview nodes or changed saved paints");
            }
        }
        // Same lease mechanism as the runtime finalizer: cancellation cannot
        // publish the proposed choice before a provider registration occurs.
        bcn::async_work::FrameTaskQueue queue;
        std::string published = "old";
        Check(queue.Submit(0x14U, 1U, [] {}, 1U), "could not queue selection");
        queue.Advance();
        auto first = queue.Take();
        Check(first.has_value(), "could not dispatch selection");
        Check(queue.Submit(0U, 0U, [&] { published = "new"; }, 1U,
                false, false, first->lease), "could not queue registration receipt");
        queue.CancelActor(0x14U);
        queue.Advance();
        auto receipt = queue.Take();
        if (receipt) receipt->run();
        Check(!receipt && queue.Pending() == 0U && published == "old",
            "cancelled finalizer published a selection it never registered");
        Check(BipedMask(Area::body) == 4U &&
                BipedMask(Area::hands) == 8U &&
                BipedMask(Area::feet) == 128U &&
                BipedMask(Area::face) == 0U,
            "area-specific RaceMenu biped masks changed");
        Check(Index(Area::face) != Index(Area::body) &&
                Index(Area::body) != Index(Area::hands) &&
                Index(Area::hands) != Index(Area::feet),
            "overlay areas no longer own independent state indices");

        std::cout << "OverlayPolicyTests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "OverlayPolicyTests failed: " << error.what() << '\n';
        return 1;
    }
}
