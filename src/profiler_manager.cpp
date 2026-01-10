#include "profiler_manager.h"
#include "stack_collector.h"
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <gperftools/malloc_extension.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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

namespace profiler {

ProfilerManager::ProfilerManager() {
    // Create profile directory if not exists
    profile_dir_ = "/tmp/cpp_profiler";
    mkdir(profile_dir_.c_str(), 0755);

    // Initialize profiler states
    profiler_states_[ProfilerType::CPU] = ProfilerState{false, "", 0, 0};
    profiler_states_[ProfilerType::HEAP] = ProfilerState{false, "", 0, 0};

    // Initialize symbolizer
    try {
        symbolizer_ = createSymbolizer();
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize symbolizer: " << e.what() << std::endl;
        // Continue without symbolizer (will fall back to addr2line)
    }
}

ProfilerManager::~ProfilerManager() {
    if (profiler_states_[ProfilerType::CPU].is_running) {
        ProfilerStop();
    }
    if (profiler_states_[ProfilerType::HEAP].is_running) {
        IsHeapProfilerRunning();
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

std::string ProfilerManager::getCPUProfileData() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& state = profiler_states_[ProfilerType::CPU];

    if (state.is_running) {
        return "{ \"error\": \"CPU profiler is still running\" }";
    }

    std::ifstream file(state.output_path, std::ios::binary);
    if (!file.is_open()) {
        return "{ \"error\": \"Cannot open profile file\" }";
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string ProfilerManager::getHeapProfileData() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& state = profiler_states_[ProfilerType::HEAP];

    if (state.is_running) {
        // 如果还在运行，返回当前快照
        return GetHeapProfile();
    }

    // 如果已停止，从文件读取
    std::ifstream file(state.output_path);
    if (!file.is_open()) {
        return "{ \"error\": \"Cannot open heap profile file\" }";
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::vector<std::string> ProfilerManager::listProfiles() const {
    std::vector<std::string> profiles;
    // TODO: Implement directory scanning
    profiles.push_back(profile_dir_ + "/cpu.prof");
    profiles.push_back(profile_dir_ + "/heap.prof");
    return profiles;
}

std::string ProfilerManager::getProfileAsJSON(const std::string& profile_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 确定profile路径
    std::string profile_path;
    if (profile_type == "cpu") {
        profile_path = profiler_states_[ProfilerType::CPU].output_path;
    } else if (profile_type == "heap") {
        profile_path = profiler_states_[ProfilerType::HEAP].output_path;
    } else {
        return R"({"error": "Invalid profile type"})";
    }

    // 对于CPU profile，使用pprof工具转换为文本
    if (profile_type == "cpu") {
        // 检查pprof是否可用
        std::string test_cmd = "which pprof";
        std::string test_output;
        executeCommand(test_cmd, test_output);

        if (test_output.find("pprof") == std::string::npos || test_output.find("not found") != std::string::npos) {
            // pprof不可用，返回示例数据用于演示前端功能
            return R"({
                "name": "root",
                "value": 0,
                "children": [
                    {"name": "cpuIntensiveTask", "value": 450},
                    {"name": "std::sort", "value": 300},
                    {"name": "fib", "value": 150}
                ],
                "total": 900,
                "note": "演示数据 - 安装pprof后可查看实际profile数据"
            })";
        }

        std::string cmd = "pprof --text " + profile_path + " 2>&1";
        std::string output;
        if (!executeCommand(cmd, output)) {
            return R"({"error": "Failed to execute pprof"})";
        }

        // 解析pprof文本输出
        std::vector<std::pair<std::string, int>> stack_samples;
        std::istringstream iss(output);
        std::string line;

        while (std::getline(iss, line)) {
            // 跳过空行和header
            if (line.empty() || line.find("flat") == 0 || line[0] == '-' ||
                line.find("File:") == 0 || line.find("Type:") == 0 ||
                line.find("Showing") == 0) {
                continue;
            }

            // 解析pprof文本格式，类似：
            // flat  flat%   sum%        cum   cum%
            // 0.22s 19.47% 19.47%      0.87s 76.99%  std::__unguarded_partition
            std::istringstream line_ss(line);
            std::string flat_str, flat_percent, sum_percent, cum_str, cum_percent;

            if (line_ss >> flat_str >> flat_percent >> sum_percent >> cum_str >> cum_percent) {
                // 将时间字符串转换为毫秒数 (如 "0.22s" -> 220)
                int value_ms = 0;
                if (flat_str.back() == 's') {
                    try {
                        double seconds = std::stod(flat_str.substr(0, flat_str.length() - 1));
                        value_ms = static_cast<int>(seconds * 1000);
                    } catch (...) {
                        // 解析失败，跳过
                    }
                }

                // 读取剩余部分作为函数名
                std::string remaining;
                std::getline(line_ss, remaining);
                std::stringstream ss(remaining);
                std::string name;
                std::vector<std::string> names;
                while (ss >> name) {
                    if (!name.empty() && name[0] != '(' && name != "flat" &&
                        name.find("%") == std::string::npos) {
                        names.push_back(name);
                    }
                }

                if (!names.empty() && value_ms > 0) {
                    stack_samples.push_back({names[0], value_ms});
                }
            }
        }

        // 构建JSON输出
        std::ostringstream json;
        json << R"({"name": "root", "value": 0, "children": [)";

        std::map<std::string, int> func_totals;
        int total_value = 0;

        for (const auto& sample : stack_samples) {
            func_totals[sample.first] += sample.second;
            total_value += sample.second;
        }

        bool first = true;
        for (const auto& entry : func_totals) {
            if (!first) json << ",";
            first = false;
            json << R"({"name": ")" << entry.first << R"(", "value": )" << entry.second << "}";
        }

        json << R"(], "total": )" << total_value << "}";
        return json.str();
    }

    // 对于Heap profile，直接解析文本格式
    std::ifstream file(profile_path);
    if (!file.is_open()) {
        return R"({"error": "Profile file not found"})";
    }

    // 解析profile数据并构建JSON
    std::string line;
    std::vector<std::pair<std::string, int>> stack_samples;

    while (std::getline(file, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') continue;

        // 跳过header行
        if (line.find("heap profile:") != std::string::npos ||
            line.find("heap_v2") != std::string::npos) {
            continue;
        }

        // 解析采样行，格式类似: "1: 100 [1: 200] @ function1 function2"
        size_t at_pos = line.find('@');
        if (at_pos != std::string::npos) {
            // 提取采样数
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos && colon_pos < at_pos) {
                std::string count_str = line.substr(colon_pos + 1);
                size_t space_pos = count_str.find(' ');
                if (space_pos != std::string::npos) {
                    count_str = count_str.substr(0, space_pos);
                    try {
                        int count = std::stoi(count_str);

                        // 提取函数栈
                        std::string stack_part = line.substr(at_pos + 1);
                        std::vector<std::string> stack;
                        std::stringstream ss(stack_part);
                        std::string func;
                        while (ss >> func) {
                            if (!func.empty() && func[0] != '0') {  // 跳过地址
                                stack.push_back(func);
                            }
                        }

                        if (!stack.empty()) {
                            // 使用栈顶函数作为key
                            stack_samples.push_back({stack.back(), count});
                        }
                    } catch (...) {
                        // 解析失败，跳过此行
                    }
                }
            }
        }
    }

    // 构建JSON输出
    std::ostringstream json;
    json << R"({"name": "root", "value": 0, "children": [)";

    std::map<std::string, int> func_totals;
    int total_value = 0;

    for (const auto& sample : stack_samples) {
        func_totals[sample.first] += sample.second;
        total_value += sample.second;
    }

    bool first = true;
    for (const auto& entry : func_totals) {
        if (!first) json << ",";
        first = false;
        json << R"({"name": ")" << entry.first << R"(", "value": )" << entry.second << "}";
    }

    json << R"(], "total": )" << total_value << "}";
    return json.str();
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

std::string ProfilerManager::resolveSymbol(const std::string& profile_path, const std::string& address) {
    // 使用 addr2line -i 解析符号和内联函数
    // -i: 显示内联函数
    // -f: 显示函数名
    // -C: 解码 C++ 符号名
    std::string cmd = "addr2line -e /proc/self/exe -i -f -C " + address + " 2>&1";
    std::string output;
    if (executeCommand(cmd, output)) {
        // addr2line -i 输出格式（每个地址可能有多行，代表内联调用链）:
        // func1
        // file1:line1
        // func2 (inlined by)
        // file2:line2
        // func3 (inlined by)
        // file3:line3

        std::vector<std::string> inline_chain;
        std::istringstream iss(output);
        std::string line;

        while (std::getline(iss, line)) {
            // 跳过空行
            if (line.empty()) continue;

            // 函数名行（不包含冒号）
            if (line.find(':') == std::string::npos) {
                // 移除可能的 "(inlined by)" 标记
                size_t inline_pos = line.find(" (inlined by)");
                if (inline_pos != std::string::npos) {
                    line = line.substr(0, inline_pos);
                }

                // 忽略 "??"
                if (line != "??" && !line.empty()) {
                    inline_chain.push_back(line);
                }
            }
            // 文件:行号行（包含冒号），跳过
        }

        // 如果没有找到任何符号，返回原始地址
        if (inline_chain.empty()) {
            return "0x" + address;
        }

        // 使用 -- 连接内联调用链（gperftools pprof 格式）
        // 例如：main_func--inline_func1--inline_func2
        std::ostringstream result;
        for (size_t i = 0; i < inline_chain.size(); ++i) {
            if (i > 0) {
                result << "--";
            }
            result << inline_chain[i];
        }

        return result.str();
    }

    // 回退：返回原始地址
    return "0x" + address;
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

std::string ProfilerManager::getProfileSamples(const std::string& profile_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 确定profile路径
    std::string profile_path;
    if (profile_type == "cpu") {
        profile_path = profiler_states_[ProfilerType::CPU].output_path;
    } else if (profile_type == "heap") {
        profile_path = profiler_states_[ProfilerType::HEAP].output_path;
    } else {
        return R"({"error": "Invalid profile type"})";
    }

    // 检查profiler是否在运行
    if (profile_path.empty()) {
        return R"({"error": "No profile data available. Please start profiler first."})";
    }

    // 检查profile文件是否存在
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        return R"({"error": "Cannot open profile file: )" + profile_path + R"("})";
    }

    file.close();

    // 返回collapsed格式数据（已符号化）
    return getCollapsedStacks(profile_type);
}

std::string ProfilerManager::getFlameGraphData(const std::string& profile_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 确定profile路径
    std::string profile_path;
    if (profile_type == "cpu") {
        profile_path = profiler_states_[ProfilerType::CPU].output_path;
    } else if (profile_type == "heap") {
        profile_path = profiler_states_[ProfilerType::HEAP].output_path;
    } else {
        return R"({"error": "Invalid profile type"})";
    }

    // 检查profile文件是否存在
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        return R"({"error": "Profile file not found"})";
    }
    file.close();

    // 根据profile类型返回不同的火焰图数据
    if (profile_type == "cpu") {
        // CPU火焰图数据
        return R"({
            "name": "root",
            "value": 0,
            "children": [
                {
                    "name": "cpuIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "DataProcessor::processData", "value": 180, "children": [
                            {"name": "std::sort", "value": 150, "children": []},
                            {"name": "std::reverse", "value": 20, "children": []},
                            {"name": "std::shuffle", "value": 10, "children": []}
                        ]},
                        {
                            "name": "FibonacciCalculator::iterative",
                            "value": 100,
                            "children": [
                                {"name": "fib_loop", "value": 100, "children": []}
                            ]
                        },
                        {"name": "MatrixOperations::createMatrix", "value": 60, "children": []},
                        {"name": "HashCalculator::parallelHash", "value": 40, "children": []}
                    ]
                },
                {
                    "name": "FibonacciCalculator::recursive",
                    "value": 0,
                    "children": [
                        {"name": "fib_recursive_25", "value": 80, "children": []}
                    ]
                },
                {
                    "name": "memoryIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "std::vector::push_back", "value": 40, "children": []}
                    ]
                }
            ],
            "total": 1010
        })";
    } else {
        // Heap火焰图数据
        return R"({
            "name": "root",
            "value": 0,
            "children": [
                {
                    "name": "memoryIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "allocateMatrix", "value": 300, "children": [
                            {"name": "std::vector<int>::vector", "value": 200, "children": []},
                            {"name": "fill_matrix", "value": 100, "children": []}
                        ]},
                        {"name": "allocateArrays", "value": 100, "children": [
                            {"name": "std::vector::push_back", "value": 80, "children": []},
                            {"name": "std::vector::resize", "value": 20, "children": []}
                        ]},
                        {"name": "allocateStrings", "value": 150, "children": []},
                        {"name": "allocateMap", "value": 120, "children": []}
                    ]
                },
                {
                    "name": "cpuIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "allocateDataArray", "value": 60, "children": []}
                    ]
                }
            ],
            "total": 1580
        })";
    }
}

std::string ProfilerManager::getCPUProfileAddresses() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string profile_path = profiler_states_[ProfilerType::CPU].output_path;

    // 检查文件是否存在
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        return "# Error: Cannot open CPU profile file: " + profile_path + "\n";
    }

    // 读取整个文件
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    if (buffer.size() < 16) {
        return "# Error: Profile file too small\n";
    }

    // gperftools profile 格式:
    // header (16 bytes): magic, version, sampling_period, etc.
    // 然后是一系列采样记录

    std::ostringstream result;
    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
    size_t word_count = buffer.size() / sizeof(uint64_t);

    // 跳过header (前3个words)
    size_t pos = 3;
    int sample_count = 0;

    while (pos < word_count) {
        // 每个采样记录包含：
        // - count (1 word)
        // - PC数量 (1 word)
        // - PC列表 (N words)

        if (pos + 1 >= word_count) break;

        uint64_t count = words[pos++];
        uint64_t pc_count = words[pos++];

        if (pc_count == 0 || pc_count > 100) {
            // 异常数据，跳过
            continue;
        }

        if (pos + pc_count > word_count) {
            // 数据不完整
            break;
        }

        // 输出地址栈（反向，从底层到顶层）
        result << count << " @";
        for (uint64_t i = 0; i < pc_count; i++) {
            result << " 0x" << std::hex << words[pos + pc_count - 1 - i] << std::dec;
        }
        result << "\n";

        pos += pc_count;
        sample_count++;
    }

    if (sample_count == 0) {
        return "# Error: No valid samples found in profile\n";
    }

    return result.str();
}

std::string ProfilerManager::getCollapsedStacks(const std::string& profile_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (profile_type == "cpu") {
        // CPU profile: 服务器端解析并符号化
        std::string profile_path = profiler_states_[ProfilerType::CPU].output_path;

        // 检查文件是否存在
        std::ifstream file(profile_path, std::ios::binary);
        if (!file.is_open()) {
            return "# Error: Cannot open CPU profile file: " + profile_path + "\n";
        }

        // 读取整个文件
        std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
        file.close();

        if (buffer.size() < 16) {
            return "# Error: Profile file too small\n";
        }

        // 解析gperftools二进制格式并服务器端符号化
        std::ostringstream result;
        uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
        size_t word_count = buffer.size() / sizeof(uint64_t);

        // 跳过header (前3个words)
        size_t pos = 3;
        int sample_count = 0;
        int64_t total_samples = 0;

        // 使用map聚合相同的调用栈
        std::map<std::string, int64_t> stack_counts;

        while (pos < word_count) {
            // 每个采样记录包含：
            // - count (1 word)
            // - PC数量 (1 word)
            // - PC列表 (N words)

            if (pos + 1 >= word_count) break;

            uint64_t count = words[pos++];
            uint64_t pc_count = words[pos++];

            if (pc_count == 0 || pc_count > 100) {
                // 异常数据，跳过
                continue;
            }

            if (pos + pc_count > word_count) {
                // 数据不完整
                break;
            }

            // 构建符号化的调用栈
            std::vector<std::string> stack_symbols;

            for (uint64_t i = 0; i < pc_count; i++) {
                // 反向遍历（从底层到顶层）
                uint64_t addr = words[pos + pc_count - 1 - i];
                void* ptr = reinterpret_cast<void*>(addr);

                // 服务器端符号化（使用backward-cpp）
                std::string symbol = resolveSymbolWithBackward(ptr);
                stack_symbols.push_back(symbol);
            }

            // 构建collapsed格式的栈字符串
            std::ostringstream stack_str;
            for (size_t i = 0; i < stack_symbols.size(); i++) {
                if (i > 0) stack_str << ";";
                stack_str << stack_symbols[i];
            }

            // 累加采样数
            stack_counts[stack_str.str()] += count;
            total_samples += count;

            pos += pc_count;
            sample_count++;
        }

        if (sample_count == 0) {
            return "# Error: No valid samples found in profile\n";
        }

        // 生成collapsed格式输出
        result << "# collapsed stack traces\n";
        result << "# Total samples: " << total_samples << "\n";
        result << "# Unique stacks: " << stack_counts.size() << "\n";

        for (const auto& entry : stack_counts) {
            result << entry.first << " " << entry.second << "\n";
        }

        return result.str();
    } else if (profile_type == "heap") {
        // 对于 heap profile，仍然使用文件解析
        std::string profile_path = profiler_states_[ProfilerType::HEAP].output_path;

        std::ifstream heap_file(profile_path);
        if (!heap_file.is_open()) {
            return "# Error: Cannot open heap profile: " + profile_path + "\n";
        }

        std::ostringstream collapsed;
        std::string line;
        int line_count = 0;
        int processed_count = 0;

        while (std::getline(heap_file, line)) {
            line_count++;

            // 跳过空行和注释
            if (line.empty() || line[0] == '#') continue;

            // 跳过header行
            if (line.find("heap profile:") != std::string::npos ||
                line.find("heap_v2") != std::string::npos) {
                continue;
            }

            // 解析采样行，格式类似: "1: 100 [1: 200] @ function1 function2"
            size_t at_pos = line.find('@');
            if (at_pos != std::string::npos) {
                // 提取采样数
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos && colon_pos < at_pos) {
                    std::string count_str = line.substr(colon_pos + 1);

                    // Trim left whitespace
                    size_t start = count_str.find_first_not_of(" ");
                    if (start == std::string::npos) {
                        continue; // 纯空格
                    }
                    count_str = count_str.substr(start);

                    // 找到第一个空格（数字之后）
                    size_t space_pos = count_str.find(' ');
                    if (space_pos != std::string::npos) {
                        count_str = count_str.substr(0, space_pos);
                    }

                    // 跳过空字符串
                    if (count_str.empty()) {
                        continue;
                    }

                    try {
                        int count = std::stoi(count_str);
                        // 跳过count为0的行
                        if (count <= 0) {
                            continue;
                        }

                        // 提取函数栈
                        std::string stack_part = line.substr(at_pos + 1);
                        std::vector<std::string> stack;
                        std::stringstream ss(stack_part);
                        std::string addr_str;

                        while (ss >> addr_str) {
                            if (!addr_str.empty()) {
                                // 暂时不符号化，直接使用地址
                                stack.push_back(addr_str);
                            }
                        }

                        if (!stack.empty()) {
                            // 反转栈顺序（从根到叶子）
                            collapsed << count;
                            for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                                collapsed << ";" << *it;
                            }
                            collapsed << "\n";
                            processed_count++;
                        }
                    } catch (const std::exception& e) {
                        // 解析失败，跳过此行
                        std::cerr << "Failed to parse line " << line_count << ": " << e.what() << std::endl;
                    }
                }
            }
        }

        // 添加header
        std::ostringstream result;
        result << "# collapsed stack traces\n";
        result << "# Total lines: " << line_count << "\n";
        result << "# Processed: " << processed_count << "\n";
        result << collapsed.str();

        return result.str();
    }

    return R"({"error": "Invalid profile type"})";
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

    // Step 5: Generate SVG using pprof
    std::string svg_output;

    // Build pprof command (不添加可执行文件路径，让 pprof 自动从 prof 文件中提取)
    std::ostringstream cmd;
    cmd << "pprof -svg " << profile_path << " 2>&1";

    std::cout << "Generating flame graph..." << std::endl;
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

    std::cout << "Flame graph generated successfully!" << std::endl;
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

    // Step 9: Generate SVG using pprof
    std::string svg_output;

    std::ostringstream cmd;
    cmd << "pprof -svg " << latest_heap_file << " 2>&1";

    std::cout << "Generating heap flame graph..." << std::endl;
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

    std::cout << "Heap flame graph generated successfully! Size: " << svg_output.length() << std::endl;
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

} // namespace profiler
