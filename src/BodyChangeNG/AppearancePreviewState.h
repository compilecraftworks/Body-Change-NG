#pragma once

#include <optional>
#include <utility>

namespace bcn
{
    // Value-only logical selection. The owner supplies its synchronization.
    // The render task may be superseded; an explicit commit may not be.
    template<class State>
    class AppearancePreviewState final
    {
    public:
        void Begin(const State& live) { if (!committed_) committed_ = live; }
        void Commit(State state) { committed_ = std::move(state); }
        [[nodiscard]] const State& Saved(const State& live) const { return committed_ ? *committed_ : live; }
        void Reset() { committed_.reset(); }
    private:
        std::optional<State> committed_;
    };
}
