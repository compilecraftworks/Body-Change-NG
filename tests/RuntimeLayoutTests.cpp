#include "BodyChangeNG/RuntimeLayout.h"
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
    Expect(bcn::racemenu_override::ResolveRoute(0U, false) == Route::legacySeV0Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::legacySeV0Papyrus),
        "legacy SE RaceMenu Override v0 must use the Papyrus route");
    Expect(bcn::racemenu_override::ResolveRoute(0U, true) == Route::aeBackportV0Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::aeBackportV0Papyrus),
        "the AE backport Override v0 must stay separate from legacy SE v0");
    Expect(bcn::racemenu_override::ResolveRoute(1U, false) == Route::officialV1Papyrus &&
        bcn::racemenu_override::ResolveRoute(1U, true) == Route::officialV1Papyrus &&
        bcn::racemenu_override::UsesPapyrus(Route::officialV1Papyrus),
        "official Override v1 must use its serialization-safe Papyrus route");
    Expect(bcn::racemenu_override::ResolveRoute(2U, false) == Route::officialV2Native &&
        bcn::racemenu_override::ResolveRoute(2U, true) == Route::officialV2Native &&
        bcn::racemenu_override::UsesNativeV2(Route::officialV2Native),
        "official Override v2 must use the native wrapper route");
    Expect(bcn::racemenu_override::ResolveRoute(3U, true) == Route::unsupported &&
        !bcn::racemenu_override::UsesPapyrus(Route::unsupported) &&
        !bcn::racemenu_override::UsesNativeV2(Route::unsupported),
        "an unaudited future Override ABI must fail closed");

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
