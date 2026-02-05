/**
 * Example: Using cpp-remote-profiler via FetchContent
 *
 * This example demonstrates how to integrate cpp-remote-profiler
 * into your project using CMake FetchContent.
 */

#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

void doWork() {
    double result = 0;
    for (int i = 0; i < 1000000; i++) {
        result += std::sqrt(i);
    }
}

int main() {
    std::cout << "=== FetchContent Integration Example ===" << std::endl;

    // Get profiler instance
    auto& profiler = profiler::ProfilerManager::getInstance();

    std::cout << "Profiler version: " << REMOTE_PROFILER_VERSION << std::endl;

    // Start CPU profiling
    std::cout << "Starting CPU profiling..." << std::endl;
    profiler.startCPUProfiler("fetch_content_profile.prof");

    // Do some work
    for (int i = 0; i < 10; i++) {
        doWork();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop profiling
    profiler.stopCPUProfiler();
    std::cout << "Profiling complete!" << std::endl;

    return 0;
}
