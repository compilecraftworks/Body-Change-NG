#pragma once
#include <atomic>

namespace bcn::async_work
{
    [[nodiscard]] constexpr bool MayCancelPreview(const bool ownerSpecified,
        const bool sameActor) noexcept { return !ownerSpecified || sameActor; }

    class CompleteOnce final
    {
    public:
        [[nodiscard]] bool TryFinish() noexcept { return !finished_.exchange(true); }
    private:
        std::atomic_bool finished_{};
    };
}
