#include "profile_parser.h"
#include "profile.pb.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace profiler {

bool ProfileParser::executeCommand(const std::string& cmd, std::string& output) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }

    try {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
    } catch (...) {
        pclose(pipe);
        return false;
    }
    pclose(pipe);
    return true;
}

std::string ProfileParser::resolveAddress(uint64_t address) {
    // 使用 addr2line 解析地址
    std::ostringstream cmd;
    cmd << "addr2line -e /proc/self/exe -f -C 0x" << std::hex << address << " 2>&1";

    std::string output;
    if (executeCommand(cmd.str(), output)) {
        // addr2line 输出格式: "function_name\nsource_file:line\n"
        size_t newline_pos = output.find('\n');
        if (newline_pos != std::string::npos) {
            std::string symbol = output.substr(0, newline_pos);
            // 如果 addr2line 返回的是 ??，说明解析失败
            if (symbol == "??" || symbol.empty()) {
                std::ostringstream addr_str;
                addr_str << "0x" << std::hex << address;
                return addr_str.str();
            }
            return symbol;
        }
        return output;
    }

    // 回退：返回原始地址
    std::ostringstream addr_str;
    addr_str << "0x" << std::hex << address;
    return addr_str.str();
}

std::string ProfileParser::extractFunctionName(
    const std::map<uint64_t, Location>& locations,
    const std::map<uint64_t, Function>& functions,
    const std::vector<uint64_t>& location_ids) {

    std::vector<std::string> stack_names;

    for (uint64_t loc_id : location_ids) {
        auto it = locations.find(loc_id);
        if (it != locations.end()) {
            const Location& loc = it->second;

            // 尝试从行号信息中获取函数名
            if (!loc.lines.empty()) {
                for (const auto& line : loc.lines) {
                    auto func_it = functions.find(line.function_id);
                    if (func_it != functions.end()) {
                        const Function& func = func_it->second;
                        if (!func.name.empty()) {
                            stack_names.push_back(func.name);
                        }
                    }
                }
            }

            // 如果没有函数名，尝试解析地址
            if (stack_names.empty() && loc.address != 0) {
                std::string symbol = resolveAddress(loc.address);
                if (!symbol.empty()) {
                    stack_names.push_back(symbol);
                }
            }
        }
    }

    // 构建调用栈字符串（从根到叶子）
    std::ostringstream stack_str;
    for (auto it = stack_names.rbegin(); it != stack_names.rend(); ++it) {
        if (!stack_str.str().empty()) {
            stack_str << ";";
        }
        stack_str << *it;
    }

    return stack_str.str();
}

std::string ProfileParser::parseToCollapsed(const std::string& profile_path) {
    perftools::profiles::Profile profile;
    std::ifstream input(profile_path, std::ios::binary);
    if (!input) {
        return "# Error: Cannot open profile file: " + profile_path + "\n";
    }

    if (!profile.ParseFromIstream(&input)) {
        return "# Error: Failed to parse profile file (invalid protobuf format)\n";
    }

    input.close();

    // 构建函数映射表
    std::map<uint64_t, Function> functions;
    for (const auto& func : profile.function()) {
        functions[func.id()] = {
            func.id(),
            func.has_name() ? profile.string_table()[func.name()] : "",
            func.has_filename() ? profile.string_table()[func.filename()] : "",
            func.start_line()
        };
    }

    // 构建位置映射表
    std::map<uint64_t, Location> locations;
    for (const auto& loc : profile.location()) {
        Location location{
            loc.id(),
            loc.address(),
            loc.mapping_id(),
            {}
        };
        for (const auto& line : loc.line()) {
            location.lines.push_back({line.function_id(), line.line()});
        }
        locations[loc.id()] = location;
    }

    // 聚合样本
    std::map<std::string, int64_t> stack_counts;
    int64_t total_samples = 0;

    for (const auto& sample : profile.sample()) {
        std::vector<uint64_t> loc_ids(sample.location_id().begin(),
                                       sample.location_id().end());
        std::string stack = extractFunctionName(locations, functions, loc_ids);

        if (!stack.empty()) {
            // 累加值（第一个值通常是采样数）
            if (!sample.value().empty()) {
                stack_counts[stack] += sample.value(0);
                total_samples += sample.value(0);
            } else {
                stack_counts[stack]++;
                total_samples++;
            }
        }
    }

    // 生成 collapsed 格式
    std::ostringstream result;
    result << "# collapsed stack traces\n";
    result << "# Total samples: " << total_samples << "\n";
    result << "# Unique stacks: " << stack_counts.size() << "\n";

    for (const auto& entry : stack_counts) {
        result << entry.first << " " << entry.second << "\n";
    }

    return result.str();
}

std::string ProfileParser::parseToJSON(const std::string& profile_path) {
    // TODO: 实现 JSON 格式输出
    return "{\n"
           "  \"error\": \"JSON output not yet implemented\"\n"
           "}\n";
}

} // namespace profiler
