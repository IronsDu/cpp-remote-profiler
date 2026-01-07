#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

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

    // Get symbolized profile data
    std::string getSymbolizedProfile(const std::string& profile_path);

    // Convert prof to SVG (火焰图)
    std::string getProfileSVG(const std::string& profile_path);

    // Generate SVG from profile type
    std::string generateSVGFromProfile(const std::string& profile_type);

    // Get profile data as JSON for flame graph rendering
    std::string getProfileAsJSON(const std::string& profile_type);

    // Get flame graph data with call stack hierarchy
    std::string getFlameGraphData(const std::string& profile_type);

    // Resolve address to symbol name (for /pprof/symbol endpoint)
    std::string resolveSymbol(const std::string& profile_path, const std::string& address);

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

    std::string profile_dir_;
    std::map<ProfilerType, ProfilerState> profiler_states_;
    mutable std::mutex mutex_;
};

} // namespace profiler
