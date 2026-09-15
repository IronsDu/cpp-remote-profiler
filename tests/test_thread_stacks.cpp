/// @file test_thread_stacks.cpp
/// @brief Tests for thread stack capture and the /api/thread/stacks output
///
/// Only depends on profiler_core (no Drogon), so it runs in the minimal
/// configuration as well.

#include "../include/profiler/http_handlers.h"
#include "../include/profiler_manager.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <thread>

namespace {

/// A thread parked on a condition variable, i.e. genuinely blocked.
///
/// The capture is signal-driven, so a thread has to exist and be waiting for the
/// test to have something to find.
class BlockedThread {
public:
    BlockedThread()
        : worker_([this]() {
              std::unique_lock<std::mutex> lock(mutex_);
              // Announce readiness before parking, so the test cannot
              // capture while this thread is still starting up and get
              // an empty result that passes by accident.
              ready_ = true;
              ready_cv_.notify_all();
              cv_.wait(lock, [this]() { return release_; });
          }) {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_cv_.wait(lock, [this]() { return ready_; });
    }

    ~BlockedThread() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            release_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable())
            worker_.join();
    }

    BlockedThread(const BlockedThread&) = delete;
    BlockedThread& operator=(const BlockedThread&) = delete;

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable ready_cv_;
    bool release_ = false;
    bool ready_ = false;
    std::thread worker_;
};

} // namespace

TEST(ThreadStacksTest, CaptureProducesASymbolisedHeader) {
    profiler::ProfilerManager profiler;

    const std::string stacks = profiler.getThreadCallStacks();

    ASSERT_FALSE(stacks.empty()) << "capture produced nothing";
    EXPECT_NE(stacks.find("Thread Call Stacks"), std::string::npos);
    EXPECT_NE(stacks.find("Total threads captured:"), std::string::npos);
}

TEST(ThreadStacksTest, ReportsThreadNamesAlongsideTids) {
    // A bare tid is unreadable after the fact; the name is what identifies the
    // thread. Linux supplies it via /proc/<tid>/comm and the kernel truncates it
    // to 15 characters, which is what ps and top show too.
    profiler::ProfilerManager profiler;
    BlockedThread parked;

    const std::string stacks = profiler.getThreadCallStacks();
    ASSERT_FALSE(stacks.empty());
    // A capture that found nothing would still contain the header, so require a
    // real capture before checking its shape.
    ASSERT_EQ(stacks.find("Total threads captured: 0"), std::string::npos) << "capture found no threads at all:\n"
                                                                           << stacks;

    // Find a "Thread <tid> (<name>):" line and check the name is plausible.
    const std::string marker = "Thread ";
    std::size_t pos = stacks.find(marker);
    ASSERT_NE(pos, std::string::npos);

    bool found_named_thread = false;
    while (pos != std::string::npos) {
        const std::size_t open = stacks.find('(', pos);
        const std::size_t close = stacks.find(')', pos);
        const std::size_t line_end = stacks.find('\n', pos);
        // The parentheses must belong to this header line (i.e. precede any colon
        // that ends it), otherwise we are looking at an unrelated bracket later.
        if (open != std::string::npos && close != std::string::npos && close < line_end && open < close) {
            const std::string name = stacks.substr(open + 1, close - open - 1);
            if (!name.empty() && name.size() <= 15) {
                found_named_thread = true;
                break;
            }
        }
        pos = stacks.find(marker, pos + marker.size());
    }
    EXPECT_TRUE(found_named_thread) << "no 'Thread <tid> (<name>):' header found in:\n" << stacks;
}

TEST(ThreadStacksTest, BlockedThreadsReportTheirBlockingFrame) {
    // The point of the endpoint is "where is each thread stuck", and the answer
    // is the first frame that is not part of the capture machinery itself.
    profiler::ProfilerManager profiler;
    BlockedThread parked;

    const std::string stacks = profiler.getThreadCallStacks();
    ASSERT_FALSE(stacks.empty());
    ASSERT_EQ(stacks.find("Total threads captured: 0"), std::string::npos) << "capture found no threads at all:\n"
                                                                           << stacks;

    const char* machinery[] = {"signalHandler", "__restore_rt", "__syscall_cancel", "__internal_syscall_cancel",
                               "pthread_kill",  "raise"};
    bool found_real_frame = false;
    std::size_t pos = 0;
    while ((pos = stacks.find("    #", pos)) != std::string::npos) {
        const std::size_t eol = stacks.find('\n', pos);
        const std::string frame = stacks.substr(pos, eol - pos);
        bool is_machinery = false;
        for (const char* m : machinery) {
            if (frame.find(m) != std::string::npos) {
                is_machinery = true;
                break;
            }
        }
        if (!is_machinery) {
            found_real_frame = true;
            break;
        }
        pos = eol;
    }
    EXPECT_TRUE(found_real_frame) << "every frame looked like capture machinery:\n" << stacks;
}

TEST(ThreadStacksTest, HandlerServesTheCapture) {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    BlockedThread parked;
    const auto resp = handlers.handleThreadStacks();

    ASSERT_EQ(resp.status, 200);
    EXPECT_NE(resp.body.find("Thread Call Stacks"), std::string::npos);
    EXPECT_EQ(resp.body.find("Total threads captured: 0"), std::string::npos) << "endpoint captured no threads:\n"
                                                                              << resp.body;

    // Thread names must survive the handler too, since this is the endpoint the
    // panel calls. Assert the actual "Thread <tid> (<name>):" shape rather than
    // merely that some parenthesis exists somewhere.
    bool named = false;
    for (std::size_t pos = resp.body.find("Thread "); pos != std::string::npos;
         pos = resp.body.find("Thread ", pos + 1)) {
        const auto open = resp.body.find('(', pos);
        const auto close = resp.body.find(')', pos);
        const auto line_end = resp.body.find('\n', pos);
        if (open != std::string::npos && close != std::string::npos && close < line_end && open < close &&
            close > open + 1) {
            named = true;
            break;
        }
    }
    EXPECT_TRUE(named) << "no named thread header in:\n" << resp.body;
}
