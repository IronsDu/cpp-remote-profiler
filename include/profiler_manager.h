/// @file profiler_manager.h
/// @brief Profiler Manager - Core profiling control and coordination

#pragma once

#include "profiler_version.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <vector>

PROFILER_NAMESPACE_BEGIN

// Forward declaration
class Symbolizer;

/// @enum ProfilerType
/// @brief Types of profiling operations supported
enum class ProfilerType {
    CPU,        ///< CPU profiling using gperftools
    HEAP,       ///< Heap memory profiling
    HEAP_GROWTH ///< Heap growth stack analysis
};

/// @struct ProfilerState
/// @brief Current state of a profiling session
struct ProfilerState {
    bool is_running;         ///< Whether profiling is currently active
    std::string output_path; ///< Path to the output profile file
    uint64_t start_time;     ///< Unix timestamp when profiling started
    uint64_t duration;       ///< Configured duration in seconds
};

/// @struct ThreadStackTrace
/// @brief Structure to hold captured stack trace for a thread
/// @note Uses fixed-size array for signal-safety
struct ThreadStackTrace {
    pid_t tid;           ///< Thread ID
    void* addresses[64]; ///< Array of instruction pointers (fixed size for signal-safety)
    int depth;           ///< Number of valid addresses in the array
    bool captured;       ///< Whether the trace was successfully captured
};

/// @struct SharedStackTrace
/// @brief Shared memory structure for inter-thread communication
/// @note Used for coordinating stack capture between threads
struct SharedStackTrace {
    std::atomic<bool> ready;                      ///< Set by thread after capturing
    char padding[64 - sizeof(std::atomic<bool>)]; ///< Padding to avoid false sharing
    pid_t tid;                                    ///< Thread ID
    int depth;                                    ///< Stack depth
    void* addresses[64];                          ///< Stack addresses
};

/// @class ProfilerManager
/// @brief Main manager class for profiling operations
///
/// ProfilerManager provides a singleton interface for controlling
/// CPU profiling, heap profiling, and thread stack capture.
/// It integrates with gperftools for profiling and provides
/// pprof-compatible output formats.
///
/// @note Thread-safe. All public methods can be called from any thread.
class ProfilerManager {
public:
    /// @brief Get the singleton instance of ProfilerManager
    /// @return Reference to the singleton instance
    static ProfilerManager& getInstance();

    /// @brief Start CPU profiling session
    /// @param output_path Path to save the profile output (default: "cpu.prof")
    /// @return true if profiling started successfully
    bool startCPUProfiler(const std::string& output_path = "cpu.prof");

    /// @brief Stop CPU profiling session
    /// @return true if profiling stopped successfully
    bool stopCPUProfiler();

    /// @brief Start Heap profiling session
    /// @param output_path Path to save the profile output (default: "heap.prof")
    /// @return true if profiling started successfully
    bool startHeapProfiler(const std::string& output_path = "heap.prof");

    /// @brief Stop Heap profiling session
    /// @return true if profiling stopped successfully
    bool stopHeapProfiler();

    /// @brief Get the current state of a profiler
    /// @param type The profiler type to query
    /// @return ProfilerState structure with current state
    ProfilerState getProfilerState(ProfilerType type) const;

    /// @brief Check if a profiler is currently running
    /// @param type The profiler type to query
    /// @return true if the specified profiler is running
    bool isProfilerRunning(ProfilerType type) const;

    /// @brief Resolve an address to its symbol name using backward-cpp
    /// @param address The instruction pointer to resolve
    /// @return Human-readable symbol string
    std::string resolveSymbolWithBackward(void* address);

    /// @brief Analyze CPU profile and return SVG flame graph
    /// @param duration Sampling duration in seconds
    /// @param output_type Output graph type: "flamegraph" (default), "iciclegraph", etc.
    /// @return SVG content as string
    std::string analyzeCPUProfile(int duration, const std::string& output_type = "flamegraph");

    /// @brief Analyze Heap profile and return SVG flame graph
    /// @param duration Sampling duration in seconds
    /// @param output_type Output graph type: "flamegraph" (default), "iciclegraph", etc.
    /// @return SVG content as string
    std::string analyzeHeapProfile(int duration, const std::string& output_type = "flamegraph");

    /// @brief Get raw CPU profile data (for /pprof/profile endpoint)
    /// @param seconds Sampling duration in seconds
    /// @return Raw profile binary data
    std::string getRawCPUProfile(int seconds);

    /// @brief Get raw heap sample data (for /pprof/heap endpoint)
    /// @return Heap sample in text format (compatible with pprof)
    std::string getRawHeapSample();

    /// @brief Get heap growth stacks data (for /pprof/growth endpoint)
    /// @return Heap growth stacks in text format (compatible with pprof)
    std::string getRawHeapGrowthStacks();

    /// @brief Get all thread stacks (for /api/thread/stacks endpoint)
    /// @return Thread stacks in text format
    std::string getThreadStacks();

    /// @brief Get thread callstack with full backtrace using signal handler
    /// @return Thread callstack information
    std::string getThreadCallStacks();

    /// @brief Set the signal to use for stack capture
    /// @param signal Signal number to use (e.g., SIGUSR1, SIGUSR2, SIGRTMIN+n)
    /// @note Must be called before first use of stack capture functionality
    /// @note Default is SIGUSR1
    static void setStackCaptureSignal(int signal);

    /// @brief Get the current signal being used for stack capture
    /// @return Current signal number
    static int getStackCaptureSignal();

    /// @brief Enable/disable signal chaining
    /// @param enable true to enable chaining, false to disable
    /// @note When enabled, the old signal handler will be called after ours
    static void setSignalChaining(bool enable);

    /// @brief Signal handler for capturing stack traces
    /// @param signum Signal number
    /// @param info siginfo_t pointer
    /// @param context User context pointer
    /// @note This is a signal-safe function used internally
    static void signalHandler(int signum, siginfo_t* info, void* context);

    /// @brief Execute a shell command and capture output
    /// @param cmd Command to execute
    /// @param output Reference to store command output
    /// @return true if command succeeded
    bool executeCommand(const std::string& cmd, std::string& output);

    /// @brief Get the path to the current executable
    /// @return Path to the executable
    std::string getExecutablePath();

private:
    ProfilerManager();
    ~ProfilerManager();
    ProfilerManager(const ProfilerManager&) = delete;
    ProfilerManager& operator=(const ProfilerManager&) = delete;

    std::string findLatestHeapProfile(const std::string& dir);

    /// @brief Generate flame graph from collapsed stack format
    /// @param collapsed_file Path to collapsed stack file
    /// @param title Title for the flame graph
    /// @return SVG content as string
    std::string generateFlameGraph(const std::string& collapsed_file, const std::string& title);

    /// @brief Capture stack traces from all threads using signals
    /// @return Vector of ThreadStackTrace structures
    std::vector<ThreadStackTrace> captureAllThreadStacks();

    /// @brief Symbolize an address using abseil
    /// @param addr Address to symbolize
    /// @return Human-readable symbol string
    std::string symbolizeAddress(void* addr);

    /// @brief Install signal handler (saves old handler)
    void installSignalHandler();

    /// @brief Restore old signal handler
    void restoreSignalHandler();

    std::string profile_dir_;                               ///< Directory for profile outputs
    std::map<ProfilerType, ProfilerState> profiler_states_; ///< Current profiler states
    mutable std::mutex mutex_;                              ///< Mutex for thread safety

    std::atomic<bool> cpu_profiling_in_progress_{false}; ///< CPU profiling concurrency control
    std::unique_ptr<Symbolizer> symbolizer_;             ///< Symbolizer instance

    static std::atomic<bool> capture_in_progress_; ///< Stack capture in progress flag
    static SharedStackTrace* shared_stacks_;       ///< Shared stack trace array
    static int stack_array_size_;                  ///< Size of stack array
    static std::atomic<pid_t> excluded_tid_;       ///< Thread ID to exclude from capture
    static std::atomic<int> completed_count_;      ///< Count of completed captures
    static int expected_count_;                    ///< Expected number of threads to capture
    static pid_t main_thread_id_;                  ///< PID of main thread
    static int stack_capture_signal_;              ///< Signal used for stack capture
    static struct sigaction old_action_;           ///< Saved old signal handler
    static bool old_action_saved_;                 ///< Whether old handler was saved
    static bool enable_signal_chaining_;           ///< Signal chaining enabled flag
};

PROFILER_NAMESPACE_END
