#pragma once

#include <cstdint>
#include <string_view>

namespace bcn::native_skin
{
    enum class GraphAction : std::uint8_t
    {
        reuse,
        rebuildFromCurrentSource,
        rejectForeignOwner
    };

    enum class SharedBaseAction : std::uint8_t
    {
        keepOwner,
        transferOwner,
        rejectConflict
    };

    [[nodiscard]] constexpr SharedBaseAction ResolveSharedBaseAction(
        const std::uint32_t ownerActor, const std::uint32_t requestingActor,
        const std::string_view desiredProfile,
        const std::string_view requestedProfile) noexcept
    {
        if (ownerActor == 0U || ownerActor == requestingActor) {
            return SharedBaseAction::keepOwner;
        }
        if (desiredProfile.empty()) return SharedBaseAction::transferOwner;
        return desiredProfile == requestedProfile ? SharedBaseAction::keepOwner :
            SharedBaseAction::rejectConflict;
    }

    // Pure ownership policy for the native form graph. RSV is the only known
    // provider allowed to become a new source while a BCNG selection remains
    // active; arbitrary pointer replacements fail closed. A detached/default
    // graph is always rebuilt when its original source changed between saves.
    [[nodiscard]] constexpr GraphAction ResolveGraphAction(
        const bool hasGraph, const bool hasAppliedProfile,
        const bool ownsCurrentPointers, const bool sourceStillMatches,
        const bool currentSourceIsRsv,
        const bool unownedSourceChanged = false) noexcept
    {
        if (!hasGraph) return GraphAction::rebuildFromCurrentSource;
        if (hasAppliedProfile && !ownsCurrentPointers) {
            return currentSourceIsRsv ? GraphAction::rebuildFromCurrentSource :
                GraphAction::rejectForeignOwner;
        }
        // A provider is free to change a face or far-skin pointer BCNG never
        // attached. Rebase from that new source before a later profile starts
        // using the component; otherwise an old private clone could overwrite
        // a legitimate update from RSV or another appearance system.
        if (unownedSourceChanged) return GraphAction::rebuildFromCurrentSource;
        if (!hasAppliedProfile && !sourceStillMatches) {
            return GraphAction::rebuildFromCurrentSource;
        }
        return GraphAction::reuse;
    }
}
