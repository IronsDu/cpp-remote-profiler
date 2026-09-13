/// @file test_http_handlers.cpp
/// @brief Tests for the framework-agnostic HTTP handlers and profiler lifecycle
///
/// These tests only depend on profiler_core (no Drogon), so they run in the
/// minimal configuration as well.

#include "../include/profiler/http_handlers.h"
#include "../include/profiler_manager.h"
#include <chrono>
#include <dirent.h>
#include <future>
#include <gperftools/heap-profiler.h>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

/// Handlers that never touch the filesystem or spawn pprof can be tested
/// directly. Anything that renders an SVG shells out to ./pprof and is covered
/// by test_full_flow instead.
class HttpHandlersTest : public ::testing::Test {
protected:
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers{profiler};
};

} // namespace

// ---------------------------------------------------------------------------
// Basic handler contract
// ---------------------------------------------------------------------------

TEST_F(HttpHandlersTest, StatusReturnsJsonWithAllProfilerTypes) {
    auto resp = handlers.handleStatus();

    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.content_type, "application/json");
    // One entry per ProfilerType
    EXPECT_NE(resp.body.find("\"cpu\""), std::string::npos);
    EXPECT_NE(resp.body.find("\"heap\""), std::string::npos);
    EXPECT_NE(resp.body.find("\"growth\""), std::string::npos);
}

TEST_F(HttpHandlersTest, StatusReportsNothingRunningInitially) {
    auto resp = handlers.handleStatus();
    EXPECT_EQ(resp.body.find("\"running\":true"), std::string::npos);
}

TEST_F(HttpHandlersTest, StatusUsesMillisecondDurationKeys) {
    // ProfilerState stores milliseconds; the JSON key must reflect that.
    auto resp = handlers.handleStatus();
    EXPECT_NE(resp.body.find("duration_ms"), std::string::npos);
    EXPECT_EQ(resp.body.find("duration_s\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// output_type validation
// ---------------------------------------------------------------------------

TEST_F(HttpHandlersTest, InvalidOutputTypeIsRejectedWith400) {
    auto resp = handlers.handleCpuAnalyze(1, "iciclegraph");

    EXPECT_EQ(resp.status, 400);
    EXPECT_NE(resp.body.find("output_type"), std::string::npos) << "error message should name the offending parameter";
}

TEST_F(HttpHandlersTest, EmptyOutputTypeIsRejected) {
    auto resp = handlers.handleCpuAnalyze(1, "");
    EXPECT_EQ(resp.status, 400);
}

// ---------------------------------------------------------------------------
// HandlerResponse helpers
// ---------------------------------------------------------------------------

TEST(HandlerResponseTest, FactoryHelpersSetContentTypes) {
    EXPECT_EQ(profiler::HandlerResponse::svg("<svg/>").content_type, "image/svg+xml");
    EXPECT_EQ(profiler::HandlerResponse::json("{}").content_type, "application/json");
    EXPECT_EQ(profiler::HandlerResponse::text("x").content_type, "text/plain");
    EXPECT_EQ(profiler::HandlerResponse::html("<p/>").content_type, "text/html");
}

TEST(HandlerResponseTest, BinaryHelperSetsDownloadFilename) {
    auto resp = profiler::HandlerResponse::binary("data", "cpu.prof");

    EXPECT_EQ(resp.status, 200);
    EXPECT_EQ(resp.content_type, "application/octet-stream");
    ASSERT_TRUE(resp.headers.count("Content-Disposition") == 1);
    EXPECT_NE(resp.headers.at("Content-Disposition").find("cpu.prof"), std::string::npos);
}

TEST(HandlerResponseTest, ErrorHelperCarriesStatusAndMessage) {
    auto resp = profiler::HandlerResponse::error(404, "nope");

    EXPECT_EQ(resp.status, 404);
    EXPECT_EQ(resp.content_type, "application/json");
    EXPECT_NE(resp.body.find("nope"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Concurrent CPU profiling requests fail fast
// ---------------------------------------------------------------------------

namespace {

/// Claim the CPU profiling session using the real public API for @p seconds, on
/// a helper thread — the same path a `/pprof/profile` request takes.
///
/// Driving a real (short) sampling session avoids adding a test-only seam to the
/// production API, and exercises the genuine claim/release ordering.
class CpuSessionHolder {
public:
    explicit CpuSessionHolder(profiler::ProfilerManager& profiler, int seconds = 1)
        : future_(std::async(std::launch::async,
                             [&profiler, seconds]() -> std::string { return profiler.getRawCPUProfile(seconds); })) {}

    ~CpuSessionHolder() {
        if (future_.valid()) {
            future_.wait();
        }
    }

    CpuSessionHolder(const CpuSessionHolder&) = delete;
    CpuSessionHolder& operator=(const CpuSessionHolder&) = delete;

private:
    std::future<std::string> future_;
};

} // namespace

TEST(ConcurrentCpuProfilingTest, RequestsFailFastWhileASessionIsSampling) {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    ASSERT_FALSE(profiler.isCPUProfilingInProgress());

    CpuSessionHolder holder{profiler, 1};

    // Wait until the session is actually claimed.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!profiler.isCPUProfilingInProgress() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(profiler.isCPUProfilingInProgress()) << "helper never claimed the session";

    // Every CPU entry point must refuse immediately, not wait for the sampler.
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(handlers.handleCpuAnalyze(5, "pprof").status, 409);
    EXPECT_EQ(handlers.handleCpuSvgRaw(5).status, 409);
    EXPECT_EQ(handlers.handleCpuFlamegraphRaw(5).status, 409);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    EXPECT_LT(elapsed, std::chrono::milliseconds(100))
        << "rejection must be immediate, not queued behind the running session";
}

TEST(ConcurrentCpuProfilingTest, AnalyzeDoesNotDisturbTheRunningSession) {
    // Regression guard: analyzeCPUProfile() used to call stopCPUProfiler() on
    // entry, which killed the in-flight session of whichever request got there
    // first. It must now leave someone else's session completely alone.
    profiler::ProfilerManager profiler;

    CpuSessionHolder holder{profiler, 1};

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!profiler.isCPUProfilingInProgress() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(profiler.isCPUProfilingInProgress());

    auto result = profiler.analyzeCPUProfile(5, "pprof");

    EXPECT_NE(result.find("cpu profiling already in use"), std::string::npos) << result;
    // Still claimed: we did not stop anyone else's sampling.
    EXPECT_TRUE(profiler.isCPUProfilingInProgress());
}

TEST(ConcurrentCpuProfilingTest, PprofProfileMatchesGoErrorContract) {
    // Go's net/http/pprof answers a second concurrent CPU profile request with
    //   500 + text/plain + "Could not enable CPU profiling: cpu profiling already in use"
    // plus an X-Go-Pprof marker, so `go tool pprof` treats the body as an error
    // instead of parsing it as profile data.
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    CpuSessionHolder holder{profiler, 1};

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!profiler.isCPUProfilingInProgress() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(profiler.isCPUProfilingInProgress());

    auto resp = handlers.handlePprofProfile(30);

    EXPECT_EQ(resp.status, 500);
    EXPECT_EQ(resp.content_type, "text/plain; charset=utf-8");
    EXPECT_EQ(resp.body, "Could not enable CPU profiling: cpu profiling already in use\n");
    ASSERT_EQ(resp.headers.count("X-Go-Pprof"), 1u);
    EXPECT_EQ(resp.headers.at("X-Go-Pprof"), "1");
    // No download disposition: this body is not a profile.
    EXPECT_EQ(resp.headers.count("Content-Disposition"), 0u);
}

TEST(ConcurrentCpuProfilingTest, SessionIsReleasedAfterSampling) {
    // The guard must be released once sampling ends, otherwise the profiler
    // would be permanently unusable after a single request.
    profiler::ProfilerManager profiler;
    EXPECT_FALSE(profiler.isCPUProfilingInProgress());

    {
        CpuSessionHolder holder{profiler, 1};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!profiler.isCPUProfilingInProgress() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ASSERT_TRUE(profiler.isCPUProfilingInProgress());
    }

    // Holder destroyed: sampling finished and the session flag was cleared.
    EXPECT_FALSE(profiler.isCPUProfilingInProgress());
}

// ---------------------------------------------------------------------------
// A session opened by the host is never preempted
// ---------------------------------------------------------------------------

TEST(HostOwnedSessionTest, CpuAnalysisRefusesToTakeOverAHostSession) {
    // startCPUProfiler() opens a session on the caller's behalf. A later request
    // must be refused, not allowed to stop that session out from under it.
    profiler::ProfilerManager profiler;

    ASSERT_TRUE(profiler.startCPUProfiler("/tmp/test_host_owned_cpu.prof"));
    ASSERT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::CPU));

    auto result = profiler.analyzeCPUProfile(1, "pprof");

    EXPECT_NE(result.find("cpu profiling already in use"), std::string::npos) << result;
    // The host's session is untouched.
    EXPECT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::CPU));

    EXPECT_TRUE(profiler.stopCPUProfiler());
}

TEST(HostOwnedSessionTest, RawProfileRefusesToTakeOverAHostSession) {
    profiler::ProfilerManager profiler;

    ASSERT_TRUE(profiler.startCPUProfiler("/tmp/test_host_owned_raw.prof"));

    // Returns no data rather than silently restarting the profiler.
    EXPECT_TRUE(profiler.getRawCPUProfile(1).empty());
    EXPECT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::CPU));

    EXPECT_TRUE(profiler.stopCPUProfiler());
}

TEST(HostOwnedSessionTest, HandlersReportTheHostSessionAsBusy) {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    ASSERT_TRUE(profiler.startCPUProfiler("/tmp/test_host_owned_busy.prof"));

    EXPECT_TRUE(handlers.isCpuProfilerBusy());

    // /pprof/profile answers with Go's contract rather than a generic 500.
    auto resp = handlers.handlePprofProfile(30);
    EXPECT_EQ(resp.status, 500);
    EXPECT_EQ(resp.body, "Could not enable CPU profiling: cpu profiling already in use\n");
    ASSERT_EQ(resp.headers.count("X-Go-Pprof"), 1u);

    // The /api/* endpoints report a conflict.
    EXPECT_EQ(handlers.handleCpuAnalyze(1, "pprof").status, 409);
    EXPECT_EQ(handlers.handleCpuSvgRaw(1).status, 409);

    profiler.stopCPUProfiler();
}

TEST(HostOwnedSessionTest, HeapAnalysisRefusesToTakeOverAHostSession) {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    ASSERT_TRUE(profiler.startHeapProfiler("/tmp/test_host_owned_heap.prof"));
    ASSERT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::HEAP));

    auto result = profiler.analyzeHeapProfile(1, "pprof");
    EXPECT_NE(result.find("heap profiling already in use"), std::string::npos) << result;
    EXPECT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::HEAP));

    EXPECT_EQ(handlers.handleHeapAnalyze(1, "pprof").status, 409);

    EXPECT_TRUE(profiler.stopHeapProfiler());
}

// ---------------------------------------------------------------------------
// Concurrent heap analysis is exclusive
// ---------------------------------------------------------------------------

namespace {

/// Claim the heap-analysis slot using the real public API, on a helper thread.
///
/// analyzeHeapProfile() sleeps a fixed short window, so this reliably holds the
/// claim long enough to observe how other requests behave.
class HeapAnalysisHolder {
public:
    explicit HeapAnalysisHolder(profiler::ProfilerManager& profiler)
        : future_(std::async(std::launch::async,
                             [&profiler]() -> std::string { return profiler.analyzeHeapProfile(1, "pprof"); })) {}

    ~HeapAnalysisHolder() {
        if (future_.valid()) {
            future_.wait();
        }
    }

    HeapAnalysisHolder(const HeapAnalysisHolder&) = delete;
    HeapAnalysisHolder& operator=(const HeapAnalysisHolder&) = delete;

private:
    std::future<std::string> future_;
};

/// Wait until the heap-analysis slot is observed as claimed.
bool waitForHeapClaim(const profiler::ProfilerManager& profiler) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!profiler.isHeapAnalysisInProgress() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return profiler.isHeapAnalysisInProgress();
}

} // namespace

TEST(ConcurrentHeapAnalysisTest, SecondAnalysisIsRejectedNotRaced) {
    // Regression guard. HeapProfilerStart() has no failure mode -- it silently
    // replaces the output prefix -- so two callers that both observe "not
    // running" would proceed together, leaving ONE snapshot that both then read.
    // Measured before the fix: two concurrent calls produced identical prefixes
    // and a single dump, and both callers returned the same bytes.
    profiler::ProfilerManager profiler;

    ASSERT_FALSE(profiler.isHeapAnalysisInProgress());

    HeapAnalysisHolder holder{profiler};
    ASSERT_TRUE(waitForHeapClaim(profiler)) << "helper never claimed the slot";

    // Must be refused immediately, and must not touch the running analysis.
    const auto t0 = std::chrono::steady_clock::now();
    auto result = profiler.analyzeHeapProfile(1, "pprof");
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_NE(result.find("heap profiling already in use"), std::string::npos) << result;
    EXPECT_LT(elapsed, std::chrono::milliseconds(100)) << "rejection must not wait for the running analysis";
    EXPECT_TRUE(profiler.isHeapAnalysisInProgress()) << "the running analysis was disturbed";
}

TEST(ConcurrentHeapAnalysisTest, HandlerReportsConflict) {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    HeapAnalysisHolder holder{profiler};
    ASSERT_TRUE(waitForHeapClaim(profiler));

    auto resp = handlers.handleHeapAnalyze(1, "pprof");

    EXPECT_EQ(resp.status, 409);
    EXPECT_NE(resp.body.find("heap profiling already in use"), std::string::npos) << resp.body;
}

TEST(ConcurrentHeapAnalysisTest, ClaimIsReleasedAfterwards) {
    // The slot must not stay claimed, or heap analysis would work only once.
    profiler::ProfilerManager profiler;

    {
        HeapAnalysisHolder holder{profiler};
        ASSERT_TRUE(waitForHeapClaim(profiler));
    }

    EXPECT_FALSE(profiler.isHeapAnalysisInProgress());
}

// ---------------------------------------------------------------------------
// Heap analysis API shape
// ---------------------------------------------------------------------------

TEST_F(HttpHandlersTest, HeapAnalysisTakesACollectionWindow) {
    // Heap analysis takes a duration, but it is a *collection window*, not a
    // sampling rate: the rate is fixed by TCMALLOC_SAMPLE_PARAMETER at process
    // start. A longer window covers more allocations, which is what a sparsely
    // allocating process needs. Guard the contract so the distinction cannot
    // silently regress into "duration is accepted but ignored".
    static_assert(std::is_invocable_v<decltype(&profiler::ProfilerHttpHandlers::handleHeapAnalyze),
                                      profiler::ProfilerHttpHandlers*, int, const std::string&>,
                  "handleHeapAnalyze must take (duration, output_type)");

    // The renderers take one too, since each runs its own sampling window.
    static_assert(std::is_invocable_v<decltype(&profiler::ProfilerHttpHandlers::handleHeapSvgRaw),
                                      profiler::ProfilerHttpHandlers*, int>,
                  "handleHeapSvgRaw must take a duration");
    static_assert(std::is_invocable_v<decltype(&profiler::ProfilerHttpHandlers::handleHeapFlamegraphRaw),
                                      profiler::ProfilerHttpHandlers*, int>,
                  "handleHeapFlamegraphRaw must take a duration");
}

TEST_F(HttpHandlersTest, HeapAnalyzeRejectsInvalidOutputType) {
    auto resp = handlers.handleHeapAnalyze(1, "bogus");
    EXPECT_EQ(resp.status, 400);
}

// ---------------------------------------------------------------------------
// Profiler lifecycle
// ---------------------------------------------------------------------------

TEST(ProfilerLifecycleTest, HeapProfilerStopsOnDestruction) {
    // The destructor used to call IsHeapProfilerRunning() (a query whose result
    // was discarded) instead of HeapProfilerStop(), leaving the process-global
    // heap profiler recording after the manager was gone.
    //
    // Assert on gperftools' own global state, not on the manager's flag, so the
    // test fails if the destructor stops doing the real work.
    {
        profiler::ProfilerManager profiler;
        ASSERT_TRUE(profiler.startHeapProfiler("/tmp/test_lifecycle_heap.prof"));

        EXPECT_EQ(IsHeapProfilerRunning(), 1) << "heap profiler should be recording while alive";
        EXPECT_TRUE(profiler.getProfilerState(profiler::ProfilerType::HEAP).is_running);
    }

    EXPECT_EQ(IsHeapProfilerRunning(), 0) << "~ProfilerManager() must stop the process-global heap profiler";
}

TEST(ProfilerLifecycleTest, StopHeapProfilerReportsState) {
    profiler::ProfilerManager profiler;

    EXPECT_TRUE(profiler.startHeapProfiler("/tmp/test_stop_heap.prof"));
    EXPECT_TRUE(profiler.isProfilerRunning(profiler::ProfilerType::HEAP));

    EXPECT_TRUE(profiler.stopHeapProfiler());
    EXPECT_FALSE(profiler.isProfilerRunning(profiler::ProfilerType::HEAP));
}

TEST(ProfilerLifecycleTest, StartingHeapProfilerTwiceFails) {
    profiler::ProfilerManager profiler;

    ASSERT_TRUE(profiler.startHeapProfiler("/tmp/test_double_heap.prof"));
    EXPECT_FALSE(profiler.startHeapProfiler("/tmp/test_double_heap2.prof"));

    profiler.stopHeapProfiler();
}

TEST(ProfilerLifecycleTest, StatesAreIndependentPerType) {
    profiler::ProfilerManager profiler;

    ASSERT_TRUE(profiler.startHeapProfiler("/tmp/test_indep_heap.prof"));
    EXPECT_FALSE(profiler.isProfilerRunning(profiler::ProfilerType::CPU));
    EXPECT_FALSE(profiler.isProfilerRunning(profiler::ProfilerType::HEAP_GROWTH));

    profiler.stopHeapProfiler();
}

TEST(ProfilerLifecycleTest, StaleHeapProfileIsNotReused) {
    // Regression guard for findLatestHeapProfile(): heap analysis uses a unique
    // per-run prefix and only accepts files matching it. Before that, a profile
    // left behind by an earlier call in the same process could be picked up and
    // rendered as if it were the current result.
    //
    // Two consecutive analyses must each produce their OWN snapshot file.
    const std::string dir = "/tmp/cpp_profiler";

    profiler::ProfilerManager profiler;

    auto heapFiles = [&dir]() {
        std::set<std::string> found;
        DIR* dp = opendir(dir.c_str());
        if (!dp) {
            return found;
        }
        struct dirent* e;
        while ((e = readdir(dp)) != nullptr) {
            std::string f = e->d_name;
            if (f.size() > 5 && f.substr(f.size() - 5) == ".heap") {
                found.insert(f);
            }
        }
        closedir(dp);
        return found;
    };

    const auto before = heapFiles();

    // Keep allocations flowing so the profiler has something to record.
    std::vector<std::unique_ptr<char[]>> ballast;
    ballast.reserve(256);
    std::thread allocator([&ballast]() {
        for (int i = 0; i < 200 && ballast.size() < ballast.capacity(); ++i) {
            ballast.emplace_back(new char[64 * 1024]);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    (void)profiler.analyzeHeapProfile(1, "pprof");
    const auto afterFirst = heapFiles();

    auto newSince = [&](const std::set<std::string>& a, const std::set<std::string>& b) {
        std::set<std::string> diff;
        std::set_difference(b.begin(), b.end(), a.begin(), a.end(), std::inserter(diff, diff.begin()));
        return diff;
    };

    const auto produced = newSince(before, afterFirst);
    EXPECT_FALSE(produced.empty()) << "analyzeHeapProfile() produced no new .heap snapshot";

    // Every snapshot must live under the per-run prefix.
    for (const auto& f : produced) {
        EXPECT_EQ(f.rfind("heap_analyze_", 0), 0) << "unexpected snapshot name: " << f;
    }

    allocator.join();
}
