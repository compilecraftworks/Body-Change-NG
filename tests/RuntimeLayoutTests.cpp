#include "BodyChangeNG/RuntimeLayout.h"
#include "BodyChangeNG/RaceMenuCompatibility.h"
#include "BodyChangeNG/RaceMenuOverrideRouting.h"
#include "BodyChangeNG/MenuCameraProjection.h"

#include <cmath>
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
    using bcn::racemenu_override::Route;
    using bcn::runtime::GameBranch;
    Expect(bcn::racemenu_override::ResolveRoute(0U, GameBranch::se) == Route::legacySeV0Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::legacySeV0Papyrus),
        "legacy SE RaceMenu Override v0 must use the Papyrus route");
    Expect(bcn::racemenu_override::ResolveRoute(0U, GameBranch::ae) == Route::aeBackportV0Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::aeBackportV0Papyrus),
        "the AE backport Override v0 must stay separate from legacy SE v0");
    Expect(bcn::racemenu_override::ResolveRoute(1U, GameBranch::se) == Route::officialV1Papyrus &&
        bcn::racemenu_override::ResolveRoute(1U, GameBranch::ae) == Route::officialV1Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::officialV1Papyrus),
        "official Override v1 must use its serialization-safe Papyrus route");
    Expect(bcn::racemenu_override::ResolveRoute(2U, GameBranch::se) == Route::officialV2Native &&
        bcn::racemenu_override::ResolveRoute(2U, GameBranch::ae) == Route::officialV2Native &&
        bcn::racemenu_override::UsesNativeV2(Route::officialV2Native),
        "official Override v2 must use the native wrapper route");
    Expect(bcn::racemenu_override::ResolveRoute(3U, GameBranch::ae) == Route::unsupported &&
        !bcn::racemenu_override::UsesPapyrus(Route::unsupported) &&
        !bcn::racemenu_override::UsesNativeV2(Route::unsupported),
        "an unaudited future Override ABI must fail closed");
    Expect(bcn::racemenu_override::ResolveRoute(2U, GameBranch::unsupported) == Route::unsupported,
        "a known Override ABI must not run on an unaudited Skyrim runtime");

    using bcn::racemenu_compat::BodyMorphAbi;
    Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(4U, GameBranch::se) == BodyMorphAbi::v4 &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(5U, GameBranch::ae) == BodyMorphAbi::v5,
        "verified RaceMenu BodyMorph ABIs must resolve explicitly");
    Expect(bcn::racemenu_compat::ResolveBodyMorphAbi(6U, GameBranch::ae) == BodyMorphAbi::unsupported &&
        bcn::racemenu_compat::ResolveBodyMorphAbi(5U, GameBranch::unsupported) == BodyMorphAbi::unsupported,
        "future BodyMorph ABIs and unknown runtimes must fail closed");

    Expect(bcn::runtime::ResolveGameBranch(REL::Version{ 1, 5, 97, 0 }) == GameBranch::se &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 6, 1170, 0 }) == GameBranch::ae,
        "verified Skyrim runtimes must resolve to an explicit SE/AE branch");
    Expect(bcn::runtime::ResolveGameBranch(REL::Version{ 1, 6, 641, 0 }) == GameBranch::unsupported &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 7, 99, 0 }) == GameBranch::unsupported &&
        bcn::runtime::ResolveGameBranch(REL::Version{ 1, 8, 0, 0 }) == GameBranch::unsupported,
        "unverified Skyrim patch and future minor versions must fail closed even when one isolated layout is known");

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
