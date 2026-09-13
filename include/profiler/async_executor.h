/// @file async_executor.h
/// @brief Single-worker executor for long-running profiler jobs, with fail-fast
///        rejection when a job is already running
///
/// Profiling operations block for seconds (or minutes, for CPU sampling), so
/// they must not run on a web framework's event-loop thread. This executor runs
/// a submitted job on a dedicated worker thread and delivers the result back to
/// the caller.
///
/// @section why-single-worker One worker, and no queue
///
/// A @b single worker is a correctness requirement, not a tuning choice:
/// gperftools keeps its CPU sampling session in process-global state, so
/// concurrent analyses would corrupt each other's results.
///
/// Submissions while a job is running are @b rejected rather than queued. This
/// mirrors Go's @c net/http/pprof, where a second concurrent CPU profile request
/// fails immediately with "cpu profiling already in use". Queueing would instead
/// accept the request and leave the caller silently waiting up to several
/// minutes behind someone else's sampling window, which is worse than an honest
/// error.

#pragma once

#include "profiler_version.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

PROFILER_NAMESPACE_BEGIN

/// @class AsyncExecutor
/// @brief Runs one job at a time on a background thread; rejects while busy
///
/// @note Thread-safe. @ref trySubmit may be called from any thread.
class AsyncExecutor {
public:
    /// @brief Construct the executor and start its worker thread
    AsyncExecutor() : worker_([this]() { run(); }) {}

    AsyncExecutor(const AsyncExecutor&) = delete;
    AsyncExecutor& operator=(const AsyncExecutor&) = delete;

    /// @brief Stop accepting work, finish any running job, then join the worker
    ///
    /// Blocks until the in-flight job has completed, so a job that captured
    /// references to caller-owned state cannot outlive that state.
    ~AsyncExecutor() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    /// @brief Hand a job to the worker without blocking on its completion
    /// @param job Callable invoked on the worker thread
    /// @return true if the job was accepted; false if a job is already running
    ///         or the executor is shutting down. On false the job is @b not run
    ///         and the caller must report backpressure.
    bool trySubmit(std::function<void()> job) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stopping_ || busy_) {
            return false; // fail fast rather than queueing behind a running job
        }
        pending_ = std::move(job);
        busy_ = true;
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    /// @brief Wait for the worker to become free, then hand it a job
    ///
    /// Use this when the decision "may this request proceed?" belongs to the job
    /// itself, because the profiler core answers it accurately (e.g. "cpu
    /// profiling already in use"). Waiting here keeps the event-loop thread free,
    /// and the wait stays short in practice because such a job is rejected
    /// promptly by the core guard.
    ///
    /// @param job Callable invoked on the worker thread
    /// @return true once the job has been handed over; false only if the executor
    ///         is shutting down (the job is then @b not run)
    bool submitBlocking(std::function<void()> job) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return stopping_ || !busy_; });
        if (stopping_) {
            return false;
        }
        pending_ = std::move(job);
        busy_ = true;
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    /// @brief Whether a job is currently executing
    bool isBusy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return busy_;
    }

    /// @brief Whether the worker thread is still accepting and running jobs
    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !stopping_;
    }

private:
    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stopping_ || pending_ != nullptr; });
                if (pending_ == nullptr) {
                    return; // stopping and idle
                }
                // Clear pending_ *before* running so a finished job can never be
                // picked up twice, and hand ownership to this thread.
                job = std::move(pending_);
                pending_ = nullptr;
            }

            job();

            // Clear busy_ and hand the worker back to any waiter. The notify
            // wakes submitBlocking(), which is why the lock is released before
            // notifying rather than after.
            std::unique_lock<std::mutex> lock(mutex_);
            busy_ = false;
            lock.unlock();
            cv_.notify_all();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::function<void()> pending_;
    bool busy_ = false;
    bool stopping_ = false;
    std::thread worker_;
};

PROFILER_NAMESPACE_END
