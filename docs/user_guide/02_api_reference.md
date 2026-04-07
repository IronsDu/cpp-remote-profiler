# API 参考手册

本文档提供了 C++ Remote Profiler 所有公共 API 的详细参考。

## 目录
- [ProfilerManager](#profilermanager)
- [ProfilerHttpHandlers](#profilerhttphandlers)
- [HandlerResponse](#handlerresponse)
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

`ProfilerManager` 是 C++ Remote Profiler 的核心类。

### 构造函数

```cpp
ProfilerManager();
```

**说明**: 创建一个新的 ProfilerManager 实例。不再使用单例模式，用户可以自由管理实例生命周期。

**示例**:
```cpp
profiler::ProfilerManager profiler;
```

### 析构函数

```cpp
~ProfilerManager();
```

**说明**: 析构时自动清理资源，包括停止正在运行的 profiler 和恢复信号处理器。

---

## ProfilerHttpHandlers

`ProfilerHttpHandlers` 提供框架无关的 HTTP 端点处理器。每个处理器返回 `HandlerResponse` 结构体，用户可以用任意 Web 框架包装响应。

### 构造函数

```cpp
explicit ProfilerHttpHandlers(ProfilerManager& profiler);
```

**参数**: `profiler` - ProfilerManager 实例引用

### 端点处理器方法

| 方法 | 签名 | 说明 |
|------|------|------|
| `handleStatus` | `HandlerResponse handleStatus()` | 返回所有 profiler 状态 (JSON) |
| `handleCpuAnalyze` | `HandlerResponse handleCpuAnalyze(int duration, const std::string& output_type)` | CPU 分析，返回 SVG |
| `handleCpuSvgRaw` | `HandlerResponse handleCpuSvgRaw(int duration)` | CPU 原始 SVG (pprof 生成) |
| `handleCpuFlamegraphRaw` | `HandlerResponse handleCpuFlamegraphRaw(int duration)` | CPU FlameGraph SVG |
| `handleHeapAnalyze` | `HandlerResponse handleHeapAnalyze(const std::string& output_type)` | Heap 分析，返回 SVG |
| `handleHeapSvgRaw` | `HandlerResponse handleHeapSvgRaw()` | Heap 原始 SVG |
| `handleHeapFlamegraphRaw` | `HandlerResponse handleHeapFlamegraphRaw()` | Heap FlameGraph SVG |
| `handleGrowthAnalyze` | `HandlerResponse handleGrowthAnalyze(const std::string& output_type)` | Growth 分析，返回 SVG |
| `handleGrowthSvgRaw` | `HandlerResponse handleGrowthSvgRaw()` | Growth 原始 SVG |
| `handleGrowthFlamegraphRaw` | `HandlerResponse handleGrowthFlamegraphRaw()` | Growth FlameGraph SVG |
| `handlePprofProfile` | `HandlerResponse handlePprofProfile(int seconds)` | 标准 pprof CPU profile (二进制) |
| `handlePprofHeap` | `HandlerResponse handlePprofHeap()` | 标准 pprof heap profile |
| `handlePprofGrowth` | `HandlerResponse handlePprofGrowth()` | 标准 pprof growth profile |
| `handlePprofSymbol` | `HandlerResponse handlePprofSymbol(const std::string& body)` | 符号化接口 (POST) |
| `handleThreadStacks` | `HandlerResponse handleThreadStacks()` | 线程调用栈 |

### 使用示例

```cpp
#include "profiler/http_handlers.h"
#include "profiler_manager.h"

profiler::ProfilerManager profiler;
profiler::ProfilerHttpHandlers handlers(profiler);

// 调用任意 handler
profiler::HandlerResponse resp = handlers.handleCpuAnalyze(10, "flamegraph");

// resp.status, resp.content_type, resp.body, resp.headers
// 用你自己的 Web 框架包装这些数据
```

---

## HandlerResponse

框架无关的 HTTP 响应结构体。

```cpp
struct HandlerResponse {
    int status = 200;
    std::string content_type = "text/plain";
    std::string body;
    std::map<std::string, std::string> headers;

    // 便捷工厂方法
    static HandlerResponse html(const std::string& content);
    static HandlerResponse json(const std::string& content);
    static HandlerResponse svg(const std::string& content);
    static HandlerResponse text(const std::string& content);
    static HandlerResponse binary(const std::string& data, const std::string& filename);
    static HandlerResponse error(int status, const std::string& message);
};
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

    virtual void log(LogLevel level,
                     const char* file,
                     int line,
                     const char* function,
                     const char* message) = 0;

    virtual void flush() {}
};
```

**说明**:
- 实现自定义 sink 后，通过 `profiler.setLogSink()` 注入
- 设置自定义 sink 后，默认的 stderr 输出将被替换
- 所有参数的生命周期仅在 `log()` 调用期间有效，如需保留请复制

---

### setLogSink

设置自定义日志 sink（ProfilerManager 实例方法）。

```cpp
void setLogSink(std::shared_ptr<LogSink> sink);
```

**参数**:
- `sink`: 自定义 LogSink 的 shared_ptr，传 `nullptr` 恢复默认 sink

**说明**:
- 设置后，profiler 的所有日志将输出到自定义 sink
- 默认 sink 输出到 stderr（使用 `std::cerr`）
- 设置 `nullptr` 可恢复默认行为

**示例**:
```cpp
#include <profiler/log_sink.h>

class MyAppLogSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel level, const char* file, int line,
             const char* function, const char* message) override {
        // 转发到应用的日志系统
        MY_APP_LOG("[Profiler] {}:{} - {}", file, line, message);
    }
};

profiler::ProfilerManager profiler;
profiler.setLogSink(std::make_shared<MyAppLogSink>());
```

---

### setLogLevel

设置最小日志级别（ProfilerManager 实例方法）。

```cpp
void setLogLevel(LogLevel level);
```

**示例**:
```cpp
profiler::ProfilerManager profiler;
profiler.setLogLevel(profiler::LogLevel::Debug);
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

**示例**:
```cpp
profiler::ProfilerManager profiler;

if (profiler.startCPUProfiler("/tmp/my_profile.prof")) {
    std::cout << "CPU profiler 启动成功" << std::endl;
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
- `false`: 停止失败

---

### analyzeCPUProfile

采样并生成火焰图 SVG。

```cpp
std::string analyzeCPUProfile(int duration, const std::string& output_type = "flamegraph");
```

**参数**:
- `duration`: 采样时长（秒）
- `output_type`: 输出类型（"flamegraph" 或 "pprof"）

**返回值**: SVG 字符串

**说明**: 便捷方法，自动完成：启动 → 采样 → 停止 → 生成 SVG

---

### getRawCPUProfile

获取原始 CPU profile 数据（二进制格式）。

```cpp
std::string getRawCPUProfile(int seconds);
```

**返回值**: 原始 profile 二进制数据（gperftools 格式，兼容 Go pprof）

---

## Heap Profiling API

### startHeapProfiler

启动 Heap profiler。

```cpp
bool startHeapProfiler(const std::string& output_path = "heap.prof");
```

**重要**: 需要设置 `TCMALLOC_SAMPLE_PARAMETER` 环境变量，必须链接 tcmalloc 库。

---

### stopHeapProfiler

停止 Heap profiler。

```cpp
bool stopHeapProfiler();
```

---

### analyzeHeapProfile

采样并生成 Heap 火焰图 SVG。

```cpp
std::string analyzeHeapProfile(int duration, const std::string& output_type = "flamegraph");
```

---

### getRawHeapSample

获取原始 heap 采样数据。

```cpp
std::string getRawHeapSample();
```

**返回值**: heap 采样文本数据（pprof 兼容格式）

---

### getRawHeapGrowthStacks

获取堆增长堆栈数据。

```cpp
std::string getRawHeapGrowthStacks();
```

**说明**: 不需要 `TCMALLOC_SAMPLE_PARAMETER` 环境变量，即时获取。

---

## 线程堆栈 API

### getThreadCallStacks

获取完整线程调用堆栈。

```cpp
std::string getThreadCallStacks();
```

**说明**: 使用 backward-cpp 进行详细符号化。

---

## 符号化 API

### resolveSymbolWithBackward

使用 backward-cpp 将地址符号化。

```cpp
std::string resolveSymbolWithBackward(void* address);
```

**说明**: 多层符号化策略：backward-cpp → dladdr → addr2line → 原始地址

---

## 工具方法

### getProfilerState

```cpp
ProfilerState getProfilerState(ProfilerType type) const;
```

### isProfilerRunning

```cpp
bool isProfilerRunning(ProfilerType type) const;
```

### executeCommand

```cpp
bool executeCommand(const std::string& cmd, std::string& output);
```

### getExecutablePath

```cpp
std::string getExecutablePath();
```

---

## 信号配置

### setStackCaptureSignal

```cpp
static void setStackCaptureSignal(int signal);
```

**说明**: 必须在第一次使用 profiler 之前调用，默认使用 SIGUSR1。

```cpp
profiler::ProfilerManager::setStackCaptureSignal(SIGUSR2);
profiler::ProfilerManager profiler;
```

### getStackCaptureSignal

```cpp
static int getStackCaptureSignal();
```

### setSignalChaining

```cpp
static void setSignalChaining(bool enable);
```

**说明**: 如果启用，profiler 处理信号后会调用旧的信号处理器。

---

## Web Server API (Drogon 便捷函数)

### registerHttpHandlers

使用 Drogon 时的一键注册函数。

```cpp
void registerHttpHandlers(profiler::ProfilerManager& profiler);
```

**说明**: 注册所有 profiling 端点到 Drogon 全局 app。需要在链接时加入 `profiler_web` 目标。

**示例**:
```cpp
#include "profiler_manager.h"
#include "web_server.h"
#include <drogon/drogon.h>

int main() {
    profiler::ProfilerManager profiler;
    profiler::registerHttpHandlers(profiler);
    drogon::app().addListener("0.0.0.0", 8080).run();
}
```

---

## 线程安全

所有公共 API 都是线程安全的，可以多线程同时调用。

---

## 错误处理

- 大多数方法返回 `bool` 表示成功/失败
- 字符串方法在失败时返回空字符串
- `HandlerResponse::error()` 返回包含错误信息的 JSON 响应

---

## 更多信息

- 💡 查看 [集成示例](03_integration_examples.md) 了解更多使用场景
- 🔧 遇到问题？查看 [故障排除指南](04_troubleshooting.md)
