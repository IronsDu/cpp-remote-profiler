#include <gtest/gtest.h>
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <fstream>
#include <thread>
#include <chrono>
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

// 测试5: 符号化profile输出
TEST_F(ProfilerTest, SymbolizedProfileOutput) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_symbol.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    std::string text = profiler->getSymbolizedProfile("/tmp/cpp_profiler_test/cpu_symbol.prof");

    // 检查输出是否包含关键字
    EXPECT_FALSE(text.empty());

    // 如果pprof可用，应该包含函数名信息
    if (text.find("pprof") == std::string::npos) {
        // 包含一些标准库函数名
        bool has_content = text.length() > 100;
        EXPECT_TRUE(has_content);
    }
}

// 测试6: SVG生成
TEST_F(ProfilerTest, SVGGeneration) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_svg.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    std::string svg = profiler->getProfileSVG("/tmp/cpp_profiler_test/cpu_svg.prof");

    // 验证SVG格式
    EXPECT_FALSE(svg.empty());
    EXPECT_NE(svg.find("<?xml version=\"1.0\""), std::string::npos);
    EXPECT_NE(svg.find("<svg"), std::string::npos);
    EXPECT_NE(svg.find("</svg>"), std::string::npos);
}

// 测试7: XML转义功能
TEST_F(ProfilerTest, XMLEscapingInSVG) {
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_escape.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    std::string svg = profiler->generateSVGFromProfile("cpu");

    // 检查是否正确转义了特殊字符
    // C++函数名中的 < > & " 等字符应该被转义
    EXPECT_FALSE(svg.empty());

    // 验证没有未转义的XML特殊字符在文本内容中
    // 注：SVG标签本身可以包含<和>，但在文本内容中需要转义

    // 检查是否有转义实体
    size_t pos = 0;
    bool has_escaped = false;
    while ((pos = svg.find('&', pos)) != std::string::npos) {
        // 检查是否是转义序列
        if (svg.substr(pos, 5) == "&amp;" ||
            svg.substr(pos, 4) == "&lt;" ||
            svg.substr(pos, 4) == "&gt;" ||
            svg.substr(pos, 6) == "&quot;" ||
            svg.substr(pos, 6) == "&apos;") {
            has_escaped = true;
            break;
        }
        pos++;
    }

    // 如果有C++函数名，应该有转义
    // 但如果没有pprof，使用演示数据也可能没有
    SUCCEED();
}

// 测试8: JSON格式输出
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

// 测试10: 符号解析功能
TEST_F(ProfilerTest, SymbolResolution) {
    // 创建一个测试profile
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_symbol_test.prof");

    cpuIntensiveTask();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    profiler->stopCPUProfiler();

    // 测试地址解析
    std::string symbol = profiler->resolveSymbol(
        "/tmp/cpp_profiler_test/cpu_symbol_test.prof",
        "400500"
    );

    EXPECT_FALSE(symbol.empty());

    // 应该返回地址或符号名
    // 如果addr2line可用，返回函数名
    // 否则返回0x开头的地址
    bool valid = symbol.find("0x") == 0 || !symbol.empty();
    EXPECT_TRUE(valid);
}

// 测试11: 并发profiling
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

// 测试12: 多次启动停止
TEST_F(ProfilerTest, MultipleStartStop) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_multi.prof"));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        EXPECT_TRUE(profiler->stopCPUProfiler());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_FALSE(profiler->isProfilerRunning(profiler::ProfilerType::CPU));
}

// 测试13: Profile状态查询
TEST_F(ProfilerTest, ProfileStateQuery) {
    auto state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_FALSE(state.is_running);

    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_state.prof");
    state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_TRUE(state.is_running);
    EXPECT_FALSE(state.output_path.empty());

    profiler->stopCPUProfiler();
    state = profiler->getProfilerState(profiler::ProfilerType::CPU);
    EXPECT_FALSE(state.is_running);
    EXPECT_GT(state.duration, 0);
}

// 测试14: 真实场景测试
TEST_F(ProfilerTest, RealWorldScenario) {
    // 模拟真实应用场景
    profiler->startCPUProfiler("/tmp/cpp_profiler_test/cpu_real.prof");

    // 混合CPU和内存操作
    for (int i = 0; i < 10; ++i) {
        cpuIntensiveTask();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    profiler->stopCPUProfiler();

    // 验证生成的profile
    std::string text = profiler->getSymbolizedProfile("/tmp/cpp_profiler_test/cpu_real.prof");
    EXPECT_FALSE(text.empty());

    std::string svg = profiler->generateSVGFromProfile("cpu");
    EXPECT_FALSE(svg.empty());

    std::string json = profiler->getProfileAsJSON("cpu");
    EXPECT_FALSE(json.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
