#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Optional SFSCore.dll interface. Never LoadLibrary the plugin. Resolve exports
// after SKSE PostPostLoad; subscribe to sender "Skyrim Fitting System" before
// querying. This is SFS's outfit decision, NOT pixel/alpha/occlusion analysis.
namespace sfs::rendered_outfit_api {
inline constexpr std::uint32_t kVersion = 1;
inline constexpr char kVersionExport[] = "SkyrimFittingSystem_GetRenderedOutfitAPIVersion";
inline constexpr char kQueryExport[] = "SkyrimFittingSystem_QueryRenderedOutfit";
inline constexpr char kGameTaskQueryExport[] = "SkyrimFittingSystem_QueryRenderedOutfitOnGameTask";
inline constexpr char kSender[] = "Skyrim Fitting System";
inline constexpr std::uint32_t kChangedMessage = 0x53465352; // SFSR

enum class Status : std::uint32_t {
  NotManaged = 0, Ready = 1, NotReady = 2, BufferTooSmall = 3,
  InvalidActor = 4, InvalidArgument = 5, WrongThread = 6
};
enum Source : std::uint32_t { Actual = 1, Registered = 2 };
// Registered denotes SFS's additional rendered armors, including compatibility
// additions. It does not guarantee membership in the user's saved card list.
enum ItemFlags : std::uint32_t { OriginalUnknown = 1 };
enum BodyFlags : std::uint32_t { ArmorCuirass = 1, ClothingBody = 2 };
enum Reason : std::uint32_t {
  StateChanged = 1, SceneChanged = 2, AvailabilityChanged = 4, EpochChanged = 8
};

#pragma pack(push, 8)
// Slot bit 0 = Skyrim slot 30; bit 31 = slot 61. All ARMO, not just torso.
// Sorted by (formID, source); no sorting/first-item priority is implied.
// Same-slot different armors remain distinct. Duplicate (formID, source) entries
// are merged. A form may have both Actual and Registered entries.
struct Item {
  std::uint32_t formID;         // Current loaded ARMO, never an RE pointer.
  std::uint32_t originalFormID; // 0 + OriginalUnknown for untraced dynamic forms.
  std::uint32_t declaredSlots;  // ARMO-declared mask (may be zero).
  std::uint32_t visibleSlots;   // Effective SFS-policy slots, not GPU visibility.
  std::uint32_t source;
  std::uint32_t flags;
};
// Caller sets structSize. All other fields are output. On BufferTooSmall no
// items are written; requiredCount reports capacity needed. Requery BOTH header
// and items: the snapshot may have changed between capacity and data calls.
// Ready + requiredCount==0 is intentionally undressed: NEVER fall back to worn.
// NotReady means defer; only NotManaged/absent API permits normal worn fallback.
struct Snapshot {
  std::uint32_t structSize;
  std::uint32_t apiVersion;
  std::uint64_t epoch;
  std::uint64_t revision;
  std::uint64_t sceneGeneration;
  std::uint32_t actorFormID; // Actor REFERENCE, not NPC/base form.
  Status status;
  std::uint32_t requiredCount;
  std::uint32_t visibleSlots;
  std::uint32_t bodyFlags; // Same ArmorCuirass/ClothingBody policy as SFS.
  std::uint32_t reserved;
};
// Borrowed for Dispatch duration only; copy values, never retain data pointer.
// actorFormID==0 + EpochChanged invalidates ALL consumer actor caches.
// Notifications run on a game task, outside SFS locks. Queue morph work instead
// of rebuilding equipment recursively. Always query latest after notification.
struct Changed {
  std::uint32_t structSize;
  std::uint32_t apiVersion;
  std::uint64_t epoch;
  std::uint64_t revision;
  std::uint64_t sceneGeneration;
  std::uint32_t actorFormID;
  Status status;
  std::uint32_t reasons;
  std::uint32_t reserved;
};
#pragma pack(pop)

// Query is game-thread only. First lookup subscribes this actor and may return
// NotReady until an existing display result is published on a game task. Query
// never reruns display/strip rules, equips items, or applies morphs. Subscribe
// to change messages instead of polling all actors every frame. Subscriptions
// are cleared on epoch reset; query again for actors the consumer still needs.
// Output is caller owned. ABI v1 requires itemSize==sizeof(Item), capacity is in
// entries. nullptr items is valid only with zero capacity. No C++ allocator ABI.
// Prefer kGameTaskQueryExport when available (same Query signature/layout).
// Its caller MUST be executing inside SKSE::TaskInterface::AddTask, not Present,
// an input callback, or an arbitrary worker. It does not compare OS thread IDs:
// the SKSE task phase can migrate between threads. All actor/root/readiness and
// revision checks still run. The original export is retained for older clients.
using GetVersion = std::uint32_t (__cdecl*)();
using Query = Status (__cdecl*)(std::uint32_t, Snapshot*, Item*, std::uint32_t,
                                std::uint32_t);
static_assert(sizeof(Item) == 24 && alignof(Item) == 4);
static_assert(sizeof(Snapshot) == 56 && alignof(Snapshot) == 8);
static_assert(sizeof(Changed) == 48 && alignof(Changed) == 8);
static_assert(offsetof(Snapshot, actorFormID) == 32);
static_assert(std::is_trivial_v<Item> && std::is_standard_layout_v<Item>);
static_assert(std::is_trivial_v<Snapshot> && std::is_standard_layout_v<Snapshot>);
static_assert(std::is_trivial_v<Changed> && std::is_standard_layout_v<Changed>);
} // namespace sfs::rendered_outfit_api
