#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
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

    // Get collapsed stack traces for flame graph (format: "func1;func2;func3 count")
    std::string getCollapsedStacks(const std::string& profile_type);

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

    // Helper methods (public for use in HTTP handlers)
    bool executeCommand(const std::string& cmd, std::string& output);
    std::string getExecutablePath();

private:
    ProfilerManager();
    ~ProfilerManager();
    ProfilerManager(const ProfilerManager&) = delete;
    ProfilerManager& operator=(const ProfilerManager&) = delete;

    std::string findLatestHeapProfile(const std::string& dir);

    std::string profile_dir_;
    std::map<ProfilerType, ProfilerState> profiler_states_;
    mutable std::mutex mutex_;

    // Concurrency control for /pprof/profile requests
    std::atomic<bool> cpu_profiling_in_progress_{false};

    // Symbolizer (backward-cpp)
    std::unique_ptr<Symbolizer> symbolizer_;
};

} // namespace profiler
