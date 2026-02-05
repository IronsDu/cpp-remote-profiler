#include "profiler_manager.h"
#include <iostream>

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // Test basic functionality
    std::cout << "C++ Remote Profiler version: "
              << REMOTE_PROFILER_VERSION << std::endl;

    std::cout << "ProfilerManager initialized successfully!" << std::endl;

    return 0;
}
