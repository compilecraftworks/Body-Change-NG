#include "BodyChangeNG/RuntimeLayout.h"
#include "BodyChangeNG/NativeAddonLayout.h"
#include "BodyChangeNG/RaceMenuCompatibility.h"
#include "BodyChangeNG/MenuCameraProjection.h"

#include <cmath>
#include <array>
#include <iostream>

namespace
{
    int failures{};

    void Expect(const bool condition, const char* message)
    {
        if (condition) return;
        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }
}

int main()
{
    using bcn::runtime::GameBranch;
    using bcn::racemenu_compat::BodyMorphAbi;
    using bcn::racemenu_compat::NodeOverrideAbi;
    using bcn::racemenu_compat::ResolveNodeOverrideAbi;
    for (const auto branch : { GameBranch::se, GameBranch::ae }) {
        Expect(ResolveNodeOverrideAbi(0, branch) == NodeOverrideAbi::papyrus,
            "missing direct contract lost the Papyrus face fallback");
        Expect(ResolveNodeOverrideAbi(1, branch) == NodeOverrideAbi::legacyV1,
            "legacy face ABI incorrectly depends on game branch or overlay interface");
        for (const auto version : { 2U, 3U, 100U, 0xFFFFFFFFU })
            Expect(ResolveNodeOverrideAbi(version, branch) == NodeOverrideAbi::publicV2,
                "newer face override interface lost the known v2 prefix fallback");
    }
    for (const auto version : { 0U, 1U, 2U, 3U })
        Expect(ResolveNodeOverrideAbi(version, GameBranch::unsupported) == NodeOverrideAbi::papyrus,
            "unknown game runtime enabled a raw face interface");
    Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(4U, GameBranch::se) == BodyMorphAbi::v4 &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(4U, GameBranch::ae) == BodyMorphAbi::v4 &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(5U, GameBranch::se) == BodyMorphAbi::v5 &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(5U, GameBranch::ae) == BodyMorphAbi::v5,
        "RaceMenu BodyMorph v4/v5 common-prefix ABIs must resolve on every supported branch");
    Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(3U, GameBranch::ae) == BodyMorphAbi::unsupported &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(5U, GameBranch::unsupported) == BodyMorphAbi::unsupported,
        "no lower BodyMorph contract and unknown game runtimes must remain unsupported");
    using bcn::racemenu_compat::OverlayStackAbi;
    Expect(bcn::racemenu_compat::ResolveOverlayStackAbi(1U, 2U, GameBranch::se) ==
            OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(2U, 1U, GameBranch::ae) ==
            OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(1U, 3U, GameBranch::ae) ==
            OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(1U, 1U, GameBranch::unsupported) ==
            OverlayStackAbi::unsupported,
        "legacy/public generation mixing and unknown game runtimes must remain unsupported");
    for (const auto branch : { GameBranch::se, GameBranch::ae }) {
        for (const auto future : { 6U, 7U, 100U, 0xFFFFFFFFU }) {
            Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(future, branch) == BodyMorphAbi::v5 &&
                    bcn::racemenu_compat::UsesBodyMorphFallback(future),
                "newer BodyMorph revisions must fall back to the highest known lower prefix");
        }
        for (const auto newer : { 3U, 4U, 100U, 0xFFFFFFFFU }) {
            for (const auto knownOrNewer : { 2U, 3U, 0xFFFFFFFFU }) {
                Expect(bcn::racemenu_compat::ResolveOverlayStackAbi(newer, knownOrNewer, branch) ==
                        OverlayStackAbi::publicV2 &&
                    bcn::racemenu_compat::ResolveOverlayStackAbi(knownOrNewer, newer, branch) ==
                        OverlayStackAbi::publicV2 &&
                    bcn::racemenu_compat::UsesOverlayFallback(newer, knownOrNewer),
                    "new public revisions must independently retain the v2 fallback");
            }
        }
    }
    Expect(!bcn::racemenu_compat::UsesBodyMorphFallback(4U) &&
        !bcn::racemenu_compat::UsesBodyMorphFallback(5U) &&
        !bcn::racemenu_compat::UsesOverlayFallback(1U, 1U) &&
        !bcn::racemenu_compat::UsesOverlayFallback(2U, 2U) &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(0U, 3U, GameBranch::ae) == OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(3U, 0U, GameBranch::ae) == OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(0U, GameBranch::ae) == BodyMorphAbi::unsupported &&
        bcn::racemenu_compat::ResolveOverlayStackAbi(3U, 3U, GameBranch::unsupported) == OverlayStackAbi::unsupported &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(6U, GameBranch::unsupported) == BodyMorphAbi::unsupported,
        "fallback must not admit missing interfaces or unverified game layouts");

    Expect(bcn::runtime::ResolveGameBranch(REL::Version{ 1, 5, 97, 0 }) == GameBranch::se &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 6, 1170, 0 }) == GameBranch::ae,
        "verified Skyrim runtimes must resolve to an explicit SE/AE branch");
    Expect(bcn::runtime::ResolveGameBranch(REL::Version{ 1, 6, 641, 0 }) == GameBranch::unsupported &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 7, 99, 0 }) == GameBranch::unsupported &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 8, 0, 0 }) == GameBranch::unsupported,
        "unverified Skyrim patch and future minor versions must fail closed even when one isolated layout is known");

    constexpr std::array supportedRuntimes{
        REL::Version{ 1, 5, 97, 0 },
        REL::Version{ 1, 6, 317, 0 },
        REL::Version{ 1, 6, 318, 0 },
        REL::Version{ 1, 6, 323, 0 },
        REL::Version{ 1, 6, 342, 0 },
        REL::Version{ 1, 6, 353, 0 },
        REL::Version{ 1, 6, 629, 0 },
        REL::Version{ 1, 6, 640, 0 },
        REL::Version{ 1, 6, 659, 0 },
        REL::Version{ 1, 6, 1130, 0 },
        REL::Version{ 1, 6, 1170, 0 },
        REL::Version{ 1, 6, 1179, 0 }
    };
    for (const auto version : supportedRuntimes) {
        const auto branch = bcn::runtime::ResolveGameBranch(version);
        const auto addon = bcn::native_addon::ResolveRuntimeLayout(version);
        Expect(addon && addon->visitor == (branch == GameBranch::se ? 15561U : 15739U) &&
            addon->directCaller == (branch == GameBranch::se ? 15535U : 15712U) &&
            addon->recursiveCaller == (branch == GameBranch::se ? 15546U : 15722U),
            "native addon routing must resolve using the loaded game's library on all 12 versions");
        Expect(branch != GameBranch::unsupported,
            "a declared BCNG SE/AE runtime disappeared from the admission table");
        Expect(bcn::runtime::ResolvePlayerTintLayout(version).has_value(),
            "player tint must have an explicit layout on every admitted runtime");
        Expect(bcn::runtime::ResolveRendererHook(version).has_value() &&
                bcn::runtime::ResolveInputPollHook(version).has_value(),
            "native UI hooks must have an explicit layout on every admitted runtime");
        Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(4U, branch) == BodyMorphAbi::v4 &&
                bcn::racemenu_compat::ResolveBodyMorphAbi(5U, branch) == BodyMorphAbi::v5,
            "BodyMorph v4/v5 must remain available on every admitted runtime");
        Expect(bcn::racemenu_compat::ResolveOverlayStackAbi(1U, 1U, branch) ==
                    OverlayStackAbi::legacyV1 &&
                bcn::racemenu_compat::ResolveOverlayStackAbi(2U, 2U, branch) ==
                    OverlayStackAbi::publicV2,
            "both official RaceMenu overlay ABI generations must resolve on every admitted runtime");
    }

    constexpr REL::Version epic{1,6,678,0};
    Expect(!bcn::native_addon::ResolveRuntimeLayout(epic) &&
        !bcn::native_addon::ResolveRuntimeLayout(REL::Version{1,7,99,0}),
        "native addon proof must not admit an unverified game layout");
    Expect(bcn::runtime::ResolveGameBranch(epic)==GameBranch::unsupported &&
        !bcn::runtime::ResolvePlayerTintLayout(epic) && !bcn::runtime::ResolveRendererHook(epic) &&
        !bcn::runtime::ResolveInputPollHook(epic),
        "Epic 1.6.678 has no supported upstream SKSE loader; never guess GOG layouts for it");


    const auto legacyTint = bcn::runtime::ResolvePlayerTintLayout(REL::Version{ 1, 6, 353, 0 });
    const auto aeTint = bcn::runtime::ResolvePlayerTintLayout(REL::Version{ 1, 6, 629, 0 });
    Expect(legacyTint && legacyTint->baseOffset == 0xB10 && legacyTint->overlayOffset == 0xB28,
        "pre-1.6.629 PlayerTint layout boundary changed");
    Expect(aeTint && aeTint->baseOffset == 0xB18 && aeTint->overlayOffset == 0xB30,
        "1.6.629+ PlayerTint layout boundary changed");
    Expect(!bcn::runtime::ResolvePlayerTintLayout(REL::Version{ 1, 6, 641, 0 }) &&
        !bcn::runtime::ResolvePlayerTintLayout(REL::Version{ 1, 7, 99, 0 }) &&
        !bcn::runtime::ResolvePlayerTintLayout(REL::Version{ 1, 7, 100, 0 }),
        "PlayerTint raw member access guessed an unaudited layout");

    const auto seRenderer = bcn::runtime::ResolveRendererHook(REL::Version{ 1, 5, 97, 0 });
    const auto seInput = bcn::runtime::ResolveInputPollHook(REL::Version{ 1, 5, 97, 0 });
    Expect(seRenderer && seRenderer->relocationID == 75595 && seRenderer->callOffset == 0x50,
        "Skyrim SE 1.5.97 renderer hook layout must remain verified");
    Expect(seInput && seInput->relocationID == 67315 && seInput->callOffset == 0x7B,
        "Skyrim SE 1.5.97 input hook layout must remain verified");

    const auto aeRenderer = bcn::runtime::ResolveRendererHook(REL::Version{ 1, 6, 1170, 0 });
    const auto aeInput = bcn::runtime::ResolveInputPollHook(REL::Version{ 1, 6, 1179, 0 });
    Expect(aeRenderer && aeRenderer->relocationID == 77226 && aeRenderer->callOffset == 0x2BC,
        "Skyrim AE 1.6.1170 renderer hook layout must remain verified");
    Expect(aeInput && aeInput->relocationID == 68617 && aeInput->callOffset == 0x7B,
        "Skyrim AE 1.6.1179 input hook layout must remain verified");

    Expect(!bcn::runtime::ResolveRendererHook(REL::Version{ 1, 6, 641, 0 }) &&
        !bcn::runtime::ResolveInputPollHook(REL::Version{ 1, 6, 641, 0 }),
        "unknown AE runtimes must fail closed");
    Expect(!bcn::runtime::ResolveRendererHook(REL::Version{ 1, 7, 99, 0 }) &&
        !bcn::runtime::ResolveInputPollHook(REL::Version{ 1, 7, 99, 0 }),
        "an isolated 1.7.99 tint layout must not imply full renderer/input support");
    Expect(!bcn::runtime::ResolveRendererHook(REL::Version{ 1, 4, 15, 0 }) &&
        !bcn::runtime::ResolveInputPollHook(REL::Version{ 1, 4, 15, 0 }),
        "Skyrim VR must not select unverified flat hook layouts");

    RE::NiFrustum perspective{ -0.916331F, 0.916331F, 0.515436F,
        -0.515436F, 0.1F, 10000.0F, false };
    const auto originalAspect = perspective.fTop / perspective.fRight;
    Expect(bcn::camera_projection::SetHorizontalFov(perspective, 70.0F),
        "perspective menu camera FOV must be adjustable");
    Expect(std::abs(perspective.fRight - 0.700208F) < 0.00001F &&
        std::abs(perspective.fLeft + 0.700208F) < 0.00001F,
        "menu camera frustum must encode horizontal FOV 70");
    Expect(std::abs(perspective.fTop / perspective.fRight - originalAspect) < 0.00001F,
        "menu camera FOV scaling must preserve viewport aspect ratio");
    Expect(perspective.fNear == 0.1F && perspective.fFar == 10000.0F,
        "menu camera FOV scaling must preserve near and far planes");

    const auto onceApplied = perspective;
    Expect(bcn::camera_projection::SetHorizontalFov(perspective, 70.0F) &&
        std::abs(perspective.fRight - onceApplied.fRight) < 0.000001F &&
        std::abs(perspective.fTop - onceApplied.fTop) < 0.000001F,
        "reapplying menu camera FOV must be stable");

    RE::NiFrustum orthographic{ -1.0F, 1.0F, 1.0F, -1.0F,
        0.1F, 10000.0F, true };
    Expect(!bcn::camera_projection::SetHorizontalFov(orthographic, 70.0F) &&
        orthographic.fRight == 1.0F,
        "orthographic projection must fail closed");

    RE::NiFrustum invalid{ 0.0F, 0.0F, 1.0F, -1.0F,
        0.1F, 10000.0F, false };
    Expect(!bcn::camera_projection::SetHorizontalFov(invalid, 70.0F),
        "degenerate perspective projection must fail closed");

    if (failures != 0) {
        std::cerr << failures << " runtime layout test(s) failed\n";
        return 1;
    }
    std::cout << "Runtime layout boundary tests passed\n";
    return 0;
}
