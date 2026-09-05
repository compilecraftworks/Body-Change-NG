#include "BodyChangeNG/AsyncWorkGuards.h"
#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
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
        auto resource = std::make_shared<int>(42);
        std::weak_ptr<int> weakResource = resource;
        CompletionPayload<std::shared_ptr<int>> retained(std::move(resource));
        {
            auto value = retained.Take();
            Require(value && **value == 42 && !weakResource.expired(), "callback payload was not transferred");
            Require(!retained.Take(), "callback payload transferred twice");
        }
        Require(weakResource.expired(), "retained finished callback kept its payload alive");
        auto unfinished = std::make_shared<int>(1);
        std::weak_ptr<int> weakUnfinished = unfinished;
        {
            CompletionPayload<std::shared_ptr<int>> dropped(std::move(unfinished));
            Require(!weakUnfinished.expired(), "uninvoked callback released work early");
        }
        Require(weakUnfinished.expired(), "discarded callback leaked its payload");
        CompletionPayload<unsigned> concurrent(123);
        winners = 0; calls.clear();
        for (unsigned n{}; n < 16; ++n) calls.emplace_back([&] {
            if (const auto value = concurrent.Take(); value && *value == 123) ++winners;
        });
        for (auto& call : calls) call.join();
        Require(winners == 1 && !concurrent.Take(), "concurrent callback completion was not exactly once");
        for (float external : {-0.4F, 0.0F, 0.35F, 1.2F}) {
            for (float target : {0.0F, -0.3F, 1.0F}) {
                const auto committed = bcn::racemenu::AbsolutePresetCorrection(0.8F, external);
                const auto outfit = bcn::racemenu::OutfitTargetCorrection(target, external + committed);
                Require(std::abs(external + committed + outfit - target) < 0.00001F,
                    "outfit target depends on foreign morph ownership");
            }
        }
        using bcn::body_family::Bit;
        using bcn::body_family::Family;
        using bcn::body_morph_policy::FemaleFamily;
        using bcn::body_morph_policy::ResolveFemaleFamily;
        Require(ResolveFemaleFamily(Bit(Family::cbbe), Bit(Family::cbbe) | Bit(Family::ube)) ==
                FemaleFamily::cbbe3ba,
            "combined preset metadata overrode a known CBBE/3BA actor");
        Require(ResolveFemaleFamily(Bit(Family::ube), Bit(Family::cbbe) | Bit(Family::ube)) ==
                FemaleFamily::ube,
            "combined preset metadata overrode a known UBE actor");
        Require(ResolveFemaleFamily(0U, Bit(Family::ube)) == FemaleFamily::ube,
            "unambiguous UBE preset fallback was rejected");
        Require(ResolveFemaleFamily(Bit(Family::cbbe) | Bit(Family::ube),
                Bit(Family::cbbe) | Bit(Family::ube)) == FemaleFamily::none,
            "ambiguous actor evidence mixed CBBE/3BA and UBE anatomy sliders");
        std::cout << "AsyncWorkGuardTests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
