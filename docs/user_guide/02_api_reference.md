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

**说明**: 析构时停止正在运行的 **CPU** profiler，并恢复构造函数之外由本对象安装的信号处理器。

**heap profiler 也会被停止**：析构函数调用 `HeapProfilerStop()`（`ProfilerManager::~ProfilerManager`），因为 heap profiler 存活在**进程级**的 tcmalloc 状态里——不停止的话，对象销毁后它仍会记录并 dump。

> 该问题曾存在（析构里只调用了查询函数 `IsHeapProfilerRunning()`，其返回值被丢弃），现已修正并有回归测试 `ProfilerLifecycleTest.HeapProfilerStopsOnDestruction` 覆盖。

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
| `handleCpuChart` | `HandlerResponse handleCpuChart(const ChartOptions&)` | CPU 采样并出图 |
| `handleHeapChart` | `HandlerResponse handleHeapChart(const ChartOptions&)` | Heap 窗口式采集并出图 |
| `handleGrowthChart` | `HandlerResponse handleGrowthChart(const ChartOptions&)` | Growth 采集并出图 |
| `handlePprofProfile` | `HandlerResponse handlePprofProfile(int seconds)` | 标准 pprof CPU profile (二进制) |
| `handlePprofHeap` | `HandlerResponse handlePprofHeap()` | 标准 pprof heap profile |
| `handlePprofGrowth` | `HandlerResponse handlePprofGrowth()` | 标准 pprof growth profile |
| `handlePprofSymbol` | `HandlerResponse handlePprofSymbol(const std::string& body)` | 符号化接口 (POST) |
| `handleThreadStacks` | `HandlerResponse handleThreadStacks()` | 线程调用栈 |

### ChartOptions

`handleCpuChart` / `handleHeapChart` / `handleGrowthChart` 共用同一个选项结构：

```cpp
struct ChartOptions {
    ChartRenderer renderer = ChartRenderer::FlameGraph;  // FlameGraph 或 CallGraph
    int duration = 10;        // 采集窗口（秒），内部钳制到 1..300
    bool inline_display = true;  // true 不发 Content-Disposition；false 强制下载
};
```

HTTP 层的 `renderer` 取值 `flamegraph` / `callgraph` 由此处的 `ChartRenderer` 决定
（`callgraph` 即 pprof 脚本经 graphviz 画出的调用图）。`handleHeapSnapshot(options, as_profile)`
另有一个 `as_profile` 开关：true 返回原始 profile 文本，false 按 `options.renderer` 出图。

### 使用示例

```cpp
#include "profiler/http_handlers.h"
#include "profiler_manager.h"

profiler::ProfilerManager profiler;
profiler::ProfilerHttpHandlers handlers(profiler);

// 调用任意 handler
profiler::ChartOptions options;   // renderer / duration / inline_display
profiler::HandlerResponse resp = handlers.handleCpuChart(options);

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
    bool is_running;         // 是否正在运行
    std::string output_path; // 输出文件路径
    uint64_t start_time;     // 开始时间戳（Unix 时间，**毫秒**）
    uint64_t duration;       // 已运行时长（**毫秒**，非秒）
};
```

> ⚠️ `start_time` 与 `duration` 的单位都是**毫秒**（见 `ProfilerState` 的字段注释），HTTP 层 `/api/status` 返回的 JSON 键名也相应为 `duration_ms`。
>
> 注意与**采集窗口**区分：`analyzeCPUProfile(duration)`、`ChartOptions::duration` 里的 `duration` 是**秒**，而 `ProfilerState::duration` 是**毫秒**。两者同名不同单位。

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
- 默认 sink 按级别分流：`Trace`/`Debug`/`Info` 写 **stdout**，`Warning`/`Error`/`Fatal` 写 **stderr**（`LogSink` 默认实现中的级别判断）
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
- `output_type`: 渲染方式（`"flamegraph"` 或 `"pprof"`）

> 这是 **C++ 层**的形参。HTTP 层的等价参数叫 `renderer`，取值 `flamegraph` / `callgraph`
> （见 README 的端点说明）——`callgraph` 对应这里的 `"pprof"`。

**返回值**: SVG 字符串

**说明**: 便捷方法，自动完成：启动 → 采样 → 停止 → 生成 SVG

---

### getRawCPUProfile

获取原始 CPU profile 数据（二进制格式）。

```cpp
std::string getRawCPUProfile(int seconds);
```

**返回值**: 原始 profile 二进制数据（gperftools 格式，兼容 Go pprof）

**说明**: CPU 采样是独占的，因此以下两种情况都会**拒绝**并返回**空字符串**：

- 已有另一个请求正在采样（`analyzeCPUProfile()` / `getRawCPUProfile()` 互斥）
- 宿主已用 `startCPUProfiler()` 开了会话

宿主用 `startCPUProfiler()` 打开的会话**归调用方所有，不会被本函数停止或接管**。调用前可用
`isProfilerRunning(ProfilerType::CPU)` 判断；HTTP 层则通过 `isCpuProfilerBusy()` 区分并返回
`500`（`/pprof/profile`，Go 风格）或 `409`（`/api/pprof/cpu`）。

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

采集 Heap 并生成图表 SVG。

```cpp
std::string analyzeHeapProfile(int duration = 1, const std::string& output_type = "flamegraph");
```

**参数**:
- `duration`: 采集窗口（秒），默认 **1**，内部钳制到 1..300
- `output_type`: 渲染方式（`"flamegraph"` 或 `"pprof"`）

**返回值**: SVG 字符串；失败时返回 `{"error":"..."}` JSON 字符串

**说明**: `duration` 是**采集窗口**，不是采样率。gperftools 的 heap profiling 按分配驱动：
`HeapProfilerStart()` 开始记录，`HeapProfilerDump()` 写出快照。窗口决定**覆盖多久的分配**，
采样率则由 `TCMALLOC_SAMPLE_PARAMETER` 在进程启动前固定，运行时不可调。

> ⚠️ **C++ 层默认 1 秒，HTTP 层默认 10 秒**。HTTP 层用更长默认值是实测结果：短窗口经常
> 采不到足够的分配而渲染失败。直接调用此函数时，分配稀疏的进程请显式传入更大的
> `duration`。

**记录的是"窗口内发生的分配"，不是"当前堆里的内存"** —— 这是最容易误解的一点：

| 情况 | 是否出现在结果里 |
|------|------------------|
| profiler 启动**之前**就已分配、期间一直存活的内存 | ❌ 不出现（实测：启动前 8MB + 期间 3MB → dump 只有 3MB） |
| 窗口内 malloc 后立即 free 的内存 | ✅ 出现（记录的是分配事件，不是 dump 时刻的状态） |
| 多个分析会话 | 各自独立、不累计（实测：2MB 会话 + 3MB 会话 → 第二次只有 3MB） |

因此：

- 进程内存虽高但**已停止分配**时，结果会是空的并返回
  `{"error": "No heap profile data was produced ..."}` —— 这是正确行为，不是 bug。
  这种场景应改用**状态式**的 `getRawHeapSample()`（即 `/pprof/heap`）
- **不需要** `TCMALLOC_SAMPLE_PARAMETER`：这条路径由
  `HEAP_PROFILE_ALLOCATION_INTERVAL`（默认 1MB）、`HEAP_PROFILE_INUSE_INTERVAL`（默认 512KB）控制。
  作为对比，`getRawHeapSample()` **必需**该变量（它走 `GetHeapSample()`）
- 想把窗口拉长（分配稀疏的进程），请直接用 `startHeapProfiler()` / `stopHeapProfiler()` 掌控时机

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

获取所有线程的调用堆栈，`/api/thread/stacks` 即调用它。

```cpp
std::string getThreadCallStacks();
```

**输出格式**：

```
Thread Call Stacks (via Signal Handler)
=========================================

Total threads captured: 2

Thread 1234 (DrogonIoLoop):
  Frames: 13
    #0 profiler::v0_1_0::ProfilerManager::signalHandler()
    #1 __restore_rt
    ...
    #5 epoll_wait            ← 该线程阻塞在这里
```

- **线程名**取自 `/proc/<tid>/comm`（内核截断到 15 字符，与 `ps`/`top` 显示一致）。只有 tid
  时输出 `Thread 1234:`，读日志时几乎无法分辨是哪个线程，因此带上名字。
- **阻塞点**是跳过信号捕获机制帧后的第一个真实帧（栈顶前几帧恒为
  `signalHandler`/`__restore_rt`/`__syscall_cancel_arch`，它们属于捕获手段而非线程状态）。
- 正在执行本请求的线程**不会**出现在结果里：信号只能采集其他线程，执行中的线程无法被抓取。
  这也是可接受的——正在运行的线程本来就没有"卡住"。

**说明**: 逐个地址调用 `symbolizeAddress()` 做符号化，该函数**优先使用 Abseil**（`absl::Symbolize`），失败后再回退到 `resolveSymbolWithBackward()`。

---

## 符号化 API

### resolveSymbolWithBackward

使用 backward-cpp 将地址符号化。

```cpp
std::string resolveSymbolWithBackward(void* address);
```

**说明**: 实际的符号化链及顺序为（`src/symbolize.cpp` 的 `symbolize()` 内，按顺序尝试）：

1. `absl::Symbolize` — 最可靠，命中即返回函数名（源文件记为 `??`、行号为 0）
2. `dladdr` + `abi::__cxa_demangle` — 回退，可拿到所在模块名
3. **backward-cpp** — 再回退，可解析出源文件与行号
4. 全部失败时返回原始地址

> 注意顺序：backward-cpp 是**最后**的回退项，而不是首选。

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

**说明**: 默认使用 `SIGUSR1`。推荐在第一次捕获线程栈之前调用；若处理器已安装后再调用，实现会先恢复旧处理器并打印一条 `[WARN]` 日志，然后切换（`ProfilerManager::setStackCaptureSignal()`）。

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

## Drogon Adapter API

### registerDrogonHandlers

使用 Drogon 时的一键注册函数。

```cpp
void registerDrogonHandlers(profiler::ProfilerManager& profiler);
```

**说明**: 注册所有 profiling 端点到 Drogon 全局 app。需要在链接时加入 `profiler_web` 目标。

**示例**:
```cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <drogon/drogon.h>

int main() {
    profiler::ProfilerManager profiler;
    profiler::registerDrogonHandlers(profiler);
    drogon::app().addListener("0.0.0.0", 8080).run();
}
```

---

## 线程安全

所有公共 API 都可在任意线程调用，但存在一条**功能层面的互斥约束**：

**同一时刻只能有一个 CPU 采样会话。** 全部三个入口（`startCPUProfiler()`、`getRawCPUProfile()`、
`analyzeCPUProfile()`）共用同一把进程级标记 `cpu_profiling_in_progress_`，并采用
**先原子认领、认领失败即拒绝**的语义：

- 后来者不会被排队，也不会打断先来者
- 宿主用 `startCPUProfiler()` 打开的会话**不会被任何 HTTP 请求抢占**
- 并发调用时恰好一个成功，其余立即失败（`analyzeCPUProfile()` 返回 `{"error":"cpu profiling already in use"}`，`getRawCPUProfile()` 返回空串）

判断当前是否忙，用 `isProfilerRunning(ProfilerType::CPU)`；HTTP 层用 `isCpuProfilerBusy()`，
它会同时考虑"已有请求在采样"和"宿主占用了会话"两种情况。

Heap 侧同理：`analyzeHeapProfile()` 与 `startHeapProfiler()` 互斥，谁先占谁赢。
`/api/pprof/growth` 不占用任何会话，不受限制。

---

## 错误处理

- 大多数控制类方法（`start*` / `stop*`）返回 `bool` 表示成功/失败
- 原始数据获取方法（`getRawCPUProfile`、`getRawHeapSample`、`getRawHeapGrowthStacks`、`getThreadCallStacks`）在失败时返回**空字符串**
- ⚠️ 但 `analyzeCPUProfile()` / `analyzeHeapProfile()` 在失败时返回的是形如 `{"error":"..."}` 的 **JSON 字符串**，而非空串（`analyzeCPUProfile()` / `analyzeHeapProfile()` 的各个失败分支）。HTTP 层正是靠 `{"` 前缀识别它并转成 500（`profiler::internal::isJsonError()`，见 `src/internal/result_parsing.h`）——自行调用这两个 API 时需要做同样的判断
- `HandlerResponse::error()` 返回包含错误信息的 JSON 响应

---

## 更多信息

- 查看 [集成示例](03_integration_examples.md) 了解更多使用场景
- 遇到问题？查看 [故障排除指南](04_troubleshooting.md)
