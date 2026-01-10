#include "symbolize.h"
#include <backward.hpp>
#include <absl/debugging/symbolize.h>
#include <sstream>
#include <algorithm>
#include <dlfcn.h>
#include <cxxabi.h>

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

    // 首先尝试使用 absl::Symbolize (最可靠)
    char symbol_buffer[512];
    if (absl::Symbolize(address, symbol_buffer, sizeof(symbol_buffer))) {
        SymbolizedFrame frame;
        frame.function_name = symbol_buffer;
        frame.source_file = "??";
        frame.line = 0;
        frame.is_inlined = false;
        frames.push_back(frame);
        return frames;
    }

    // 如果absl::Symbolize失败，尝试使用 dladdr
    Dl_info info;
    if (dladdr(address, &info)) {
        SymbolizedFrame frame;

        if (info.dli_sname) {
            // dladdr找到了符号
            frame.function_name = info.dli_sname;

            // 如果有C++函数名，尝试demangle（dladdr通常已经demangle了）
            int status = 0;
            char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                frame.function_name = demangled;
                free(demangled);
            }

            frame.source_file = info.dli_fname ? info.dli_fname : "??";
            frame.line = 0;
            frame.is_inlined = false;
            frames.push_back(frame);
            return frames;
        }
    }

    // 如果dladdr失败，尝试backward-cpp
    try {
        impl_->resolver_.load_addresses(&address, 1);

        backward::Trace trace;
        trace.addr = address;
        trace.idx = 0;

        backward::ResolvedTrace resolved = impl_->resolver_.resolve(trace);

        if (!resolved.source.function.empty() && resolved.source.function != "??") {
            SymbolizedFrame frame;
            frame.function_name = resolved.source.function;
            frame.source_file = resolved.source.filename;
            frame.line = resolved.source.line;
            frame.is_inlined = false;
            frames.push_back(frame);

            // 内联函数
            for (const auto& inlined : resolved.inliners) {
                if (!inlined.function.empty() && inlined.function != "??") {
                    SymbolizedFrame inline_frame;
                    inline_frame.function_name = inlined.function;
                    inline_frame.source_file = inlined.filename;
                    inline_frame.line = inlined.line;
                    inline_frame.is_inlined = true;
                    frames.push_back(inline_frame);
                }
            }
            return frames;
        }
    } catch (const std::exception& e) {
        // backward-cpp也失败，继续到下面的fallback
    }

    // 如果所有方法都失败，返回地址
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
