/// @file test_async_executor.cpp
/// @brief Tests for the single-worker executor used to keep profiling off a web
///        framework's event-loop thread
///
/// The guarantees matter for correctness, not just performance: gperftools keeps
/// its profiling session in process-global state, so two analyses running at
/// once would corrupt each other's results. Submissions while a job runs are
/// rejected rather than queued, so a caller never waits behind someone else's
/// sampling window.

#include "../include/profiler/async_executor.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

using profiler::AsyncExecutor;
using namespace std::chrono_literals;

namespace {

/// Blocks the worker until released(), so rejection can be observed reliably.
class Blocker {
public:
    void hold() {
        std::unique_lock<std::mutex> lock(m_);
        started_ = true;
        cv_.notify_all();
        cv_.wait(lock, [&]() { return release_; });
    }

    void waitUntilStarted() {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait_for(lock, 5s, [&]() { return started_; });
    }

    void release() {
        {
            std::lock_guard<std::mutex> lock(m_);
            release_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    bool started_ = false;
    bool release_ = false;
};

} // namespace

// ---------------------------------------------------------------------------
// Serialization: the whole point of a single worker
// ---------------------------------------------------------------------------

TEST(AsyncExecutorTest, JobsNeverRunConcurrently) {
    AsyncExecutor executor;

    std::atomic<int> concurrent{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> completed{0};

    // Submit one job at a time, waiting for each to be accepted: submissions
    // while busy are refused by design, so a burst would be mostly rejected.
    constexpr int kJobs = 8;
    for (int i = 0; i < kJobs; ++i) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!executor.trySubmit([&]() {
            int now = ++concurrent;
            int seen = max_concurrent.load();
            while (now > seen && !max_concurrent.compare_exchange_weak(seen, now)) {}
            std::this_thread::sleep_for(1ms);
            --concurrent;
            ++completed;
        })) {
            ASSERT_LT(std::chrono::steady_clock::now(), deadline) << "job " << i << " never accepted";
            std::this_thread::sleep_for(1ms);
        }
    }

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (completed.load() < kJobs && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(completed.load(), kJobs);
    EXPECT_EQ(max_concurrent.load(), 1) << "single worker must never overlap jobs";
}

// ---------------------------------------------------------------------------
// Fail fast: no queue, so a second submission is refused immediately
// ---------------------------------------------------------------------------

TEST(AsyncExecutorTest, SubmitBlockingWaitsForTheRunningJob) {
    // Used by routes whose "may I proceed?" decision belongs to the core guard:
    // waiting here keeps the event-loop thread free while the worker finishes.
    AsyncExecutor executor;
    Blocker blocker;
    std::atomic<int> ran{0};

    ASSERT_TRUE(executor.submitBlocking([&]() { blocker.hold(); }));
    blocker.waitUntilStarted();

    // A second submission must wait rather than be rejected or run concurrently.
    std::atomic<bool> second_started{false};
    std::thread second([&]() {
        EXPECT_TRUE(executor.submitBlocking([&]() {
            second_started = true;
            ++ran;
        }));
    });

    // While the first job holds the worker, the second must not have run.
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(second_started.load());

    blocker.release();
    second.join();

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (ran.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(second_started.load());
    EXPECT_EQ(ran.load(), 1);
}

TEST(AsyncExecutorTest, RejectsWhileAJobIsRunning) {
    // trySubmit never waits: it is the fail-fast mode.
    AsyncExecutor executor;
    Blocker blocker;

    ASSERT_TRUE(executor.trySubmit([&]() { blocker.hold(); }));
    blocker.waitUntilStarted();
    EXPECT_TRUE(executor.isBusy());

    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(executor.trySubmit([]() {}));
    EXPECT_FALSE(executor.trySubmit([]() {}));
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    EXPECT_LT(elapsed, 100ms) << "rejection must not wait for the running job";

    blocker.release();
}

TEST(AsyncExecutorTest, AcceptsWorkAgainOnceTheJobFinishes) {
    AsyncExecutor executor;
    Blocker blocker;
    std::atomic<int> ran{0};

    ASSERT_TRUE(executor.trySubmit([&]() { blocker.hold(); }));
    blocker.waitUntilStarted();
    EXPECT_FALSE(executor.trySubmit([&]() { ++ran; }));

    blocker.release();

    // Wait for the worker to become idle again.
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (executor.isBusy() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_FALSE(executor.isBusy());

    EXPECT_TRUE(executor.trySubmit([&]() { ++ran; }));

    const auto deadline2 = std::chrono::steady_clock::now() + 5s;
    while (ran.load() < 1 && std::chrono::steady_clock::now() < deadline2) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(ran.load(), 1);
}

// ---------------------------------------------------------------------------
// Shutdown: an in-flight job must not outlive the executor
// ---------------------------------------------------------------------------

TEST(AsyncExecutorTest, DestructorWaitsForRunningJob) {
    std::atomic<int> ran{0};

    {
        AsyncExecutor executor;
        // Give the worker something to be busy with, then destroy immediately.
        ASSERT_TRUE(executor.trySubmit([&]() {
            std::this_thread::sleep_for(20ms);
            ++ran;
        }));
        // The destructor must block until the job has finished, so a job that
        // captured caller-owned state never runs after that state is gone.
    }

    EXPECT_EQ(ran.load(), 1) << "in-flight job was abandoned during shutdown";
}

TEST(AsyncExecutorTest, NewExecutorIsIdleAndRunning) {
    AsyncExecutor executor;
    EXPECT_TRUE(executor.isRunning());
    EXPECT_FALSE(executor.isBusy());
}
