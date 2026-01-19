// 示例：以库形式使用时，如何配置不同的信号

#include "profiler_manager.h"
#include <iostream>

// 示例 1：使用默认的 SIGUSR1
void example1_default_signal() {
    std::cout << "=== Example 1: Using default SIGUSR1 ===" << std::endl;

    // 直接使用，默认使用 SIGUSR1
    auto& profiler = profiler::ProfilerManager::getInstance();

    std::cout << "Current signal: " << profiler.getStackCaptureSignal() << std::endl;
    std::cout << "(SIGUSR1 = " << SIGUSR1 << ")" << std::endl;

    // ... 使用 profiler ...
}

// 示例 2：使用自定义信号（如果 SIGUSR1 被占用）
void example2_custom_signal() {
    std::cout << "\n=== Example 2: Using custom signal SIGUSR2 ===" << std::endl;

    // 在使用 profiler 之前设置信号
    profiler::ProfilerManager::setStackCaptureSignal(SIGUSR2);

    auto& profiler = profiler::ProfilerManager::getInstance();

    std::cout << "Current signal: " << profiler.getStackCaptureSignal() << std::endl;
    std::cout << "(SIGUSR2 = " << SIGUSR2 << ")" << std::endl;

    // ... 使用 profiler ...
}

// 示例 3：使用实时信号（SIGRTMIN + n）
void example3_realtime_signal() {
    std::cout << "\n=== Example 3: Using real-time signal ===" << std::endl;

    // 使用实时信号（SIGRTMIN + 5）
    int custom_signal = SIGRTMIN + 5;
    profiler::ProfilerManager::setStackCaptureSignal(custom_signal);

    auto& profiler = profiler::ProfilerManager::getInstance();

    std::cout << "Current signal: " << profiler.getStackCaptureSignal() << std::endl;
    std::cout << "(SIGRTMIN + 5 = " << custom_signal << ")" << std::endl;

    // ... 使用 profiler ...
}

// 示例 4：启用信号链（慎用）
void example4_signal_chaining() {
    std::cout << "\n=== Example 4: Signal chaining (use with caution) ===" << std::endl;

    // 启用信号链（在处理器中调用旧的处理器）
    profiler::ProfilerManager::setSignalChaining(true);

    auto& profiler = profiler::ProfilerManager::getInstance();

    // ... 使用 profiler ...
}

int main() {
    // 只运行其中一个示例（因为 ProfilerManager 是单例）

    // 示例 1：默认信号（推荐）
    example1_default_signal();

    // 示例 2：自定义信号
    // example2_custom_signal();

    // 示例 3：实时信号
    // example3_realtime_signal();

    // 示例 4：信号链（慎用）
    // example4_signal_chaining();

    std::cout << "\n=== Signal configuration tips ===" << std::endl;
    std::cout << "1. Check if your application uses SIGUSR1/SIGUSR2" << std::endl;
    std::cout << "2. Use setStackCaptureSignal() before calling getInstance()" << std::endl;
    std::cout << "3. Real-time signals (SIGRTMIN+n) are safer" << std::endl;
    std::cout << "4. Signal chaining can be enabled but may interfere with stack capture" << std::endl;

    return 0;
}
