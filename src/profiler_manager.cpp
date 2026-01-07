#include "profiler_manager.h"
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>

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

std::string ProfilerManager::getSymbolizedProfile(const std::string& profile_path) {
    // 检查 pprof 是否可用
    std::string test_cmd = "which pprof 2>/dev/null";
    std::string test_output;
    executeCommand(test_cmd, test_output);

    if (test_output.empty() || test_output.find("not found") != std::string::npos) {
        // pprof 不可用，返回友好错误
        std::string output = "错误: 系统未安装 Google pprof 工具\n\n";
        output += "📊 推荐方案 - 使用在线工具（无需安装）：\n";
        output += "   1. 点击\"下载 Profile\"按钮下载 profile 文件\n";
        output += "   2. 访问 https://www.speedscope.app/\n";
        output += "   3. 上传文件查看交互式火焰图\n\n";
        output += "🔧 安装 pprof 工具（完整功能）：\n";
        output += "   wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz\n";
        output += "   sudo tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz\n";
        output += "   export PATH=$PATH:/usr/local/go/bin\n";
        output += "   go install github.com/google/pprof@latest\n";
        return output;
    }

    std::string cmd = "pprof --text " + profile_path + " 2>&1";
    std::string output;
    executeCommand(cmd, output);
    return output;
}

std::string ProfilerManager::getProfileSVG(const std::string& profile_path) {
    // 检查 pprof 是否可用
    std::string test_cmd = "which pprof";
    std::string test_output;
    executeCommand(test_cmd, test_output);

    if (test_output.find("pprof") == std::string::npos || test_output.find("not found") != std::string::npos) {
        // pprof 不可用，返回提示 SVG
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <rect width="800" height="400" fill="#f5f5f5"/>
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">🔥 Flame Graph 生成失败</text>
  <text x="20" y="80" font-family="Arial" font-size="14" fill="#666">系统未安装 Google pprof 工具</text>

  <text x="20" y="130" font-family="Arial" font-size="16" fill="#1976D2">解决方案：</text>

  <text x="20" y="160" font-family="Arial" font-size="14" fill="#555">方法 1: 使用在线工具（推荐）</text>
  <text x="30" y="185" font-family="Arial" font-size="13" fill="#666">1. 点击"下载 Profile"按钮下载 profile 文件</text>
  <text x="30" y="210" font-family="Arial" font-size="13" fill="#666">2. 访问 https://www.speedscope.app/</text>
  <text x="30" y="235" font-family="Arial" font-size="13" fill="#666">3. 上传文件查看交互式火焰图</text>

  <text x="20" y="275" font-family="Arial" font-size="14" fill="#555">方法 2: 安装 pprof 工具</text>
  <text x="30" y="300" font-family="Arial" font-size="13" fill="#666">wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz</text>
  <text x="30" y="325" font-family="Arial" font-size="13" fill="#666">sudo tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz</text>
  <text x="30" y="350" font-family="Arial" font-size="13" fill="#666">go install github.com/google/pprof@latest</text>
</svg>
)";
    }

    std::string cmd = "pprof --svg " + profile_path + " 2>&1";
    std::string output;
    executeCommand(cmd, output);
    return output;
}

std::string ProfilerManager::generateSVGFromProfile(const std::string& profile_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 确定profile路径
    std::string profile_path;
    if (profile_type == "cpu") {
        profile_path = profiler_states_[ProfilerType::CPU].output_path;
    } else if (profile_type == "heap") {
        profile_path = profiler_states_[ProfilerType::HEAP].output_path;
    } else {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">Error: Invalid profile type</text>
</svg>)";
    }

    // 解析profile数据
    std::vector<std::pair<std::string, int>> stack_samples;

    if (profile_type == "cpu") {
        // 使用pprof --text解析CPU profile
        std::string test_cmd = "which pprof";
        std::string test_output;
        executeCommand(test_cmd, test_output);

        if (test_output.find("pprof") == std::string::npos || test_output.find("not found") != std::string::npos) {
            // pprof不可用，返回提示信息
            return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <rect width="800" height="400" fill="#f5f5f5"/>
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">CPU Flame Graph</text>
  <text x="20" y="80" font-family="Arial" font-size="14" fill="#666">此功能需要 pprof 工具</text>
  <text x="20" y="110" font-family="Arial" font-size="12" fill="#888">请使用 pprof 生成 SVG 或下载 profile 文件使用在线工具</text>
</svg>)";
        }

        std::string cmd = "pprof --text " + profile_path + " 2>&1";
        std::string output;
        if (!executeCommand(cmd, output)) {
            return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">Error: Failed to execute pprof</text>
</svg>)";
        }

        // 解析pprof文本输出
        std::istringstream iss(output);
        std::string line;

        while (std::getline(iss, line)) {
            if (line.empty() || line.find("flat") == 0 || line[0] == '-' ||
                line.find("File:") == 0 || line.find("Type:") == 0 ||
                line.find("Showing") == 0) {
                continue;
            }

            std::istringstream line_ss(line);
            std::string flat_str, flat_percent, sum_percent, cum_str, cum_percent;

            if (line_ss >> flat_str >> flat_percent >> sum_percent >> cum_str >> cum_percent) {
                int value_ms = 0;
                if (flat_str.back() == 's') {
                    try {
                        double seconds = std::stod(flat_str.substr(0, flat_str.length() - 1));
                        value_ms = static_cast<int>(seconds * 1000);
                    } catch (...) {
                        continue;
                    }
                }

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
    } else {
        // 对Heap profile也使用pprof --text来获取符号化的数据
        std::string test_cmd = "which pprof";
        std::string test_output;
        executeCommand(test_cmd, test_output);

        if (test_output.find("pprof") == std::string::npos || test_output.find("not found") != std::string::npos) {
            // pprof不可用，尝试直接解析heap profile文本格式
            std::ifstream file(profile_path);
            if (!file.is_open()) {
                return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">Error: Cannot open heap profile file</text>
</svg>)";
            }

            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;
                if (line.find("heap profile:") != std::string::npos ||
                    line.find("heap_v2") != std::string::npos) {
                    continue;
                }

                size_t at_pos = line.find('@');
                if (at_pos != std::string::npos) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos && colon_pos < at_pos) {
                        std::string count_str = line.substr(colon_pos + 1);
                        size_t space_pos = count_str.find(' ');
                        if (space_pos != std::string::npos) {
                            count_str = count_str.substr(0, space_pos);
                            try {
                                int count = std::stoi(count_str);

                                // 使用地址作为标识
                                std::string stack_part = line.substr(at_pos + 1);
                                std::stringstream ss(stack_part);
                                std::string addr;
                                if (ss >> addr) {
                                    // 简化显示：只显示第一个地址
                                    stack_samples.push_back({"memory_alloc", count});
                                }
                            } catch (...) {
                                continue;
                            }
                        }
                    }
                }
            }
        } else {
            // 使用pprof --text获取符号化的heap profile
            std::string cmd = "pprof --text " + profile_path + " 2>&1";
            std::string output;
            if (!executeCommand(cmd, output)) {
                return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">Error: Failed to execute pprof</text>
</svg>)";
            }

            // 解析pprof文本输出（与CPU相同）
            std::istringstream iss(output);
            std::string line;

            while (std::getline(iss, line)) {
                if (line.empty() || line.find("flat") == 0 || line[0] == '-' ||
                    line.find("File:") == 0 || line.find("Type:") == 0 ||
                    line.find("Showing") == 0) {
                    continue;
                }

                std::istringstream line_ss(line);
                std::string flat_str, flat_percent, sum_percent, cum_str, cum_percent;

                if (line_ss >> flat_str >> flat_percent >> sum_percent >> cum_str >> cum_percent) {
                    int value_bytes = 0;
                    if (flat_str.back() == 'B') {
                        // 处理类似 "1048576B" 的格式
                        try {
                            value_bytes = std::stoi(flat_str.substr(0, flat_str.length() - 1));
                        } catch (...) {
                            continue;
                        }
                    } else {
                        try {
                            value_bytes = std::stoi(flat_str);
                        } catch (...) {
                            continue;
                        }
                    }

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

                    if (!names.empty() && value_bytes > 0) {
                        stack_samples.push_back({names[0], value_bytes});
                    }
                }
            }
        }
    }

    if (stack_samples.empty()) {
        return R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="400">
  <rect width="800" height="400" fill="#f5f5f5"/>
  <text x="20" y="40" font-family="Arial" font-size="20" fill="#333">No profile data available</text>
  <text x="20" y="80" font-family="Arial" font-size="14" fill="#666">请先启动并停止 profiler</text>
</svg>)";
    }

    // XML转义辅助函数
    auto xmlEscape = [](const std::string& str) -> std::string {
        std::string result;
        result.reserve(str.length() * 1.2);
        for (char c : str) {
            switch (c) {
                case '&':  result.append("&amp;"); break;
                case '<':  result.append("&lt;"); break;
                case '>':  result.append("&gt;"); break;
                case '"':  result.append("&quot;"); break;
                case '\'': result.append("&apos;"); break;
                default:   result.push_back(c); break;
            }
        }
        return result;
    };

    // 生成SVG火焰图
    std::ostringstream svg;
    int svg_width = 1200;
    int svg_height = 600;
    int margin = 20;
    int bar_height = 40;
    int total_value = 0;

    std::map<std::string, int> func_totals;
    for (const auto& sample : stack_samples) {
        func_totals[sample.first] += sample.second;
        total_value += sample.second;
    }

    // 按值排序
    std::vector<std::pair<std::string, int>> sorted_funcs(
        func_totals.begin(), func_totals.end());
    std::sort(sorted_funcs.begin(), sorted_funcs.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    svg << R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width=")" << svg_width << R"(" height=")" << svg_height << R"(">
  <style>
    .func-bar { stroke: white; stroke-width: 1px; }
    .func-bar:hover { stroke: black; stroke-width: 2px; }
    .func-label { font-family: Arial, sans-serif; font-size: 12px; fill: white; pointer-events: none; }
    .title { font-family: Arial, sans-serif; font-size: 20px; fill: #333; }
    .subtitle { font-family: Arial, sans-serif; font-size: 14px; fill: #666; }
  </style>
  <rect width=")" << svg_width << R"(" height=")" << svg_height << R"(" fill="#f5f5f5"/>
  <text x=")" << margin << R"(" y=")" << (margin + 10) << R"(" class="title">)" << (profile_type == "cpu" ? "CPU" : "Heap") << R"( Flame Graph</text>
  <text x=")" << margin << R"(" y=")" << (margin + 35) << R"(" class="subtitle">Total: )" << total_value << R"( | Functions: )" << sorted_funcs.size() << R"(</text>
)";

    // 生成颜色函数（warm colors: orange to red）
    auto getColor = [](size_t index, size_t total) -> std::string {
        double ratio = static_cast<double>(index) / total;
        int r = 255;
        int g = static_cast<int>(200 * (1.0 - ratio));
        int b = static_cast<int>(150 * (1.0 - ratio));
        char buf[20];
        snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", r, g, b);
        return std::string(buf);
    };

    // 绘制火焰图
    int y = margin + 60;
    int x = margin;
    int available_width = svg_width - 2 * margin;

    for (size_t i = 0; i < sorted_funcs.size(); ++i) {
        const auto& entry = sorted_funcs[i];
        int bar_width = static_cast<int>((static_cast<double>(entry.second) / total_value) * available_width);

        if (bar_width < 2) bar_width = 2; // 最小宽度

        std::string color = getColor(i, sorted_funcs.size());
        std::string escaped_name = xmlEscape(entry.first);

        svg << "  <rect class=\"func-bar\" x=\"" << x << "\" y=\"" << y
            << "\" width=\"" << bar_width << "\" height=\"" << bar_height
            << "\" fill=\"" << color << "\">\n"
            << "    <title>" << escaped_name << ": " << entry.second
            << " (" << ((entry.second * 100.0) / total_value) << "%)</title>\n"
            << "  </rect>\n";

        // 添加函数名标签（如果宽度足够）
        if (bar_width > 50) {
            std::string display_name = entry.first;
            if (display_name.length() > 40) {
                display_name = display_name.substr(0, 37) + "...";
            }
            std::string escaped_display_name = xmlEscape(display_name);

            svg << "  <text x=\"" << (x + bar_width / 2) << "\" y=\""
                << (y + bar_height / 2 + 5) << "\" class=\"func-label\" text-anchor=\"middle\">"
                << escaped_display_name << "</text>\n";
        }

        x += bar_width;
    }

    svg << "</svg>";
    return svg.str();
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

    // 对于CPU profile，需要使用pprof工具转换为文本
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
                    {"name": "sort", "value": 300},
                    {"name": "fib", "value": 150}
                ],
                "total": 900,
                "note": "演示数据 - 安装pprof后可查看实际profile数据"
            })";
        }

        // 使用pprof转换为文本格式
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
    // 使用 addr2line 或 nm 解析符号
    std::string cmd = "addr2line -e /proc/self/exe -f -C " + address + " 2>&1";
    std::string output;
    if (executeCommand(cmd, output)) {
        // addr2line 输出格式: "function_name\nsource_file:line\n"
        size_t newline_pos = output.find('\n');
        if (newline_pos != std::string::npos) {
            return output.substr(0, newline_pos);
        }
        return output;
    }

    // 回退：返回原始地址
    return "0x" + address;
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

    // 检查pprof是否可用
    std::string test_cmd = "which pprof";
    std::string test_output;
    executeCommand(test_cmd, test_output);

    if (test_output.find("pprof") == std::string::npos || test_output.find("not found") != std::string::npos) {
        // pprof不可用，返回演示数据
        return R"({
            "name": "root",
            "value": 0,
            "children": [
                {
                    "name": "cpuIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "std::sort", "value": 300, "children": []},
                        {
                            "name": "fib",
                            "value": 0,
                            "children": [
                                {"name": "fib_recursive", "value": 150, "children": []}
                            ]
                        }
                    ]
                },
                {
                    "name": "memoryIntensiveTask",
                    "value": 0,
                    "children": [
                        {"name": "std::vector::push_back", "value": 200, "children": []},
                        {"name": "operator new", "value": 100, "children": []}
                    ]
                }
            ],
            "total": 750,
            "note": "演示数据 - 安装pprof后可查看实际profile数据"
        })";
    }

    // 使用 pprof --text 获取profile数据
    std::string cmd = "pprof --text " + profile_path + " 2>&1";
    std::string output;
    if (!executeCommand(cmd, output)) {
        return R"({"error": "Failed to execute pprof"})";
    }

    // 解析pprof输出，构建层次化的火焰图数据
    std::vector<std::pair<std::string, int>> flat_samples;
    std::istringstream iss(output);
    std::string line;
    int total_value = 0;

    while (std::getline(iss, line)) {
        if (line.empty() || line.find("flat") == 0 || line[0] == '-' ||
            line.find("File:") == 0 || line.find("Type:") == 0 ||
            line.find("Showing") == 0) {
            continue;
        }

        std::istringstream line_ss(line);
        std::string flat_str, flat_percent, sum_percent, cum_str, cum_percent;

        if (line_ss >> flat_str >> flat_percent >> sum_percent >> cum_str >> cum_percent) {
            int value = 0;
            if (profile_type == "cpu") {
                if (flat_str.back() == 's') {
                    try {
                        double seconds = std::stod(flat_str.substr(0, flat_str.length() - 1));
                        value = static_cast<int>(seconds * 1000);
                    } catch (...) {
                        continue;
                    }
                }
            } else { // heap
                if (flat_str.back() == 'B') {
                    try {
                        value = std::stoi(flat_str.substr(0, flat_str.length() - 1));
                    } catch (...) {
                        continue;
                    }
                } else {
                    try {
                        value = std::stoi(flat_str);
                    } catch (...) {
                        continue;
                    }
                }
            }

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

            if (!names.empty() && value > 0) {
                flat_samples.push_back({names[0], value});
                total_value += value;
            }
        }
    }

    // 构建层次化的JSON结构
    std::ostringstream json;
    json << R"({"name": "root", "value": 0, "children": [)";

    // 合并同名函数
    std::map<std::string, int> func_totals;
    for (const auto& sample : flat_samples) {
        func_totals[sample.first] += sample.second;
    }

    // 转换为vector并排序
    std::vector<std::pair<std::string, int>> sorted_funcs(
        func_totals.begin(), func_totals.end());
    std::sort(sorted_funcs.begin(), sorted_funcs.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // 限制函数数量避免JSON过大
    size_t max_funcs = std::min(size_t(50), sorted_funcs.size());

    bool first = true;
    for (size_t i = 0; i < max_funcs; ++i) {
        if (!first) json << ",";
        first = false;

        const auto& entry = sorted_funcs[i];
        json << R"({"name": ")" << entry.first
             << R"(", "value": )" << entry.second
             << R"(, "children": []})";
    }

    json << R"(], "total": )" << total_value << "}";
    return json.str();
}

} // namespace profiler
