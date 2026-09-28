#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace bcn::catalog_refresh
{
    // File-only catalog scans. Never put engine forms or actor operations here.
    // A single owned worker and one pending/running job per ordinary catalog
    // bound thread count and repeated Refresh clicks. SubmitLatest additionally
    // allows one replaceable successor for session metadata. No detached threads.
    class Queue final
    {
    public:
        Queue() : worker_([this](std::stop_token stop) { Run(stop); }) {}
        ~Queue() { worker_.request_stop(); ready_.notify_all(); }
        bool Submit(const void* key, std::function<void()> work,
            std::function<void(std::exception_ptr)> failed = {})
        {
            if (!key || !work) return false;
            std::scoped_lock lock(lock_);
            if (worker_.get_stop_token().stop_requested() || active_.contains(key)) return false;
            // Publish the key only after allocating the queued job succeeds.
            jobs_.push_back({ key, std::move(work), std::move(failed) });
            try { active_.insert(key); }
            catch (...) { jobs_.pop_back(); throw; }
            ready_.notify_one();
            return true;
        }
        // Session metadata may be invalidated while its file scan is running.
        // Keep at most one replacement, releasing every superseded capture.
        bool SubmitLatest(const void* key, std::function<void()> work,
            std::function<void(std::exception_ptr)> failed = {})
        {
            if (!key || !work) return false;
            std::scoped_lock lock(lock_);
            if (worker_.get_stop_token().stop_requested()) return false;
            for (auto& pending : jobs_) {
                if (pending.key == key) {
                    pending = { key, std::move(work), std::move(failed) };
                    return true;
                }
            }
            jobs_.push_back({ key, std::move(work), std::move(failed) });
            try { active_.insert(key); }
            catch (...) { jobs_.pop_back(); throw; }
            ready_.notify_one();
            return true;
        }
    private:
        struct Job {
            const void* key{};
            std::function<void()> work;
            std::function<void(std::exception_ptr)> failed;
        };
        void Run(std::stop_token stop)
        {
            while (!stop.stop_requested()) {
                Job job;
                {
                    std::unique_lock lock(lock_);
                    ready_.wait(lock, stop, [this] { return !jobs_.empty(); });
                    if (stop.stop_requested()) return;
                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                }
                try { job.work(); }
                catch (...) {
                    if (job.failed) {
                        try { job.failed(std::current_exception()); }
                        catch (...) {} // Reporting failure cannot terminate the worker.
                    }
                }
                std::scoped_lock lock(lock_);
                if (std::ranges::none_of(jobs_, [&](const auto& pending) { return pending.key == job.key; }))
                    active_.erase(job.key);
            }
        }
        std::mutex lock_;
        std::condition_variable_any ready_;
        std::deque<Job> jobs_;
        std::unordered_set<const void*> active_;
        std::jthread worker_; // joins before the queue, keys, condition and mutex die
    };

    inline Queue& Get()
    {
        static Queue queue;
        return queue;
    }
}
