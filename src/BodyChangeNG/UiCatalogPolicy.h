#pragma once

#include "BodyChangeNG/AssetIdentity.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace bcn::ui_catalog
{
    // Automatic distribution preview only. Do not apply this allow-list to
    // direct actor selection or to the actual distribution rule matcher.
    [[nodiscard]] inline bool HumanElfPreviewRace(const std::uint32_t formID, std::string_view editorID)
    {
        std::string lower(editorID);
        for (auto& c : lower) c = static_cast<char>(asset_identity::FoldCase(static_cast<unsigned char>(c)));
        if (lower.find("khajiit") != std::string::npos || lower.find("argonian") != std::string::npos) return false;
        constexpr std::array names{"nordrace", "bretonrace", "imperialrace", "redguardrace",
            "highelfrace", "woodelfrace", "darkelfrace", "orcrace"};
        for (const auto* name : names) if (lower.find(name) != std::string::npos) return true;
        // Exact Skyrim.esm IDs, including vampire counterparts. Never mask a
        // foreign plugin's runtime ID down to these local IDs.
        constexpr std::array<std::uint32_t, 16> builtins{79681,79682,79683,79684,79686,79687,79688,79689,
            559164,559165,559168,559172,558996,688825,559174,559236};
        return std::ranges::find(builtins, formID) != builtins.end();
    }

    // ActorCatalog supplies NPCs in distance order. The player is only a
    // fallback, never a distance-zero candidate ahead of a matching NPC.
    template <class Entries, class Eligible>
    [[nodiscard]] std::uint32_t NearestDistributionActor(const Entries& entries,
        const bool female, const std::uint32_t playerFormID, Eligible&& eligible)
    {
        for (const auto& entry : entries) {
            if (!entry.player && entry.female == female && eligible(entry.formID)) {
                return entry.formID;
            }
        }
        return playerFormID;
    }

    struct PendingChoice final
    {
        std::uint32_t actorFormID{};
        std::string id;
        std::string originalId;
        bool useDefault{};
    };

    // Keep one reversible selection per catalog, not one record per click.
    // The first baseline survives A/B/A navigation and checkbox changes.
    inline void RememberPreview(std::optional<PendingChoice>& pending,
        const std::uint32_t actorFormID, std::string id, const bool useDefault,
        std::string originalId)
    {
        if (!actorFormID) return;
        if (!pending || pending->actorFormID != actorFormID) {
            pending = PendingChoice{ actorFormID, std::move(id),
                std::move(originalId), useDefault };
        } else {
            pending->id = std::move(id);
            pending->useDefault = useDefault;
        }
    }

    [[nodiscard]] constexpr bool CommitsActorChoice(
        const bool confirm, const bool distributionSelecting) noexcept
    {
        return confirm && !distributionSelecting;
    }

    enum class Tab : std::uint8_t
    {
        body,
        skin,
        tint,
        futanari,
        overlay,
        count
    };

    constexpr std::array kCanonicalOrder{
        Tab::body, Tab::skin, Tab::tint, Tab::futanari, Tab::overlay
    };

    struct AvailableTabs final
    {
        std::array<Tab, static_cast<std::size_t>(Tab::count)> values{};
        std::size_t size{};
    };

    [[nodiscard]] constexpr AvailableTabs ResolveAvailableTabs(
        const bool playerSelected, const bool futanariAvailable) noexcept
    {
        AvailableTabs result;
        result.values[result.size++] = Tab::body;
        result.values[result.size++] = Tab::skin;
        if (playerSelected) result.values[result.size++] = Tab::tint;
        if (futanariAvailable) result.values[result.size++] = Tab::futanari;
        result.values[result.size++] = Tab::overlay;
        return result;
    }

    enum class ChoiceIntent : std::uint8_t
    {
        none,
        preview,
        confirm
    };

    // ImGui reports the second click of a double-click as both clicked and
    // double-clicked. Confirmation must therefore take precedence.
    [[nodiscard]] constexpr ChoiceIntent MouseIntent(
        const bool clicked, const bool doubleClicked) noexcept
    {
        if (doubleClicked) return ChoiceIntent::confirm;
        return clicked ? ChoiceIntent::preview : ChoiceIntent::none;
    }
}
