#include "profiler_manager.h"
#include "embed_pprof.h"
#include "embed_flamegraph.h"
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <gperftools/malloc_extension.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <vector>
#include <dlfcn.h>
#include <cstdint>
#include <execinfo.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <ucontext.h>
#include <unwind.h>
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"

namespace profiler {

// Static member initialization
std::atomic<bool> ProfilerManager::capture_in_progress_{false};
SharedStackTrace* ProfilerManager::shared_stacks_ = nullptr;
int ProfilerManager::max_threads_ = 0;
pid_t ProfilerManager::main_thread_id_ = 0;

ProfilerManager::ProfilerManager() {
    // Write embedded pprof script to current directory
    writePprofScript("./pprof");

    // Write embedded flamegraph.pl script to current directory
    if (!writeFlamegraphScript("./flamegraph.pl")) {
        std::cerr << "Warning: Failed to write flamegraph.pl script" << std::endl;
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
        std::cerr << "Failed to initialize symbolizer: " << e.what() << std::endl;
        // Continue without symbolizer (will fall back to addr2line)
    }

    // Store main thread ID
    main_thread_id_ = gettid();

    // Initialize abseil symbolizer
    absl::InitializeSymbolizer("");

    // Initialize shared memory for stack traces (allocate for max 256 threads)
    max_threads_ = 256;
    size_t shared_size = sizeof(SharedStackTrace) * max_threads_;

    // Use anonymous mmap for shared memory
    void* mem = mmap(nullptr, shared_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        std::cerr << "Failed to allocate shared memory for stack traces: "
                  << strerror(errno) << std::endl;
        shared_stacks_ = nullptr;
        max_threads_ = 0;
    } else {
        shared_stacks_ = static_cast<SharedStackTrace*>(mem);
        // Initialize all slots
        for (int i = 0; i < max_threads_; ++i) {
            shared_stacks_[i].ready = false;
            shared_stacks_[i].depth = 0;
        }
    }

    // Register signal handler for stack tracing
    struct sigaction sa;
    sa.sa_sigaction = &ProfilerManager::signalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(STACK_CAPTURE_SIGNAL, &sa, nullptr) != 0) {
        std::cerr << "Failed to register signal handler: " << strerror(errno) << std::endl;
    }
}

ProfilerManager::~ProfilerManager() {
    if (profiler_states_[ProfilerType::CPU].is_running) {
        ProfilerStop();
    }
    if (profiler_states_[ProfilerType::HEAP].is_running) {
        IsHeapProfilerRunning();
    }

    // Clean up shared memory
    if (shared_stacks_ != nullptr) {
        size_t shared_size = sizeof(SharedStackTrace) * max_threads_;
        munmap(shared_stacks_, shared_size);
        shared_stacks_ = nullptr;
    }
}

ProfilerManager& ProfilerManager::getInstance() {
    static ProfilerManager instance;
    return instance;
}

bool ProfilerManager::startCPUProfiler(const std::string& output_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (profiler_states_[ProfilerType::CPU].is_running) {
        return false; // Already running
    }

    std::string full_path = output_path.empty() ?
        profile_dir_ + "/cpu.prof" : output_path;

    // 如果是相对路径，转换为绝对路径
    if (full_path[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            full_path = std::string(cwd) + "/" + full_path;
        }
    }

    if (ProfilerStart(full_path.c_str())) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::CPU] = ProfilerState{
            true, full_path, timestamp, 0
        };

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
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    profiler_states_[ProfilerType::CPU].is_running = false;
    profiler_states_[ProfilerType::CPU].duration =
        timestamp - profiler_states_[ProfilerType::CPU].start_time;

    return true;
}

bool ProfilerManager::startHeapProfiler(const std::string& output_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (profiler_states_[ProfilerType::HEAP].is_running) {
        return false; // Already running
    }

    std::string full_path = output_path.empty() ?
        profile_dir_ + "/heap.prof" : output_path;

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
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP] = ProfilerState{
            true, full_path, timestamp, 0
        };
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
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP].is_running = false;
        profiler_states_[ProfilerType::HEAP].duration =
            timestamp - profiler_states_[ProfilerType::HEAP].start_time;

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
std::string ProfilerManager::generateFlameGraph(
    const std::string& collapsed_file,
    const std::string& title) {

    std::string svg_output;

    // Build flamegraph.pl command
    // Use "perl ./flamegraph.pl" instead of "./flamegraph.pl" for better compatibility
    std::ostringstream cmd;
    cmd << "perl ./flamegraph.pl"
        << " --title=\"" << title << "\""
        << " --width=1200"
        << " " << collapsed_file
        << " 2>/dev/null";

    std::cout << "Generating FlameGraph: " << cmd.str() << std::endl;

    if (!executeCommand(cmd.str(), svg_output)) {
        std::cerr << "Failed to execute flamegraph.pl" << std::endl;
        return R"({"error": "Failed to execute flamegraph.pl command"})";
    }

    // Validate output
    if (svg_output.find("<?xml") == std::string::npos &&
        svg_output.find("<svg") == std::string::npos) {
        std::cerr << "flamegraph.pl did not generate valid SVG" << std::endl;
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
        cmd << "addr2line -e /proc/self/exe -f -C 0x"
            << std::hex << relative_addr;

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
        std::cerr << "Error in resolveSymbolWithBackward: " << e.what() << std::endl;
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
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();

            profiler_states_[ProfilerType::CPU] = ProfilerState{
                true, profile_path, timestamp, 0
            };
        } else {
            return R"({"error": "Failed to start CPU profiler"})";
        }
    }

    // Step 3: Wait for specified duration
    std::cout << "Profiling for " << duration << " seconds..." << std::endl;
    sleep(duration);

    // Step 4: Stop profiler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ProfilerStop();

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::CPU].is_running = false;
        profiler_states_[ProfilerType::CPU].duration =
            timestamp - profiler_states_[ProfilerType::CPU].start_time;
    }

    // Wait for file to be flushed
    usleep(200000); // 200ms

    // Step 5: Generate SVG using pprof or flamegraph.pl
    std::string svg_output;
    std::string exe_path = getExecutablePath();

    // Check output_type
    if (output_type == "flamegraph") {
        // PATH 1: Generate FlameGraph using Brendan Gregg's tool
        std::cout << "Generating FlameGraph output..." << std::endl;

        // Step 5a: Generate collapsed format using pprof --collapsed
        std::string collapsed_file = "/tmp/cpu_collapsed.prof";
        std::ostringstream collapsed_cmd;
        collapsed_cmd << "./pprof --collapsed " << exe_path << " " << profile_path
                      << " > " << collapsed_file << " 2>&1";

        std::cout << "Running collapsed command: " << collapsed_cmd.str() << std::endl;

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
            std::cout << "pprof --collapsed produced no data" << std::endl;
            return R"({"error": "pprof --collapsed produced no data"})";
        }

        std::cout << "Collapsed stacks written to " << collapsed_file << std::endl;

        // Step 5b: Generate FlameGraph from collapsed format
        svg_output = generateFlameGraph(collapsed_file, "CPU Flame Graph");

        // Check if it's an error
        if (svg_output.size() > 10 && svg_output[0] == '{' && svg_output[1] == '"') {
            // Error occurred, return as-is
            return svg_output;
        }

    } else {
        // PATH 2: Default - Generate pprof SVG (existing behavior)
        std::cout << "Generating pprof SVG output..." << std::endl;

        // Build pprof command (使用可执行文件的绝对路径进行符号化)
        std::ostringstream cmd;
        cmd << "./pprof --svg " << exe_path << " " << profile_path << " 2>/dev/null";

        if (!executeCommand(cmd.str(), svg_output)) {
            return R"({"error": "Failed to execute pprof command"})";
        }

        // Check if SVG was generated (should start with <?xml or <svg)
        // 更精确的错误检查：只检查是否包含SVG标记
        if (svg_output.find("<?xml") == std::string::npos &&
            svg_output.find("<svg") == std::string::npos) {
            return R"({"error": "pprof did not generate valid SVG. Output: )" + svg_output + R"("})";
        }

        // 检查是否是pprof的错误消息（pprof的错误通常以"error:"开头）
        // 但要排除SVG中的JavaScript代码
        if (svg_output.size() < 100) {
            // 输出太短，可能是错误消息
            if (svg_output.find("error") != std::string::npos ||
                svg_output.find("Error") != std::string::npos) {
                return R"({"error": "pprof error: )" + svg_output + R"("})";
            }
        }

        std::cout << "pprof SVG generated successfully!" << std::endl;

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
                    std::cout << "Added viewBox to SVG for proper scaling" << std::endl;
                }
            }
        }
    }

    return svg_output;
}

std::string ProfilerManager::analyzeHeapProfile(int duration, const std::string& output_type) {
    std::string profile_prefix = profile_dir_ + "/heap_analyze";

    std::cout << "=== Starting Heap Profile Analysis ===" << std::endl;
    std::cout << "Profile prefix: " << profile_prefix << std::endl;
    std::cout << "Duration: " << duration << " seconds" << std::endl;

    // Step 1: Stop any existing heap profiler
    if (profiler_states_[ProfilerType::HEAP].is_running) {
        std::cout << "Stopping existing heap profiler..." << std::endl;
        stopHeapProfiler();
        usleep(100000);
    }

    // Step 2: Set environment variable for heap profiling BEFORE starting profiler
    unsetenv("HEAPPROFILE");
    unsetenv("HEAP_PROFILE_ALLOCATION_INTERVAL");
    unsetenv("HEAP_PROFILE_INUSE_INTERVAL");

    setenv("HEAPPROFILE", profile_prefix.c_str(), 1);
    setenv("HEAP_PROFILE_ALLOCATION_INTERVAL", "1048576", 1); // 1MB
    setenv("HEAP_PROFILE_INUSE_INTERVAL", "524288", 1); // 512KB

    std::cout << "Environment variables set" << std::endl;

    // Step 3: Start Heap profiler - NOTE: HeapProfilerStart might not work as expected
    // gperftools heap profiler works primarily through environment variables
    std::cout << "Calling HeapProfilerStart()..." << std::endl;
    HeapProfilerStart(profile_prefix.c_str());
    std::cout << "HeapProfilerStart() completed" << std::endl;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP] = ProfilerState{
            true, profile_prefix + ".prof", timestamp, 0
        };
    }

    // Step 4: 启动后台线程进行内存分配
    std::atomic<bool> keep_running(true);
    std::atomic<size_t> allocations_count(0);

    std::cout << "Starting memory allocation thread..." << std::endl;
    std::thread memory_thread([&keep_running, &allocations_count]() {
        std::cout << "Memory thread started" << std::endl;
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
                std::cout << "Memory thread: " << iteration << " iterations, "
                         << allocations_count << " allocations" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Memory thread ending after " << iteration << " iterations" << std::endl;
    });

    // Step 5: Wait for specified duration
    std::cout << "Heap profiling for " << duration << " seconds..." << std::endl;
    sleep(duration);
    std::cout << "Sleep completed, stopping profiler..." << std::endl;

    // Step 6: Stop profiler
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "Setting keep_running = false..." << std::endl;
        keep_running = false;

        std::cout << "Calling HeapProfilerStop()..." << std::endl;
        HeapProfilerStop();
        std::cout << "HeapProfilerStop() completed" << std::endl;

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        profiler_states_[ProfilerType::HEAP].is_running = false;
        profiler_states_[ProfilerType::HEAP].duration =
            timestamp - profiler_states_[ProfilerType::HEAP].start_time;
    }

    // 等待内存分配线程结束
    std::cout << "Waiting for memory thread to join..." << std::endl;
    if (memory_thread.joinable()) {
        memory_thread.join();
    }
    std::cout << "Memory thread joined. Total allocations: " << allocations_count << std::endl;

    // Step 7: Check if heap files were generated
    std::cout << "Searching for heap profile files in " << profile_dir_ << std::endl;
    std::string latest_heap_file = findLatestHeapProfile(profile_dir_);

    if (latest_heap_file.empty()) {
        std::cout << "No .heap file found. Listing all files in directory:" << std::endl;
        std::string ls_cmd = "ls -la " + profile_dir_ + "/";
        system(ls_cmd.c_str());

        std::cout << "\nWARNING: gperftools heap profiler requires special configuration." << std::endl;
        std::cout << "Heap profiling needs to be enabled at program startup via HEAPPROFILE environment variable." << std::endl;
        std::cout << "\nFor now, returning a helpful error message." << std::endl;

        return R"({"error": "Heap profiling requires the program to be started with HEAPPROFILE environment variable set. Please restart the program with: HEAPPROFILE=/tmp/cpp_profiler/heap ./profiler_example. Alternatively, use CPU profiling which works without special configuration."})";
    }

    std::cout << "Using heap profile: " << latest_heap_file << std::endl;

    // Step 9: Generate SVG using pprof or flamegraph.pl
    std::string svg_output;
    std::string exe_path = getExecutablePath();

    // Check output_type
    if (output_type == "flamegraph") {
        // PATH 1: Generate FlameGraph using Brendan Gregg's tool
        std::cout << "Generating Heap FlameGraph..." << std::endl;

        // Step 9a: Generate collapsed format using pprof --collapsed
        std::string collapsed_file = "/tmp/heap_collapsed.prof";
        std::ostringstream collapsed_cmd;
        collapsed_cmd << "./pprof --collapsed " << exe_path << " " << latest_heap_file
                      << " > " << collapsed_file << " 2>&1";

        std::cout << "Running collapsed command: " << collapsed_cmd.str() << std::endl;

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
            std::cout << "pprof --collapsed produced no data" << std::endl;
            return R"({"error": "pprof --collapsed produced no data"})";
        }

        std::cout << "Collapsed stacks written to " << collapsed_file << std::endl;

        // Step 9b: Generate FlameGraph from collapsed format
        svg_output = generateFlameGraph(collapsed_file, "Heap Flame Graph");

        // Check if it's an error
        if (svg_output.size() > 10 && svg_output[0] == '{' && svg_output[1] == '"') {
            // Error occurred, return as-is
            return svg_output;
        }

        std::cout << "Heap FlameGraph generated successfully! Size: " << svg_output.length() << std::endl;

    } else {
        // PATH 2: Default - Generate pprof SVG (existing behavior)
        std::cout << "Generating Heap pprof SVG..." << std::endl;

        // Build pprof command (使用可执行文件的绝对路径进行符号化)
        std::ostringstream cmd;
        cmd << "./pprof --svg " << exe_path << " " << latest_heap_file << " 2>/dev/null";

        std::cout << "Command: " << cmd.str() << std::endl;

        if (!executeCommand(cmd.str(), svg_output)) {
            std::cout << "pprof command failed" << std::endl;
            return R"({"error": "Failed to execute pprof command"})";
        }

        // Check if SVG was generated
        if (svg_output.find("<?xml") == std::string::npos &&
            svg_output.find("<svg") == std::string::npos) {
            std::cout << "pprof did not return SVG. Output: " << svg_output.substr(0, 200) << std::endl;
            return R"({"error": "pprof did not generate valid SVG. Output: )" + svg_output + R"("})";
        }

        std::cout << "Heap pprof SVG generated successfully! Size: " << svg_output.length() << std::endl;

        // 后处理 SVG：添加 viewBox 以支持正确的缩放和显示
        size_t svg_start = svg_output.find("<svg");
        if (svg_start != std::string::npos) {
            size_t svg_tag_end = svg_output.find(">", svg_start);
            if (svg_tag_end != std::string::npos) {
                std::string svg_tag = svg_output.substr(svg_start, svg_tag_end - svg_start);
                if (svg_tag.find("viewBox") == std::string::npos) {
                    std::string viewbox_attr = " viewBox=\"0 -1000 2000 1000\"";
                    svg_output.insert(svg_tag_end, viewbox_attr);
                    std::cout << "Added viewBox to heap SVG for proper scaling" << std::endl;
                }
            }
        }
    }

    return svg_output;
}

std::string ProfilerManager::findLatestHeapProfile(const std::string& dir) {
    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        std::cerr << "Failed to open directory: " << dir << std::endl;
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
        std::cerr << "Invalid seconds parameter: " << seconds
                  << ". Must be between 1 and 300." << std::endl;
        return "";
    }

    // Check concurrency control
    bool expected = false;
    if (!cpu_profiling_in_progress_.compare_exchange_strong(expected, true)) {
        std::cerr << "CPU profiling already in progress. Only one request at a time." << std::endl;
        return "";
    }

    // Use RAII to ensure flag is reset even if exception occurs
    struct FlagGuard {
        std::atomic<bool>& flag;
        ~FlagGuard() { flag.store(false); }
    };
    FlagGuard guard{cpu_profiling_in_progress_};

    // Generate temporary profile file path
    std::string profile_path = profile_dir_ + "/pprof_cpu_temp.prof";

    // Stop any existing CPU profiler first
    if (profiler_states_[ProfilerType::CPU].is_running) {
        std::cout << "Stopping existing CPU profiler..." << std::endl;
        ProfilerStop();
        profiler_states_[ProfilerType::CPU].is_running = false;
        usleep(100000); // 100ms to ensure file is written
    }

    // Start CPU profiler
    std::cout << "Starting CPU profiler for " << seconds << " seconds..." << std::endl;
    if (!ProfilerStart(profile_path.c_str())) {
        std::cerr << "Failed to start CPU profiler" << std::endl;
        return "";
    }

    // Update state
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    profiler_states_[ProfilerType::CPU] = ProfilerState{true, profile_path, timestamp, 0};

    // Wait for specified duration
    sleep(seconds);

    // Stop profiler
    std::cout << "Stopping CPU profiler..." << std::endl;
    ProfilerStop();

    now = std::chrono::system_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    profiler_states_[ProfilerType::CPU].is_running = false;
    profiler_states_[ProfilerType::CPU].duration =
        timestamp - profiler_states_[ProfilerType::CPU].start_time;

    // Wait for file to be flushed
    usleep(200000); // 200ms

    // Read profile file
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open profile file: " << profile_path << std::endl;
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

    std::cout << "Profile data size: " << profile_data.size() << " bytes" << std::endl;
    return profile_data;
}

std::string ProfilerManager::getRawHeapSample() {
    // Get heap sample from tcmalloc
    // MallocExtensionWriter is typedef'd as std::string
    std::string heap_sample;

    // GetHeapSample writes the heap profile into the string
    MallocExtension::instance()->GetHeapSample(&heap_sample);

    if (heap_sample.empty()) {
        std::cerr << "Failed to get heap sample. "
                  << "Make sure TCMALLOC_SAMPLE_PARAMETER environment variable is set."
                  << std::endl;
        return "";
    }

    std::cout << "Heap sample size: " << heap_sample.size() << " bytes" << std::endl;
    return heap_sample;
}

std::string ProfilerManager::getRawHeapGrowthStacks() {
    // Get heap growth stacks from tcmalloc
    // MallocExtensionWriter is typedef'd as std::string
    std::string heap_growth_stacks;

    // GetHeapGrowthStacks writes the heap growth stacks into the string
    MallocExtension::instance()->GetHeapGrowthStacks(&heap_growth_stacks);

    if (heap_growth_stacks.empty()) {
        std::cerr << "Failed to get heap growth stacks. "
                  << "No heap growth data available."
                  << std::endl;
        return "";
    }

    std::cout << "Heap growth stacks size: " << heap_growth_stacks.size() << " bytes" << std::endl;
    return heap_growth_stacks;
}

std::string ProfilerManager::getThreadStacks() {
    std::ostringstream result;

    // Open /proc/self/task directory to list all threads
    DIR* task_dir = opendir("/proc/self/task");
    if (!task_dir) {
        std::cerr << "Failed to open /proc/self/task" << std::endl;
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
                            case 'R': state_str = "Running"; break;
                            case 'S': state_str = "Sleeping"; break;
                            case 'D': state_str = "Disk sleep"; break;
                            case 'Z': state_str = "Zombie"; break;
                            case 'T': state_str = "Stopped"; break;
                            case 't': state_str = "Tracing stop"; break;
                            case 'X': state_str = "Dead"; break;
                            case 'x': state_str = "Dead"; break;
                            case 'K': state_str = "Wakekill"; break;
                            case 'W': state_str = "Waking"; break;
                            case 'P': state_str = "Parked"; break;
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
    std::cout << "Thread stacks collected, size: " << output.size() << " bytes" << std::endl;

    return output;
}

// Signal handler for capturing stack traces (signal-safe)
void ProfilerManager::signalHandler(int signum, siginfo_t* info, void* context) {
    // Check if we should capture
    if (!capture_in_progress_.load(std::memory_order_relaxed)) {
        return;
    }

    // Check shared memory
    if (!shared_stacks_) {
        return;
    }

    // Get current thread ID
    pid_t tid = gettid();

    // Calculate slot based on TID
    int slot = tid % max_threads_;

    // Try to capture full stack trace using backtrace()
    // Note: backtrace() is not officially async-signal-safe, but often works on Linux
    int depth = backtrace(shared_stacks_[slot].addresses, 64);

    // Store metadata
    shared_stacks_[slot].tid = tid;
    shared_stacks_[slot].depth = depth;

    // Mark as ready
    shared_stacks_[slot].ready.store(true, std::memory_order_release);

    // Suppress unused parameter warnings
    (void)signum;
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

    if (!shared_stacks_) {
        std::cerr << "Shared memory not initialized" << std::endl;
        // Fallback: capture current thread only
        pid_t current_tid = gettid();
        ThreadStackTrace trace;
        trace.tid = current_tid;
        trace.depth = backtrace(trace.addresses, 64);
        trace.captured = true;
        result.push_back(trace);
        return result;
    }

    // Get all thread IDs
    std::vector<pid_t> tids;
    DIR* task_dir = opendir("/proc/self/task");
    if (!task_dir) {
        std::cerr << "Failed to open /proc/self/task" << std::endl;
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(task_dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        pid_t tid = atoi(entry->d_name);
        if (tid > 0) {
            tids.push_back(tid);
        }
    }
    closedir(task_dir);

    std::cout << "Found " << tids.size() << " threads" << std::endl;

    // Clear all ready flags in shared memory
    for (int i = 0; i < max_threads_; ++i) {
        shared_stacks_[i].ready = false;
        shared_stacks_[i].depth = 0;
        shared_stacks_[i].tid = 0;
        // Clear addresses to avoid stale data
        for (int j = 0; j < 64; ++j) {
            shared_stacks_[i].addresses[j] = nullptr;
        }
    }

    // Memory barrier to ensure clears are visible
    __sync_synchronize();

    // Set capture flag
    capture_in_progress_.store(true, std::memory_order_release);

    pid_t current_tid = gettid();
    pid_t current_pid = getpid();

    // Send signal to all threads including current thread
    // Use tgkill for more precise signal delivery
    int signals_sent = 0;
    int signals_failed = 0;

    for (pid_t tid : tids) {
        // For current thread, use raise() which is safer
        if (tid == current_tid) {
            std::cout << "Using raise() for current thread: " << tid << std::endl;
            raise(STACK_CAPTURE_SIGNAL);
            signals_sent++;
        } else {
            // For other threads, try tgkill first, then pthread_kill
            int ret = syscall(SYS_tgkill, current_pid, tid, STACK_CAPTURE_SIGNAL);
            if (ret == 0) {
                signals_sent++;
            } else if (ret == ESRCH) {
                // Thread doesn't exist anymore
                signals_failed++;
            } else {
                // Other errors - try pthread_kill as fallback
                ret = pthread_kill(tid, STACK_CAPTURE_SIGNAL);
                if (ret == 0) {
                    signals_sent++;
                } else {
                    signals_failed++;
                }
            }
        }
    }

    std::cout << "Sent signal to " << signals_sent << " threads ("
              << signals_failed << " failed)" << std::endl;

    // Wait for all threads to handle the signal
    // Give them time to respond
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Clear capture flag
    capture_in_progress_.store(false, std::memory_order_release);

    // Memory barrier to ensure flag clear is visible
    __sync_synchronize();

    // Small additional delay to ensure all signal handlers have completed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Collect results from shared memory
    int collected = 0;
    for (int i = 0; i < max_threads_; ++i) {
        // Memory barrier before reading
        __sync_synchronize();

        if (shared_stacks_[i].ready && shared_stacks_[i].depth > 0) {
            ThreadStackTrace trace;
            trace.tid = shared_stacks_[i].tid;
            trace.depth = shared_stacks_[i].depth;
            trace.captured = true;

            // Copy addresses
            for (int j = 0; j < trace.depth && j < 64; ++j) {
                trace.addresses[j] = shared_stacks_[i].addresses[j];
            }

            result.push_back(trace);
            collected++;
        }
    }

    std::cout << "Collected " << collected << " thread stacks from shared memory" << std::endl;
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
                result << "0x" << std::hex << reinterpret_cast<uintptr_t>(addr)
                       << std::dec << "\n";
            } else {
                result << symbolized << "\n";
            }
        }

        result << "\n";
    }

    std::string output = result.str();
    std::cout << "Thread callstacks collected, size: " << output.size() << " bytes" << std::endl;

    return output;
}

} // namespace profiler

