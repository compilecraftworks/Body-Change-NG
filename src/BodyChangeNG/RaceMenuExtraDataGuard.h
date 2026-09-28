#pragma once
#include <cstdint>
#include <span>

namespace bcn::racemenu_extra_data
{
    // Called once at PostPostLoad, before any save/actor geometry is loaded.
    // Exact UBE SE file identity + live code required. No late repair, actor
    // pointers, per-frame scans, disk DLL edits or changes to morph arithmetic.
    bool Install(void* skeeModule, bool isSkyrim1597) noexcept;
    const char* Status() noexcept;

    [[nodiscard]] constexpr int CompareNameAddresses(std::uintptr_t left, std::uintptr_t right) noexcept
    {
        // Subtraction/truncation is the original defect. Compare the complete
        // unsigned addresses, including when their distance exceeds INT_MAX.
        return (left > right) - (left < right);
    }
    int CompareExtraData(const void* left, const void* right) noexcept;

#ifdef BODY_CHANGE_NG_EXTRA_DATA_TEST
    // Tests use a private image copy, not LoadLibrary/the original DLL code.
    bool InstallTestImage(void* image, std::span<const std::uint8_t> file, bool isSkyrim1597) noexcept;
#endif
}
