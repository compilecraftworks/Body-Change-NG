#include "BodyChangeNG/FutanariSupport.h"

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/SkinTargetResolver.h"

#include <RE/A/Actor.h>
#include <RE/B/BGSBipedObjectForm.h>
#include <RE/B/BGSKeyword.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>

#include <algorithm>
#include <cctype>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    using bcn::futanari::AddonKind;
    using bcn::futanari_support::Provider;

    struct InstalledAddon final
    {
        Provider provider{ Provider::none };
        AddonKind kind{ AddonKind::none };
        std::string plugin;
    };

    struct Registry final
    {
        bool scanned{};
        bcn::futanari_support::Availability availability;
        std::vector<InstalledAddon> addons;
        std::vector<std::pair<RE::TESFaction*, AddonKind>> sosFactions;
    };

    Registry g_registry;
    std::mutex g_registryLock;

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    void Append(std::string& target, const std::string_view value)
    {
        if (value.empty()) return;
        if (!target.empty()) target.push_back(' ');
        target.append(value);
    }

    void AppendFormSignals(std::string& target, const RE::TESForm* form)
    {
        if (!form) return;
        if (const auto* file = form->GetFile(0)) Append(target, file->GetFilename());
        Append(target, form->GetFormEditorID());
        if (const auto* name = form->GetName(); name && name[0] != '\0') Append(target, name);
    }

    [[nodiscard]] std::string FemaleModelSignals(
        const RE::TESObjectARMO* armor, const RE::TESObjectARMA* addon)
    {
        std::string signals;
        AppendFormSignals(signals, armor);
        AppendFormSignals(signals, addon);
        if (addon) {
            if (const auto* path = addon->bipedModels[1U].GetModel(); path) Append(signals, path);
            if (const auto* path = addon->bipedModel1stPersons[1U].GetModel(); path) Append(signals, path);
        }
        return signals;
    }

    [[nodiscard]] AddonKind KindFromSignals(const std::string_view source)
    {
        const auto direct = bcn::futanari::ClassifyEvidence(source, source, source);
        if (direct != AddonKind::none) return direct;
        const auto lower = LowerAscii(std::string{ source });
        const auto femaleAddon = lower.contains("futanari") || lower.contains("futa") ||
            lower.contains("skinwithpenis") || lower.contains("skin_wp");
        if (!femaleAddon) return AddonKind::none;
        if (lower.contains("!ube") || lower.contains("ube_") || lower.contains(" ube")) {
            return AddonKind::ube;
        }
        if (lower.contains("erf")) return AddonKind::erf;
        if (lower.contains("trx")) return AddonKind::trx;
        return AddonKind::none;
    }

    [[nodiscard]] bool HasFemaleModel(const RE::TESObjectARMA* addon)
    {
        if (!addon) return false;
        const auto* third = addon->bipedModels[1U].GetModel();
        const auto* first = addon->bipedModel1stPersons[1U].GetModel();
        return (third && third[0] != '\0') || (first && first[0] != '\0');
    }

    [[nodiscard]] std::string SourcePlugin(const RE::TESForm* form)
    {
        const auto* file = form ? form->GetFile(0) : nullptr;
        return file ? LowerAscii(std::string{ file->GetFilename() }) : std::string{};
    }

    [[nodiscard]] bool SameAddon(
        const InstalledAddon& left, const InstalledAddon& right) noexcept
    {
        return left.provider == right.provider && left.kind == right.kind &&
            left.plugin == right.plugin;
    }

    void AddInstalled(Registry& registry, InstalledAddon addon)
    {
        if (addon.kind == AddonKind::none || addon.provider == Provider::none ||
            addon.plugin.empty() || std::ranges::any_of(registry.addons,
                [&](const InstalledAddon& existing) { return SameAddon(existing, addon); })) return;
        registry.addons.push_back(std::move(addon));
        const auto& added = registry.addons.back();
        registry.availability.providers |= static_cast<std::uint8_t>(added.provider);
        registry.availability.trx = registry.availability.trx || added.kind == AddonKind::trx;
        registry.availability.erf = registry.availability.erf || added.kind == AddonKind::erf;
        registry.availability.ube = registry.availability.ube || added.kind == AddonKind::ube;
    }

    [[nodiscard]] bool IsFemaleTngAddon(const RE::TESObjectARMO* armor)
    {
        return armor && armor->HasKeywordString("TNG_AddonFemale") &&
            armor->HasKeywordString("TNG_SkinWithPenis");
    }

    [[nodiscard]] bool IsSosAddon(const RE::TESObjectARMO* armor,
        const std::string_view signals)
    {
        return armor && !IsFemaleTngAddon(armor) &&
            (armor->HasKeywordString("SOS_Genitals") ||
                bcn::futanari::ContainsIgnoreAsciiCase(signals, "SOS_Addon_") ||
                bcn::futanari::ContainsIgnoreAsciiCase(signals, "Schlongs of Skyrim"));
    }

    void Scan(Registry& registry)
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return;
        registry.availability = {};
        registry.addons.clear();
        registry.sosFactions.clear();

        constexpr auto slot52 = static_cast<std::uint32_t>(
            RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);
        for (auto* armor : data->GetFormArray<RE::TESObjectARMO>()) {
            if (!armor || (armor->GetSlotMask().underlying() & slot52) == 0U) continue;
            for (auto* addon : armor->armorAddons) {
                if (!addon || (addon->GetSlotMask().underlying() & slot52) == 0U ||
                    !HasFemaleModel(addon)) continue;
                const auto signals = FemaleModelSignals(armor, addon);
                const auto kind = KindFromSignals(signals);
                if (kind == AddonKind::none) continue;
                const auto provider = IsFemaleTngAddon(armor) ? Provider::tng :
                    IsSosAddon(armor, signals) ? Provider::sos : Provider::none;
                AddInstalled(registry, {
                    .provider = provider,
                    .kind = kind,
                    .plugin = SourcePlugin(armor)
                });
            }
        }

        // SOS keeps the selected addon in its addon-specific faction even
        // while the slot-52 armor is covered or temporarily unequipped.
        for (auto* faction : data->GetFormArray<RE::TESFaction>()) {
            if (!faction) continue;
            std::string signals;
            AppendFormSignals(signals, faction);
            const auto plugin = SourcePlugin(faction);
            const auto lower = LowerAscii(signals);
            if (!lower.contains("faction") ||
                (!lower.contains("futa") && !lower.contains("futanari"))) continue;
            for (const auto& addon : registry.addons) {
                if (addon.provider == Provider::sos && addon.plugin == plugin) {
                    registry.sosFactions.emplace_back(faction, addon.kind);
                }
            }
        }
        registry.scanned = true;
    }

    [[nodiscard]] Registry SnapshotRegistry()
    {
        std::scoped_lock lock(g_registryLock);
        if (!g_registry.scanned) Scan(g_registry);
        return g_registry;
    }

    [[nodiscard]] std::optional<bcn::FutanariSkinType> TypeFor(
        const AddonKind kind, const bcn::body_family::Mask actorFamily)
    {
        const auto ube = bcn::body_family::Bit(bcn::body_family::Family::ube);
        const auto cbbe = bcn::body_family::Bit(bcn::body_family::Family::cbbe);
        if (kind == AddonKind::ube && (actorFamily & ube) != 0U) {
            return bcn::FutanariSkinType::ubeTrx;
        }
        if (kind == AddonKind::trx) {
            if ((actorFamily & ube) != 0U) return bcn::FutanariSkinType::ubeTrx;
            if ((actorFamily & cbbe) != 0U) return bcn::FutanariSkinType::cbbeTrx;
        }
        if (kind == AddonKind::erf && (actorFamily & cbbe) != 0U) {
            return bcn::FutanariSkinType::erf;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<AddonKind> SkinArmorKind(RE::Actor* actor)
    {
        auto* skin = actor ? actor->GetSkin() : nullptr;
        if (!skin) return std::nullopt;
        AddonKind found{ AddonKind::none };
        for (auto* addon : skin->armorAddons) {
            if (!addon || !HasFemaleModel(addon)) continue;
            const auto kind = KindFromSignals(FemaleModelSignals(skin, addon));
            if (kind == AddonKind::none) continue;
            if (found != AddonKind::none && found != kind) return std::nullopt;
            found = kind;
        }
        return found == AddonKind::none ? std::nullopt : std::optional{ found };
    }
}

namespace bcn::futanari_support
{
    void RefreshInstalledAddons()
    {
        std::scoped_lock lock(g_registryLock);
        g_registry.scanned = false;
        Scan(g_registry);
    }

    Availability InstalledAddons()
    {
        std::scoped_lock lock(g_registryLock);
        if (!g_registry.scanned) Scan(g_registry);
        return g_registry.availability;
    }

    bool Available()
    {
        return InstalledAddons().Any();
    }

    std::optional<FutanariSkinType> RegisteredType(RE::Actor* actor)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !base || base->GetSex() != RE::SEX::kFemale) return std::nullopt;
        const auto registry = SnapshotRegistry();
        if (!registry.availability.Any()) return std::nullopt;

        // A live route is the strongest proof and also covers providers whose
        // registration marker is not accessible while their addon is visible.
        if (const auto loaded = skin_target::FindLoadedFutanariRoute(actor);
            loaded.type) return loaded.type;

        const auto family = body_family::ResolveActor(actor);
        if (registry.availability.Has(Provider::tng) &&
            base->HasKeywordString("TNG_Gentlewoman")) {
            if (const auto kind = SkinArmorKind(actor)) return TypeFor(*kind, family);
        }

        std::optional<AddonKind> sosKind;
        if (registry.availability.Has(Provider::sos)) {
            for (const auto& [faction, kind] : registry.sosFactions) {
                if (!faction || !actor->IsInFaction(faction)) continue;
                if (sosKind && *sosKind != kind) return std::nullopt;
                sosKind = kind;
            }
        }
        return sosKind ? TypeFor(*sosKind, family) : std::nullopt;
    }

    bool Registered(RE::Actor* actor)
    {
        return RegisteredType(actor).has_value();
    }
}
