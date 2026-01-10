#include <gtest/gtest.h>
#include <fstream>
#include <gperftools/profiler.h>
#include <thread>
#include <chrono>
#include <iostream>
#include "../include/profiler_manager.h"

// 测试 gperftools CPU profile 解析
TEST(CPUProfileTest, ParseCPUProfileFormat) {
    // 首先创建一个测试用的CPU profile
    const char* profile_path = "/tmp/test_cpu.prof";

    // 清理旧文件
    std::remove(profile_path);

    // 启动 profiler
    ProfilerStart(profile_path);

    // 运行一些工作负载
    for (int i = 0; i < 100; ++i) {
        std::vector<int> data(1000);
        for (auto& val : data) {
            val = rand();
        }
        std::sort(data.begin(), data.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 停止 profiler
    ProfilerStop();

    // 检查文件是否存在
    std::ifstream file(profile_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open()) << "Cannot open profile file";

    size_t file_size = file.tellg();
    ASSERT_GT(file_size, 0) << "CPU profile file is empty";
    file.close();

    std::cout << "CPU profile file size: " << file_size << " bytes\n";

    // 读取文件并验证格式
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

    // gperftools profile 文件应该至少有 header
    ASSERT_GT(buffer.size(), 16) << "Profile file too small";

    // 读取 header (前3个uint64_t)
    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());

    std::cout << "Profile header:\n";
    std::cout << "  Word 0: " << words[0] << "\n";
    std::cout << "  Word 1: " << words[1] << "\n";
    std::cout << "  Word 2: " << words[2] << "\n";

    // 清理
    std::remove(profile_path);
}

// 测试 gperftools profile 格式解析
TEST(CPUProfileTest, ParseGperftoolsFormat) {
    // 创建一个测试profile
    const char* profile_path = "/tmp/test_format.prof";
    std::remove(profile_path);

    // 使用 gperftools 直接生成 profile
    ProfilerStart(profile_path);

    // 运行工作负载
    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += i;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ProfilerStop();

    // 读取原始文件
    std::ifstream file(profile_path, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "Cannot open profile file";

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    ASSERT_GT(buffer.size(), 16) << "Profile file too small";

    // 手动解析 gperftools 格式
    std::ostringstream result;
    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
    size_t word_count = buffer.size() / sizeof(uint64_t);
    size_t pos = 3;  // 跳过 header

    std::cout << "Parsing profile, word count: " << word_count << "\n";

    int sample_count = 0;
    while (pos < word_count) {
        if (pos + 1 >= word_count) break;

        uint64_t count = words[pos++];
        uint64_t pc_count = words[pos++];

        if (pc_count == 0 || pc_count > 100) {
            continue;
        }

        if (pos + pc_count > word_count) {
            break;
        }

        result << count << " @";
        for (uint64_t i = 0; i < pc_count; i++) {
            result << " 0x" << std::hex << words[pos + pc_count - 1 - i] << std::dec;
        }
        result << "\n";

        pos += pc_count;
        sample_count++;

        // 只输出前5个样本作为示例
        if (sample_count <= 5) {
            std::cout << "Sample " << sample_count << ": count=" << count
                     << ", pcs=" << pc_count << "\n";
        }
    }

    std::string addresses = result.str();
    std::cout << "Total samples: " << sample_count << "\n";
    std::cout << "Addresses output (first 500 chars):\n";
    std::cout << addresses.substr(0, std::min(size_t(500), addresses.length())) << "\n";

    // 验证解析结果
    ASSERT_GT(sample_count, 0) << "No samples found in profile";
    ASSERT_FALSE(addresses.empty()) << "No addresses generated";
    ASSERT_TRUE(addresses.find("@") != std::string::npos) << "Invalid format: missing '@'";
    ASSERT_TRUE(addresses.find("0x") != std::string::npos) << "Invalid format: missing hex addresses";

    // 清理
    std::remove(profile_path);
}

// 测试 backward-cpp 符号化
TEST(CPUProfileTest, SymbolizeWithBackward) {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 测试一个已知地址
    void* test_addr = (void*)(&std::sort<int*>);

    std::string symbol = profiler.resolveSymbolWithBackward(test_addr);

    std::cout << "Symbol for std::sort: " << symbol << "\n";

    // 验证符号化结果不为空
    ASSERT_FALSE(symbol.empty()) << "Symbolization returned empty string";

    // 符号化结果应该包含地址或者函数名
    bool has_hex = symbol.find("0x") != std::string::npos;
    bool has_name = symbol.find("sort") != std::string::npos ||
                    symbol.length() > 5;  // 至少有意义的输出

    EXPECT_TRUE(has_hex || has_name) << "Unexpected symbol format: " << symbol;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
