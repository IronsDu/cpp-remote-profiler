#pragma once

#include <memory>
#include <string>
#include <vector>

namespace profiler {

// 符号化结果
struct SymbolizedFrame {
    std::string function_name; // 函数名
    std::string source_file;   // 源文件路径
    unsigned int line = 0;     // 行号
    bool is_inlined = false;   // 是否是内联函数
};

// 符号化器接口
class Symbolizer {
  public:
    virtual ~Symbolizer() = default;

    // 符号化单个地址
    virtual std::vector<SymbolizedFrame> symbolize(void* address) = 0;

    // 符号化多个地址
    virtual std::vector<std::vector<SymbolizedFrame>> symbolizeBatch(const std::vector<void*>& addresses) = 0;
};

// Backward-cpp 符号化器实现
class BackwardSymbolizer : public Symbolizer {
  public:
    BackwardSymbolizer();
    ~BackwardSymbolizer() override;

    std::vector<SymbolizedFrame> symbolize(void* address) override;
    std::vector<std::vector<SymbolizedFrame>> symbolizeBatch(const std::vector<void*>& addresses) override;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 工厂函数
std::unique_ptr<Symbolizer> createSymbolizer();

} // namespace profiler
