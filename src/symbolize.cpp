#include "symbolize.h"
#include <backward.hpp>
#include <sstream>
#include <algorithm>

namespace profiler {

// BackwardSymbolizer 的内部实现
class BackwardSymbolizer::Impl {
public:
    backward::TraceResolver resolver_;

    Impl() : resolver_() {
        // TraceResolver will be initialized when needed
    }
};

BackwardSymbolizer::BackwardSymbolizer()
    : impl_(std::make_unique<Impl>()) {
}

BackwardSymbolizer::~BackwardSymbolizer() = default;

std::vector<SymbolizedFrame> BackwardSymbolizer::symbolize(void* address) {
    std::vector<SymbolizedFrame> frames;

    try {
        // 使用 load_addresses 加载单个地址
        impl_->resolver_.load_addresses(&address, 1);

        // 创建一个 Trace 对象并设置地址
        backward::Trace trace;
        trace.addr = address;
        trace.idx = 0;

        // 解析栈帧
        backward::ResolvedTrace resolved = impl_->resolver_.resolve(trace);

        // 主函数
        if (!resolved.source.function.empty() && resolved.source.function != "??") {
            SymbolizedFrame frame;
            frame.function_name = resolved.source.function;
            frame.source_file = resolved.source.filename;
            frame.line = resolved.source.line;
            frame.is_inlined = false;
            frames.push_back(frame);
        }

        // 内联函数（如果有的话）
        for (const auto& inlined : resolved.inliners) {
            if (!inlined.function.empty() && inlined.function != "??") {
                SymbolizedFrame frame;
                frame.function_name = inlined.function;
                frame.source_file = inlined.filename;
                frame.line = inlined.line;
                frame.is_inlined = true;
                frames.push_back(frame);
            }
        }
    } catch (const std::exception& e) {
        // 如果解析失败，返回地址
        SymbolizedFrame frame;
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<unsigned long long>(address);
        frame.function_name = oss.str();
        frame.source_file = "??";
        frame.line = 0;
        frame.is_inlined = false;
        frames.push_back(frame);
        return frames;
    }

    // 如果没有找到任何符号，返回地址
    if (frames.empty()) {
        SymbolizedFrame frame;
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<unsigned long long>(address);
        frame.function_name = oss.str();
        frame.source_file = "??";
        frame.line = 0;
        frame.is_inlined = false;
        frames.push_back(frame);
    }

    return frames;
}

std::vector<std::vector<SymbolizedFrame>> BackwardSymbolizer::symbolizeBatch(
    const std::vector<void*>& addresses) {
    std::vector<std::vector<SymbolizedFrame>> results;
    results.reserve(addresses.size());

    for (void* addr : addresses) {
        results.push_back(symbolize(addr));
    }

    return results;
}

// 工厂函数
std::unique_ptr<Symbolizer> createSymbolizer() {
    return std::make_unique<BackwardSymbolizer>();
}

} // namespace profiler
