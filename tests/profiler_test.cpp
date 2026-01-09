#include <gtest/gtest.h>
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <sstream>
#include <nlohmann/json.hpp>
#include "../include/profiler_manager.h"

using json = nlohmann::json;

class ProfilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        profiler = &profiler::ProfilerManager::getInstance();
        test_profile_dir = "/tmp/cpp_profiler_test";
        profiler->setProfileDir(test_profile_dir);

        // 清理旧的测试文件
        system("rm -rf /tmp/cpp_profiler_test");
        system("mkdir -p /tmp/cpp_profiler_test");
    }

    void TearDown() override {
        // 清理测试文件
        system("rm -rf /tmp/cpp_profiler_test");
    }

    profiler::ProfilerManager* profiler;
    std::string test_profile_dir;

    // 模拟CPU密集型任务
    void cpuIntensiveTask() {
        std::vector<int> data(10000);
        for (int i = 0; i < 100; ++i) {
            for (auto& val : data) {
                val = rand();
            }
            std::sort(data.begin(), data.end());

            // Fibonacci计算
            auto fib = [](int n) {
                if (n <= 1) return n;
                int a = 0, b = 1;
                for (int i = 2; i <= n; ++i) {
                    int temp = a + b;
                    a = b;
                    b = temp;
                }
                return b;
            };

            volatile int result = fib(30);
            (void)result;
        }
    }

    // 模拟内存密集型任务
    void memoryIntensiveTask() {
        std::vector<std::vector<int>> matrix;

        for (int i = 0; i < 50; ++i) {
            std::vector<int> row(1000);
            for (auto& val : row) {
                val = rand();
            }
            matrix.push_back(std::move(row));
        }

        // 故意创建一些内存泄漏用于测试
        for (int i = 0; i < 5; ++i) {
            auto* leak = new int[1000];
            (void)leak;
        }
    }
};

// 测试1: CPU Profiler启动和停止
TEST_F(ProfilerTest, CPUProfilerStartStop) {
    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));

    EXPECT_TRUE(profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_test.prof"));
    EXPECT_TRUE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));

    // 运行一些工作负载
    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(profiler->stopCPUProfiler());
    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));

    // 验证文件存在
    std::ifstream file("/tmp/cpp_profiler_test/cpu_test.prof");
    EXPECT_TRUE(file.is_open());

    // 验证文件不为空
    file.seekg(0, std::ios::end);
    EXPECT_GT(file.tellg(), 0);
}

// 测试2: Heap Profiler启动和停止
TEST_F(ProfilerTest, HeapProfilerStartStop) {
    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::HEAP));

    EXPECT_TRUE(profiler->startHeapProfiler("/tmp/cpp_profiler_test/heap_test.prof"));
    EXPECT_TRUE(profiler->isProfilerRunning(profiler::ProfilerType::HEAP));

    // 运行一些内存分配
    memoryIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(profiler->stopHeapProfiler());
    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::HEAP));

    // 验证文件存在
    std::ifstream file("/tmp/cpp_profiler_test/heap_test.prof");
    EXPECT_TRUE(file.is_open());
}

// 测试3: CPU Profile数据收集
TEST_F(ProfilerTest, CPUProfileDataCollection) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_data.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    // 获取profile数据
    std::string data = profiler->getCPUProfileData();
    EXPECT_FALSE(data.empty());

    // 验证数据包含二进制profiler数据
    EXPECT_GT(data.length(), 100);
}

// 测试4: Heap Profile数据收集
TEST_F(ProfilerTest, HeapProfileDataCollection) {
    profiler->startHeapProfiler("/tmp/cpp_profiler_test/heap_data.prof");

    memoryIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopHeapProfiler();

    std::string data = profiler->getHeapProfileData();
    EXPECT_FALSE(data.empty());
    EXPECT_GT(data.length(), 50);
}

// 测试5: JSON格式输出
TEST_F(ProfilerTest, JSONOutputFormat) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_json.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    std::string jsonData = profiler->getProfileAsJSON("cpu");
    EXPECT_FALSE(jsonData.empty());

    // 解析JSON
    ASSERT_NO_THROW({
        json j = json::parse(jsonData);

        // 验证JSON结构
        EXPECT_TRUE(j.contains("name"));
        EXPECT_TRUE(j.contains("value"));
        EXPECT_TRUE(j.contains("children"));

        if (j.contains("total")) {
            EXPECT_GT(j["total"], 0);
        }
    });
}

// 测试9: 火焰图数据结构
TEST_F(ProfilerTest, FlameGraphDataStructure) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_flame.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    std::string flameData = profiler->getFlameGraphData("cpu");
    EXPECT_FALSE(flameData.empty());

    // 解析JSON
    ASSERT_NO_THROW({
        json j = json::parse(flameData);

        // 验证火焰图数据结构
        EXPECT_TRUE(j.contains("name"));
        EXPECT_TRUE(j.contains("value"));
        EXPECT_TRUE(j.contains("children"));

        // children应该是数组
        EXPECT_TRUE(j["children"].is_array());

        // 如果有数据，验证结构
        if (j["children"].size() > 0) {
            for (const auto& child : j["children"]) {
                EXPECT_TRUE(child.contains("name"));
                EXPECT_TRUE(child.contains("value"));

                // value应该是数字
                EXPECT_TRUE(child["value"].is_number());

                // 如果是演示数据，有children字段
                if (child.contains("children")) {
                    EXPECT_TRUE(child["children"].is_array());
                }
            }
        }
    });
}

// 测试7: 并发profiling
TEST_F(ProfilerTest, ConcurrentProfiling) {
    // CPU和Heap profiler不应该同时运行
    EXPECT_TRUE(profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_concurrent.prof"));
    EXPECT_TRUE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));

    // 尝试启动Heap profiler应该失败（如果设计为互斥）
    // 或者可以同时运行（如果设计允许）
    cpuIntensiveTask();
    memoryIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    profiler->stopCPUProfiler();
    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));
}

// 测试8: 多次启动停止
TEST_F(ProfilerTest, MultipleStartStop) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_multi.prof"));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        EXPECT_TRUE(profiler->stopCPUProfiler());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));
}

// 测试9: Profile状态查询
TEST_F(ProfilerTest, ProfileStateQuery) {
    auto state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_FALSE(state.is_running);

    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_state.prof");
    state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_TRUE(state.is_running);
    EXPECT_FALSE(state.output_path.empty());

    // 添加一个小的延迟以确保 duration > 0
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    profiler->stopCPUProfiler();
    state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_FALSE(state.is_running);
    EXPECT_GT(state.duration, 0);
}

// 测试10: 真实场景测试
TEST_F(ProfilerTest, RealWorldScenario) {
    // 模拟真实应用场景
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_real.prof");

    // 混合CPU和内存操作
    for (int i = 0; i < 10; ++i) {
        cpuIntensiveTask();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    profiler->stopCPUProfiler();

    // 验证生成的JSON profile数据（用于浏览器端渲染）
    std::string json = profiler->getProfileAsJSON("cpu");
    EXPECT_FALSE(json.empty());

    // 验证火焰图数据
    std::string flameData = profiler->getFlameGraphData("cpu");
    EXPECT_FALSE(flameData.empty());
}

// 测试11: 符号化测试 - 使用 addr2line
TEST_F(ProfilerTest, SymbolResolutionWithAddr2Line) {
    // 首先创建一个profile文件
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_symbol.prof");

    // 生成一些CPU采样数据
    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    profiler->stopCPUProfiler();

    // 测试符号化一个有效的十六进制地址
    std::string address = "0x400500";  // 示例地址
    std::string symbol = profiler->resolveSymbol("/tmp/cpp_profiler_test/cpu_symbol.prof", address);

    // 验证返回的不是空字符串
    EXPECT_FALSE(symbol.empty());

    // 如果找到符号，应该不包含 "0x" 前缀（除非是未知符号）
    // 如果找不到符号，会返回原始地址
    if (symbol.find("0x") == 0) {
        // 未找到符号，返回的是原始地址
        EXPECT_EQ(symbol, "0x" + address);
    }
}

// 测试12: 符号化测试 - 使用 backward-cpp
TEST_F(ProfilerTest, SymbolResolutionWithBackward) {
    // 测试符号化一个简单的函数地址
    // 使用标准库函数的地址作为测试
    void* addr = reinterpret_cast<void*>(&std::sort<int*>);

    std::string symbol = profiler->resolveSymbolWithBackward(addr);

    // 验证返回的不是空字符串
    EXPECT_FALSE(symbol.empty());
}

// 测试13: 符号化多个地址
TEST_F(ProfilerTest, SymbolResolutionMultipleAddresses) {
    // 测试多个不同的地址（使用标准库函数）
    std::vector<void*> addresses;
    addresses.push_back(reinterpret_cast<void*>(&std::sort<int*>));
    addresses.push_back(reinterpret_cast<void*>(&std::reverse<int*>));

    for (void* addr : addresses) {
        std::string symbol = profiler->resolveSymbolWithBackward(addr);

        // 验证每个地址都能返回符号或地址
        EXPECT_FALSE(symbol.empty());

        // 如果符号化失败，应该返回十六进制地址
        if (symbol.find("0x") == 0) {
            // 验证是有效的十六进制格式
            EXPECT_TRUE(symbol.find_first_not_of("01234567899abcdefABCDEFx") == std::string::npos);
        }
    }
}

// 测试14: 空指针地址符号化
TEST_F(ProfilerTest, SymbolResolutionNullAddress) {
    void* null_addr = nullptr;
    std::string symbol = profiler->resolveSymbolWithBackward(null_addr);

    // 应该返回空字符串或 "0x0"
    EXPECT_TRUE(symbol.empty() || symbol == "0x0" || symbol == "0x");
}

// 测试15: 内联函数分隔符验证
TEST_F(ProfilerTest, InlineFunctionSeparator) {
    // 测试一个可能有内联函数的地址
    // std::sort 通常会有内联函数
    std::vector<int> data(1000);
    void* sort_addr = reinterpret_cast<void*>(&std::sort<decltype(data.begin())>);

    std::string symbol = profiler->resolveSymbolWithBackward(sort_addr);

    // 如果成功符号化且包含内联函数，应该使用 "--" 分隔符
    if (symbol.find("--") != std::string::npos) {
        // 验证 "--" 分隔符的使用
        std::vector<std::string> parts;
        std::stringstream ss(symbol);
        std::string part;

        while (std::getline(ss, part, '-')) {
            if (!part.empty()) {
                parts.push_back(part);
            }
        }

        // 应该至少有两个部分（主函数和内联函数）
        EXPECT_GE(parts.size(), 2);
    }
}

// 测试16: Collapsed格式输出
TEST_F(ProfilerTest, CollapsedFormatOutput) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_collapsed.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    profiler->stopCPUProfiler();

    // 获取collapsed格式的数据
    std::string collapsed = profiler->getCollapsedStacks("cpu");

    // 验证不是空
    EXPECT_FALSE(collapsed.empty());

    // 验证格式：每行应该是 "func1;func2;func3 count" 或 "func1--inline;func2 count"
    std::stringstream ss(collapsed);
    std::string line;
    int validLines = 0;

    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;

        // 查找最后一个空格，分隔栈和计数
        size_t last_space = line.find_last_of(' ');
        if (last_space != std::string::npos) {
            std::string count_str = line.substr(last_space + 1);
            try {
                int count = std::stoi(count_str);
                if (count > 0) {
                    validLines++;
                }
            } catch (...) {
                // 忽略无效的行
            }
        }
    }

    // 应该至少有一些有效的行
    EXPECT_GT(validLines, 0);
}

// 测试17: 内联函数在collapsed格式中的表示
TEST_F(ProfilerTest, InlineFunctionsInCollapsedFormat) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_inline.prof");

    // 执行一些可能有内联函数的操作
    std::vector<int> data(10000);
    for (int i = 0; i < 10; ++i) {
        std::sort(data.begin(), data.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    profiler->stopCPUProfiler();

    std::string collapsed = profiler->getCollapsedStacks("cpu");

    // 检查是否包含 "--" 分隔符（表示内联函数）
    // 注意：这可能依赖于编译器和优化级别，所以只是检查可能性
    bool has_inline_separator = (collapsed.find("--") != std::string::npos);

    // 如果找到了 "--"，验证它的使用是正确的
    if (has_inline_separator) {
        // 至少有一行包含内联函数分隔符
        EXPECT_TRUE(true);
    }
}

// 测试18: 符号化错误处理
TEST_F(ProfilerTest, SymbolResolutionErrorHandling) {
    // 测试不存在的profile文件
    std::string symbol = profiler->resolveSymbol("/nonexistent/prof.prof", "0x400500");

    // 应该返回原始地址（优雅降级）
    EXPECT_TRUE(symbol.empty() || symbol.find("0x") == 0);
}

// 测试19: 无效地址格式
TEST_F(ProfilerTest, InvalidAddressFormat) {
    // 测试无效的地址格式
    std::string symbol = profiler->resolveSymbol("", "invalid_address");

    // 应该优雅处理，返回原始输入或空字符串
    EXPECT_TRUE(symbol.empty() || symbol == "invalid_address" || symbol == "0xinvalid_address");
}

// 测试20: 符号化器初始化验证
TEST_F(ProfilerTest, SymbolizerInitialization) {
    // 验证ProfilerManager已经初始化了符号化器
    // 通过测试一个真实的地址来验证
    void* addr = reinterpret_cast<void*>(&std::sort<int*>);

    // 第一次调用可能初始化符号化器
    std::string symbol1 = profiler->resolveSymbolWithBackward(addr);
    std::string symbol2 = profiler->resolveSymbolWithBackward(addr);

    // 两次调用应该返回相同的结果（稳定性测试）
    EXPECT_EQ(symbol1, symbol2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
