#pragma once

#include <unordered_set>

namespace bcn::native_skin
{
    // The process lists do not guarantee inclusion of the player or selected
    // actor. Visit the explicit target first, then each eligible peer once.
    template <class Actor, class Enumerate, class Eligible, class Refresh>
    void VisitRefreshTargets(Actor* selected, Enumerate enumerate,
        Eligible eligible, Refresh refresh)
    {
        std::unordered_set<Actor*> visited;
        const auto visit = [&](Actor* actor) {
            if (actor && eligible(actor) && visited.insert(actor).second) refresh(actor);
        };
        visit(selected);
        enumerate(visit);
    }
}
