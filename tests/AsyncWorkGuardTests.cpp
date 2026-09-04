#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
int main()
{
    try {
        using namespace bcn::async_work;
        Require(!MayCancelPreview(true, false), "different NPC cancelled UI preview");
        Require(MayCancelPreview(true, true), "same-actor commit could not clear its preview");
        Require(MayCancelPreview(false, false), "UI close could not cancel global preview");
        CompleteOnce completion;
        std::atomic_uint winners{};
        std::vector<std::thread> calls;
        for (unsigned n{}; n < 16; ++n) calls.emplace_back([&] {
            if (completion.TryFinish()) ++winners;
        });
        for (auto& call : calls) call.join();
        Require(winners == 1 && !completion.TryFinish(), "callback/destructor double completion");
        CompleteOnce discarded;
        Require(discarded.TryFinish(), "discarded callback did not finish");
        Require(!discarded.TryFinish(), "discarded callback finished twice");
        for (float external : {-0.4F, 0.0F, 0.35F, 1.2F}) {
            for (float target : {0.0F, -0.3F, 1.0F}) {
                const auto committed = bcn::racemenu::AbsolutePresetCorrection(0.8F, external);
                const auto outfit = bcn::racemenu::OutfitTargetCorrection(target, external + committed);
                Require(std::abs(external + committed + outfit - target) < 0.00001F,
                    "outfit target depends on foreign morph ownership");
            }
        }
        std::cout << "AsyncWorkGuardTests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
