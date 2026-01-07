#include "stack_collector.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace profiler {

StackCollector::StackCollector()
    : running_(false)
    , sampling_interval_ms_(100) {
}

StackCollector::~StackCollector() {
    stop();
}

StackCollector& StackCollector::getInstance() {
    static StackCollector instance;
    return instance;
}

bool StackCollector::start(int sampling_interval_ms) {
    if (running_.load()) {
        return false; // Already running
    }

    sampling_interval_ms_ = sampling_interval_ms;
    running_.store(true);

    // 启动采样线程
    sampling_thread_ = std::thread(&StackCollector::samplingThread, this);

    return true;
}

void StackCollector::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // 通知采样线程退出
    cv_.notify_all();

    // 等待线程结束
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
    }
}

void StackCollector::addSample(const std::vector<void*>& stack) {
    if (stack.empty()) {
        return;
    }

    StackSample sample;
    sample.addresses = stack;
    sample.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(samples_mutex_);
    samples_.push_back(std::move(sample));
}

std::vector<StackSample> StackCollector::getSamples() const {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    return samples_;
}

void StackCollector::clear() {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    samples_.clear();
}

std::string StackCollector::resolveSymbol(void* addr) {
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_sname) {
        // 如果有 C++ 符号，尝试 demangle
        int status = 0;
        char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::string result(demangled);
            free(demangled);

            // 如果符号名是空的或者是通用名称，尝试添加更多信息
            if (info.dli_fname && strlen(info.dli_fname) > 0) {
                // 如果是共享库，添加库名
                const char* libname = strrchr(info.dli_fname, '/');
                if (libname) {
                    libname++; // 跳过 '/'
                } else {
                    libname = info.dli_fname;
                }
                return std::string(libname) + "::" + result;
            }
            return result;
        }
        return std::string(info.dli_sname);
    }

    // dladdr 失败，可能是系统库中的地址
    // 尝试获取共享库名称
    if (dladdr(addr, &info) && info.dli_fname) {
        const char* libname = strrchr(info.dli_fname, '/');
        if (libname) {
            libname++; // 跳过 '/'
        } else {
            libname = info.dli_fname;
        }

        // 计算相对地址
        uintptr_t offset = reinterpret_cast<uintptr_t>(addr) -
                          reinterpret_cast<uintptr_t>(info.dli_fbase);

        std::ostringstream oss;
        oss << libname << "+0x" << std::hex << offset;
        return oss.str();
    }

    // 完全无法解析，返回原始地址
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(addr);
    return oss.str();
}

void StackCollector::samplingThread() {
    const int max_frames = 64;
    void* addr_list[max_frames];

    while (running_.load()) {
        // 收集调用栈
        int num_frames = backtrace(addr_list, max_frames);

        if (num_frames > 0) {
            // 跳过前几帧（包括采样线程本身）
            // 从索引 1 或 2 开始，跳过 samplingThread 和相关的帧
            std::vector<void*> stack;
            for (int i = 1; i < num_frames; ++i) {
                stack.push_back(addr_list[i]);
            }

            if (!stack.empty()) {
                addSample(stack);
            }
        }

        // 等待下一个采样间隔
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(sampling_interval_ms_),
                    [this] { return !running_.load(); });
    }
}

std::string StackCollector::getCollapsedStacks() const {
    std::lock_guard<std::mutex> lock(samples_mutex_);

    if (samples_.empty()) {
        return "# No samples collected\n";
    }

    // 聚合相同的调用栈
    std::map<std::string, size_t> stack_counts;

    for (const auto& sample : samples_) {
        std::ostringstream stack_str;

        // 从叶子到根构建栈字符串（反转顺序）
        // backtrace 返回的是从当前帧向下的顺序
        // 我们需要从根到叶子的顺序
        for (auto it = sample.addresses.rbegin(); it != sample.addresses.rend(); ++it) {
            std::string symbol = resolveSymbol(*it);

            // 跳过一些已知的系统调用
            if (symbol.find("StackCollector::samplingThread") != std::string::npos ||
                symbol.find("std::thread::_State_impl") != std::string::npos) {
                continue;
            }

            if (!stack_str.str().empty()) {
                stack_str << ";";
            }
            stack_str << symbol;
        }

        std::string key = stack_str.str();
        if (!key.empty()) {
            stack_counts[key]++;
        }
    }

    // 生成 collapsed 格式输出
    std::ostringstream result;

    // 添加 header
    result << "# collapsed stack traces\n";
    result << "# Total samples: " << samples_.size() << "\n";
    result << "# Unique stacks: " << stack_counts.size() << "\n";

    // 按样本数排序（从高到低）
    std::vector<std::pair<std::string, size_t>> sorted_stacks(
        stack_counts.begin(), stack_counts.end());
    std::sort(sorted_stacks.begin(), sorted_stacks.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    for (const auto& entry : sorted_stacks) {
        result << entry.first << " " << entry.second << "\n";
    }

    return result.str();
}

} // namespace profiler
