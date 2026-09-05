#include "BodyChangeNG/RaceMenuBodyMorph.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/Settings.h"

#include <RE/B/BSVisit.h>
#include <RE/N/NiStringExtraData.h>
#include <SKSE/Logger.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    void QueueActorTask(const RE::ActorHandle& handle, std::uint32_t channel, std::function<void()> work)
    {
        const auto actor = handle.get();
        if (actor) bcn::frame_tasks::Queue(actor->GetFormID(), std::move(work), 1, channel, channel == 200);
    }
    namespace skee
    {
        class IPluginInterface
        {
        public:
            virtual ~IPluginInterface() = default;
            virtual std::uint32_t GetVersion() = 0;
            virtual void Revert() = 0;
        };

        class IInterfaceMap
        {
        public:
            virtual IPluginInterface* QueryInterface(const char* a_name) = 0;
            virtual bool AddInterface(const char* a_name, IPluginInterface* a_pluginInterface) = 0;
            virtual IPluginInterface* RemoveInterface(const char* a_name) = 0;
        };

        struct InterfaceExchangeMessage
        {
            enum : std::uint32_t { kExchangeInterface = 0x9E3779B9 };
            IInterfaceMap* interfaceMap{};
        };

        // This is the shared RaceMenu BodyMorph v4/v5 prefix. Version 5 only
        // appends AddMorphShapeCallback, so every entry used here retains the
        // same slot on SE v4 and AE v5.
        class IBodyMorphInterface : public IPluginInterface
        {
        public:
            class MorphKeyVisitor { public: virtual void Visit(const char*, float) = 0; };
            class StringVisitor { public: virtual void Visit(const char*) = 0; };
            class ActorVisitor { public: virtual void Visit(RE::TESObjectREFR*) = 0; };
            class MorphValueVisitor { public: virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float) = 0; };
            class MorphVisitor { public: virtual void Visit(RE::TESObjectREFR*, const char*) = 0; };

            virtual void SetMorph(RE::TESObjectREFR*, const char*, const char*, float) = 0;
            virtual float GetMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
            virtual void ClearMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
            virtual float GetBodyMorphs(RE::TESObjectREFR*, const char*) = 0;
            virtual void ClearBodyMorphNames(RE::TESObjectREFR*, const char*) = 0;
            virtual void VisitMorphs(RE::TESObjectREFR*, MorphVisitor&) = 0;
            virtual void VisitKeys(RE::TESObjectREFR*, const char*, MorphKeyVisitor&) = 0;
            virtual void VisitMorphValues(RE::TESObjectREFR*, MorphValueVisitor&) = 0;
            virtual void ClearMorphs(RE::TESObjectREFR*) = 0;
            virtual void ApplyVertexDiff(RE::TESObjectREFR*, RE::NiAVObject*, bool = false) = 0;
            virtual void ApplyBodyMorphs(RE::TESObjectREFR*, bool = true) = 0;
            virtual void UpdateModelWeight(RE::TESObjectREFR*, bool = false) = 0;
            virtual void SetCacheLimit(std::size_t) = 0;
            virtual bool HasMorphs(RE::TESObjectREFR*) = 0;
            virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR*) = 0;
            virtual bool HasBodyMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
            virtual bool HasBodyMorphName(RE::TESObjectREFR*, const char*) = 0;
            virtual bool HasBodyMorphKey(RE::TESObjectREFR*, const char*) = 0;
            virtual void ClearBodyMorphKeys(RE::TESObjectREFR*, const char*) = 0;
            virtual void VisitStrings(StringVisitor&) = 0;
            virtual void VisitActors(ActorVisitor&) = 0;
            virtual std::size_t ClearMorphCache() = 0;
        };
    }

    constexpr auto kCommittedKey = "BodyChangeNG";
    constexpr auto kPreviewKey = "BodyChangeNGPreview";
    constexpr auto kOutfitKey = "BodyChangeNGOutfit";
    constexpr auto kLegacyCommittedKey = "BodyChangerNG";
    constexpr auto kLegacyPreviewKey = "BodyChangerNGPreview";
    constexpr auto kLegacyOutfitKey = "BodyChangerNGOutfit";
    // OBody NG stores every body-slider contribution under this public
    // NiOverride key (see OBodyNative.psc).  Clearing only this key is the
    // safe migration path for saves where OBody NG has been disabled: its old
    // values would otherwise be added to the new Body Change NG preset.
    constexpr auto kLegacyOBodyKey = "OBody";
    constexpr auto kLegacyOClotheKey = "OClothe";
    std::atomic<skee::IBodyMorphInterface*> g_bodyMorph{};
    std::atomic<skee::IInterfaceMap*> g_interfaceMap{};
    std::atomic_uint32_t g_version{};
    std::mutex g_initializeLock;
    std::mutex g_previewLock;
    std::mutex g_selectionLock;
    std::mutex g_applyGenerationLock;
    RE::ActorHandle g_previewActor;
    std::uint64_t g_previewGeneration{};
    std::atomic_uint64_t g_nextApplyGeneration{ 1U };
    std::unordered_map<RE::FormID, std::string> g_currentPresetIds;
    std::unordered_map<std::uint64_t, std::uint64_t> g_applyGenerations;

    [[nodiscard]] std::uint64_t ApplyGenerationKey(
        const RE::FormID actorFormID, const bcn::racemenu::ApplyMode mode) noexcept
    {
        return (static_cast<std::uint64_t>(actorFormID) << 8U) |
            static_cast<std::uint8_t>(mode);
    }

    [[nodiscard]] std::uint64_t BeginApply(
        const RE::FormID actorFormID, const bcn::racemenu::ApplyMode mode)
    {
        std::scoped_lock lock(g_applyGenerationLock);
        const auto generation = g_nextApplyGeneration.fetch_add(1U, std::memory_order_relaxed);
        g_applyGenerations.insert_or_assign(ApplyGenerationKey(actorFormID, mode), generation);
        return generation;
    }

    [[nodiscard]] bool IsCurrentApply(const RE::FormID actorFormID,
        const bcn::racemenu::ApplyMode mode, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_applyGenerationLock);
        const auto found = g_applyGenerations.find(ApplyGenerationKey(actorFormID, mode));
        return found != g_applyGenerations.end() && found->second == generation;
    }

    [[nodiscard]] std::uint64_t CurrentApplyGeneration(
        const RE::FormID actorFormID, const bcn::racemenu::ApplyMode mode)
    {
        std::scoped_lock lock(g_applyGenerationLock);
        const auto found = g_applyGenerations.find(ApplyGenerationKey(actorFormID, mode));
        return found == g_applyGenerations.end() ? 0U : found->second;
    }

    void InvalidateActorApplies(const RE::FormID actorFormID)
    {
        std::scoped_lock lock(g_applyGenerationLock);
        for (const auto mode : { bcn::racemenu::ApplyMode::preview,
                 bcn::racemenu::ApplyMode::commit, bcn::racemenu::ApplyMode::outfit }) {
            g_applyGenerations.insert_or_assign(ApplyGenerationKey(actorFormID, mode),
                g_nextApplyGeneration.fetch_add(1U, std::memory_order_relaxed));
        }
    }

    std::uint64_t InvalidateApply(const RE::FormID actorFormID, const bcn::racemenu::ApplyMode mode)
    {
        std::scoped_lock lock(g_applyGenerationLock);
        const auto generation = g_nextApplyGeneration.fetch_add(1U, std::memory_order_relaxed);
        g_applyGenerations.insert_or_assign(ApplyGenerationKey(actorFormID, mode), generation);
        SKSE::log::debug("BodyAudit invalidated actor={:08X} mode={} generation={}",
            actorFormID, static_cast<std::uint32_t>(mode), generation);
        return generation;
    }

    // A preview is layered above the last committed Body Change NG preset.
    // Collect that committed key first so the preview can cancel every old
    // slider that is absent from the newly previewed preset as well.
    class OwnedMorphCollector final : public skee::IBodyMorphInterface::MorphValueVisitor
    {
    public:
        explicit OwnedMorphCollector(const std::string_view key) : key_(key) {}

        void Visit(RE::TESObjectREFR*, const char* name, const char* key, const float value) override
        {
            if (name && *name && key && key_ == key) values.emplace_back(name, value);
        }

        std::vector<std::pair<std::string, float>> values;

    private:
        std::string_view key_;
    };

    void MigrateLegacyBodyChangeKeys(skee::IBodyMorphInterface& bodyMorph, RE::Actor* actor)
    {
        if (!actor) return;
        bool migrated{};
        if (bodyMorph.HasBodyMorphKey(actor, kLegacyCommittedKey)) {
            if (!bodyMorph.HasBodyMorphKey(actor, kCommittedKey)) {
                OwnedMorphCollector legacy{ kLegacyCommittedKey };
                bodyMorph.VisitMorphValues(actor, legacy);
                for (const auto& [name, value] : legacy.values) {
                    bodyMorph.SetMorph(actor, name.c_str(), kCommittedKey, value);
                }
            }
            bodyMorph.ClearBodyMorphKeys(actor, kLegacyCommittedKey);
            migrated = true;
        }
        for (const auto* key : { kLegacyPreviewKey, kLegacyOutfitKey }) {
            if (!bodyMorph.HasBodyMorphKey(actor, key)) continue;
            bodyMorph.ClearBodyMorphKeys(actor, key);
            migrated = true;
        }
        if (migrated) {
            SKSE::log::info("Body Change NG migrated legacy BodyChangerNG morph keys for actor {:08X}",
                actor->GetFormID());
        }
    }

    class MorphActorCollector final : public skee::IBodyMorphInterface::ActorVisitor
    {
    public:
        void Visit(RE::TESObjectREFR* reference) override
        {
            if (reference) actorFormIDs.push_back(reference->GetFormID());
        }

        std::vector<RE::FormID> actorFormIDs;
    };

    [[nodiscard]] skee::IBodyMorphInterface* Interface() noexcept
    {
        return g_bodyMorph.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool IsCurrentPreview(const RE::ActorHandle& actorHandle, const std::uint64_t generation)
    {
        std::scoped_lock lock(g_previewLock);
        return g_previewGeneration == generation && g_previewActor == actorHandle;
    }

    [[nodiscard]] std::pair<RE::ActorHandle, std::uint64_t> BeginPreview(const RE::ActorHandle& actorHandle)
    {
        std::scoped_lock lock(g_previewLock);
        const auto previous = g_previewActor;
        g_previewActor = actorHandle;
        return { previous, ++g_previewGeneration };
    }

    [[nodiscard]] RE::ActorHandle CancelPreviewTracking(const RE::ActorHandle owner = {})
    {
        std::scoped_lock lock(g_previewLock);
        // Automatic work on a different NPC must not cancel the UI actor's
        // preview. Explicit UI close still cancels globally (empty owner).
        if (!bcn::async_work::MayCancelPreview(static_cast<bool>(owner), owner == g_previewActor)) return {};
        auto previous = g_previewActor;
        g_previewActor.reset();
        ++g_previewGeneration;
        return previous;
    }

    void LogBodyTriState(RE::Actor* actor, const std::string_view reason)
    {
        // Scene-graph traversal is intentionally a debug-only diagnostic.
        // Running it twice for every automatically distributed NPC was more
        // expensive than the rule lookup it was meant to observe.
        const auto* logger = spdlog::default_logger_raw();
        if (!actor || !logger || !logger->should_log(spdlog::level::debug)) return;
        for (const bool firstPerson : { false, true }) {
            if (firstPerson && actor != RE::PlayerCharacter::GetSingleton()) continue;
            auto* root = actor->Get3D(firstPerson);
            std::size_t count{};
            if (root) {
                RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* object) {
                    if (!object) return RE::BSVisit::BSVisitControl::kContinue;
                    const auto* data = object->GetExtraData<RE::NiStringExtraData>("BODYTRI");
                    if (!data || !data->value || data->value[0] == '\0') {
                        return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    ++count;
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            }
            SKSE::log::debug("BodyAudit BODYTRI actor={:08X} view={} reason='{}' count={}",
                actor->GetFormID(), firstPerson ? "first-person" : "third-person", reason, count);
        }
    }

    void ApplyVisibleMorphs(skee::IBodyMorphInterface& bodyMorph, RE::Actor* actor,
        const bool deferUpdate)
    {
        LogBodyTriState(actor, "before apply");
        // Preserve the public RaceMenu refresh preference. On the main thread
        // RaceMenu may still do synchronous work even with deferUpdate=true;
        // frame_tasks owns BCNG's cadence/budget, not this API flag.
        bodyMorph.ApplyBodyMorphs(actor, deferUpdate);
        LogBodyTriState(actor, "after apply");
    }

    void ClearPreviewNow(const RE::ActorHandle actorHandle)
    {
        auto* bodyMorph = Interface();
        const auto actor = actorHandle.get();
        if (!bodyMorph || !actor || !actor->Is3DLoaded()) return;
        const auto hadPreview = bodyMorph->HasBodyMorphKey(actor.get(), kPreviewKey) ||
            bodyMorph->HasBodyMorphKey(actor.get(), kLegacyPreviewKey);
        bodyMorph->ClearBodyMorphKeys(actor.get(), kPreviewKey);
        bodyMorph->ClearBodyMorphKeys(actor.get(), kLegacyPreviewKey);
        if (hadPreview) ApplyVisibleMorphs(*bodyMorph, actor.get(), false);
        SKSE::log::debug("Body Change NG cleared preview morphs for actor {:08X}", actor->GetFormID());
    }

    [[nodiscard]] std::uint64_t StableRandomSeed(const RE::FormID actorFormID, const std::string_view presetName,
                                                  const std::string_view sliderName, const std::uint32_t salt)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto append = [&hash](const std::uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
        for (const auto value : { actorFormID, salt }) {
            append(static_cast<std::uint8_t>(value));
            append(static_cast<std::uint8_t>(value >> 8U));
            append(static_cast<std::uint8_t>(value >> 16U));
            append(static_cast<std::uint8_t>(value >> 24U));
        }
        for (const auto character : presetName) append(static_cast<std::uint8_t>(character));
        for (const auto character : sliderName) append(static_cast<std::uint8_t>(character));
        return hash;
    }

    [[nodiscard]] float StableRange(const RE::FormID actorFormID, const std::string_view presetName,
                                    const std::string_view sliderName, const std::uint32_t salt,
                                    const float low, const float high)
    {
        const auto seed = StableRandomSeed(actorFormID, presetName, sliderName, salt);
        const auto fraction = static_cast<float>(seed & 0x00FFFFFFULL) / static_cast<float>(0x00FFFFFFULL);
        return low + (high - low) * fraction;
    }

    [[nodiscard]] bool StableChance(const RE::FormID actorFormID, const std::string_view presetName,
                                    const std::string_view sliderName, const std::uint32_t salt, const float chance)
    {
        return StableRange(actorFormID, presetName, sliderName, salt, 0.0F, 1.0F) < chance;
    }

    [[nodiscard]] bool IsNippleRefitSlider(const std::string_view sliderName)
    {
        std::string lowered{ sliderName };
        std::ranges::transform(lowered, lowered.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return lowered.contains("nipple") || lowered.contains("areola") || lowered.starts_with("nip");
    }

    void ApplyNow(RE::ActorHandle actorHandle, bcn::BodyPreset preset, const bcn::racemenu::ApplyMode mode,
                  const std::uint64_t applyGeneration, const std::uint64_t previewGeneration = 0,
                  const std::uint64_t outfitSignature = 0U,
                  const bcn::racemenu::UpdatePolicy updatePolicy =
                      bcn::racemenu::UpdatePolicy::synchronous)
    {
        const auto startedAt = std::chrono::steady_clock::now();
        if (mode == bcn::racemenu::ApplyMode::preview && !IsCurrentPreview(actorHandle, previewGeneration)) {
            SKSE::log::debug("Body Change NG discarded a stale body preview task");
            return;
        }
        auto* bodyMorph = Interface();
        if (!bodyMorph) {
            SKSE::log::error("Body Change NG could not apply '{}': RaceMenu BodyMorph is unavailable", preset.name);
            return;
        }
        if (preset.sliders.empty()) {
            SKSE::log::warn("Body Change NG could not apply '{}': the preset has no sliders", preset.name);
            return;
        }
        const auto actor = actorHandle.get();
        if (!actor) {
            SKSE::log::warn("Body Change NG could not apply '{}': the selected actor handle expired", preset.name);
            return;
        }
        if (!actor->Is3DLoaded()) {
            SKSE::log::warn("Body Change NG could not apply '{}' to actor {:08X}: 3D is not loaded", preset.name,
                            actor->GetFormID());
            return;
        }
        if (!IsCurrentApply(actor->GetFormID(), mode, applyGeneration)) {
            SKSE::log::debug(
                "BodyAudit superseded preset='{}' actor={:08X} mode={} requested-generation={} current-generation={}",
                preset.name, actor->GetFormID(), static_cast<std::uint32_t>(mode), applyGeneration,
                CurrentApplyGeneration(actor->GetFormID(), mode));
            return;
        }

        const auto actorBase = actor->GetActorBase();
        if (preset.cachedContentHash != bcn::PresetCatalog::Get().ContentHash(preset.PersistentId())) {
            [[maybe_unused]] const auto refreshed = bcn::racemenu::QueueApply(actor.get(),
                preset.PersistentId(), mode, outfitSignature, updatePolicy);
            return;
        }
        if (!actorBase || preset.male != (actorBase->GetSex() != RE::SEX::kFemale) ||
            !bcn::body_family::Matches(bcn::body_family::PresetMask(preset.family, preset.male),
                bcn::body_family::ResolveActor(actor.get()))) return;
        const auto weight = std::clamp(actorBase ? actorBase->GetWeight() / 100.0F : 0.0F, 0.0F, 1.0F);
        const auto settings = bcn::Settings::Get().MorphOptions();
        const auto key = mode == bcn::racemenu::ApplyMode::preview ? kPreviewKey :
            mode == bcn::racemenu::ApplyMode::outfit ? kOutfitKey : kCommittedKey;
        MigrateLegacyBodyChangeKeys(*bodyMorph, actor.get());
        if (mode != bcn::racemenu::ApplyMode::outfit && bodyMorph->HasBodyMorphKey(actor.get(), kLegacyOBodyKey)) {
            bodyMorph->ClearBodyMorphKeys(actor.get(), kLegacyOBodyKey);
            SKSE::log::info("Body Change NG cleared legacy OBody NG morphs for actor {:08X}", actor->GetFormID());
        }
        if (mode != bcn::racemenu::ApplyMode::outfit && bodyMorph->HasBodyMorphKey(actor.get(), kLegacyOClotheKey)) {
            bodyMorph->ClearBodyMorphKeys(actor.get(), kLegacyOClotheKey);
            SKSE::log::info("Body Change NG cleared legacy OBody NG ORefit morphs for actor {:08X}", actor->GetFormID());
        }
        std::unordered_map<std::string, float> desiredMorphs;
        if (mode == bcn::racemenu::ApplyMode::outfit) {
            bodyMorph->ClearBodyMorphKeys(actor.get(), key);
            for (const auto& slider : preset.sliders) {
                // A named -Refit preset is still governed by the UI's
                // dependent nipple-refit switch.
                if (!settings.outfitNippleCorrection && IsNippleRefitSlider(slider.name)) continue;
                const auto value = slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight;
                bodyMorph->SetMorph(actor.get(), slider.name.c_str(), key, value);
            }
        } else {
            // Commit starts from a clean set of keys owned by this mod.  The
            // outfit task queued after a body change rebuilds its correction
            // against the new preset. Preview keeps the current outfit layer
            // and only replaces its own transient key.
            if (mode == bcn::racemenu::ApplyMode::commit) {
                bodyMorph->ClearBodyMorphKeys(actor.get(), kPreviewKey);
                bodyMorph->ClearBodyMorphKeys(actor.get(), kOutfitKey);
            }
            bodyMorph->ClearBodyMorphKeys(actor.get(), key);

            const auto actorMale = !actorBase || actorBase->GetSex() != RE::SEX::kFemale;
            const auto detectedActorFamily = bcn::body_family::ResolveActor(actor.get());
            const auto universeFamily = detectedActorFamily != 0U ? detectedActorFamily :
                bcn::body_family::PresetMask(preset.family, preset.male);
            const auto universe = bcn::PresetCatalog::Get().CompatibleSliderUniverse(actorMale, universeFamily);
            desiredMorphs.reserve((universe ? universe->size() : 0U) + 40U);
            if (universe) {
                // A BodySlide XML may deliberately omit a slider to mean zero.
                // Seed the entire compatible-family universe with zero before
                // overlaying values explicitly authored by this preset.
                for (const auto& name : *universe) desiredMorphs.try_emplace(name, 0.0F);
            }
            for (const auto& slider : preset.sliders) {
                desiredMorphs.insert_or_assign(
                    slider.name, slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight);
            }
        }
        if (mode == bcn::racemenu::ApplyMode::commit && actorBase && actorBase->GetSex() == RE::SEX::kFemale) {
            const auto presetFamily = bcn::body_family::PresetMask(preset.family, false);
            const auto femaleFamily = bcn::body_morph_policy::ResolveFemaleFamily(
                bcn::body_family::ResolveActor(actor.get()), presetFamily);
            const auto setRandom = [&](const char* name, const std::uint32_t salt, const float low, const float high) {
                desiredMorphs.insert_or_assign(
                    name, StableRange(actor->GetFormID(), preset.name, name, salt, low, high));
            };
            if (settings.nippleRandomization &&
                femaleFamily == bcn::body_morph_policy::FemaleFamily::cbbe3ba) {
                const auto smallAreola = StableChance(actor->GetFormID(), preset.name, "AreolaSize", 2, .15F);
                setRandom("AreolaSize", 1, smallAreola ? -1.0F : 0.0F, smallAreola ? 0.0F : 1.0F);
                if (StableChance(actor->GetFormID(), preset.name, "AreolaPull_v2", 3, .75F)) setRandom("AreolaPull_v2", 4, -.25F, 1.0F);
                const auto longerNipple = StableChance(actor->GetFormID(), preset.name, "NippleLength", 6, .15F);
                setRandom("NippleLength", 5, longerNipple ? .2F : 0.0F, longerNipple ? .3F : .1F);
                setRandom("NippleManga", 7, -.3F, .8F);
                if (StableChance(actor->GetFormID(), preset.name, "NipplePerkManga", 8, .25F)) setRandom("NipplePerkManga", 9, -.3F, 1.2F);
                if (StableChance(actor->GetFormID(), preset.name, "NipBGone", 10, .15F)) setRandom("NipBGone", 11, .6F, 1.0F);
                setRandom("NippleSize", 12, -.5F, .3F);
                setRandom("NippleDip", 13, 0.0F, 1.0F);
                setRandom("NippleCrease_v2", 14, -.4F, 1.0F);
                if (StableChance(actor->GetFormID(), preset.name, "NipplePuffy_v2", 15, .06F)) setRandom("NipplePuffy_v2", 16, .4F, .7F);
                if (StableChance(actor->GetFormID(), preset.name, "NippleThicc_v2", 17, .35F)) setRandom("NippleThicc_v2", 18, 0.0F, .9F);
                if (StableChance(actor->GetFormID(), preset.name, "NippleInvert_v2", 19, .02F)) setRandom("NippleInvert_v2", 20, .65F, 1.0F);
            } else if (settings.nippleRandomization &&
                femaleFamily == bcn::body_morph_policy::FemaleFamily::ube) {
                const auto largeAreola = StableChance(
                    actor->GetFormID(), preset.name, "AreolaeSizeBig", 101, .45F);
                const auto areolaSize = StableRange(actor->GetFormID(), preset.name,
                    largeAreola ? "AreolaeSizeBig" : "AreolaeSizeSmall", 102, .05F, .45F);
                desiredMorphs.insert_or_assign(largeAreola ? "AreolaeSizeBig" : "AreolaeSizeSmall", areolaSize);
                desiredMorphs.insert_or_assign(
                    largeAreola ? "AreolaeSizeBig_UV_fix" : "AreolaeSizeSmall_UV_fix", areolaSize);
                setRandom("AreolaErection", 103, 0.0F, .35F);
                setRandom("NippleLength", 104, 0.0F, .18F);
                setRandom("NippleDiameter n|p", 105, -.20F, .20F);
                setRandom("NipplesPerkiness", 107, 0.0F, .35F);
                if (StableChance(actor->GetFormID(), preset.name, "NippleCircularCrease", 108, .20F)) {
                    setRandom("NippleCircularCrease", 109, 0.0F, .35F);
                }
                if (StableChance(actor->GetFormID(), preset.name, "Nippleinverted", 110, .04F)) {
                    setRandom("Nippleinverted", 111, .25F, .65F);
                }
            }
            if (settings.genitalRandomization &&
                femaleFamily == bcn::body_morph_policy::FemaleFamily::cbbe3ba) {
                const auto innie = StableChance(actor->GetFormID(), preset.name, "Innieoutie", 21, .20F);
                const auto average = !innie && StableChance(actor->GetFormID(), preset.name, "Innieoutie", 22, .75F);
                setRandom("Innieoutie", 23, innie ? .95F : average ? .4F : -.25F, innie ? 1.1F : average ? .75F : .3F);
                setRandom("Labiapuffyness", 24, innie ? .75F : average ? .5F : .2F, innie ? 1.25F : average ? 1.0F : .5F);
                setRandom("LabiaMorePuffyness_v2", 25, 0.0F, innie ? 1.0F : average ? .75F : .35F);
                setRandom("Labiaprotrude", 26, 0.0F, innie ? .5F : 1.0F);
                setRandom("Labiaprotrude2", 27, 0.0F, innie ? .1F : average ? .75F : 1.0F);
                setRandom("Labiaprotrudeback", 28, 0.0F, innie ? .1F : 1.0F);
                setRandom("Labiaspread", 29, 0.0F, innie ? 0.0F : 1.0F);
                setRandom("LabiaCrumpled_v2", 30, 0.0F, innie ? .3F : average ? .7F : 1.0F);
                setRandom("LabiaBulgogi_v2", 31, 0.0F, innie ? 0.0F : average ? .3F : 1.0F);
                setRandom("LabiaNeat_v2", 32, 0.0F, innie || average ? 0.0F : .25F);
                setRandom("VaginaHole", 33, innie ? -.2F : average ? -.2F : 0.0F, innie ? .05F : average ? .4F : 1.0F);
                setRandom("Clit", 34, -.4F, .25F);
                setRandom("Vaginasize", 35, 0.0F, 1.0F);
                setRandom("ClitSwell_v2", 36, -.3F, 1.1F);
                setRandom("Cutepuffyness", 37, 0.0F, 1.0F);
                setRandom("LabiaTightUp", 38, 0.0F, 1.0F);
                setRandom("CBPC", 39, StableChance(actor->GetFormID(), preset.name, "CBPC", 40, .60F) ? -.25F : .6F,
                    StableChance(actor->GetFormID(), preset.name, "CBPC", 40, .60F) ? .25F : 1.0F);
                setRandom("AnalPosition_v2", 41, 0.0F, 1.0F);
                setRandom("AnalTexPos_v2", 42, 0.0F, 1.0F);
                setRandom("AnalTexPosRe_v2", 43, 0.0F, 1.0F);
                desiredMorphs.insert_or_assign("AnalLoose_v2", -.1F);
            } else if (settings.genitalRandomization &&
                femaleFamily == bcn::body_morph_policy::FemaleFamily::ube) {
                // UBE 2.0 Release Body.osp names from the live TAKEALOOK
                // installation. Deliberately avoid hidden/opening controls
                // such as Vagina_spread and AnusSpread.
                setRandom("AnusSize", 121, -.10F, .30F);
                if (StableChance(actor->GetFormID(), preset.name, "BiggerAnus", 122, .20F)) {
                    setRandom("BiggerAnus", 123, 0.0F, .25F);
                }
                if (StableChance(actor->GetFormID(), preset.name, "AnusTriangular", 124, .25F)) {
                    setRandom("AnusTriangular", 125, 0.0F, .30F);
                }
                setRandom("ClitorisErection", 126, -.10F, .25F);
                setRandom("PussyCute", 127, 0.0F, .35F);
                setRandom("Vagina_shape", 128, -.20F, .25F);
                setRandom("Vagina_shape_wider", 129, -.10F, .20F);
            }
        }
        if (mode != bcn::racemenu::ApplyMode::outfit) {
            std::size_t correctionCount{};
            for (const auto& [name, desired] : desiredMorphs) {
                // Never delete another mod's key. Instead, store the exact
                // compensating delta under Body Change NG's own key so the
                // evaluated result equals the selected preset. Preview keeps
                // the existing outfit correction visible.
                const auto outfitValue = mode == bcn::racemenu::ApplyMode::preview ?
                    bodyMorph->GetMorph(actor.get(), name.c_str(), kOutfitKey) : 0.0F;
                const auto correction = bcn::racemenu::AbsolutePresetCorrection(
                    desired, bodyMorph->GetBodyMorphs(actor.get(), name.c_str()), outfitValue);
                if (std::abs(correction) <= 0.00001F) continue;
                bodyMorph->SetMorph(actor.get(), name.c_str(), key, correction);
                ++correctionCount;
            }
            SKSE::log::debug(
                "BodyAudit normalized preset='{}' actor={:08X} target-sliders={} stored-corrections={} family-mask={}",
                preset.name, actor->GetFormID(), desiredMorphs.size(), correctionCount,
                bcn::body_family::ResolveActor(actor.get()));
        }
        const auto firstSliderValue = bodyMorph->GetMorph(
            actor.get(), preset.sliders.front().name.c_str(), key);
        SKSE::log::debug("BodyAudit stored preset='{}' key='{}' actor={:08X} generation={} sliders={} first='{}' value={}",
            preset.name, key, actor->GetFormID(), applyGeneration, preset.sliders.size(),
            preset.sliders.front().name, firstSliderValue);
        // UI requests keep RaceMenu's partition update synchronous so an older
        // internal morph job cannot arrive after a later list selection.
        // Automatic distribution and outfit correction explicitly select the
        // deferred policy to avoid blocking a dense-cell frame.
        ApplyVisibleMorphs(*bodyMorph, actor.get(),
            updatePolicy == bcn::racemenu::UpdatePolicy::deferred);
        SKSE::log::debug("BodyAudit applied preset='{}' actor={:08X} generation={} key-present={} first-value={}",
            preset.name, actor->GetFormID(), applyGeneration, bodyMorph->HasBodyMorphKey(actor.get(), key),
            bodyMorph->GetMorph(actor.get(), preset.sliders.front().name.c_str(), key));
        if (mode == bcn::racemenu::ApplyMode::preview && !IsCurrentPreview(actorHandle, previewGeneration)) {
            ClearPreviewNow(actorHandle);
            return;
        }
        SKSE::log::debug("Body Change NG {} {} sliders for preset '{}' to actor {:08X} ({})",
            mode == bcn::racemenu::ApplyMode::preview ? "previewed" : "applied",
            preset.sliders.size(), preset.name, actor->GetFormID(), "OBody-compatible refresh");
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count();
        SKSE::log::debug("BodyAudit completed preset='{}' actor={:08X} generation={} elapsed-ms={} update={}",
            preset.name, actor->GetFormID(), applyGeneration, elapsed,
            updatePolicy == bcn::racemenu::UpdatePolicy::deferred ? "deferred" : "synchronous");
        if (elapsed >= 16) {
            SKSE::log::warn(
                "Body Change NG body morph setup exceeded one 60-FPS frame: preset='{}' actor={:08X} sliders={} elapsed-ms={} update={}",
                preset.name, actor->GetFormID(), preset.sliders.size(), elapsed,
                updatePolicy == bcn::racemenu::UpdatePolicy::deferred ? "deferred" : "synchronous");
        }
        if (mode == bcn::racemenu::ApplyMode::commit) {
            if (!IsCurrentApply(actor->GetFormID(), mode, applyGeneration)) return;
            if (preset.cachedContentHash != bcn::PresetCatalog::Get().ContentHash(preset.PersistentId())) {
                [[maybe_unused]] const auto refreshed = bcn::racemenu::QueueApply(actor.get(),
                    preset.PersistentId(), mode, outfitSignature, updatePolicy);
                return;
            }
            {
                std::scoped_lock lock(g_selectionLock);
                g_currentPresetIds[actor->GetFormID()] = preset.PersistentId();
            }
            bcn::ActorRegistry::Get().MarkBodyApplied(actor.get(), preset.PersistentId(), false);
        } else if (mode == bcn::racemenu::ApplyMode::outfit) {
            bcn::ActorRegistry::Get().MarkOutfitApplied(actor.get(), outfitSignature);
        }
    }

    void ApplyProceduralOutfitNow(RE::ActorHandle actorHandle, const std::uint64_t outfitSignature)
    {
        auto* bodyMorph = Interface();
        const auto actor = actorHandle.get();
        if (!bodyMorph || !actor || !actor->Is3DLoaded()) return;

        const auto base = actor->GetActorBase();
        const auto weight = std::clamp(base ? base->GetWeight() / 100.0F : 0.0F, 0.0F, 1.0F);
        MigrateLegacyBodyChangeKeys(*bodyMorph, actor.get());
        const auto derive = [&](const char* name, const float target) {
            bodyMorph->SetMorph(actor.get(), name, kOutfitKey,
                bcn::racemenu::OutfitTargetCorrection(target, bodyMorph->GetBodyMorphs(actor.get(), name)));
        };
        const auto fixed = [&](const char* name, const float low, const float high) {
            bodyMorph->SetMorph(actor.get(), name, kOutfitKey, low + (high - low) * weight);
        };

        bodyMorph->ClearBodyMorphKeys(actor.get(), kOutfitKey);
        // The procedural fallback below is authored for female breast/nipple
        // sliders. Male actors may still use an explicitly supplied male
        // outfit preset, but must never receive this female fallback.
        if (!base || base->GetSex() != RE::SEX::kFemale) {
            ApplyVisibleMorphs(*bodyMorph, actor.get(), true);
            bcn::ActorRegistry::Get().MarkOutfitApplied(actor.get(), outfitSignature);
            return;
        }
        bcn::body_family::Mask presetFamily{};
        if (const auto currentPresetId = bcn::racemenu::CurrentPresetId(actor.get())) {
            if (const auto currentPreset = bcn::PresetCatalog::Get().Find(*currentPresetId)) {
                presetFamily = bcn::body_family::PresetMask(currentPreset->family, false);
            }
        }
        const auto femaleFamily = bcn::body_morph_policy::ResolveFemaleFamily(
            bcn::body_family::ResolveActor(actor.get()), presetFamily);
        const auto settings = bcn::Settings::Get().MorphOptions();
        if (femaleFamily == bcn::body_morph_policy::FemaleFamily::cbbe3ba) {
            derive("BreastSideShape", 0.0F);
            derive("BreastUnderDepth", 0.0F);
            derive("BreastCleavage", 1.0F);
            fixed("BreastGravity2", -0.1F, -0.05F);
            fixed("BreastTopSlope", -0.2F, -0.35F);
            fixed("BreastsTogether", 0.3F, 0.35F);
            fixed("Breasts", -0.05F, -0.05F);
            fixed("BreastHeight", 0.15F, 0.15F);
            derive("ButtDimples", 0.0F);
            derive("ButtUnderFold", 0.0F);
            fixed("AppleCheeks", -0.05F, -0.05F);
            fixed("Butt", -0.05F, -0.05F);
            derive("Clavicle_v2", 0.0F);
            derive("NavelEven", 1.0F);
            derive("HipCarved", 0.0F);

            if (settings.outfitNippleCorrection) {
                derive("NippleDip", 0.0F);
                derive("NippleTip", 0.0F);
                derive("NipplePuffy_v2", 0.0F);
                derive("AreolaSize", -0.3F);
                derive("NipBGone", 1.0F);
                fixed("NippleDistance", 0.05F, 0.08F);
                fixed("NippleDown", 0.0F, -0.1F);
                derive("NipplePerkManga", -0.25F);
            }
        } else if (femaleFamily == bcn::body_morph_policy::FemaleFamily::ube) {
            // UBE uses a different breast/nipple vocabulary; writing the 3BA
            // names produces no correction and can leave a false "applied"
            // signature. These targets are present in UBE 2.0's body and
            // outfit SliderSets in TAKEALOOK.
            derive("Big_SaggyBreasts", 0.0F);
            derive("SaggingBreastsZone", 0.0F);
            derive("Juicy_breasts", 0.0F);
            derive("BreastsCupSag n|p", 0.0F);
            derive("BreastsRotate_Y", 0.0F);
            fixed("Breasts_Perky", .10F, .15F);
            if (settings.outfitNippleCorrection) {
                derive("AreolaErection", 0.0F);
                derive("NippleLength", 0.0F);
                derive("NipplesShowUp", 0.0F);
                derive("Nipples_Fantasy", 0.0F);
                derive("Nippleinverted", 0.0F);
                derive("NippleInverted_PuffyAreola", 0.0F);
                derive("NippleInverted_PuffyAreola_UV_fix", 0.0F);
                derive("NippleFlatten", 1.0F);
            }
        }
        // Outfit correction is an automatic runtime operation. RaceMenu's
        // deferred partition update avoids blocking the frame that delivered
        // the equip/actor event while preserving the final morph keys.
        ApplyVisibleMorphs(*bodyMorph, actor.get(), true);
        bcn::ActorRegistry::Get().MarkOutfitApplied(actor.get(), outfitSignature);
    }
}

namespace bcn::racemenu
{
    void Initialize()
    {
        if (IsReady()) return;
        std::scoped_lock lock(g_initializeLock);
        if (IsReady()) return;

        const auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) return;
        skee::InterfaceExchangeMessage message{};
        if (!messaging->Dispatch(skee::InterfaceExchangeMessage::kExchangeInterface, &message, sizeof(message), "skee") ||
            !message.interfaceMap) {
            SKSE::log::warn("Body Change NG could not exchange RaceMenu interfaces yet");
            return;
        }
        const auto bodyMorph = static_cast<skee::IBodyMorphInterface*>(message.interfaceMap->QueryInterface("BodyMorph"));
        if (!bodyMorph) {
            SKSE::log::warn("Body Change NG could not obtain RaceMenu's BodyMorph interface");
            return;
        }
        const auto version = bodyMorph->GetVersion();
        if (version < 4U || version > 5U) {
            SKSE::log::warn("Body Change NG rejected unsupported RaceMenu BodyMorph interface version {}", version);
            return;
        }
        g_version.store(version, std::memory_order_release);
        g_interfaceMap.store(message.interfaceMap, std::memory_order_release);
        g_bodyMorph.store(bodyMorph, std::memory_order_release);
        SKSE::log::info("Body Change NG received RaceMenu BodyMorph interface version {}", version);
    }

    void ResetSessionState()
    {
        {
            std::scoped_lock lock(g_applyGenerationLock);
            g_applyGenerations.clear();
        }
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentPresetIds.clear();
        }
        [[maybe_unused]] const auto previousPreview = CancelPreviewTracking();
    }

    bool IsReady() noexcept
    {
        return Interface() != nullptr;
    }

    std::uint32_t Version() noexcept
    {
        return g_version.load(std::memory_order_acquire);
    }

    void* QueryInterface(const char* name) noexcept
    {
        if (!name || name[0] == '\0') return nullptr;
        Initialize();
        auto* interfaceMap = g_interfaceMap.load(std::memory_order_acquire);
        return interfaceMap ? interfaceMap->QueryInterface(name) : nullptr;
    }

    std::optional<std::string> CurrentPresetId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        {
            std::scoped_lock lock(g_selectionLock);
            const auto found = g_currentPresetIds.find(actor->GetFormID());
            if (found != g_currentPresetIds.end()) return found->second.empty() ?
                std::nullopt : std::optional<std::string>(found->second);
        }
        return bcn::ActorRegistry::Get().AppliedBodyId(actor);
    }

    std::optional<bool> LiveBodyChangeStateMatches(const RE::Actor* actor, const bool expectDefault)
    {
        auto* bodyMorph = Interface();
        if (!actor || !bodyMorph) return std::nullopt;
        auto* reference = const_cast<RE::Actor*>(actor);
        const auto hasCommitted = bodyMorph->HasBodyMorphKey(reference, kCommittedKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyCommittedKey);
        if (!expectDefault) return hasCommitted;
        const auto hasAnyOwned = hasCommitted ||
            bodyMorph->HasBodyMorphKey(reference, kPreviewKey) ||
            bodyMorph->HasBodyMorphKey(reference, kOutfitKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyPreviewKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyOutfitKey);
        return !hasAnyOwned;
    }

    bool HasOutfitCorrection(const RE::Actor* actor)
    {
        auto* bodyMorph = Interface();
        if (!actor || !bodyMorph) return false;
        auto* reference = const_cast<RE::Actor*>(actor);
        return bodyMorph->HasBodyMorphKey(reference, kOutfitKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyOutfitKey);
    }

    bool HasActivePreview(const RE::Actor* actor)
    {
        std::scoped_lock lock(g_previewLock);
        return actor && g_previewActor.get().get() == actor;
    }

    void ForgetActorState(const std::uint32_t actorFormID)
    {
        if (actorFormID == 0) return;
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentPresetIds.erase(actorFormID);
        }
        {
            std::scoped_lock lock(g_applyGenerationLock);
            for (const auto mode : { ApplyMode::preview, ApplyMode::commit, ApplyMode::outfit }) {
                g_applyGenerations.erase(ApplyGenerationKey(actorFormID, mode));
            }
        }
    }

    ApplyResult QueueApply(RE::Actor* actor, std::string presetId, const ApplyMode mode,
        const std::uint64_t outfitSignature, const UpdatePolicy updatePolicy)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!IsReady()) return ApplyResult::unavailable;
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        const auto found = PresetCatalog::Get().Find(presetId, mode == ApplyMode::outfit);
        if (!found) return ApplyResult::missingPreset;
        if (found->sliders.empty()) return ApplyResult::emptyPreset;
        const auto* base = actor->GetActorBase();
        if (!base) return ApplyResult::invalidActor;
        const auto actorMale = base->GetSex() != RE::SEX::kFemale;
        if (found->male != actorMale) return ApplyResult::incompatibleSex;
        if (!bcn::body_family::Matches(
            bcn::body_family::PresetMask(found->family, found->male),
            bcn::body_family::ResolveActor(actor))) return ApplyResult::incompatibleBodyFamily;
        const auto actorHandle = actor->GetHandle();
        const auto applyGeneration = BeginApply(actor->GetFormID(), mode);
        SKSE::log::debug("BodyAudit requested preset='{}' id='{}' actor={:08X} mode={} generation={} sliders={}",
            found->name, found->PersistentId(), actor->GetFormID(), static_cast<std::uint32_t>(mode),
            applyGeneration, found->sliders.size());
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            if (mode == ApplyMode::commit) {
                // Commit clears the outfit key even if the preset ID did not
                // change. Its completion signature must not suppress repaint.
                bcn::ActorRegistry::Get().InvalidateOutfit(actor);
                // Reflect the newest accepted user choice in the list
                // immediately. Do this only after the game task interface was
                // acquired so a failed submission cannot show a false current
                // preset.
                std::scoped_lock lock(g_selectionLock);
                g_currentPresetIds[actor->GetFormID()] = found->PersistentId();
            }
            auto preset = *found;
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            if (mode == ApplyMode::preview) {
                const auto [previousActor, generation] = BeginPreview(actorHandle);
                if (previousActor && previousActor != actorHandle) {
                    QueueActorTask(previousActor, 203, [previousActor, session] {
                        if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                    });
                }
                QueueActorTask(actorHandle, 200, [actorHandle, preset = std::move(preset), applyGeneration, generation,
                    updatePolicy] mutable {
                    ApplyNow(actorHandle, std::move(preset), ApplyMode::preview, applyGeneration,
                        generation, 0U, updatePolicy);
                });
            } else {
                const auto previousActor = mode == ApplyMode::commit ? CancelPreviewTracking(actorHandle) : RE::ActorHandle{};
                if (previousActor && previousActor != actorHandle) {
                    QueueActorTask(previousActor, 203, [previousActor, session] {
                        if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                    });
                }
                QueueActorTask(actorHandle, 200 + static_cast<std::uint32_t>(mode), [actorHandle, preset = std::move(preset), mode, applyGeneration,
                    outfitSignature, updatePolicy] mutable {
                    ApplyNow(actorHandle, std::move(preset), mode, applyGeneration, 0U,
                        outfitSignature, updatePolicy);
                });
            }
            return ApplyResult::queued;
        }
        return ApplyResult::noTaskInterface;
    }

    void QueueReapplyCurrent(RE::Actor* actor)
    {
        const auto presetId = CurrentPresetId(actor);
        if (!presetId) return;
        SKSE::log::debug("BodyAudit rebuild-reapply requested id='{}' actor={:08X}",
            *presetId, actor ? actor->GetFormID() : 0U);
        if (QueueApply(actor, *presetId, ApplyMode::commit) == ApplyResult::queued) {
            bcn::OutfitRefit::Get().ProcessActor(actor);
        }
    }

    ApplyResult QueueApplyOutfit(RE::Actor* actor, std::string refitPresetId,
        const std::uint64_t outfitSignature)
    {
        return QueueApply(actor, std::move(refitPresetId), ApplyMode::outfit,
            outfitSignature, UpdatePolicy::deferred);
    }

    void QueueCancelPreview()
    {
        const auto actorHandle = CancelPreviewTracking();
        if (!actorHandle) return;
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            QueueActorTask(actorHandle, 203, [actorHandle, session] {
                if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(actorHandle);
            });
        }
    }

    void QueueApplyProceduralOutfit(RE::Actor* actor, const std::uint64_t outfitSignature)
    {
        if (!IsReady() || !actor) return;
        const auto actorHandle = actor->GetHandle();
        const auto generation = BeginApply(actor->GetFormID(), ApplyMode::outfit);
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            QueueActorTask(actorHandle, 202, [actorHandle, outfitSignature, session, generation] {
                const auto actor = actorHandle.get();
                if (actor && bcn::ActorRegistry::Get().SessionGeneration() == session &&
                    IsCurrentApply(actor->GetFormID(), ApplyMode::outfit, generation)) {
                    ApplyProceduralOutfitNow(actorHandle, outfitSignature);
                }
            });
        }
    }

    void QueueClearOutfit(RE::Actor* actor, const std::uint64_t outfitSignature)
    {
        if (!IsReady() || !actor) return;
        // Clearing a refit must never cancel a body preset that was just
        // queued by the same UI click.  Body, preview and outfit use separate
        // RaceMenu keys and therefore require separate generations as well.
        const auto generation = InvalidateApply(actor->GetFormID(), ApplyMode::outfit);
        const auto actorHandle = actor->GetHandle();
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            QueueActorTask(actorHandle, 202, [actorHandle, outfitSignature, session, generation] {
                if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
                auto* bodyMorph = Interface();
                const auto resolved = actorHandle.get();
                if (!bodyMorph || !resolved || !resolved->Is3DLoaded()) return;
                if (!IsCurrentApply(resolved->GetFormID(), ApplyMode::outfit, generation)) return;
                const auto hadOutfitMorph =
                    bodyMorph->HasBodyMorphKey(resolved.get(), kOutfitKey) ||
                    bodyMorph->HasBodyMorphKey(resolved.get(), kLegacyOutfitKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kOutfitKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyOutfitKey);
                // Do not rebuild every body just to clear a key that never
                // existed. This was a full synchronous morph pass even when
                // outfit correction was disabled or the actor was naked.
                if (hadOutfitMorph) ApplyVisibleMorphs(*bodyMorph, resolved.get(), true);
                bcn::ActorRegistry::Get().MarkOutfitApplied(resolved.get(), outfitSignature);
            });
        }
    }

    void QueueClearBodyChangeMorphs(RE::Actor* actor)
    {
        if (!bcn::frame_tasks::Active() || !IsReady() || !actor) return;
        InvalidateActorApplies(actor->GetFormID());
        const auto clearGeneration = BeginApply(actor->GetFormID(), ApplyMode::commit);
        {
            std::scoped_lock lock(g_selectionLock);
            // A pending default must not fall back to an older applied ID.
            g_currentPresetIds[actor->GetFormID()] = {};
        }
        const auto actorHandle = actor->GetHandle();
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            const auto previousActor = CancelPreviewTracking(actorHandle);
            if (previousActor && previousActor != actorHandle) {
                QueueActorTask(previousActor, 203, [previousActor, session] {
                    if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                });
            }
            QueueActorTask(actorHandle, 201, [actorHandle, session, clearGeneration] {
                if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
                auto* bodyMorph = Interface();
                const auto resolved = actorHandle.get();
                if (!bodyMorph || !resolved) return;
                if (!IsCurrentApply(resolved->GetFormID(), ApplyMode::commit, clearGeneration)) return;
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kPreviewKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kCommittedKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kOutfitKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyPreviewKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyCommittedKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyOutfitKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyOBodyKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyOClotheKey);
                if (resolved->Is3DLoaded()) ApplyVisibleMorphs(*bodyMorph, resolved.get(), false);
                bcn::ActorRegistry::Get().MarkBodyApplied(resolved.get(), {}, true);
            });
        }
    }

    bool QueueClearAllBodyChangeMorphs()
    {
        if (!bcn::frame_tasks::Active() || !IsReady()) return false;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return false;
        {
            std::scoped_lock lock(g_selectionLock);
            g_currentPresetIds.clear();
        }
        [[maybe_unused]] const auto previousActor = CancelPreviewTracking();
        const auto session = bcn::ActorRegistry::Get().SessionGeneration();
        bcn::frame_tasks::Queue(0, [session] {
            if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
            auto* bodyMorph = Interface();
            if (!bodyMorph) return;
            MorphActorCollector collector;
            bodyMorph->VisitActors(collector);
            std::ranges::sort(collector.actorFormIDs);
            collector.actorFormIDs.erase(
                std::unique(collector.actorFormIDs.begin(), collector.actorFormIDs.end()),
                collector.actorFormIDs.end());
            std::size_t cleared{};
            for (const auto formID : collector.actorFormIDs) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (!actor) continue;
                const auto owned = bodyMorph->HasBodyMorphKey(actor, kPreviewKey) ||
                    bodyMorph->HasBodyMorphKey(actor, kCommittedKey) ||
                    bodyMorph->HasBodyMorphKey(actor, kOutfitKey) ||
                    bodyMorph->HasBodyMorphKey(actor, kLegacyPreviewKey) ||
                    bodyMorph->HasBodyMorphKey(actor, kLegacyCommittedKey) ||
                    bodyMorph->HasBodyMorphKey(actor, kLegacyOutfitKey);
                if (!owned) continue;
                InvalidateActorApplies(formID);
                const auto generation = BeginApply(formID, ApplyMode::commit);
                const auto handle = actor->GetHandle();
                QueueActorTask(handle, 201, [handle, generation] {
                    const auto resolved = handle.get();
                    auto* morph = Interface();
                    if (!resolved || !morph || !IsCurrentApply(resolved->GetFormID(), ApplyMode::commit, generation)) return;
                    // Preserve the original bulk-reset scope: only BCNG keys.
                    for (const auto key : {kPreviewKey, kCommittedKey, kOutfitKey,
                             kLegacyPreviewKey, kLegacyCommittedKey, kLegacyOutfitKey}) morph->ClearBodyMorphKeys(resolved.get(), key);
                    if (resolved->Is3DLoaded()) ApplyVisibleMorphs(*morph, resolved.get(), false);
                });
                ++cleared;
            }
            SKSE::log::info("Body Change NG queued owned body morph reset for {} saved actors", cleared);
        });
        return true;
    }
}
