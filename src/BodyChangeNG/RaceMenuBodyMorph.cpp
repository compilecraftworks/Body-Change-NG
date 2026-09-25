#include "BodyChangeNG/RaceMenuBodyMorph.h"

#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/RenderedOutfit.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/BodyRandomizationPolicy.h"
#include "BodyChangeNG/UbeGenitalRandomization.h"
#include "BodyChangeNG/UbeNippleRandomization.h"
#include "BodyChangeNG/UbeAnatomyRuntime.h"
#include "BodyChangeNG/BodyMorphKeys.h"
#include "BodyChangeNG/RaceMenuCompatibility.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/SliderName.h"
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
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    void QueueActorTask(const RE::ActorHandle& handle,
        const bcn::appearance::WorkChannel channel, std::function<void()> work)
    {
        const auto actor = handle.get();
        if (!actor) return;
        if (channel == bcn::appearance::WorkChannel::bodyPreviewCleanup) {
            bcn::frame_tasks::QueueRestoration(actor->GetFormID(), std::move(work), 1U, channel);
        } else {
            bcn::frame_tasks::Queue(actor->GetFormID(), std::move(work), 1U,
                channel, channel == bcn::appearance::WorkChannel::bodyPreview);
        }
    }

    [[nodiscard]] constexpr bcn::appearance::WorkChannel ChannelForApplyMode(
        const bcn::racemenu::ApplyMode mode) noexcept
    {
        switch (mode) {
        case bcn::racemenu::ApplyMode::preview:
            return bcn::appearance::WorkChannel::bodyPreview;
        case bcn::racemenu::ApplyMode::outfit:
            return bcn::appearance::WorkChannel::outfitRefit;
        default:
            return bcn::appearance::WorkChannel::bodyCommit;
        }
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

        // Shared BodyMorph v4/v5 prefix through VisitActors. Early v4 (0.4.12)
        // does not yet declare ClearMorphCache. Do not include unused tail
        // methods just because a later header reports the same ABI version.
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
            // Not called: old v4 takes UInt32, newer sources use skee_u64.
            virtual void ReservedSetCacheLimit() = 0;
            virtual bool HasMorphs(RE::TESObjectREFR*) = 0;
            virtual std::uint32_t EvaluateBodyMorphs(RE::TESObjectREFR*) = 0;
            virtual bool HasBodyMorph(RE::TESObjectREFR*, const char*, const char*) = 0;
            virtual bool HasBodyMorphName(RE::TESObjectREFR*, const char*) = 0;
            virtual bool HasBodyMorphKey(RE::TESObjectREFR*, const char*) = 0;
            virtual void ClearBodyMorphKeys(RE::TESObjectREFR*, const char*) = 0;
            virtual void VisitStrings(StringVisitor&) = 0;
            virtual void VisitActors(ActorVisitor&) = 0;
        };
    }

    constexpr auto kCommittedKey = bcn::racemenu::keys::body;
    constexpr auto kPreviewKey = bcn::racemenu::keys::preview;
    constexpr auto kOutfitKey = bcn::racemenu::keys::outfit;
    constexpr auto kLegacyCommittedKey = bcn::racemenu::keys::legacyBody;
    constexpr auto kLegacyPreviewKey = bcn::racemenu::keys::legacyPreview;
    constexpr auto kLegacyOutfitKey = bcn::racemenu::keys::legacyOutfit;
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

    class PreviewBaseCollector final : public skee::IBodyMorphInterface::MorphValueVisitor
    {
    public:
        PreviewBaseCollector(const bool preserve, const bool replaceOutfit) : base(preserve, replaceOutfit) {}
        void Visit(RE::TESObjectREFR*, const char* name, const char* key, const float value) override
        {
            base.Visit(name, key, value);
        }
        bcn::racemenu::keys::PreviewBase base;
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

    void ApplyVisibleMorphs(skee::IBodyMorphInterface& bodyMorph, RE::Actor* actor,
        const bool deferUpdate)
    {
        // Preserve the public RaceMenu refresh preference. On the main thread
        // RaceMenu may still do synchronous work even with deferUpdate=true;
        // frame_tasks owns BCNG's cadence/budget, not this API flag.
        bodyMorph.ApplyBodyMorphs(actor, deferUpdate);
    }

    void ClearPreviewNow(const RE::ActorHandle actorHandle)
    {
        if (!bcn::frame_tasks::CurrentWorkAllowed()) return;
        auto* bodyMorph = Interface();
        const auto actor = actorHandle.get();
        if (!bodyMorph || !actor || bcn::racemenu::HasActivePreview(actor.get())) return;
        const auto hadPreview = bodyMorph->HasBodyMorphKey(actor.get(), kPreviewKey) ||
            bodyMorph->HasBodyMorphKey(actor.get(), kLegacyPreviewKey);
        if (!bcn::frame_tasks::CurrentWorkAllowed()) return;
        bcn::racemenu::keys::ClearPreview(*bodyMorph, actor.get());
        // Stored morph keys belong to the reference even when its 3D is gone.
        // Only the visual refresh needs loaded geometry.
        if (hadPreview && actor->Is3DLoaded()) ApplyVisibleMorphs(*bodyMorph, actor.get(), false);
        // Display changes received during a body preview were intentionally
        // deferred. Re-evaluate the final SFS outfit once preview ends.
        if (actor->Is3DLoaded()) bcn::rendered_outfit::Request(actor.get());
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
        if (!bcn::frame_tasks::CurrentWorkAllowed()) return;
        if (mode == bcn::racemenu::ApplyMode::preview && !IsCurrentPreview(actorHandle, previewGeneration)) {
            return;
        }
        auto* bodyMorph = Interface();
        if (!bodyMorph) {
            SKSE::log::error("Body Change NG could not apply '{}': RaceMenu BodyMorph is unavailable", preset.name);
            return;
        }
        if (preset.sliders.empty() && !preset.UsesBuildDefaults()) {
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
        const auto weight = actorBase->GetWeight() / 100.0F;
        const auto settings = bcn::Settings::Get().MorphOptions();
        const auto key = mode == bcn::racemenu::ApplyMode::preview ? kPreviewKey :
            mode == bcn::racemenu::ApplyMode::outfit ? kOutfitKey : kCommittedKey;
        // Replace BCNG and OBody's competing base-body/refit keys. Unrelated
        // keys survive by default. Preview must never clear persistent keys.
        if (!bcn::frame_tasks::CurrentWorkAllowed()) return;
        if (mode == bcn::racemenu::ApplyMode::commit) {
            bcn::racemenu::keys::BeginPresetCommit(*bodyMorph, actor.get(), settings.preserveOtherMorphs);
        }
        bcn::slider_name::Map<float> desiredMorphs;
        if (mode == bcn::racemenu::ApplyMode::outfit) {
            if (!bcn::rendered_outfit::ValidateApply(actor.get())) return;
            bodyMorph->ClearBodyMorphKeys(actor.get(), key);
            for (const auto& slider : preset.sliders) {
                // A named -Refit preset is still governed by the UI's
                // dependent nipple-refit switch.
                if (!settings.outfitNippleCorrection && IsNippleRefitSlider(slider.name)) continue;
                const auto value = slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight;
                bodyMorph->SetMorph(actor.get(), slider.name.c_str(), key, value);
            }
        } else {
            if (mode == bcn::racemenu::ApplyMode::preview) {
                bodyMorph->ClearBodyMorphKeys(actor.get(), kPreviewKey);
                bodyMorph->ClearBodyMorphKeys(actor.get(), kLegacyPreviewKey);
            }
            desiredMorphs.reserve(preset.sliders.size() + 40U);
            for (const auto& slider : preset.sliders) {
                desiredMorphs.insert_or_assign(
                    slider.name, slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight);
            }
        }
        if (mode == bcn::racemenu::ApplyMode::commit && actorBase && actorBase->GetSex() == RE::SEX::kFemale) {
            const auto presetFamily = bcn::body_family::PresetMask(preset.family, false);
            const auto femaleFamily = bcn::body_morph_policy::ResolveFemaleFamily(
                bcn::body_family::ResolveActor(actor.get()), presetFamily);
            const auto conventionalAnatomy = bcn::body_morph_policy::UsesConventionalAnatomyRecipe(femaleFamily);
            const auto write = [&](const char* name, float value) {
                desiredMorphs.insert_or_assign(name, value);
            };
            if (settings.nippleRandomization && conventionalAnatomy) {
                bcn::body_morph_policy::GenerateNippleMorphs(
                    bcn::body_morph_policy::OBodyChance, bcn::body_morph_policy::OBodyRandom, write);
            }
            if (settings.genitalRandomization && conventionalAnatomy) {
                bcn::body_morph_policy::GenerateGenitalMorphs(
                    bcn::body_morph_policy::OBodyChance, bcn::body_morph_policy::OBodyRandom, write);
            }
            if (femaleFamily == bcn::body_morph_policy::FemaleFamily::ube &&
                (settings.nippleRandomization || settings.genitalRandomization)) {
                if (settings.nippleRandomization) {
                    bcn::ube_nipple::Generate(
                        bcn::body_morph_policy::OBodyChance, bcn::body_morph_policy::OBodyRandom, write);
                }
                if (settings.genitalRandomization) {
                    // Only genital extras need TRI metadata. Nipple draws
                    // use the common UBE/Necoco dialect, with no file lookup.
                    const auto extras = bcn::ube_anatomy::SupportedExtras(actor.get());
                    static thread_local std::mt19937 random{ std::random_device{}() };
                    const auto shape = bcn::ube_genital::SelectShape(std::uniform_int_distribution<unsigned>(0U, 99U)(random));
                    const auto blend = std::uniform_real_distribution<float>(0.F, 1.F)(random);
                    bcn::ube_genital::Generate(shape, blend, extras, write);
                }
            }
        }
        if (mode != bcn::racemenu::ApplyMode::outfit) {
            bcn::slider_name::Map<float> replaced;
            if (mode == bcn::racemenu::ApplyMode::preview) {
                const auto plan = bcn::OutfitRefit::Get().Evaluate(actor.get(), &preset);
                const auto replaceOutfit = plan.action != bcn::OutfitRefit::Action::defer;
                PreviewBaseCollector collector(settings.preserveOtherMorphs, replaceOutfit);
                bodyMorph->VisitMorphValues(actor.get(), collector);
                replaced = std::move(collector.base.values);
                bcn::slider_name::Map<float> previewOutfit;
                if (plan.action == bcn::OutfitRefit::Action::named && plan.preset) {
                    for (const auto& slider : plan.preset->sliders) {
                        if (!settings.outfitNippleCorrection && IsNippleRefitSlider(slider.name)) continue;
                        previewOutfit.insert_or_assign(slider.name,
                            slider.lowWeight + (slider.highWeight - slider.lowWeight) * weight);
                    }
                } else if (plan.action == bcn::OutfitRefit::Action::procedural && !preset.male) {
                    const auto family = bcn::body_morph_policy::ResolveFemaleFamily(
                        bcn::body_family::ResolveActor(actor.get()),
                        bcn::body_family::PresetMask(preset.family, false));
                    bcn::body_morph_policy::GenerateOutfitMorphs(family, weight,
                        settings.outfitNippleCorrection,
                        [&](const char* name) {
                            const auto found = desiredMorphs.find(name);
                            return found == desiredMorphs.end() ? 0.0F : found->second;
                        },
                        [&](const char* name, const float value) { previewOutfit.insert_or_assign(name, value); });
                }
                for (const auto& [name, value] : previewOutfit) desiredMorphs[name] += value;
                // Also cancel old slider names absent from the selected XML,
                // including foreign-only names in non-preserving previews.
                for (const auto& [name, value] : replaced) desiredMorphs.try_emplace(name, 0.0F);
            }
            for (const auto& [name, desired] : desiredMorphs) {
                // Catalog values use XML / 100 (with legacy UNP inversion).
                // Commit never compensates against another mod. Only preview
                // is a temporary delta over unchanged persistent keys.
                const auto value = mode == bcn::racemenu::ApplyMode::preview ?
                    bcn::racemenu::PreviewPresetCorrection(desired, replaced[name]) : desired;
                bodyMorph->SetMorph(actor.get(), name.c_str(), key, value);
            }
        }
        // UI requests keep RaceMenu's partition update synchronous so an older
        // internal morph job cannot arrive after a later list selection.
        // Automatic distribution and outfit correction explicitly select the
        // deferred policy to avoid blocking a dense-cell frame.
        ApplyVisibleMorphs(*bodyMorph, actor.get(),
            updatePolicy == bcn::racemenu::UpdatePolicy::deferred);
        if (mode == bcn::racemenu::ApplyMode::preview && !IsCurrentPreview(actorHandle, previewGeneration)) {
            ClearPreviewNow(actorHandle);
            return;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt).count();
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
        const auto weight = base ? base->GetWeight() / 100.0F : 0.0F;
        MigrateLegacyBodyChangeKeys(*bodyMorph, actor.get());

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
        bcn::body_morph_policy::GenerateOutfitMorphs(femaleFamily, weight,
            settings.outfitNippleCorrection,
            [&](const char* name) { return bodyMorph->GetMorph(actor.get(), name, kCommittedKey); },
            [&](const char* name, const float value) { bodyMorph->SetMorph(actor.get(), name, kOutfitKey, value); });
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

        auto* interfaceMap = g_interfaceMap.load(std::memory_order_acquire);
        if (!interfaceMap) {
            const auto* messaging = SKSE::GetMessagingInterface();
            if (!messaging) return;
            skee::InterfaceExchangeMessage message{};
            if (!messaging->Dispatch(skee::InterfaceExchangeMessage::kExchangeInterface,
                    &message, sizeof(message), "skee") || !message.interfaceMap) {
                SKSE::log::warn("Body Change NG could not exchange RaceMenu interfaces yet");
                return;
            }
            interfaceMap = message.interfaceMap;
            // The interface map belongs to the provider, not to BodyMorph.
            // Keep it available so overlays remain functional when BodyMorph
            // is disabled or its revision is rejected.
            g_interfaceMap.store(interfaceMap, std::memory_order_release);
        }
        const auto bodyMorph = static_cast<skee::IBodyMorphInterface*>(
            interfaceMap->QueryInterface("BodyMorph"));
        if (!bodyMorph) {
            SKSE::log::warn("Body Change NG could not obtain RaceMenu's BodyMorph interface");
            return;
        }
        const auto version = bodyMorph->GetVersion();
        const auto runtimeVersion = REL::Module::get().version();
        const auto branch = bcn::runtime::ResolveGameBranch(runtimeVersion);
        const auto abi = bcn::racemenu_compat::ResolveBodyMorphAbi(version, branch);
        if (abi == bcn::racemenu_compat::BodyMorphAbi::unsupported) {
            SKSE::log::warn(
                "Body Change NG rejected RaceMenu BodyMorph interface version {} on runtime {} ({})",
                version, runtimeVersion.string(), bcn::runtime::GameBranchLabel(branch));
            return;
        }
        g_version.store(version, std::memory_order_release);
        g_bodyMorph.store(bodyMorph, std::memory_order_release);
        if (bcn::racemenu_compat::UsesBodyMorphFallback(version)) {
            SKSE::log::warn("Body Change NG BodyMorph v{} is newer than audited: using the {} shared prefix; requires provider backward ABI compatibility",
                version, bcn::racemenu_compat::BodyMorphAbiLabel(abi));
        }
        SKSE::log::info("Body Change NG received RaceMenu {} on runtime {} ({})",
            bcn::racemenu_compat::BodyMorphAbiLabel(abi), runtimeVersion.string(),
            bcn::runtime::GameBranchLabel(branch));
    }

    void ResetSessionState()
    {
        bcn::ube_anatomy::ClearSupportCache();
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
        auto* interfaceMap = g_interfaceMap.load(std::memory_order_acquire);
        if (!interfaceMap) {
            Initialize();
            interfaceMap = g_interfaceMap.load(std::memory_order_acquire);
        }
        return interfaceMap ? interfaceMap->QueryInterface(name) : nullptr;
    }

    std::optional<std::string> CurrentPresetId(const RE::Actor* actor)
    {
        if (!actor) return std::nullopt;
        if (!actor->Is3DLoaded()) return bcn::ActorRegistry::Get().SelectedBodyId(actor);
        {
            std::scoped_lock lock(g_selectionLock);
            const auto found = g_currentPresetIds.find(actor->GetFormID());
            if (found != g_currentPresetIds.end()) return found->second.empty() ?
                std::nullopt : std::optional<std::string>(found->second);
        }
        if (const auto state = bcn::ActorRegistry::Get().Snapshot(actor);
            state && state->body.selection.useDefault) return std::nullopt;
        if (const auto selected = bcn::ActorRegistry::Get().SelectedBodyId(actor)) return selected;
        return bcn::ActorRegistry::Get().AppliedBodyId(actor);
    }

    std::optional<bool> LiveBodyChangeStateMatches(const RE::Actor* actor, const bool expectDefault)
    {
        auto* bodyMorph = Interface();
        if (!actor || !bodyMorph) return std::nullopt;
        auto* reference = const_cast<RE::Actor*>(actor);
        const auto hasCommitted = bodyMorph->HasBodyMorphKey(reference, kCommittedKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyCommittedKey);
        const auto hasOBody = bodyMorph->HasBodyMorphKey(reference, keys::obody) ||
            bodyMorph->HasBodyMorphKey(reference, keys::oclothe);
        if (!expectDefault) {
            // An empty UBE Zeroed XML deliberately writes no morph key. Its
            // saved selection is valid when the competing base keys are gone.
            const auto id = CurrentPresetId(actor);
            const auto preset = id ? PresetCatalog::Get().Find(*id) : std::nullopt;
            const auto usesBuildDefaults = preset && preset->UsesBuildDefaults();
            return (usesBuildDefaults ? !hasCommitted : hasCommitted) && !hasOBody;
        }
        // A Default base body can legitimately wear an independent refit.
        // Explicit Default/reset still clears every owned layer; automatic
        // base verification must not mistake clothing for a changed preset.
        const auto hasBaseOrPreview = hasCommitted ||
            bodyMorph->HasBodyMorphKey(reference, kPreviewKey) ||
            bodyMorph->HasBodyMorphKey(reference, kLegacyPreviewKey);
        return !hasBaseOrPreview && !hasOBody;
    }

    void QueueVerifySavedBody(RE::Actor* actor)
    {
        if (!actor || !ActorRegistry::Get().Snapshot(actor)) return;
        const auto handle = actor->GetHandle();
        frame_tasks::Queue(actor->GetFormID(), [handle] {
            const auto resolved = handle.get();
            auto* actor = resolved.get();
            if (!actor || !actor->Is3DLoaded() || !IsReady() ||
                frame_tasks::HasPreview(actor->GetFormID()) || HasActivePreview(actor) ||
                frame_tasks::HasActorChannelWork(actor->GetFormID(), appearance::WorkChannel::bodyCommit) ||
                frame_tasks::HasActorChannelWork(actor->GetFormID(), appearance::WorkChannel::bodyPreviewCleanup)) return;
            const auto state = ActorRegistry::Get().Snapshot(actor);
            if (!state) return;
            const auto& selection = state->body.selection;
            if (!selection.useDefault && selection.selectedId.empty()) return;
            if (!ActorRegistry::Get().NeedsBodyApply(actor, selection.selectedId, selection.useDefault)) return;
            if (selection.useDefault) {
                QueueClearBodyChangeMorphs(actor);
                OutfitRefit::Get().ProcessActor(actor);
            } else if (QueueApply(actor, selection.selectedId, ApplyMode::commit, 0U,
                           UpdatePolicy::deferred) == ApplyResult::queued) {
                // Restore the outfit layer through its normal policy after
                // the base layer is queued; never reroll distribution rules.
                OutfitRefit::Get().ProcessActor(actor);
            }
        }, 2U, appearance::WorkChannel::bodyVerify);
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
        const auto found = PresetCatalog::Get().Find(presetId, mode == ApplyMode::outfit);
        if (!found) return ApplyResult::missingPreset;
        if (found->sliders.empty() && !found->UsesBuildDefaults()) return ApplyResult::emptyPreset;
        const auto* base = actor->GetActorBase();
        if (!base) return ApplyResult::invalidActor;
        const auto actorMale = base->GetSex() != RE::SEX::kFemale;
        if (found->male != actorMale) return ApplyResult::incompatibleSex;
        if (!bcn::body_family::Matches(
            bcn::body_family::PresetMask(found->family, found->male),
            bcn::body_family::ResolveActor(actor))) return ApplyResult::incompatibleBodyFamily;
        // The caller records accepted commit intent in ActorRegistry. No
        // geometry, morph key or preview is touched for an unloaded actor;
        // the normal attach queue validates and applies that saved intent.
        if (!actor->Is3DLoaded()) return mode == ApplyMode::commit ?
            ApplyResult::queued : ApplyResult::actor3DUnavailable;
        const auto actorHandle = actor->GetHandle();
        const auto applyGeneration = BeginApply(actor->GetFormID(), mode);
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
                    QueueActorTask(previousActor,
                        bcn::appearance::WorkChannel::bodyPreviewCleanup, [previousActor, session] {
                        if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                    });
                }
                QueueActorTask(actorHandle, bcn::appearance::WorkChannel::bodyPreview,
                    [actorHandle, preset = std::move(preset), applyGeneration, generation,
                    updatePolicy] mutable {
                    ApplyNow(actorHandle, std::move(preset), ApplyMode::preview, applyGeneration,
                        generation, 0U, updatePolicy);
                });
            } else {
                const auto previousActor = mode == ApplyMode::commit ? CancelPreviewTracking(actorHandle) : RE::ActorHandle{};
                if (previousActor && previousActor != actorHandle) {
                    QueueActorTask(previousActor,
                        bcn::appearance::WorkChannel::bodyPreviewCleanup, [previousActor, session] {
                        if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                    });
                }
                QueueActorTask(actorHandle, ChannelForApplyMode(mode),
                    [actorHandle, preset = std::move(preset), mode, applyGeneration,
                    outfitSignature, updatePolicy] mutable {
                    ApplyNow(actorHandle, std::move(preset), mode, applyGeneration, 0U,
                        outfitSignature, updatePolicy);
                });
            }
            return ApplyResult::queued;
        }
        return ApplyResult::noTaskInterface;
    }

    ApplyResult QueuePreviewDefault(RE::Actor* actor)
    {
        if (!bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        if (!IsReady()) return ApplyResult::unavailable;
        if (!actor) return ApplyResult::invalidActor;
        if (!actor->Is3DLoaded()) return ApplyResult::actor3DUnavailable;
        if (!SKSE::GetTaskInterface()) return ApplyResult::noTaskInterface;

        const auto actorHandle = actor->GetHandle();
        const auto applyGeneration = BeginApply(actor->GetFormID(), ApplyMode::preview);
        const auto [previousActor, previewGeneration] = BeginPreview(actorHandle);
        const auto session = bcn::ActorRegistry::Get().SessionGeneration();
        if (previousActor && previousActor != actorHandle) {
            QueueActorTask(previousActor,
                bcn::appearance::WorkChannel::bodyPreviewCleanup, [previousActor, session] {
                    if (bcn::ActorRegistry::Get().SessionGeneration() == session) {
                        ClearPreviewNow(previousActor);
                    }
                });
        }
        QueueActorTask(actorHandle, bcn::appearance::WorkChannel::bodyPreview,
            [actorHandle, applyGeneration, previewGeneration, session] {
                if (bcn::ActorRegistry::Get().SessionGeneration() != session ||
                    !IsCurrentPreview(actorHandle, previewGeneration)) return;
                auto* bodyMorph = Interface();
                const auto resolved = actorHandle.get();
                if (!bodyMorph || !resolved || !resolved->Is3DLoaded() ||
                    !IsCurrentApply(resolved->GetFormID(), ApplyMode::preview,
                        applyGeneration)) return;

                bodyMorph->ClearBodyMorphKeys(resolved.get(), kPreviewKey);
                bodyMorph->ClearBodyMorphKeys(resolved.get(), kLegacyPreviewKey);
                PreviewBaseCollector collector(true, true);
                bodyMorph->VisitMorphValues(resolved.get(), collector);
                for (const auto& [name, value] : collector.base.values) {
                    if (std::abs(value) > 0.00001F) {
                        bodyMorph->SetMorph(resolved.get(), name.c_str(),
                            kPreviewKey, -value);
                    }
                }
                ApplyVisibleMorphs(*bodyMorph, resolved.get(), false);
            });
        return ApplyResult::queued;
    }

    void QueueReapplyCurrent(RE::Actor* actor)
    {
        const auto presetId = CurrentPresetId(actor);
        if (!presetId) return;
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
            // Survive detach while waiting out RaceMenu. Keep actor FIFO and
            // the existing new-preview/session guards instead of anonymous undo.
            QueueActorTask(actorHandle, bcn::appearance::WorkChannel::bodyPreviewCleanup,
                [actorHandle, session] {
                    if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(actorHandle);
                });
        }
    }

    void QueueClearInactivePreview(RE::Actor* actor)
    {
        if (!actor || !IsReady()) return;
        const auto handle = actor->GetHandle();
        const auto session = ActorRegistry::Get().SessionGeneration();
        QueueActorTask(handle, appearance::WorkChannel::bodyPreviewCleanup, [handle, session] {
            if (ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(handle);
        });
    }

    void QueueApplyProceduralOutfit(RE::Actor* actor, const std::uint64_t outfitSignature)
    {
        if (!IsReady() || !actor) return;
        const auto actorHandle = actor->GetHandle();
        const auto generation = BeginApply(actor->GetFormID(), ApplyMode::outfit);
        if (const auto* tasks = SKSE::GetTaskInterface()) {
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            QueueActorTask(actorHandle, bcn::appearance::WorkChannel::outfitRefit,
                [actorHandle, outfitSignature, session, generation] {
                const auto actor = actorHandle.get();
                if (actor && bcn::ActorRegistry::Get().SessionGeneration() == session &&
                    IsCurrentApply(actor->GetFormID(), ApplyMode::outfit, generation)) {
                    if (!bcn::rendered_outfit::ValidateApply(actor.get())) return;
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
            QueueActorTask(actorHandle, bcn::appearance::WorkChannel::outfitRefit,
                [actorHandle, outfitSignature, session, generation] {
                if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
                auto* bodyMorph = Interface();
                const auto resolved = actorHandle.get();
                if (!bodyMorph || !resolved || !resolved->Is3DLoaded()) return;
                if (!IsCurrentApply(resolved->GetFormID(), ApplyMode::outfit, generation)) return;
                if (!bcn::rendered_outfit::ValidateApply(resolved.get())) return;
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

    void CancelPendingOutfit(RE::Actor* actor)
    {
        if (actor) InvalidateApply(actor->GetFormID(), ApplyMode::outfit);
    }

    void QueueClearBodyChangeMorphs(RE::Actor* actor, const bool ownedOnly)
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
            // This clear also removes the outfit key. A later equipment/SFS
            // evaluation must not skip it because of an old success signature.
            bcn::ActorRegistry::Get().InvalidateOutfit(actor);
            const auto session = bcn::ActorRegistry::Get().SessionGeneration();
            const auto previousActor = CancelPreviewTracking(actorHandle);
            if (previousActor && previousActor != actorHandle) {
                QueueActorTask(previousActor,
                    bcn::appearance::WorkChannel::bodyPreviewCleanup, [previousActor, session] {
                    if (bcn::ActorRegistry::Get().SessionGeneration() == session) ClearPreviewNow(previousActor);
                });
            }
            QueueActorTask(actorHandle, bcn::appearance::WorkChannel::bodyCommit,
                [actorHandle, session, clearGeneration, ownedOnly] {
                if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
                auto* bodyMorph = Interface();
                const auto resolved = actorHandle.get();
                if (!bodyMorph || !resolved) return;
                if (!IsCurrentApply(resolved->GetFormID(), ApplyMode::commit, clearGeneration)) return;
                if (ownedOnly) keys::ClearOwned(*bodyMorph, resolved.get());
                else keys::ClearReplacedBody(*bodyMorph, resolved.get());
                if (resolved->Is3DLoaded()) ApplyVisibleMorphs(*bodyMorph, resolved.get(), false);
                bcn::ActorRegistry::Get().MarkBodyApplied(resolved.get(), {}, true);
            });
        }
    }

    bool QueueClearAllBodyChangeMorphs(std::vector<std::uint32_t> alreadyReset, const bool ownedOnly)
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
        const auto resetCutoff = g_nextApplyGeneration.load(std::memory_order_relaxed);
        std::ranges::sort(alreadyReset);
        return bcn::frame_tasks::Queue(0, [session, resetCutoff, ownedOnly, alreadyReset = std::move(alreadyReset)] {
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
                if (std::ranges::binary_search(alreadyReset, formID) ||
                    CurrentApplyGeneration(formID, ApplyMode::commit) >= resetCutoff ||
                    CurrentApplyGeneration(formID, ApplyMode::preview) >= resetCutoff) continue;
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
                QueueActorTask(handle, bcn::appearance::WorkChannel::bodyCommit,
                    [handle, generation, ownedOnly] {
                    const auto resolved = handle.get();
                    auto* morph = Interface();
                    if (!resolved || !morph || !IsCurrentApply(resolved->GetFormID(), ApplyMode::commit, generation)) return;
                    // Keep the BCNG-actor eligibility above; remove competing
                    // OBody layers on these actors just like individual reset.
                    if (ownedOnly) keys::ClearOwned(*morph, resolved.get());
                    else keys::ClearReplacedBody(*morph, resolved.get());
                    if (resolved->Is3DLoaded()) ApplyVisibleMorphs(*morph, resolved.get(), false);
                });
                ++cleared;
            }
            SKSE::log::info("Body Change NG queued owned body morph reset for {} saved actors", cleared);
        });
    }

    std::optional<std::size_t> RemainingOwnedMorphActors()
    {
        auto* morph = Interface();
        if (!morph || !IsReady()) return std::nullopt;
        MorphActorCollector collector;
        morph->VisitActors(collector);
        std::size_t remaining{};
        for (const auto id : collector.actorFormIDs) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor) { ++remaining; continue; }
            if (std::ranges::any_of(keys::owned, [&](const auto* key) {
                    return morph->HasBodyMorphKey(actor, key);
                })) ++remaining;
        }
        return remaining;
    }
}
