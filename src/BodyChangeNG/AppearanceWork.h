#pragma once

#include <cstdint>

namespace bcn::appearance
{
    // A channel is a semantic replacement key, not an arbitrary priority
    // number. Jobs for the same actor and channel are latest-wins. Keep every
    // user-visible operation distinct so unrelated appearance changes can
    // never replace one another in the queue.
    enum class WorkChannel : std::uint32_t
    {
        none = 0,

        actorReconcile = 100,
        initialDistribution = 101,
        equipmentReconcile = 102,
        raceMenuRestore = 103,
        equipmentVerify = 104,
        renderedOutfitReconcile = 105,

        bodyPreview = 200,
        bodyCommit = 201,
        outfitRefit = 202,
        bodyPreviewCleanup = 203,
        skinApply = 204,
        futanariSkinApply = 205,
        tintApply = 206,
        maleGenitalSkinApply = 207,
        overlayFaceApply = 208,
        overlayBodyApply = 209,
        overlayHandsApply = 210,
        overlayFeetApply = 211,
        overlayCatalog = 212,
        overlayCatalogRequest = 213,
        skinFaceRefresh = 214
    };

    [[nodiscard]] constexpr std::uint32_t ChannelValue(const WorkChannel channel) noexcept
    {
        return static_cast<std::uint32_t>(channel);
    }

    [[nodiscard]] constexpr bool IsInteractiveChannel(const WorkChannel channel) noexcept
    {
        switch (channel) {
        case WorkChannel::bodyPreview:
        case WorkChannel::bodyCommit:
        case WorkChannel::outfitRefit:
        case WorkChannel::bodyPreviewCleanup:
        case WorkChannel::skinApply:
        case WorkChannel::futanariSkinApply:
        case WorkChannel::tintApply:
        case WorkChannel::maleGenitalSkinApply:
        case WorkChannel::overlayFaceApply:
        case WorkChannel::overlayBodyApply:
        case WorkChannel::overlayHandsApply:
        case WorkChannel::overlayFeetApply:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr bool IsInteractiveChannel(const std::uint32_t channel) noexcept
    {
        return IsInteractiveChannel(static_cast<WorkChannel>(channel));
    }
}
