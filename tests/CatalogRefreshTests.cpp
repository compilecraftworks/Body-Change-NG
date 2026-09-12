#include "BodyChangeNG/CatalogRefreshQueue.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
int main()
try {
    using namespace std::chrono_literals;
    bcn::catalog_refresh::Queue queue;
    int first{}, second{}, fence{};
    std::promise<void> started, release, failed, finished, retried;
    auto barrier = release.get_future().share();
    auto startedFuture = started.get_future();
    auto failedFuture = failed.get_future();
    auto finishedFuture = finished.get_future();
    auto retriedFuture = retried.get_future();
    auto retained = std::make_shared<int>(1);
    std::weak_ptr<int> lifetime = retained;
    Check(queue.Submit(&first, [&, retained, barrier] { started.set_value(); barrier.wait(); }), "first refresh rejected");
    retained.reset();
    if (startedFuture.wait_for(2s) != std::future_status::ready) {
        release.set_value();
        throw std::runtime_error("worker did not start");
    }
    bool duplicated{};
    for (int i = 0; i < 10000; ++i) duplicated |= queue.Submit(&first, [] {});
    const bool queuedFailure = queue.Submit(&second, [] { throw std::runtime_error("test"); },
        [&](std::exception_ptr error) { Check(error != nullptr, "missing exception"); failed.set_value(); throw 1; });
    const bool queuedFence = queue.Submit(&fence, [&] { finished.set_value(); });
    release.set_value();
    Check(!duplicated && queuedFailure && queuedFence, "duplicate suppression blocked another catalog");
    Check(failedFuture.wait_for(2s) == std::future_status::ready &&
        finishedFuture.wait_for(2s) == std::future_status::ready, "exception terminated the refresh worker");
    Check(lifetime.expired(), "completed refresh retained its closure");
    Check(queue.Submit(&second, [&] { retried.set_value(); }), "failed refresh permanently blocked its catalog");
    Check(retriedFuture.wait_for(2s) == std::future_status::ready, "refresh retry did not complete");
    std::cout << "CatalogRefreshTests passed (10000 coalesced clicks, failure/retry, closure release)\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
