#pragma once
#include <cstdint>

namespace bcn::racemenu_form_delete
{
#ifdef BODY_CHANGE_NG_GUARD_PROBE
    struct Stats final { std::uint64_t skipped{}, forwarded{}, lastSkipped{}; };
    Stats Statistics() noexcept;
#endif
    // Startup only; requires a verified loaded VM handle encoding. Recognizes
    // a complete erroneous callback across DLL address/file-version changes.
    // Failure never disables BCNG's normal versioned RaceMenu API support.
    bool Install(void* skeeModule, bool verifiedHandleLayout) noexcept;
    const char* Status() noexcept;
    void InstallInGame();
}
