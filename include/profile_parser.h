#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace profiler {

// Profile 解析器
// 使用 protobuf 解析 gperftools 生成的 profile 文件
class ProfileParser {
public:
    // 解析 profile 文件并生成 collapsed 格式
    static std::string parseToCollapsed(const std::string& profile_path);

    // 解析 profile 文件并生成 JSON 格式的火焰图数据
    static std::string parseToJSON(const std::string& profile_path);

    // 获取 profile 中的所有样本
    struct Sample {
        std::vector<uint64_t> location_ids;  // 调用栈的位置 ID
        std::vector<int64_t> values;          // 采样值
    };

    // 函数信息
    struct Function {
        uint64_t id;
        std::string name;
        std::string filename;
        int64_t start_line;
    };

    // 位置信息（包含地址和行号）
    struct Location {
        uint64_t id;
        uint64_t address;
        uint64_t mapping_id;
        struct Line {
            uint64_t function_id;
            int64_t line;
        };
        std::vector<Line> lines;
    };

private:
    // 从 profile 中提取函数名
    static std::string extractFunctionName(
        const std::map<uint64_t, Location>& locations,
        const std::map<uint64_t, Function>& functions,
        const std::vector<uint64_t>& location_ids);

    // 解析地址为函数名（使用 addr2line）
    static std::string resolveAddress(uint64_t address);

    // 执行命令并获取输出
    static bool executeCommand(const std::string& cmd, std::string& output);
};

} // namespace profiler
