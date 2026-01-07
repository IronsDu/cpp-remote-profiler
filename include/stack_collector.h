#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace profiler {

// 存储单个调用栈样本
struct StackSample {
    std::vector<void*> addresses;  // 调用栈地址
    uint64_t timestamp;             // 时间戳
};

// 调用栈收集器
class StackCollector {
public:
    static StackCollector& getInstance();

    // 启动收集器
    bool start(int sampling_interval_ms = 100);

    // 停止收集器
    void stop();

    // 检查是否正在运行
    bool isRunning() const { return running_; }

    // 添加一个样本（由采样线程调用）
    void addSample(const std::vector<void*>& stack);

    // 获取所有收集的样本
    std::vector<StackSample> getSamples() const;

    // 清空样本
    void clear();

    // 生成 collapsed 格式的数据
    // 格式: "func1;func2;func3 count"
    std::string getCollapsedStacks() const;

    // 获取采样统计信息
    size_t getSampleCount() const { return samples_.size(); }

private:
    StackCollector();
    ~StackCollector();

    // 禁止拷贝
    StackCollector(const StackCollector&) = delete;
    StackCollector& operator=(const StackCollector&) = delete;

    // 采样线程函数
    void samplingThread();

    // 解析地址为符号名
    static std::string resolveSymbol(void* addr);

    // 采样线程
    std::thread sampling_thread_;

    // 运行标志
    std::atomic<bool> running_;

    // 采样间隔（毫秒）
    int sampling_interval_ms_;

    // 存储所有样本
    std::vector<StackSample> samples_;

    // 保护 samples_ 的互斥锁
    mutable std::mutex samples_mutex_;

    // 用于通知采样线程退出
    std::condition_variable cv_;
    std::mutex cv_mutex_;
};

} // namespace profiler
