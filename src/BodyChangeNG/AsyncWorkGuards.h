#pragma once
#include <atomic>
#include <mutex>
#include <optional>
#include <utility>

namespace bcn::async_work
{
    // VM may retain a callback object after invoking it. Transfer its payload
    // at invocation, not object destruction; the recipient owns unfinished
    // native work until it really completes. Also handles duplicate callbacks.
    template <class T>
    class CompletionPayload final
    {
    public:
        explicit CompletionPayload(T payload) : payload_(std::move(payload)) {}
        std::optional<T> Take()
        {
            std::scoped_lock lock(lock_);
            auto value = std::move(payload_);
            payload_.reset();
            return value;
        }
    private:
        std::mutex lock_;
        std::optional<T> payload_;
    };

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
