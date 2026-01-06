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

std::vector<std::string> ProfilerManager::listProfiles() const {
    std::vector<std::string> profiles;
    // TODO: Implement directory scanning
    profiles.push_back(profile_dir_ + "/cpu.prof");
    profiles.push_back(profile_dir_ + "/heap.prof");
    return profiles;
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

} // namespace profiler
