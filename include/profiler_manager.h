#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "stack_collector.h"
#include "profile_parser.h"
#include "symbolize.h"

namespace profiler {

enum class ProfilerType {
    CPU,
    HEAP
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

    // Get profiling data as string
    std::string getCPUProfileData();

    // Get heap profile data
    std::string getHeapProfileData();

    // Get profile data as JSON for flame graph rendering
    std::string getProfileAsJSON(const std::string& profile_type);

    // Get flame graph data with call stack hierarchy
    std::string getFlameGraphData(const std::string& profile_type);

    // Resolve address to symbol name (for /pprof/symbol endpoint)
    std::string resolveSymbol(const std::string& profile_path, const std::string& address);

    // Resolve address to symbol with inline frames using backward-cpp
    std::string resolveSymbolWithBackward(void* address);

    // Get raw profile samples (addresses) for frontend rendering
    std::string getProfileSamples(const std::string& profile_type);

    // Get collapsed stack traces for flame graph (format: "func1;func2;func3 count")
    std::string getCollapsedStacks(const std::string& profile_type);

    // Get CPU profile address stacks (format: "addr1 addr2 addr3 count")
    std::string getCPUProfileAddresses();

    // Analyze CPU profile and return SVG flame graph
    // duration: sampling duration in seconds
    // output_type: "flamegraph" (default), "iciclegraph", etc.
    std::string analyzeCPUProfile(int duration, const std::string& output_type = "flamegraph");

    // Analyze Heap profile and return SVG flame graph
    // duration: sampling duration in seconds
    // output_type: "flamegraph" (default), "iciclegraph", etc.
    std::string analyzeHeapProfile(int duration, const std::string& output_type = "flamegraph");

    // List available profiles
    std::vector<std::string> listProfiles() const;

    // Get profile directory
    std::string getProfileDir() const { return profile_dir_; }

    void setProfileDir(const std::string& dir) { profile_dir_ = dir; }

private:
    ProfilerManager();
    ~ProfilerManager();
    ProfilerManager(const ProfilerManager&) = delete;
    ProfilerManager& operator=(const ProfilerManager&) = delete;

    bool executeCommand(const std::string& cmd, std::string& output);
    std::string findLatestHeapProfile(const std::string& dir);

    std::string profile_dir_;
    std::map<ProfilerType, ProfilerState> profiler_states_;
    mutable std::mutex mutex_;

    // Symbolizer (backward-cpp)
    std::unique_ptr<Symbolizer> symbolizer_;
};

} // namespace profiler
