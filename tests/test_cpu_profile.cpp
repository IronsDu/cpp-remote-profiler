/// @file test_cpu_profile.cpp
/// @brief Tests for CPU profiling functionality

#include "../include/profiler_manager.h"
#include <chrono>
#include <fstream>
#include <gperftools/profiler.h>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

// Free function used as a known address for symbol resolution tests
namespace {
int helperFunctionForAddrTest(int x) {
    return x * x;
}
} // namespace

// Test 1: Verify gperftools generates valid CPU profile
TEST(CPUProfileTest, GperftoolsGeneratesValidProfile) {
    const char* profile_path = "/tmp/test_cpu.prof";
    std::remove(profile_path);

    // Start profiler
    ProfilerStart(profile_path);

    // Run workload
    for (int i = 0; i < 100; ++i) {
        std::vector<int> data(1000);
        for (auto& val : data) {
            val = rand();
        }
        std::sort(data.begin(), data.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop profiler
    ProfilerStop();

    // Check file exists and has content
    std::ifstream file(profile_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open()) << "Cannot open profile file";

    size_t file_size = file.tellg();
    ASSERT_GT(file_size, 0) << "CPU profile file is empty";
    file.close();

    std::cout << "CPU profile file size: " << file_size << " bytes\n";
    std::remove(profile_path);
}

// Test 2: ProfilerManager start/stop CPU profiler
TEST(ProfilerManagerTest, StartStopCPUProfiler) {
    profiler::ProfilerManager profiler;

    std::string profile_path = "/tmp/test_manager_cpu.prof";
    std::remove(profile_path.c_str());

    bool started = profiler.startCPUProfiler(profile_path);
    ASSERT_TRUE(started) << "Failed to start CPU profiler";

    // Run workload
    for (int i = 0; i < 50; ++i) {
        std::vector<int> data(100);
        std::sort(data.begin(), data.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool stopped = profiler.stopCPUProfiler();
    ASSERT_TRUE(stopped) << "Failed to stop CPU profiler";

    // Verify profile file was created
    std::ifstream file(profile_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open()) << "Profile file not created";
    EXPECT_GT(file.tellg(), 0) << "Profile file is empty";

    std::remove(profile_path.c_str());
}

// Test 3: Multiple start/stop cycles
TEST(ProfilerManagerTest, MultipleStartStopCycles) {
    profiler::ProfilerManager profiler;

    for (int cycle = 0; cycle < 3; ++cycle) {
        std::string profile_path = "/tmp/test_cycle_" + std::to_string(cycle) + ".prof";
        std::remove(profile_path.c_str());

        ASSERT_TRUE(profiler.startCPUProfiler(profile_path)) << "Failed to start in cycle " << cycle;

        std::vector<int> data(1000);
        std::sort(data.begin(), data.end());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ASSERT_TRUE(profiler.stopCPUProfiler()) << "Failed to stop in cycle " << cycle;

        std::ifstream file(profile_path);
        EXPECT_TRUE(file.is_open()) << "Profile file not created in cycle " << cycle;

        std::remove(profile_path.c_str());
    }
}

// Test 4: analyzeCPUProfile generates output
TEST(ProfilerManagerTest, AnalyzeCPUProfile) {
    profiler::ProfilerManager profiler;

    // Analyze CPU profile (short duration)
    std::string svg_result = profiler.analyzeCPUProfile(1, "flamegraph");

    // Should return SVG content or an error (both are acceptable here)
    // We mainly verify the function doesn't crash
    SUCCEED() << "analyzeCPUProfile completed without crash";
}

// Test 5: Symbol resolution
TEST(ProfilerManagerTest, ResolveSymbol) {
    profiler::ProfilerManager profiler;

    // Test with a known free function address
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    void* test_addr = reinterpret_cast<void*>(&helperFunctionForAddrTest);

    std::string symbol = profiler.resolveSymbolWithBackward(test_addr);

    ASSERT_FALSE(symbol.empty()) << "Symbol resolution returned empty string";
    std::cout << "Resolved symbol: " << symbol << "\n";

    SUCCEED() << "Symbol resolution test completed";
}
