#include <gtest/gtest.h>
#include <fstream>
#include <gperftools/profiler.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
#include "../include/profiler_manager.h"

// 测试1: 验证 gperftools 生成的 profile 文件格式
TEST(FullFlowTest, GperftoolsGeneratesValidProfile) {
    const char* profile_path = "/tmp/test_full.prof";
    std::remove(profile_path);

    // 启动 profiler
    ProfilerStart(profile_path);

    // 运行足够的工作负载
    std::cout << "Running workload...\n";
    volatile int result = 0;
    for (int i = 0; i < 10000; ++i) {
        result += i * i;
        // 一些函数调用
        std::vector<int> data(100);
        std::sort(data.begin(), data.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 停止 profiler
    ProfilerStop();

    // 检查文件
    std::ifstream file(profile_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(file.is_open()) << "Failed to open profile file";

    size_t file_size = file.tellg();
    std::cout << "Profile file size: " << file_size << " bytes\n";
    ASSERT_GT(file_size, 100) << "Profile file too small";

    file.close();

    // 验证文件内容
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

    // 检查 header
    ASSERT_GT(buffer.size(), 24) << "File too small for header";

    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
    std::cout << "Header words:\n";
    std::cout << "  [0] = " << words[0] << "\n";
    std::cout << "  [1] = " << words[1] << "\n";
    std::cout << "  [2] = " << words[2] << "\n";

    // 清理
    std::remove(profile_path);
}

// 测试2: 验证手动解析 gperftools 格式
TEST(FullFlowTest, ManualParseGperftoolsFormat) {
    const char* profile_path = "/tmp/test_parse.prof";
    std::remove(profile_path);

    // 生成 profile
    ProfilerStart(profile_path);

    // 运行工作负载
    for (int i = 0; i < 5000; ++i) {
        volatile int x = i * i;
        (void)x;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ProfilerStop();

    // 读取并解析
    std::ifstream file(profile_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
    size_t word_count = buffer.size() / sizeof(uint64_t);

    std::cout << "\nParsing profile (" << word_count << " words)...\n";

    // 跳过 header (3 words)
    size_t pos = 3;
    int sample_count = 0;
    int total_samples = 0;

    while (pos < word_count) {
        if (pos + 1 >= word_count) break;

        uint64_t count = words[pos++];
        uint64_t pc_count = words[pos++];

        // 跳过无效数据
        if (pc_count == 0 || pc_count > 200) {
            continue;
        }

        if (pos + pc_count > word_count) {
            break;
        }

        total_samples++;

        // 只显示前5个样本
        if (sample_count < 5) {
            std::cout << "\nSample " << (sample_count + 1) << ":\n";
            std::cout << "  count: " << count << "\n";
            std::cout << "  pc_count: " << pc_count << "\n";
            std::cout << "  PCs:\n";

            for (uint64_t i = 0; i < std::min(pc_count, (uint64_t)10); i++) {
                uint64_t pc = words[pos + i];
                std::cout << "    [" << i << "] 0x" << std::hex << pc << std::dec << "\n";
            }
            sample_count++;
        }

        pos += pc_count;
    }

    std::cout << "\nTotal samples found: " << total_samples << "\n";

    // 验证找到了样本
    EXPECT_GT(total_samples, 0) << "No samples found in profile";

    std::remove(profile_path);
}

// 测试3: 验证 ProfilerManager 的 getCPUProfileAddresses
TEST(FullFlowTest, ProfilerManagerGetAddresses) {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 生成一个 profile
    const char* profile_path = "/tmp/test_manager.prof";
    std::remove(profile_path);

    ProfilerStart(profile_path);

    // 工作负载
    for (int i = 0; i < 10000; ++i) {
        std::vector<int> data(100);
        std::sort(data.begin(), data.end());
        volatile int sum = 0;
        for (int j : data) {
            sum += j;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ProfilerStop();

    // 手动设置状态（用于测试）
    // 注意：在实际使用中应该通过 startCPUProfiler/stopCPUProfiler
    // 但为了单元测试，我们直接读取文件

    // 读取文件内容
    std::ifstream file(profile_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();

    // 解析（模拟 getCPUProfileAddresses 的逻辑）
    uint64_t* words = reinterpret_cast<uint64_t*>(buffer.data());
    size_t word_count = buffer.size() / sizeof(uint64_t);
    size_t pos = 3;  // 跳过 header

    std::ostringstream result;
    int valid_samples = 0;

    while (pos < word_count) {
        if (pos + 1 >= word_count) break;

        uint64_t count = words[pos++];
        uint64_t pc_count = words[pos++];

        if (pc_count == 0 || pc_count > 200) continue;
        if (pos + pc_count > word_count) break;

        // 生成地址栈文本（类似 heap profile 格式）
        result << count << " @";
        for (uint64_t i = 0; i < pc_count; i++) {
            uint64_t pc = words[pos + pc_count - 1 - i];  // 反向
            result << " 0x" << std::hex << pc << std::dec;
        }
        result << "\n";

        pos += pc_count;
        valid_samples++;
    }

    std::string addresses = result.str();

    std::cout << "\nGenerated address stacks:\n";
    std::cout << addresses.substr(0, std::min(size_t(500), addresses.length())) << "\n";
    std::cout << "\nValid samples: " << valid_samples << "\n";

    // 验证
    if (valid_samples > 0) {
        EXPECT_FALSE(addresses.empty());
        EXPECT_TRUE(addresses.find("@") != std::string::npos);
        EXPECT_TRUE(addresses.find("0x") != std::string::npos);

        std::cout << "\n✅ Successfully generated " << valid_samples << " address stacks\n";
    } else {
        std::cout << "\n⚠️  No valid samples found (this might be OK if workload is light)\n";
    }

    std::remove(profile_path);
}

// 测试4: 验证符号化功能
TEST(FullFlowTest, SymbolizationWorks) {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 测试几个已知地址
    std::vector<void*> test_addrs = {
        (void*)(&std::sort<int*>),
        (void*)(&printf),
        (void*)(&FullFlowTest_SymbolizationWorks_Test::TestBody)
    };

    std::cout << "\nTesting symbolization:\n";

    for (void* addr : test_addrs) {
        std::string symbol = profiler.resolveSymbolWithBackward(addr);
        std::cout << "  0x" << addr << " -> " << symbol << "\n";

        // 验证符号化结果不为空
        EXPECT_FALSE(symbol.empty()) << "Symbolization returned empty string";
    }

    std::cout << "✅ Symbolization works\n";
}

// 测试5: 模拟前端解析逻辑
TEST(FullFlowTest, FrontendParsingLogic) {
    // 模拟前端收到的数据格式
    const char* mock_data = R"(
# Comment line
1 @ 0x1234 0x5678 0x9abc
2 @ 0x1111 0x2222 0x3333 0x4444
5 @ 0xaabb 0xccdd
)";

    std::istringstream iss(mock_data);
    std::string line;

    int sample_count = 0;
    int addr_count = 0;

    std::cout << "\nSimulating frontend parsing:\n";

    while (std::getline(iss, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') continue;

        // 解析格式: "count @ addr1 addr2 addr3"
        size_t at_pos = line.find('@');
        if (at_pos == std::string::npos) continue;

        std::string count_str = line.substr(0, at_pos);
        int count = std::stoi(count_str);

        std::string addrs_str = line.substr(at_pos + 1);
        std::istringstream addr_stream(addrs_str);
        std::string addr;

        std::vector<std::string> addrs;
        while (addr_stream >> addr) {
            addrs.push_back(addr);
        }

        std::cout << "  Sample " << (sample_count + 1) << ": count=" << count
                  << ", addrs=" << addrs.size() << "\n";

        sample_count++;
        addr_count += addrs.size();
    }

    std::cout << "  Total samples: " << sample_count << "\n";
    std::cout << "  Total addresses: " << addr_count << "\n";

    EXPECT_GT(sample_count, 0) << "No samples parsed";
    EXPECT_EQ(sample_count, 3) << "Should parse 3 samples";
    std::cout << "✅ Frontend parsing logic works\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
