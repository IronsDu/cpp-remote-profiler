# API 参考手册

本文档提供了 C++ Remote Profiler 所有公共 API 的详细参考。

## 目录
- [ProfilerManager](#profilermanager)
- [类型定义](#类型定义)
- [日志系统 API](#日志系统-api)
- [CPU Profiling API](#cpu-profiling-api)
- [Heap Profiling API](#heap-profiling-api)
- [线程堆栈 API](#线程堆栈-api)
- [符号化 API](#符号化-api)
- [工具方法](#工具方法)
- [信号配置](#信号配置)

---

## ProfilerManager

`ProfilerManager` 是 C++ Remote Profiler 的核心类，采用单例模式设计。

### 获取实例

```cpp
static ProfilerManager& getInstance();
```

**返回值**: ProfilerManager 的唯一实例引用

**示例**:
```cpp
auto& profiler = profiler::ProfilerManager::getInstance();
```

---

## 类型定义

### ProfilerType

Profiler 类型枚举。

```cpp
enum class ProfilerType {
    CPU,         // CPU 性能分析
    HEAP,        // 内存堆分析
    HEAP_GROWTH  // 堆增长分析
};
```

### ProfilerState

Profiler 状态结构。

```cpp
struct ProfilerState {
    bool is_running;        // 是否正在运行
    std::string output_path; // 输出文件路径
    uint64_t start_time;    // 开始时间戳（Unix 时间）
    uint64_t duration;      // 采样时长（秒）
};
```

### ThreadStackTrace

线程堆栈跟踪结构（用于内部实现）。

```cpp
struct ThreadStackTrace {
    pid_t tid;              // 线程 ID
    void* addresses[64];    // 调用栈地址数组
    int depth;              // 堆栈深度
    bool captured;          // 是否成功捕获
};
```

---

## 日志系统 API

profiler 库提供可配置的日志系统，允许用户将 profiler 的日志集成到自己的日志系统中。

### LogLevel

日志级别枚举。

```cpp
enum class LogLevel {
    Trace,    // 详细调试信息
    Debug,    // 调试信息
    Info,     // 一般信息（默认级别）
    Warning,  // 警告信息
    Error,    // 错误信息
    Fatal     // 致命错误
};
```

---

### LogSink

日志输出接口，用户可继承此类实现自定义日志输出。

```cpp
class LogSink {
public:
    virtual ~LogSink() = default;

    /// @brief 写入日志消息
    /// @param level 日志级别
    /// @param file 源文件名（可能为 nullptr）
    /// @param line 源行号
    /// @param function 函数名（可能为 nullptr）
    /// @param message 格式化后的日志消息
    virtual void log(LogLevel level,
                     const char* file,
                     int line,
                     const char* function,
                     const char* message) = 0;

    /// @brief 刷新缓冲区（可选实现）
    virtual void flush() {}
};
```

**说明**:
- 实现自定义 sink 后，通过 `setSink()` 注入到 profiler
- 设置自定义 sink 后，默认的 stderr 输出将被替换
- 所有参数的生命周期仅在 `log()` 调用期间有效，如需保留请复制

---

### setSink

设置自定义日志 sink。

```cpp
void setSink(std::shared_ptr<LogSink> sink);
```

**参数**:
- `sink`: 自定义 LogSink 的 shared_ptr，传 `nullptr` 恢复默认 sink

**说明**:
- 设置后，profiler 的所有日志将输出到自定义 sink
- 默认 sink 输出到 stderr（使用 spdlog）
- 设置 `nullptr` 可恢复默认行为

**示例**:
```cpp
#include <profiler/log_sink.h>
#include <profiler/logger.h>

// 自定义 sink：集成到应用日志系统
class MyAppLogSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel level, const char* file, int line,
             const char* function, const char* message) override {
        // 转发到应用的日志系统
        switch (level) {
            case profiler::LogLevel::Error:
                MY_APP_ERROR("[Profiler] {}:{} - {}", file, line, message);
                break;
            case profiler::LogLevel::Warning:
                MY_APP_WARN("[Profiler] {}:{} - {}", file, line, message);
                break;
            default:
                MY_APP_INFO("[Profiler] {}:{} - {}", file, line, message);
                break;
        }
    }
};

// 设置自定义 sink
profiler::setSink(std::make_shared<MyAppLogSink>());

// 恢复默认 sink
profiler::setSink(nullptr);
```

---

### setLogLevel

设置最小日志级别。

```cpp
void setLogLevel(LogLevel level);
```

**参数**:
- `level`: 最小日志级别，低于此级别的消息将被过滤

**说明**:
- 默认级别为 `LogLevel::Info`
- 日志级别从低到高：Trace < Debug < Info < Warning < Error < Fatal

**示例**:
```cpp
#include <profiler/logger.h>

// 启用调试日志
profiler::setLogLevel(profiler::LogLevel::Debug);

// 只显示错误和致命错误
profiler::setLogLevel(profiler::LogLevel::Error);

// 显示所有日志（包括 Trace）
profiler::setLogLevel(profiler::LogLevel::Trace);
```

---

### 完整示例：集成到 spdlog

```cpp
#include <profiler/log_sink.h>
#include <profiler/logger.h>
#include <spdlog/spdlog.h>

class SpdlogSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel level, const char* file, int line,
             const char* function, const char* message) override {
        // 映射日志级别
        spdlog::level::level_enum spdlog_level;
        switch (level) {
            case profiler::LogLevel::Trace:    spdlog_level = spdlog::level::trace; break;
            case profiler::LogLevel::Debug:    spdlog_level = spdlog::level::debug; break;
            case profiler::LogLevel::Info:     spdlog_level = spdlog::level::info; break;
            case profiler::LogLevel::Warning:  spdlog_level = spdlog::level::warn; break;
            case profiler::LogLevel::Error:    spdlog_level = spdlog::level::err; break;
            case profiler::LogLevel::Fatal:    spdlog_level = spdlog::level::critical; break;
        }

        // 输出到 spdlog
        spdlog::log(spdlog::level::info, "[Profiler] {}:{} - {}", file, line, message);
    }

    void flush() override {
        spdlog::default_logger()->flush();
    }
};

int main() {
    // 配置 spdlog
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    // 集成 profiler 日志到 spdlog
    profiler::setSink(std::make_shared<SpdlogSink>());
    profiler::setLogLevel(profiler::LogLevel::Debug);

    // 使用 profiler...
}
```

---

### 完整示例：完全禁用日志

```cpp
#include <profiler/log_sink.h>
#include <profiler/logger.h>

// 空 sink：丢弃所有日志
class NullSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel, const char*, int,
             const char*, const char*) override {
        // 什么都不做
    }
};

// 方法 1：使用空 sink
profiler::setSink(std::make_shared<NullSink>());

// 方法 2：设置日志级别为 Fatal（只显示致命错误）
profiler::setLogLevel(profiler::LogLevel::Fatal);
```

---

## CPU Profiling API

### startCPUProfiler

启动 CPU profiler。

```cpp
bool startCPUProfiler(const std::string& output_path = "cpu.prof");
```

**参数**:
- `output_path`: profile 文件输出路径（默认: "cpu.prof"）

**返回值**:
- `true`: 启动成功
- `false`: 启动失败（例如：profiler 已在运行）

**说明**:
- 使用 gperftools 的 `ProfilerStart()` 启动 CPU profiling
- 同一时间只能有一个 CPU profiler 在运行
- 采样频率由 gperftools 控制（通常 100 Hz，即每 10ms 采样一次）

**示例**:
```cpp
auto& profiler = profiler::ProfilerManager::getInstance();

if (profiler.startCPUProfiler("/tmp/my_profile.prof")) {
    std::cout << "CPU profiler 启动成功" << std::endl;
} else {
    std::cerr << "CPU profiler 启动失败" << std::endl;
}
```

---

### stopCPUProfiler

停止 CPU profiler。

```cpp
bool stopCPUProfiler();
```

**返回值**:
- `true`: 停止成功
- `false`: 停止失败（例如：profiler 未运行）

**示例**:
```cpp
profiler.startCPUProfiler();
// ... 运行需要分析的代码 ...
profiler.stopCPUProfiler();
std::cout << "Profiling 完成" << std::endl;
```

---

### analyzeCPUProfile

采样并生成火焰图 SVG。

```cpp
std::string analyzeCPUProfile(int duration, const std::string& output_type = "flamegraph");
```

**参数**:
- `duration`: 采样时长（秒）
- `output_type`: 输出类型（默认: "flamegraph"）

**返回值**: SVG 字符串

**说明**:
- 这是一个便捷方法，自动完成：启动 → 采样 → 停止 → 生成 SVG
- 内部使用 FlameGraph 工具生成火焰图
- 包含符号化处理，显示函数名而非地址

**示例**:
```cpp
// 采样 10 秒并生成火焰图
std::string svg = profiler.analyzeCPUProfile(10, "flamegraph");

// 保存到文件
std::ofstream out("cpu_flamegraph.svg");
out << svg;
out.close();
```

---

### getRawCPUProfile

获取原始 CPU profile 数据（二进制格式）。

```cpp
std::string getRawCPUProfile(int seconds);
```

**参数**:
- `seconds`: 采样时长（秒）

**返回值**: 原始 profile 二进制数据（gperftools 格式）

**说明**:
- 返回的数据兼容 Go pprof 工具
- 可用于保存到文件后用 pprof 分析

**示例**:
```cpp
// 采样 10 秒
std::string raw_data = profiler.getRawCPUProfile(10);

// 保存到文件
std::ofstream out("cpu.prof", std::ios::binary);
out.write(raw_data.data(), raw_data.size());
out.close();

// 然后可以用 pprof 分析
// go tool pprof -http=:8080 cpu.prof
```

---

## Heap Profiling API

### startHeapProfiler

启动 Heap profiler。

```cpp
bool startHeapProfiler(const std::string& output_path = "heap.prof");
```

**参数**:
- `output_path`: profile 文件输出路径（默认: "heap.prof"）

**返回值**:
- `true`: 启动成功
- `false`: 启动失败

**重要**:
- 需要设置 `TCMALLOC_SAMPLE_PARAMETER` 环境变量
- 必须链接 tcmalloc 库

**示例**:
```cpp
// 在程序启动时设置环境变量
setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 1);

auto& profiler = profiler::ProfilerManager::getInstance();
profiler.startHeapProfiler("/tmp/heap.prof");

// ... 分配内存 ...

profiler.stopHeapProfiler();
```

---

### stopHeapProfiler

停止 Heap profiler。

```cpp
bool stopHeapProfiler();
```

**返回值**:
- `true`: 停止成功
- `false`: 停止失败

---

### analyzeHeapProfile

采样并生成 Heap 火焰图 SVG。

```cpp
std::string analyzeHeapProfile(int duration, const std::string& output_type = "flamegraph");
```

**参数**:
- `duration`: 采样时长（秒）
- `output_type`: 输出类型（默认: "flamegraph"）

**返回值**: SVG 字符串

**说明**:
- 使用 tcmalloc 的 heap sampling 功能
- 显示内存分配热点

**示例**:
```cpp
std::string svg = profiler.analyzeHeapProfile(10);
std::ofstream out("heap_flamegraph.svg");
out << svg;
out.close();
```

---

### getRawHeapSample

获取原始 heap 采样数据。

```cpp
std::string getRawHeapSample();
```

**返回值**: heap 采样文本数据（pprof 兼容格式）

**说明**:
- 使用 `MallocExtension::GetHeapSample()` 获取当前 heap 状态
- 返回文本格式，可直接用 pprof 分析

**示例**:
```cpp
std::string heap_data = profiler.getRawHeapSample();
std::cout << heap_data << std::endl;
```

---

### getRawHeapGrowthStacks

获取堆增长堆栈数据。

```cpp
std::string getRawHeapGrowthStacks();
```

**返回值**: heap growth stacks 文本数据（pprof 兼容格式）

**说明**:
- 使用 `MallocExtension::GetHeapGrowthStacks()` API
- 不需要 `TCMALLOC_SAMPLE_PARAMETER` 环境变量
- 即时获取，无需采样时长

**示例**:
```cpp
std::string growth_data = profiler.getRawHeapGrowthStacks();
std::cout << "Heap growth stacks:\n" << growth_data << std::endl;
```

---

## 线程堆栈 API

### getThreadStacks

获取所有线程的调用堆栈。

```cpp
std::string getThreadStacks();
```

**返回值**: 线程堆栈文本数据

**说明**:
- 使用信号处理器安全地捕获所有线程的堆栈
- 自动符号化，显示函数名
- 包含线程 ID 和堆栈深度信息

**示例**:
```cpp
std::string stacks = profiler.getThreadStacks();
std::cout << "所有线程堆栈:\n" << stacks << std::endl;
```

**输出格式**:
```
Thread 12345 (depth: 5):
  #0 main
  #1 workerThread
  #2 processTask
  #3 compute
  #4 calculate

Thread 12346 (depth: 3):
  #0 main
  #1 ioThread
  #2 waitForData
```

---

### getThreadCallStacks

获取完整线程调用堆栈（使用 backward-cpp）。

```cpp
std::string getThreadCallStacks();
```

**返回值**: 格式化的线程堆栈字符串

**说明**:
- 使用 backward-cpp 库进行详细符号化
- 包含文件名和行号（如果有调试符号）

---

## 符号化 API

### resolveSymbolWithBackward

使用 backward-cpp 将地址符号化。

```cpp
std::string resolveSymbolWithBackward(void* address);
```

**参数**:
- `address`: 需要符号化的内存地址

**返回值**: 符号化后的字符串（包含函数名、文件名、行号）

**说明**:
- 多层符号化策略：
  1. backward-cpp (最详细)
  2. dladdr (动态链接信息)
  3. addr2line (符号表查询)
  4. 原始地址（降级显示）

**示例**:
```cpp
void* addr = some_function;
std::string symbol = profiler.resolveSymbolWithBackward(addr);
std::cout << "符号: " << symbol << std::endl;

// 输出示例:
// symbol_name(int, double) at /path/to/file.cpp:123
```

---

## 工具方法

### getProfilerState

获取 profiler 的当前状态。

```cpp
ProfilerState getProfilerState(ProfilerType type) const;
```

**参数**:
- `type`: Profiler 类型（CPU, HEAP, HEAP_GROWTH）

**返回值**: ProfilerState 结构

**示例**:
```cpp
auto state = profiler.getProfilerState(ProfilerType::CPU);
std::cout << "CPU profiler 运行中: " << state.is_running << std::endl;
std::cout << "输出文件: " << state.output_path << std::endl;
```

---

### isProfilerRunning

检查 profiler 是否正在运行。

```cpp
bool isProfilerRunning(ProfilerType type) const;
```

**参数**:
- `type`: Profiler 类型

**返回值**: `true` 如果正在运行，否则 `false`

**示例**:
```cpp
if (profiler.isProfilerRunning(ProfilerType::CPU)) {
    std::cout << "CPU profiling 正在进行中..." << std::endl;
}
```

---

### executeCommand

执行 shell 命令并获取输出。

```cpp
bool executeCommand(const std::string& cmd, std::string& output);
```

**参数**:
- `cmd`: 要执行的命令
- `output`: 输出参数（用于接收命令输出）

**返回值**: `true` 如果命令执行成功，否则 `false`

**示例**:
```cpp
std::string output;
if (profiler.executeCommand("ls -l", output)) {
    std::cout << "命令输出:\n" << output << std::endl;
}
```

---

### getExecutablePath

获取当前可执行文件的路径。

```cpp
std::string getExecutablePath();
```

**返回值**: 可执行文件的绝对路径

**示例**:
```cpp
std::string exe_path = profiler.getExecutablePath();
std::cout << "可执行文件: " << exe_path << std::endl;
```

---

## 信号配置

线程堆栈捕获使用信号处理器实现。如果你的程序已经使用了某些信号，可能需要配置。

### setStackCaptureSignal

设置用于堆栈捕获的信号。

```cpp
static void setStackCaptureSignal(int signal);
```

**参数**:
- `signal`: 信号编号（如 SIGUSR1, SIGUSR2, SIGRTMIN+3）

**说明**:
- 必须在第一次使用 profiler 之前调用
- 默认使用 SIGUSR1

**示例**:
```cpp
// 在 main 函数开始时设置
profiler::ProfilerManager::setStackCaptureSignal(SIGUSR2);
auto& profiler = profiler::ProfilerManager::getInstance();
```

---

### getStackCaptureSignal

获取当前用于堆栈捕获的信号。

```cpp
static int getStackCaptureSignal();
```

**返回值**: 当前使用的信号编号

**示例**:
```cpp
int signal = profiler::ProfilerManager::getStackCaptureSignal();
std::cout << "使用信号: " << signal << std::endl;
```

---

### setSignalChaining

启用/禁用信号链（调用旧的信号处理器）。

```cpp
static void setSignalChaining(bool enable);
```

**参数**:
- `enable`: `true` 启用信号链，`false` 禁用

**说明**:
- 如果启用，profiler 处理信号后会调用旧的信号处理器
- 适用于程序已有信号处理器的情况

**示例**:
```cpp
// 启用信号链
profiler::ProfilerManager::setSignalChaining(true);
```

---

## Web Server API

### registerHttpHandlers

注册所有 profiling 相关的 HTTP 端点。

```cpp
void registerHttpHandlers(profiler::ProfilerManager& profiler);
```

**参数**:
- `profiler`: ProfilerManager 实例

**说明**:
- 注册以下端点：

**标准 pprof 接口**：
  - `/pprof/profile` - CPU profile（返回原始文件，兼容 Go pprof）
  - `/pprof/heap` - Heap profile（返回原始文件，兼容 Go pprof）
  - `/pprof/growth` - Heap growth profile（返回原始文件，兼容 Go pprof）
  - `/pprof/symbol` - 符号化接口（POST，兼容 Go pprof）

**一键分析接口**：
  - `/api/cpu/analyze` - CPU 火焰图 SVG（浏览器显示）
  - `/api/heap/analyze` - Heap 火焰图 SVG（浏览器显示）
  - `/api/growth/analyze` - Growth 火焰图 SVG（浏览器显示）

**原始 SVG 下载接口**：
  - `/api/cpu/svg_raw` - CPU 原始 SVG（pprof 生成，触发下载）
  - `/api/heap/svg_raw` - Heap 原始 SVG（pprof 生成，触发下载）
  - `/api/growth/svg_raw` - Growth 原始 SVG（pprof 生成，触发下载）
  - `/api/cpu/flamegraph_raw` - CPU FlameGraph 原始 SVG（触发下载）
  - `/api/heap/flamegraph_raw` - Heap FlameGraph 原始 SVG（触发下载）
  - `/api/growth/flamegraph_raw` - Growth FlameGraph 原始 SVG（触发下载）

**其他接口**：
  - `/api/thread/stacks` - 线程堆栈
  - `/api/status` - 全局状态
  - `/` - Web 主界面
  - `/show_svg.html` - CPU 火焰图查看器
  - `/show_heap_svg.html` - Heap 火焰图查看器
  - `/show_growth_svg.html` - Growth 火焰图查看器

**示例**:
```cpp
#include <drogon/drogon.h>
#include "profiler_manager.h"
#include "web_server.h"

int main() {
    profiler::ProfilerManager& profiler = profiler::ProfilerManager::getInstance();

    // 注册所有 HTTP 处理器
    profiler::registerHttpHandlers(profiler);

    // 启动服务器
    drogon::app().addListener("0.0.0.0", 8080);
    drogon::app().run();

    return 0;
}
```

---

## 使用示例

### 完整示例：CPU Profiling

```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 检查状态
    if (!profiler.isProfilerRunning(profiler::ProfilerType::CPU)) {
        // 启动 profiler
        profiler.startCPUProfiler("my_app.prof");
        std::cout << "CPU profiler 已启动" << std::endl;
    }

    // 运行需要分析的代码
    doWork();

    // 停止 profiler
    profiler.stopCPUProfiler();
    std::cout << "Profiling 完成" << std::endl;

    return 0;
}
```

### 完整示例：Heap Profiling

```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    // 设置环境变量
    setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 1);

    auto& profiler = profiler::ProfilerManager::getInstance();

    // 启动 heap profiler
    profiler.startHeapProfiler("my_app_heap.prof");

    // 分配内存
    for (int i = 0; i < 1000; i++) {
        char* data = new char[1024];
        // 使用 data...
        delete[] data;
    }

    // 停止 profiler
    profiler.stopHeapProfiler();

    // 获取 heap 数据
    std::string heap_data = profiler.getRawHeapSample();
    std::cout << heap_data << std::endl;

    return 0;
}
```

---

## 线程安全

所有公共 API 都是线程安全的，可以多线程同时调用。

---

## 错误处理

- 大多数方法返回 `bool` 表示成功/失败
- 字符串方法在失败时返回空字符串
- 建议检查返回值以确保操作成功

---

## 更多信息

- 💡 查看 [集成示例](03_integration_examples.md) 了解更多使用场景
- 🔧 遇到问题？查看 [故障排除指南](04_troubleshooting.md)
