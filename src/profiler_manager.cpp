#include "profiler_manager.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "internal/embed_flamegraph.h"
#include "internal/embed_pprof.h"
#include "internal/log_manager.h"
#include "internal/log_macros.h"
#include "internal/symbolize.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cxxabi.h>
#include <dirent.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <fstream>
#include <gperftools/heap-profiler.h>
#include <gperftools/malloc_extension.h>
#include <gperftools/profiler.h>
#include <iostream>
#include <limits.h>
#include <signal.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>
#include <vector>

PROFILER_NAMESPACE_BEGIN

// Static member initialization
std::atomic<bool> ProfilerManager::capture_in_progress_{false};
SharedStackTrace* ProfilerManager::shared_stacks_ = nullptr;
int ProfilerManager::stack_array_size_ = 0;
std::atomic<pid_t> ProfilerManager::excluded_tid_{0};
std::atomic<int> ProfilerManager::completed_count_{0};
int ProfilerManager::expected_count_ = 0;
pid_t ProfilerManager::main_thread_id_ = 0;

// Signal configuration
int ProfilerManager::stack_capture_signal_ = SIGUSR1; // Default: SIGUSR1
struct sigaction ProfilerManager::old_action_;
bool ProfilerManager::old_action_saved_ = false;
bool ProfilerManager::enable_signal_chaining_ = false;

ProfilerManager::ProfilerManager()
    : log_manager_(std::make_unique<internal::LogManager>()) {
    // Write embedded pprof script to current directory
    writePprofScript("./pprof");

    // Write embedded flamegraph.pl script to current directory
    if (!writeFlamegraphScript("./flamegraph.pl")) {
        PROFILER_WARNING("Failed to write flamegraph.pl script");
    }

    // Create profile directory if not exists
    profile_dir_ = "/tmp/cpp_profiler";
    mkdir(profile_dir_.c_str(), 0755);

    // Initialize profiler states
    profiler_states_[ProfilerType::CPU] = ProfilerState{false, "", 0, 0};
    profiler_states_[ProfilerType::HEAP] = ProfilerState{false, "", 0, 0};
    profiler_states_[ProfilerType::HEAP_GROWTH] = ProfilerState{false, "", 0, 0};

    // Initialize symbolizer
    try {
        symbolizer_ = createSymbolizer();
    } catch (const std::exception& e) {
        PROFILER_ERROR("Failed to initialize symbolizer: {}", e.what());
        // Continue without symbolizer (will fall back to addr2line)
    }

    // Store main thread ID
    main_thread_id_ = gettid();

    // Initialize abseil symbolizer
    absl::InitializeSymbolizer("");

    // Note: Signal handler is installed lazily on first captureAllThreadStacks() call
}

ProfilerManager::~ProfilerManager() {
    if (profiler_states_[ProfilerType::CPU].is_running) {
        ProfilerStop();
    }
    if (profiler_states_[ProfilerType::HEAP].is_running) {
        IsHeapProfilerRunning();
    }

    // Restore old signal handler if we installed one
    if (signal_handler_installed_) {
        restoreSignalHandler();
    }
}

void ProfilerManager::setLogSink(std::shared_ptr<LogSink> sink) {
    log_manager_->setSink(std::move(sink));
}

void ProfilerManager::setLogLevel(LogLevel level) {
    log_manager_->setLogLevel(level);
}

void ProfilerManager::setStackCaptureSignal(int signal) {
    // Check if signal is valid
    if (signal < 1 || signal > SIGRTMAX) {
        std::cerr << "[ERROR] Invalid signal number: " << signal << std::endl;
        return;
    }

    // If handler is already installed, restore old one first
    if (old_action_saved_) {
        std::cerr << "[WARN] Signal handler already installed for signal " << stack_capture_signal_
                  << ". Restoring before changing." << std::endl;
        restoreSignalHandler();
    }

    stack_capture_signal_ = signal;
    std::cout << "[INFO] Stack capture signal set to: " << signal << std::endl;
}

int ProfilerManager::getStackCaptureSignal() {
    return stack_capture_signal_;
}

void ProfilerManager::setSignalChaining(bool enable) {
    enable_signal_chaining_ = enable;
    std::cout << "[INFO] Signal chaining " << (enable ? "enabled" : "disabled") << std::endl;
}

void ProfilerManager::installSignalHandler() {
    if (signal_handler_installed_) {
        return; // Already installed
    }

    struct sigaction sa;
    sa.sa_sigaction = &ProfilerManager::signalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);

    // Save old signal handler
    if (sigaction(stack_capture_signal_, &sa, &old_action_) == 0) {
        old_action_saved_ = true;
        signal_handler_installed_ = true;
        PROFILER_INFO("Registered signal handler for signal {}", stack_capture_signal_);
    } else {
        PROFILER_ERROR("Failed to register signal handler for signal {}: {}", stack_capture_signal_, strerror(errno));
    }
}

void ProfilerManager::restoreSignalHandler() {
    if (old_action_saved_) {
        if (sigaction(stack_capture_signal_, &old_action_, nullptr) == 0) {
            std::cout << "[INFO] Restored old signal handler for signal " << stack_capture_signal_ << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to restore old signal handler: " << strerror(errno) << std::endl;
        }
        old_action_saved_ = false;
    }
}

bool ProfilerManager::startCPUProfiler(const std::string& output_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (profiler_states_[ProfilerType::CPU].is_running) {
        return false; // Already running
    }

    std::string full_path = output_path.empty() ? profile_dir_ + "/cpu.prof" : output_path;

    // 如果是相对路径，转换为绝对路径
    if (full_path[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            full_path = std::string(cwd) + "/" + full_path;
        }
    }

    if (ProfilerStart(full_path.c_str())) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::CPU] = ProfilerState{true, full_path, static_cast<uint64_t>(timestamp), 0};

        // 不再使用 StackCollector，gperftools 会直接写入文件
        return true;
    }
    return false;
}

bool ProfilerManager::stopCPUProfiler() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!profiler_states_[ProfilerType::CPU].is_running) {
        return false; // Not running
    }

    ProfilerStop();

    // 不再使用 StackCollector

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    profiler_states_[ProfilerType::CPU].is_running = false;
    profiler_states_[ProfilerType::CPU].duration = timestamp - profiler_states_[ProfilerType::CPU].start_time;

    return true;
}

bool ProfilerManager::startHeapProfiler(const std::string& output_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (profiler_states_[ProfilerType::HEAP].is_running) {
        return false; // Already running
    }

    std::string full_path = output_path.empty() ? profile_dir_ + "/heap.prof" : output_path;

    // 如果是相对路径，转换为绝对路径
    if (full_path[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            full_path = std::string(cwd) + "/" + full_path;
        }
    }

    if (!IsHeapProfilerRunning()) {
        HeapProfilerStart(full_path.c_str());
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP] = ProfilerState{true, full_path, static_cast<uint64_t>(timestamp), 0};
        return true;
    }
    return false;
}

bool ProfilerManager::stopHeapProfiler() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!profiler_states_[ProfilerType::HEAP].is_running) {
        return false; // Not running
    }

    if (IsHeapProfilerRunning()) {
        // 保存 heap profile 到文件
        std::string heap_profile = GetHeapProfile();
        std::string output_path = profiler_states_[ProfilerType::HEAP].output_path;

        std::ofstream file(output_path);
        if (file.is_open()) {
            file << heap_profile;
            file.close();
        }

        HeapProfilerStop();
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP].is_running = false;
        profiler_states_[ProfilerType::HEAP].duration = timestamp - profiler_states_[ProfilerType::HEAP].start_time;

        return true;
    }
    return false;
}

ProfilerState ProfilerManager::getProfilerState(ProfilerType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profiler_states_.at(type);
}

bool ProfilerManager::isProfilerRunning(ProfilerType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profiler_states_.at(type).is_running;
}

// 获取当前可执行文件的绝对路径
std::string ProfilerManager::getExecutablePath() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        // 如果读取失败，回退到 /proc/self/exe
        return "/proc/self/exe";
    }
    exe_path[len] = '\0';
    return std::string(exe_path);
}

bool ProfilerManager::executeCommand(const std::string& cmd, std::string& output) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }

    try {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
    } catch (...) {
        pclose(pipe);
        return false;
    }
    pclose(pipe);
    return true;
}

// Generate flame graph from collapsed format using flamegraph.pl
std::string ProfilerManager::generateFlameGraph(const std::string& collapsed_file, const std::string& title) {
    std::string svg_output;

    // Build flamegraph.pl command
    // Use "perl ./flamegraph.pl" instead of "./flamegraph.pl" for better compatibility
    std::ostringstream cmd;
    cmd << "perl ./flamegraph.pl"
        << " --title=\"" << title << "\""
        << " --width=1200"
        << " " << collapsed_file << " 2>/dev/null";

    PROFILER_INFO("Generating FlameGraph: {}", cmd.str());

    if (!executeCommand(cmd.str(), svg_output)) {
        PROFILER_ERROR("Failed to execute flamegraph.pl");
        return R"({"error": "Failed to execute flamegraph.pl command"})";
    }

    // Validate output
    if (svg_output.find("<?xml") == std::string::npos && svg_output.find("<svg") == std::string::npos) {
        PROFILER_ERROR("flamegraph.pl did not generate valid SVG");
        return R"({"error": "flamegraph.pl did not generate valid SVG"})";
    }

    return svg_output;
}

std::string ProfilerManager::resolveSymbolWithBackward(void* address) {
    // 如果 symbolizer 不可用，返回地址
    if (!symbolizer_) {
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<unsigned long long>(address);
        return oss.str();
    }

    try {
        // 使用 backward-cpp 符号化地址
        std::vector<SymbolizedFrame> frames = symbolizer_->symbolize(address);

        if (!frames.empty() && frames[0].function_name.find("0x") != 0) {
            // 符号化成功，使用 -- 连接内联调用链
            std::ostringstream result;
            for (size_t i = 0; i < frames.size(); ++i) {
                if (i > 0) {
                    result << "--";
                }
                result << frames[i].function_name;
            }
            return result.str();
        }

        // backward-cpp失败，尝试使用 addr2line
        // 需要计算相对地址（对于PIE可执行文件）
        Dl_info info;
        uintptr_t addr = reinterpret_cast<uintptr_t>(address);
        uintptr_t relative_addr = addr;

        if (dladdr(address, &info) && info.dli_fbase) {
            // 计算相对地址
            relative_addr = addr - reinterpret_cast<uintptr_t>(info.dli_fbase);
        }

        std::ostringstream cmd;
        cmd << "addr2line -e /proc/self/exe -f -C 0x" << std::hex << relative_addr;

        std::string output;
        if (executeCommand(cmd.str(), output) && !output.empty()) {
            // addr2line 输出格式: "function_name\nsource_file:line\n"
            size_t newline_pos = output.find('\n');
            if (newline_pos != std::string::npos) {
                std::string symbol = output.substr(0, newline_pos);
                if (symbol != "??" && !symbol.empty()) {
                    return symbol;
                }
            }
        }

        // 都失败了，返回地址
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<unsigned long long>(address);
        return oss.str();

    } catch (const std::exception& e) {
        PROFILER_ERROR("Error in resolveSymbolWithBackward: {}", e.what());
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<unsigned long long>(address);
        return oss.str();
    }
}

std::string ProfilerManager::analyzeCPUProfile(int duration, const std::string& output_type) {
    std::string profile_path = profile_dir_ + "/cpu_analyze.prof";

    // Step 1: Stop any existing profiler
    if (profiler_states_[ProfilerType::CPU].is_running) {
        stopCPUProfiler();
        // Wait a bit for file to be written
        usleep(100000); // 100ms
    }

    // Step 2: Start CPU profiler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ProfilerStart(profile_path.c_str())) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            profiler_states_[ProfilerType::CPU] =
                ProfilerState{true, profile_path, static_cast<uint64_t>(timestamp), 0};
        } else {
            return R"({"error": "Failed to start CPU profiler"})";
        }
    }

    // Step 3: Wait for specified duration
    PROFILER_INFO("Profiling for {} seconds...", duration);
    sleep(duration);

    // Step 4: Stop profiler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ProfilerStop();

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::CPU].is_running = false;
        profiler_states_[ProfilerType::CPU].duration = timestamp - profiler_states_[ProfilerType::CPU].start_time;
    }

    // Wait for file to be flushed
    usleep(200000); // 200ms

    // Step 5: Generate SVG using pprof or flamegraph.pl
    std::string svg_output;
    std::string exe_path = getExecutablePath();

    // Check output_type
    if (output_type == "flamegraph") {
        // PATH 1: Generate FlameGraph using Brendan Gregg's tool
        PROFILER_INFO("Generating FlameGraph output...");

        // Step 5a: Generate collapsed format using pprof --collapsed
        std::string collapsed_file = "/tmp/cpu_collapsed.prof";
        std::ostringstream collapsed_cmd;
        collapsed_cmd << "./pprof --collapsed " << exe_path << " " << profile_path << " > " << collapsed_file
                      << " 2>&1";

        PROFILER_DEBUG("Running collapsed command: {}", collapsed_cmd.str());

        std::string collapsed_output;
        if (!executeCommand(collapsed_cmd.str(), collapsed_output)) {
            return R"({"error": "Failed to execute pprof --collapsed command"})";
        }

        // Check if collapsed file was created and has content
        std::ifstream collapsed_in(collapsed_file);
        if (!collapsed_in.is_open()) {
            return R"({"error": "Failed to create collapsed file"})";
        }

        // Verify file has actual data (not just "Using local file..." messages)
        std::string line;
        bool has_data = false;
        while (std::getline(collapsed_in, line)) {
            if (!line.empty() && line[0] != '#') {
                has_data = true;
                break;
            }
        }
        collapsed_in.close();

        if (!has_data) {
            PROFILER_WARNING("pprof --collapsed produced no data");
            return R"({"error": "pprof --collapsed produced no data"})";
        }

        PROFILER_INFO("Collapsed stacks written to {}", collapsed_file);

        // Step 5b: Generate FlameGraph from collapsed format
        svg_output = generateFlameGraph(collapsed_file, "CPU Flame Graph");

        // Check if it's an error
        if (svg_output.size() > 10 && svg_output[0] == '{' && svg_output[1] == '"') {
            // Error occurred, return as-is
            return svg_output;
        }

    } else {
        // PATH 2: Default - Generate pprof SVG (existing behavior)
        PROFILER_INFO("Generating pprof SVG output...");

        // Build pprof command (使用可执行文件的绝对路径进行符号化)
        std::ostringstream cmd;
        cmd << "./pprof --svg " << exe_path << " " << profile_path << " 2>/dev/null";

        if (!executeCommand(cmd.str(), svg_output)) {
            return R"({"error": "Failed to execute pprof command"})";
        }

        // Check if SVG was generated (should start with <?xml or <svg)
        // 更精确的错误检查：只检查是否包含SVG标记
        if (svg_output.find("<?xml") == std::string::npos && svg_output.find("<svg") == std::string::npos) {
            return R"({"error": "pprof did not generate valid SVG. Output: )" + svg_output + R"("})";
        }

        // 检查是否是pprof的错误消息（pprof的错误通常以"error:"开头）
        // 但要排除SVG中的JavaScript代码
        if (svg_output.size() < 100) {
            // 输出太短，可能是错误消息
            if (svg_output.find("error") != std::string::npos || svg_output.find("Error") != std::string::npos) {
                return R"({"error": "pprof error: )" + svg_output + R"("})";
            }
        }

        PROFILER_INFO("pprof SVG generated successfully!");

        // 后处理 SVG：添加 viewBox 以支持正确的缩放和显示
        // 查找 <svg> 标签并添加 viewBox 属性
        size_t svg_start = svg_output.find("<svg");
        if (svg_start != std::string::npos) {
            size_t svg_tag_end = svg_output.find(">", svg_start);
            if (svg_tag_end != std::string::npos) {
                // 检查是否已有 viewBox
                std::string svg_tag = svg_output.substr(svg_start, svg_tag_end - svg_start);
                if (svg_tag.find("viewBox") == std::string::npos) {
                    // pprof 生成的 SVG 通常使用负坐标，典型的范围大约是：
                    // X: 0-2000, Y: -1000-0
                    // 我们使用一个保守的 viewBox
                    std::string viewbox_attr = " viewBox=\"0 -1000 2000 1000\"";

                    // 在 <svg> 标签内插入 viewBox 属性
                    svg_output.insert(svg_tag_end, viewbox_attr);
                    PROFILER_DEBUG("Added viewBox to SVG for proper scaling");
                }
            }
        }
    }

    return svg_output;
}

std::string ProfilerManager::analyzeHeapProfile(int duration, const std::string& output_type) {
    std::string profile_prefix = profile_dir_ + "/heap_analyze";

    PROFILER_INFO("=== Starting Heap Profile Analysis ===");
    PROFILER_INFO("Profile prefix: {}", profile_prefix);
    PROFILER_INFO("Duration: {} seconds", duration);

    // Step 1: Stop any existing heap profiler
    if (profiler_states_[ProfilerType::HEAP].is_running) {
        PROFILER_INFO("Stopping existing heap profiler...");
        stopHeapProfiler();
        usleep(100000);
    }

    // Step 2: Set environment variable for heap profiling BEFORE starting profiler
    unsetenv("HEAPPROFILE");
    unsetenv("HEAP_PROFILE_ALLOCATION_INTERVAL");
    unsetenv("HEAP_PROFILE_INUSE_INTERVAL");

    setenv("HEAPPROFILE", profile_prefix.c_str(), 1);
    setenv("HEAP_PROFILE_ALLOCATION_INTERVAL", "1048576", 1); // 1MB
    setenv("HEAP_PROFILE_INUSE_INTERVAL", "524288", 1);       // 512KB

    PROFILER_DEBUG("Environment variables set");

    // Step 3: Start Heap profiler - NOTE: HeapProfilerStart might not work as expected
    // gperftools heap profiler works primarily through environment variables
    PROFILER_DEBUG("Calling HeapProfilerStart()...");
    HeapProfilerStart(profile_prefix.c_str());
    PROFILER_DEBUG("HeapProfilerStart() completed");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP] =
            ProfilerState{true, profile_prefix + ".prof", static_cast<uint64_t>(timestamp), 0};
    }

    // Step 4: 启动后台线程进行内存分配
    std::atomic<bool> keep_running(true);
    std::atomic<size_t> allocations_count(0);

    PROFILER_DEBUG("Starting memory allocation thread...");
    std::thread memory_thread([this, &keep_running, &allocations_count]() {
        PROFILER_DEBUG("Memory thread started");
        int iteration = 0;
        while (keep_running && iteration < 100) { // 限制最大迭代次数
            // 进行各种内存分配 - 直接分配不释放，确保 heap profiler 能采样
            std::vector<std::vector<int>>* matrixData = new std::vector<std::vector<int>>();
            for (int i = 0; i < 10; ++i) {
                std::vector<int>* largeArray = new std::vector<int>(10000);
                for (auto& val : *largeArray) {
                    val = rand();
                }
                matrixData->push_back(*largeArray);
            }

            std::vector<std::string>* stringData = new std::vector<std::string>();
            for (int j = 0; j < 100; ++j) {
                stringData->push_back("Heap profiling test data " + std::to_string(j));
            }

            allocations_count++;
            iteration++;

            if (iteration % 10 == 0) {
                PROFILER_TRACE("Memory thread: {} iterations, {} allocations", iteration, allocations_count.load());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        PROFILER_DEBUG("Memory thread ending after {} iterations", iteration);
    });

    // Step 5: Wait for specified duration
    PROFILER_INFO("Heap profiling for {} seconds...", duration);
    sleep(duration);
    PROFILER_DEBUG("Sleep completed, stopping profiler...");

    // Step 6: Stop profiler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        PROFILER_DEBUG("Setting keep_running = false...");
        keep_running = false;

        PROFILER_DEBUG("Calling HeapProfilerStop()...");
        HeapProfilerStop();
        PROFILER_DEBUG("HeapProfilerStop() completed");

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP].is_running = false;
        profiler_states_[ProfilerType::HEAP].duration = timestamp - profiler_states_[ProfilerType::HEAP].start_time;
    }

    // 等待内存分配线程结束
    PROFILER_DEBUG("Waiting for memory thread to join...");
    if (memory_thread.joinable()) {
        memory_thread.join();
    }
    PROFILER_INFO("Memory thread joined. Total allocations: {}", allocations_count.load());

    // Step 7: Check if heap files were generated
    PROFILER_INFO("Searching for heap profile files in {}", profile_dir_);
    std::string latest_heap_file = findLatestHeapProfile(profile_dir_);

    if (latest_heap_file.empty()) {
        PROFILER_WARNING("No .heap file found. Listing all files in directory:");
        std::string ls_cmd = "ls -la " + profile_dir_ + "/";
        [[maybe_unused]] int result = system(ls_cmd.c_str());

        PROFILER_WARNING("gperftools heap profiler requires special configuration.");
        PROFILER_WARNING("Heap profiling needs to be enabled at program startup via HEAPPROFILE environment variable.");
        PROFILER_WARNING("For now, returning a helpful error message.");

        return R"({"error": "Heap profiling requires the program to be started with HEAPPROFILE environment variable set. Please restart the program with: HEAPPROFILE=/tmp/cpp_profiler/heap ./profiler_example. Alternatively, use CPU profiling which works without special configuration."})";
    }

    PROFILER_INFO("Using heap profile: {}", latest_heap_file);

    // Step 9: Generate SVG using pprof or flamegraph.pl
    std::string svg_output;
    std::string exe_path = getExecutablePath();

    // Check output_type
    if (output_type == "flamegraph") {
        // PATH 1: Generate FlameGraph using Brendan Gregg's tool
        PROFILER_INFO("Generating Heap FlameGraph...");

        // Step 9a: Generate collapsed format using pprof --collapsed
        std::string collapsed_file = "/tmp/heap_collapsed.prof";
        std::ostringstream collapsed_cmd;
        collapsed_cmd << "./pprof --collapsed " << exe_path << " " << latest_heap_file << " > " << collapsed_file
                      << " 2>&1";

        PROFILER_DEBUG("Running collapsed command: {}", collapsed_cmd.str());

        std::string collapsed_output;
        if (!executeCommand(collapsed_cmd.str(), collapsed_output)) {
            return R"({"error": "Failed to execute pprof --collapsed command"})";
        }

        // Check if collapsed file was created and has content
        std::ifstream collapsed_in(collapsed_file);
        if (!collapsed_in.is_open()) {
            return R"({"error": "Failed to create collapsed file"})";
        }

        // Verify file has actual data (not just "Using local file..." messages)
        std::string line;
        bool has_data = false;
        while (std::getline(collapsed_in, line)) {
            if (!line.empty() && line[0] != '#') {
                has_data = true;
                break;
            }
        }
        collapsed_in.close();

        if (!has_data) {
            PROFILER_WARNING("pprof --collapsed produced no data");
            return R"({"error": "pprof --collapsed produced no data"})";
        }

        PROFILER_INFO("Collapsed stacks written to {}", collapsed_file);

        // Step 9b: Generate FlameGraph from collapsed format
        svg_output = generateFlameGraph(collapsed_file, "Heap Flame Graph");

        // Check if it's an error
        if (svg_output.size() > 10 && svg_output[0] == '{' && svg_output[1] == '"') {
            // Error occurred, return as-is
            return svg_output;
        }

        PROFILER_INFO("Heap FlameGraph generated successfully! Size: {}", svg_output.length());

    } else {
        // PATH 2: Default - Generate pprof SVG (existing behavior)
        PROFILER_INFO("Generating Heap pprof SVG...");

        // Build pprof command (使用可执行文件的绝对路径进行符号化)
        std::ostringstream cmd;
        cmd << "./pprof --svg " << exe_path << " " << latest_heap_file << " 2>/dev/null";

        PROFILER_DEBUG("Command: {}", cmd.str());

        if (!executeCommand(cmd.str(), svg_output)) {
            PROFILER_ERROR("pprof command failed");
            return R"({"error": "Failed to execute pprof command"})";
        }

        // Check if SVG was generated
        if (svg_output.find("<?xml") == std::string::npos && svg_output.find("<svg") == std::string::npos) {
            PROFILER_ERROR("pprof did not return SVG. Output: {}", svg_output.substr(0, 200));
            return R"({"error": "pprof did not generate valid SVG. Output: )" + svg_output + R"("})";
        }

        PROFILER_INFO("Heap pprof SVG generated successfully! Size: {}", svg_output.length());

        // 后处理 SVG：添加 viewBox 以支持正确的缩放和显示
        size_t svg_start = svg_output.find("<svg");
        if (svg_start != std::string::npos) {
            size_t svg_tag_end = svg_output.find(">", svg_start);
            if (svg_tag_end != std::string::npos) {
                std::string svg_tag = svg_output.substr(svg_start, svg_tag_end - svg_start);
                if (svg_tag.find("viewBox") == std::string::npos) {
                    std::string viewbox_attr = " viewBox=\"0 -1000 2000 1000\"";
                    svg_output.insert(svg_tag_end, viewbox_attr);
                    PROFILER_DEBUG("Added viewBox to heap SVG for proper scaling");
                }
            }
        }
    }

    return svg_output;
}

std::string ProfilerManager::findLatestHeapProfile(const std::string& dir) {
    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        PROFILER_ERROR("Failed to open directory: {}", dir);
        return "";
    }

    std::string latest_file;
    time_t latest_time = 0;

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        std::string filename = entry->d_name;

        // Look for files ending with .heap (heap profiler output format)
        // Files are named like: heap_prof.0001.heap, heap_prof.0002.heap, etc.
        if (filename.length() > 5 && filename.substr(filename.length() - 5) == ".heap") {
            std::string full_path = dir + "/" + filename;

            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == 0) {
                if (file_stat.st_mtime > latest_time) {
                    latest_time = file_stat.st_mtime;
                    latest_file = full_path;
                }
            }
        }
    }

    closedir(dp);
    return latest_file;
}

std::string ProfilerManager::getRawCPUProfile(int seconds) {
    // Validate seconds parameter
    if (seconds < 1 || seconds > 300) {
        PROFILER_ERROR("Invalid seconds parameter: {}. Must be between 1 and 300.", seconds);
        return "";
    }

    // Check concurrency control
    bool expected = false;
    if (!cpu_profiling_in_progress_.compare_exchange_strong(expected, true)) {
        PROFILER_ERROR("CPU profiling already in progress. Only one request at a time.");
        return "";
    }

    // Use RAII to ensure flag is reset even if exception occurs
    struct FlagGuard {
        std::atomic<bool>& flag;
        ~FlagGuard() {
            flag.store(false);
        }
    };
    FlagGuard guard{cpu_profiling_in_progress_};

    // Generate temporary profile file path
    std::string profile_path = profile_dir_ + "/pprof_cpu_temp.prof";

    // Stop any existing CPU profiler first
    if (profiler_states_[ProfilerType::CPU].is_running) {
        PROFILER_INFO("Stopping existing CPU profiler...");
        ProfilerStop();
        profiler_states_[ProfilerType::CPU].is_running = false;
        usleep(100000); // 100ms to ensure file is written
    }

    // Start CPU profiler
    PROFILER_INFO("Starting CPU profiler for {} seconds...", seconds);
    if (!ProfilerStart(profile_path.c_str())) {
        PROFILER_ERROR("Failed to start CPU profiler");
        return "";
    }

    // Update state
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    profiler_states_[ProfilerType::CPU] = ProfilerState{true, profile_path, static_cast<uint64_t>(timestamp), 0};

    // Wait for specified duration
    sleep(seconds);

    // Stop profiler
    PROFILER_INFO("Stopping CPU profiler...");
    ProfilerStop();

    now = std::chrono::system_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    profiler_states_[ProfilerType::CPU].is_running = false;
    profiler_states_[ProfilerType::CPU].duration = timestamp - profiler_states_[ProfilerType::CPU].start_time;

    // Wait for file to be flushed
    usleep(200000); // 200ms

    // Read profile file
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        PROFILER_ERROR("Failed to open profile file: {}", profile_path);
        return "";
    }

    // Read file content
    std::string profile_data;
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size > 0) {
        profile_data.resize(file_size);
        file.read(&profile_data[0], file_size);
    }

    file.close();

    PROFILER_INFO("Profile data size: {} bytes", profile_data.size());
    return profile_data;
}

std::string ProfilerManager::getRawHeapSample() {
    // Get heap sample from tcmalloc
    // MallocExtensionWriter is typedef'd as std::string
    std::string heap_sample;

    // GetHeapSample writes the heap profile into the string
    MallocExtension::instance()->GetHeapSample(&heap_sample);

    if (heap_sample.empty()) {
        PROFILER_ERROR("Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER environment variable is set.");
        return "";
    }

    PROFILER_INFO("Heap sample size: {} bytes", heap_sample.size());
    return heap_sample;
}

std::string ProfilerManager::getRawHeapGrowthStacks() {
    // Get heap growth stacks from tcmalloc
    // MallocExtensionWriter is typedef'd as std::string
    std::string heap_growth_stacks;

    // GetHeapGrowthStacks writes the heap growth stacks into the string
    MallocExtension::instance()->GetHeapGrowthStacks(&heap_growth_stacks);

    if (heap_growth_stacks.empty()) {
        PROFILER_ERROR("Failed to get heap growth stacks. No heap growth data available.");
        return "";
    }

    PROFILER_INFO("Heap growth stacks size: {} bytes", heap_growth_stacks.size());
    return heap_growth_stacks;
}

std::string ProfilerManager::getThreadStacks() {
    std::ostringstream result;

    // Open /proc/self/task directory to list all threads
    DIR* task_dir = opendir("/proc/self/task");
    if (!task_dir) {
        PROFILER_ERROR("Failed to open /proc/self/task");
        return "";
    }

    result << "Thread Stacks Snapshot\n";
    result << "======================\n\n";

    // Iterate through all thread directories
    struct dirent* entry;
    int thread_count = 0;

    while ((entry = readdir(task_dir)) != nullptr) {
        // Skip "." and ".." entries
        if (entry->d_name[0] == '.') {
            continue;
        }

        // Get thread ID
        pid_t tid = atoi(entry->d_name);
        if (tid == 0) {
            continue;
        }

        thread_count++;

        result << "Thread " << tid << ":\n";

        // Read thread stat to get state and name
        std::string stat_file = std::string("/proc/self/task/") + entry->d_name + "/stat";
        std::ifstream stat_stream(stat_file);

        if (stat_stream.is_open()) {
            std::string line;
            if (std::getline(stat_stream, line)) {
                // Parse stat file (format: pid (comm) state ...)
                size_t open_paren = line.find('(');
                size_t close_paren = line.find(')', open_paren);

                if (open_paren != std::string::npos && close_paren != std::string::npos) {
                    std::string name = line.substr(open_paren + 1, close_paren - open_paren - 1);

                    // Get state character (after close_paren + 2)
                    if (close_paren + 2 < line.length()) {
                        char state = line[close_paren + 2];

                        // Convert state to readable format
                        const char* state_str = "Unknown";
                        switch (state) {
                        case 'R':
                            state_str = "Running";
                            break;
                        case 'S':
                            state_str = "Sleeping";
                            break;
                        case 'D':
                            state_str = "Disk sleep";
                            break;
                        case 'Z':
                            state_str = "Zombie";
                            break;
                        case 'T':
                            state_str = "Stopped";
                            break;
                        case 't':
                            state_str = "Tracing stop";
                            break;
                        case 'X':
                            state_str = "Dead";
                            break;
                        case 'x':
                            state_str = "Dead";
                            break;
                        case 'K':
                            state_str = "Wakekill";
                            break;
                        case 'W':
                            state_str = "Waking";
                            break;
                        case 'P':
                            state_str = "Parked";
                            break;
                        }

                        result << "  Name: " << name << "\n";
                        result << "  State: " << state_str << " (" << state << ")\n";
                    }
                }
            }
            stat_stream.close();
        }

        // Try to read wchan (what thread is waiting on)
        std::string wchan_file = std::string("/proc/self/task/") + entry->d_name + "/wchan";
        std::ifstream wchan_stream(wchan_file);

        if (wchan_stream.is_open()) {
            std::string wchan;
            if (std::getline(wchan_stream, wchan) && !wchan.empty()) {
                result << "  Waiting in: " << wchan << "\n";
            }
            wchan_stream.close();
        }

        result << "\n";
    }

    closedir(task_dir);

    result << "Total threads: " << thread_count << "\n";

    std::string output = result.str();
    PROFILER_INFO("Thread stacks collected, size: {} bytes", output.size());

    return output;
}

// Signal handler for capturing stack traces (signal-safe)
void ProfilerManager::signalHandler(int signum, siginfo_t* info, void* context) {
    // Check if this is our configured signal
    if (signum != stack_capture_signal_) {
        // If signal chaining is enabled, call the old handler
        if (enable_signal_chaining_ && old_action_saved_ && old_action_.sa_sigaction) {
            old_action_.sa_sigaction(signum, info, context);
        }
        return;
    }

    // Check if we should capture
    if (!capture_in_progress_.load(std::memory_order_relaxed)) {
        // If signal chaining is enabled, call the old handler
        if (enable_signal_chaining_ && old_action_saved_ && old_action_.sa_sigaction) {
            old_action_.sa_sigaction(signum, info, context);
        }
        return;
    }

    // Check shared memory
    if (!shared_stacks_) {
        return;
    }

    // Get current thread ID
    pid_t tid = gettid();

    // Boundary check: skip if thread ID exceeds array size
    if (tid >= stack_array_size_) {
        return;
    }

    // Skip excluded thread (the one handling the HTTP request)
    pid_t excluded = excluded_tid_.load(std::memory_order_relaxed);
    if (excluded != 0 && tid == excluded) {
        return;
    }

    // Use tid directly as array index
    int depth = backtrace(shared_stacks_[tid].addresses, 64);

    // Store metadata
    shared_stacks_[tid].tid = tid;
    shared_stacks_[tid].depth = depth;

    // Mark as ready
    shared_stacks_[tid].ready.store(true, std::memory_order_release);

    // Increment completed counter
    completed_count_.fetch_add(1, std::memory_order_release);

    // Note: We don't call old handler here to avoid interfering with stack capture
    // If signal chaining is needed, the user should use a different signal
    (void)info;
    (void)context;
}

std::string ProfilerManager::symbolizeAddress(void* addr) {
    // Try to symbolize using abseil
    char symbol_buf[1024];
    if (absl::Symbolize(addr, symbol_buf, sizeof(symbol_buf))) {
        return std::string(symbol_buf);
    }

    // Fallback to backward-cpp
    return resolveSymbolWithBackward(addr);
}

std::vector<ThreadStackTrace> ProfilerManager::captureAllThreadStacks() {
    std::vector<ThreadStackTrace> result;

    // Lazily install signal handler on first use
    installSignalHandler();

    // 1. Read all thread IDs and find maximum
    std::vector<pid_t> tids;
    pid_t max_tid = 0;
    DIR* task_dir = opendir("/proc/self/task");
    if (!task_dir) {
        PROFILER_ERROR("Failed to open /proc/self/task");
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(task_dir)) != nullptr) {
        if (entry->d_name[0] == '.')
            continue;
        pid_t tid = atoi(entry->d_name);
        if (tid > 0) {
            tids.push_back(tid);
            if (tid > max_tid) {
                max_tid = tid;
            }
        }
    }
    closedir(task_dir);

    PROFILER_INFO("Found {} threads, max_tid = {}", tids.size(), max_tid);

    // 2. Allocate array based on max_tid (tid as direct index)
    int array_size = max_tid + 1;
    size_t shared_size = sizeof(SharedStackTrace) * array_size;

    SharedStackTrace* temp_stacks =
        (SharedStackTrace*)mmap(nullptr, shared_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (temp_stacks == MAP_FAILED) {
        PROFILER_ERROR("Failed to allocate shared memory for stack traces");
        return result;
    }

    // Initialize array
    for (int i = 0; i < array_size; ++i) {
        temp_stacks[i].ready = false;
        temp_stacks[i].depth = 0;
        temp_stacks[i].tid = 0;
        for (int j = 0; j < 64; ++j) {
            temp_stacks[i].addresses[j] = nullptr;
        }
    }

    // Save to static variables (signal handler needs access)
    shared_stacks_ = temp_stacks;
    stack_array_size_ = array_size;

    __sync_synchronize();

    // 3. Set capture flag and excluded thread
    completed_count_.store(0, std::memory_order_release);
    capture_in_progress_.store(true, std::memory_order_release);

    pid_t current_tid = gettid();
    pid_t current_pid = getpid();

    // Exclude current thread (the one handling HTTP request)
    excluded_tid_.store(current_tid, std::memory_order_release);

    // 4. Send signal to all threads EXCEPT current thread
    int signals_sent = 0;
    int signals_failed = 0;

    for (pid_t tid : tids) {
        // Skip the HTTP request handling thread
        if (tid == current_tid) {
            continue;
        }

        int ret = syscall(SYS_tgkill, current_pid, tid, stack_capture_signal_);
        if (ret == 0) {
            signals_sent++;
        } else if (ret == ESRCH) {
            signals_failed++;
        } else {
            // Try pthread_kill as fallback
            ret = pthread_kill(tid, stack_capture_signal_);
            if (ret == 0) {
                signals_sent++;
            } else {
                signals_failed++;
            }
        }
    }

    PROFILER_INFO("Sent signal to {} threads ({} failed)", signals_sent, signals_failed);

    // Set expected count based on ACTUAL signals sent (not original thread count)
    // This handles the case where threads exit between enumerating and sending signals
    expected_count_ = signals_sent;

    // If no threads were signaled, nothing to do
    if (expected_count_ == 0) {
        PROFILER_INFO("No threads to capture (all may have exited)");
        capture_in_progress_.store(false, std::memory_order_release);
        excluded_tid_.store(0, std::memory_order_release);
        munmap(temp_stacks, shared_size);
        shared_stacks_ = nullptr;
        stack_array_size_ = 0;
        return result;
    }

    // 5. Wait for all signal handlers to complete
    // Use a counter-based approach instead of fixed sleep
    const int MAX_WAIT_MS = 2000;     // Maximum wait time
    const int CHECK_INTERVAL_MS = 10; // Check interval
    int elapsed = 0;

    while (elapsed < MAX_WAIT_MS) {
        int completed = completed_count_.load(std::memory_order_acquire);

        if (completed >= expected_count_) {
            PROFILER_INFO("All threads completed stack capture in {}ms", elapsed);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
        elapsed += CHECK_INTERVAL_MS;
    }

    if (elapsed >= MAX_WAIT_MS) {
        int completed = completed_count_.load(std::memory_order_acquire);
        PROFILER_WARNING("Timeout waiting for threads to complete. Expected {}, got {}", expected_count_, completed);
    }

    // Now safe to clear flags
    capture_in_progress_.store(false, std::memory_order_release);
    excluded_tid_.store(0, std::memory_order_release);

    __sync_synchronize();

    // 6. Collect results from shared memory (filter valid elements)
    int collected = 0;
    for (int i = 0; i < array_size; ++i) {
        __sync_synchronize();

        // Filter: only collect valid entries (ready=true and depth>0)
        if (temp_stacks[i].ready && temp_stacks[i].depth > 0) {
            ThreadStackTrace trace;
            trace.tid = temp_stacks[i].tid;
            trace.depth = temp_stacks[i].depth;
            trace.captured = true;

            for (int j = 0; j < trace.depth && j < 64; ++j) {
                trace.addresses[j] = temp_stacks[i].addresses[j];
            }

            result.push_back(trace);
            collected++;
        }
    }

    PROFILER_INFO("Collected {} thread stacks from {} slots ({} threads total)", collected, array_size, tids.size());

    // 7. Clean up
    munmap(temp_stacks, shared_size);
    shared_stacks_ = nullptr;
    stack_array_size_ = 0;

    return result;
}

std::string ProfilerManager::getThreadCallStacks() {
    std::ostringstream result;

    result << "Thread Call Stacks (via Signal Handler)\n";
    result << "=========================================\n\n";

    // Capture all thread stacks via signal handler
    auto stacks = captureAllThreadStacks();

    result << "Total threads captured: " << stacks.size() << "\n\n";

    // Process each thread's stack
    for (const auto& trace : stacks) {
        result << "Thread " << trace.tid << ":\n";
        result << "  Frames: " << trace.depth << "\n";

        // Symbolize and print each frame using abseil
        for (int i = 0; i < trace.depth; ++i) {
            void* addr = trace.addresses[i];
            std::string symbolized = symbolizeAddress(addr);

            result << "    #" << i << " ";

            if (symbolized.empty() || symbolized[0] == '0') {
                result << "0x" << std::hex << reinterpret_cast<uintptr_t>(addr) << std::dec << "\n";
            } else {
                result << symbolized << "\n";
            }
        }

        result << "\n";
    }

    std::string output = result.str();
    PROFILER_INFO("Thread callstacks collected, size: {} bytes", output.size());

    return output;
}

PROFILER_NAMESPACE_END
