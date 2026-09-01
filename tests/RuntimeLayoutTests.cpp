#include "BodyChangerNG/RuntimeLayout.h"

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
    using bcn::runtime::ResolveWorldFovOffset;

    const auto se = ResolveWorldFovOffset(REL::Version{ 1, 5, 97, 0 });
    Expect(se && se->relocationID == 527997,
        "Skyrim SE 1.5.97 must use the verified world-FOV offset ID");

    const auto aeFirst = ResolveWorldFovOffset(REL::Version{ 1, 6, 317, 0 });
    const auto aeBoundary = ResolveWorldFovOffset(REL::Version{ 1, 6, 629, 0 });
    const auto aeSteam = ResolveWorldFovOffset(REL::Version{ 1, 6, 1170, 0 });
    const auto aeGog = ResolveWorldFovOffset(REL::Version{ 1, 6, 1179, 0 });
    Expect(aeFirst && aeFirst->relocationID == 414942,
        "first supported AE runtime must use the verified world-FOV offset ID");
    Expect(aeBoundary && aeBoundary->relocationID == 414942,
        "Skyrim AE 1.6.629 must use the verified world-FOV offset ID");
    Expect(aeSteam && aeSteam->relocationID == 414942,
        "Skyrim AE 1.6.1170 must use the verified world-FOV offset ID");
    Expect(aeGog && aeGog->relocationID == 414942,
        "Skyrim AE 1.6.1179 must use the verified world-FOV offset ID");

    Expect(!ResolveWorldFovOffset(REL::Version{ 1, 5, 96, 0 }),
        "unverified SE runtimes must fail closed");
    Expect(!ResolveWorldFovOffset(REL::Version{ 1, 6, 628, 0 }),
        "unverified AE boundary runtimes must fail closed");
    Expect(!ResolveWorldFovOffset(REL::Version{ 1, 6, 641, 0 }),
        "unknown AE runtimes must fail closed");
    Expect(!ResolveWorldFovOffset(REL::Version{ 1, 4, 15, 0 }),
        "Skyrim VR must not select a flat runtime layout");
    Expect(!ResolveWorldFovOffset(REL::Version{ 1, 7, 99, 0 }),
        "future unverified runtimes must fail closed");

    if (failures != 0) {
        std::cerr << failures << " runtime layout test(s) failed\n";
        return 1;
    }
    std::cout << "Runtime layout boundary tests passed\n";
    return 0;
}
