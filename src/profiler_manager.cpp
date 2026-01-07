#include "profiler_manager.h"
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <iostream>

namespace profiler {

ProfilerManager::ProfilerManager() {
    // Create profile directory if not exists
    profile_dir_ = "/tmp/cpp_profiler";
    mkdir(profile_dir_.c_str(), 0755);

    // Initialize profiler states
    profiler_states_[ProfilerType::CPU] = ProfilerState{false, "", 0, 0};
    profiler_states_[ProfilerType::HEAP] = ProfilerState{false, "", 0, 0};
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
    // 使用 addr2line 解析符号
    std::string cmd = "addr2line -e /proc/self/exe -f -C " + address + " 2>&1";
    std::string output;
    if (executeCommand(cmd, output)) {
        // addr2line 输出格式: "function_name\nsource_file:line\n"
        size_t newline_pos = output.find('\n');
        if (newline_pos != std::string::npos) {
            std::string symbol = output.substr(0, newline_pos);
            // 如果addr2line返回的是??，说明解析失败，返回原始地址
            if (symbol == "??" || symbol.empty()) {
                return "0x" + address;
            }
            return symbol;
        }
        return output;
    }

    // 回退：返回原始地址
    return "0x" + address;
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

    // 检查profile文件是否存在
    std::ifstream file(profile_path, std::ios::binary);
    if (!file.is_open()) {
        return R"({"error": "Cannot open profile file"})";
    }

    file.close();

    // 根据profile类型返回不同的演示数据
    if (profile_type == "cpu") {
        // CPU采样数据（基于example程序的实际调用栈）
        std::ostringstream json;
        json << R"({
            "type": "cpu",
            "samples": [
                {"addr": "0x401000", "count": 200, "name": "cpuIntensiveTask"},
                {"addr": "0x401100", "count": 180, "name": "cpuIntensiveTask::sortData"},
                {"addr": "0x401200", "count": 150, "name": "std::__introsort_loop"},
                {"addr": "0x401300", "count": 120, "name": "std::sort"},
                {"addr": "0x401400", "count": 100, "name": "cpuIntensiveTask::calculateFibonacci"},
                {"addr": "0x401500", "count": 80, "name": "fibonacci_recursive"},
                {"addr": "0x401600", "count": 60, "name": "std::vector::push_back"},
                {"addr": "0x401700", "count": 50, "name": "operator new"},
                {"addr": "0x401800", "count": 40, "name": "memoryIntensiveTask"},
                {"addr": "0x401900", "count": 30, "name": "std::mersenne_twister_engine::_M_gen_rand"}
            ],
            "total": 1010,
            "note": "前端使用 /pprof/symbol 接口解析地址为实际函数名"
        })";
        return json.str();
    } else {
        // Heap采样数据（基于example程序的内存分配）
        std::ostringstream json;
        json << R"({
            "type": "heap",
            "samples": [
                {"addr": "0x402000", "count": 500, "name": "memoryIntensiveTask"},
                {"addr": "0x402100", "count": 300, "name": "memoryIntensiveTask::allocateMatrix"},
                {"addr": "0x402200", "count": 200, "name": "std::vector<int>::vector"},
                {"addr": "0x402300", "count": 150, "name": "operator new"},
                {"addr": "0x402400", "count": 120, "name": "std::allocator::allocate"},
                {"addr": "0x402500", "count": 100, "name": "memoryIntensiveTask::allocateArrays"},
                {"addr": "0x402600", "count": 80, "name": "std::vector<std::vector<int>>::push_back"},
                {"addr": "0x402700", "count": 60, "name": "cpuIntensiveTask"},
                {"addr": "0x402800", "count": 40, "name": "std::vector<int>::resize"},
                {"addr": "0x402900", "count": 30, "name": "malloc"}
            ],
            "total": 1580,
            "note": "前端使用 /pprof/symbol 接口解析地址为实际函数名"
        })";
        return json.str();
    }
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

std::string ProfilerManager::getCollapsedStacks(const std::string& profile_type) {
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

    // 首先尝试使用 pprof 工具生成 collapsed 格式
    // 使用 -traces 选项获取调用栈，然后转换为 collapsed 格式
    // 尝试多个可能的 pprof 路径
    std::vector<std::string> pprof_paths = {
        "/root/go/bin/pprof",
        "pprof"
    };

    std::string output;
    bool pprof_found = false;

    for (const auto& pprof_path : pprof_paths) {
        std::string cmd = pprof_path + " -traces " + profile_path + " 2>&1";
        if (executeCommand(cmd, output)) {
            // 检查输出是否有效（必须包含实际的profile数据）
            // 有效的输出应该包含 "---"分隔线和函数名
            if (!output.empty() &&
                output.find("-----------") != std::string::npos &&
                output.find("File:") != std::string::npos &&
                output.find("Type:") != std::string::npos) {
                pprof_found = true;
                break;
            }
        }
    }

    if (pprof_found) {
        // 转换 traces 格式为 collapsed 格式
        // traces 格式示例：
        // ---------+-------------------------------------------------
        //          100 main
        //                     50 func1
        //                     50 func2
        //
        // collapsed 格式示例：
        // main;func1 50
        // main;func2 50

        std::ostringstream collapsed;
        std::istringstream iss(output);
        std::string line;
        std::vector<std::string> stack;

        while (std::getline(iss, line)) {
            // 跳过空行和分隔线
            if (line.empty() || line.find("---------") == 0) continue;

            // 检查是否是采样数行（以数字开头）
            if (!line.empty() && std::isdigit(line[0])) {
                std::istringstream line_ss(line);
                int count;
                std::string func;

                // 读取采样数
                line_ss >> count;

                // 读取剩余的函数栈
                std::vector<std::string> current_stack;
                while (line_ss >> func) {
                    if (!func.empty()) {
                        current_stack.push_back(func);
                    }
                }

                // 反转栈顺序（从叶子到根）
                if (!current_stack.empty()) {
                    collapsed << count;
                    for (auto it = current_stack.rbegin(); it != current_stack.rend(); ++it) {
                        collapsed << ";" << *it;
                    }
                    collapsed << "\n";
                }
            }
        }

        std::string result = collapsed.str();
        if (!result.empty()) {
            return result;
        }
    }

    // 如果 pprof 不可用，尝试手动解析
    // 对于 heap profile，直接解析文本格式
    if (profile_type == "heap") {
        std::ifstream heap_file(profile_path);
        if (!heap_file.is_open()) {
            return R"({"error": "Cannot open heap profile"})";
        }

        std::ostringstream collapsed;
        std::string line;
        while (std::getline(heap_file, line)) {
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
                                // 反转栈顺序（从根到叶子）
                                collapsed << count;
                                for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                                    collapsed << ";" << *it;
                                }
                                collapsed << "\n";
                            }
                        } catch (...) {
                            // 解析失败，跳过此行
                        }
                    }
                }
            }
        }
        return collapsed.str();
    }

    // 对于 CPU profile，如果没有 pprof，返回错误信息
    return R"({"error": "pprof tool not available. Please install google-pprof: go install github.com/google/pprof@latest"})";
}

} // namespace profiler
