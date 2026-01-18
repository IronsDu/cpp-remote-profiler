#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <signal.h>
#include "symbolize.h"

namespace profiler {

enum class ProfilerType {
    CPU,
    HEAP,
    HEAP_GROWTH
};

struct ProfilerState {
    bool is_running;
    std::string output_path;
    uint64_t start_time;
    uint64_t duration;
};

// Structure to hold captured stack trace for a thread
struct ThreadStackTrace {
    pid_t tid;
    void* addresses[64];  // Fixed-size array for signal-safety
    int depth;
    bool captured;
};

// Shared memory structure for inter-thread communication
struct SharedStackTrace {
    std::atomic<bool> signal_received;  // Set by signal handler
    std::atomic<bool> ready;             // Set by thread after capturing
    char padding[64];                     // To avoid false sharing
    pid_t tid;
    int depth;
    void* addresses[64];
};

class ProfilerManager {
public:
    static ProfilerManager& getInstance();

    // Start CPU profiler
    bool startCPUProfiler(const std::string& output_path = "cpu.prof");

    // Stop CPU profiler
    bool stopCPUProfiler();

    // Start Heap profiler
    bool startHeapProfiler(const std::string& output_path = "heap.prof");

    // Stop Heap profiler
    bool stopHeapProfiler();

    // Get profiler state
    ProfilerState getProfilerState(ProfilerType type) const;

    // Check if profiler is running
    bool isProfilerRunning(ProfilerType type) const;

    // Resolve address to symbol with inline frames using backward-cpp
    std::string resolveSymbolWithBackward(void* address);

    // Analyze CPU profile and return SVG flame graph
    // duration: sampling duration in seconds
    // output_type: "flamegraph" (default), "iciclegraph", etc.
    std::string analyzeCPUProfile(int duration, const std::string& output_type = "flamegraph");

    // Analyze Heap profile and return SVG flame graph
    // duration: sampling duration in seconds
    // output_type: "flamegraph" (default), "iciclegraph", etc.
    std::string analyzeHeapProfile(int duration, const std::string& output_type = "flamegraph");

    // Get raw CPU profile data (for /pprof/profile endpoint)
    // seconds: sampling duration in seconds
    // Returns raw profile binary data
    std::string getRawCPUProfile(int seconds);

    // Get raw heap sample data (for /pprof/heap endpoint)
    // Returns heap sample in text format (compatible with pprof)
    std::string getRawHeapSample();

    // Get heap growth stacks data (for /pprof/growth endpoint)
    // Returns heap growth stacks in text format (compatible with pprof)
    std::string getRawHeapGrowthStacks();

    // Get all thread stacks (for /api/thread/stacks endpoint)
    // Returns thread stacks in text format
    std::string getThreadStacks();

    // Get thread callstack with full backtrace using signal handler
    std::string getThreadCallStacks();

    // Signal to use for stack capture (SIGUSR1 - Drogon may use SIGUSR2)
    static constexpr int STACK_CAPTURE_SIGNAL = SIGUSR1;

    // Signal handler for capturing stack traces (signal-safe)
    static void signalHandler(int signum, siginfo_t* info, void* context);

    // Helper methods (public for use in HTTP handlers)
    bool executeCommand(const std::string& cmd, std::string& output);
    std::string getExecutablePath();

private:
    ProfilerManager();
    ~ProfilerManager();
    ProfilerManager(const ProfilerManager&) = delete;
    ProfilerManager& operator=(const ProfilerManager&) = delete;

    std::string findLatestHeapProfile(const std::string& dir);

    // Generate flame graph from collapsed format using flamegraph.pl
    std::string generateFlameGraph(
        const std::string& collapsed_file,
        const std::string& title);

    // Capture stack traces from all threads using signals
    std::vector<ThreadStackTrace> captureAllThreadStacks();

    // Symbolize addresses using abseil
    std::string symbolizeAddress(void* addr);

    std::string profile_dir_;
    std::map<ProfilerType, ProfilerState> profiler_states_;
    mutable std::mutex mutex_;

    // Concurrency control for /pprof/profile requests
    std::atomic<bool> cpu_profiling_in_progress_{false};

    // Symbolizer (backward-cpp)
    std::unique_ptr<Symbolizer> symbolizer_;

    // Flag to indicate stack capture is in progress (atomic for signal-safety)
    static std::atomic<bool> capture_in_progress_;

    // Shared memory for stack traces (accessible by all threads)
    static SharedStackTrace* shared_stacks_;
    static int max_threads_;

    // PID of main thread
    static pid_t main_thread_id_;
};

} // namespace profiler
