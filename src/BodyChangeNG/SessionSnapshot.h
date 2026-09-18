#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace bcn::async_work
{
    // A one-shot, owned result. Reset invalidates both queued and in-flight
    // requests; a late publisher cannot overwrite a newer UI session.
    template<class T>
    class SessionSnapshot final
    {
    public:
        using Ticket = std::uint64_t;
        enum class State { idle, pending, ready, failed };

        std::optional<Ticket> Begin()
        {
            std::scoped_lock lock(lock_);
            if (state_ != State::idle) return {};
            state_ = State::pending;
            return generation_;
        }
        bool Current(Ticket ticket) const
        {
            std::scoped_lock lock(lock_);
            return ticket == generation_ && state_ == State::pending;
        }
        bool Publish(Ticket ticket, T value)
        {
            auto owned = std::make_shared<const T>(std::move(value));
            std::scoped_lock lock(lock_);
            if (ticket != generation_ || state_ != State::pending) return false;
            value_ = std::move(owned);
            state_ = State::ready;
            return true;
        }
        void Fail(Ticket ticket)
        {
            std::scoped_lock lock(lock_);
            if (ticket == generation_ && state_ == State::pending) state_ = State::failed;
        }
        void Reset()
        {
            std::scoped_lock lock(lock_);
            ++generation_;
            value_.reset();
            state_ = State::idle;
        }
        std::shared_ptr<const T> Read() const
        {
            std::scoped_lock lock(lock_);
            return value_;
        }
        State Status() const
        {
            std::scoped_lock lock(lock_);
            return state_;
        }
    private:
        mutable std::mutex lock_;
        Ticket generation_{};
        State state_{ State::idle };
        std::shared_ptr<const T> value_;
    };
}
