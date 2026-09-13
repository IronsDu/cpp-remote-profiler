# C++ Remote Profiler

类似 Go pprof 和 brpc pprof service 的 C++ 远程性能分析库：在被分析进程内嵌入 HTTP 接口，远程采集 CPU / Heap profile，并直接产出火焰图 SVG。

核心 profiling 基于 [gperftools](https://github.com/gperftools/gperftools)；Web 层基于 [Drogon](https://github.com/drogonframework/drogon)，**可选**。

**当前版本**: v0.1.0（开发阶段，API 可能随时变化，不建议用于生产环境）

## 目录

- [功能特性](#功能特性)
- [设计理念](#设计理念)
- [快速开始](#快速开始)
- [运行时依赖](#运行时依赖)
- [CMake 构建选项](#cmake-构建选项)
- [安装与集成](#安装与集成)
- [API 端点](#api-端点)
- [配置说明](#配置说明)
- [注意事项](#注意事项)
- [项目结构](#项目结构)
- [开发](#开发)
- [许可证](#许可证)

## 功能特性

- **CPU Profiling** — 基于 gperftools 的采样式 CPU 分析
- **Heap Profiling** — 堆内存分配分析与泄漏检测
- **Heap Growth Profiling** — 堆增长栈分析，无需 `TCMALLOC_SAMPLE_PARAMETER`
- **线程堆栈捕获** — 采集进程内所有线程的调用栈，支持动态线程数
- **标准 pprof 接口** — `/pprof/*` 兼容 Go pprof 工具链
- **一键分析** — `/api/*/analyze` 直接返回火焰图 SVG，浏览器可看
- **框架无关** — `ProfilerHttpHandlers` 返回普通结构体，可接入任意 Web 框架
- **可选 Web 层** — 核心库不依赖任何 Web 框架
- **可配置日志** — 实现 `LogSink` 即可接入宿主应用的日志系统
- **信号安全** — 保存并恢复宿主程序原有的信号处理器

## 设计理念

参考 Go pprof 提供两种互补的使用方式：

1. **标准 pprof 模式** — `/pprof/profile`、`/pprof/heap` 返回原始 profile 文件，交给 `go tool pprof` 分析
2. **一键分析模式** — `/api/cpu/analyze` 等直接返回 SVG，适合浏览器即时查看

架构上分为两层，边界清晰：

- `profiler_core` — 纯 profiling 核心，**不依赖** Drogon/spdlog，可独立使用
- `profiler_web` — 可选的 Drogon 适配层，仅做路由注册与请求/响应转换
- `ProfilerHttpHandlers` — 位于核心库中，返回 `HandlerResponse{status, content_type, body, headers}`，不绑定任何框架
- `ProfilerManager` — 普通类而非单例，生命周期由使用者管理

接口命名规则：`/pprof/*` 为 Go pprof 标准接口；`/api/*` 为项目自定义的分析/辅助接口；`/show_*.html` 为内置的 SVG 查看页。

## 快速开始

### 前置要求

- Linux（已在 Ubuntu 与 WSL2 上测试；实现依赖 `/proc/self/exe`、`/proc/self/task` 与 `sigaction`，**不支持 macOS/Windows**）
- CMake 3.15+（使用 `CMakePresets.json` 时需 3.20+）
- g++ 10+ 或 clang++ 12+（需 C++20）
- git、pkg-config、graphviz

### 1. 安装系统依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y cmake build-essential git pkg-config graphviz libgoogle-perftools-dev

# Fedora/RHEL/CentOS
sudo dnf install -y cmake gcc-c++ make git pkg-config graphviz gperftools-devel
```

其余依赖（drogon、gtest、nlohmann-json、openssl、zlib、protobuf、backward-cpp）由 vcpkg 提供。

### 2. 初始化 vcpkg

```bash
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
fi
```

`vcpkg.json` 位于仓库根目录，CMake 工具链会在首次 configure 时自动安装清单里的依赖，无需手动 `vcpkg install`。

### 3. 编译

推荐使用 CMake Presets（与 CI 一致）：

```bash
cmake --preset=release
cmake --build build/release -j$(nproc)
```

或手动指定：

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
cmake --build build -j$(nproc)
```

`build.sh` 是等价的便捷脚本（内部同样走 vcpkg 工具链）。

构建类型（`CMAKE_BUILD_TYPE`，默认 `RelWithDebInfo`）：

| 构建类型 | 说明 |
|---------|------|
| `Release` | `-O2 -g`，适合生产与 profiling |
| `RelWithDebInfo` | `-O2 -g`（**默认**），推荐用于 profiling |
| `Debug` | `-O0 -g`，适合调试 |

> 所有构建类型都带 `-g`：缺少调试符号时火焰图只能显示地址而非函数名。

### 4. 运行示例服务

```bash
./start.sh
# 或
cd build && ./profiler_example
```

服务监听 `http://localhost:8080`。

## 运行时依赖

`ProfilerManager` 构造时会**向进程当前工作目录写入两个脚本**，分析接口通过相对路径调用它们：

| 文件 | 用途 | 依赖 |
|------|------|------|
| `./pprof` | 解析 gperftools profile，生成 SVG / collapsed 格式 | `perl` |
| `./flamegraph.pl` | 由 collapsed 数据渲染火焰图 | `perl` |

由此带来三点要求：

1. **工作目录必须可写**。以只读目录（如 `/`）为 CWD 启动会导致脚本写入失败，所有 `/api/*/analyze`、`/api/*/svg_raw`、`/api/*/flamegraph_raw` 接口返回 500。
2. **必须安装 perl**：`sudo apt-get install -y perl`。
3. **不要删除这两个文件**，每个进程实例都会重新生成。

不需要图表功能时（例如只用 `/pprof/profile` 拿原始 profile 文件），上述依赖不影响使用。

### 请求调度与并发

所有会产生图表的接口都需要数秒（CPU 采样最长 300 秒），因此 **Drogon 适配层不会在事件循环线程上执行它们**：这些请求被投递到一个后台工作线程，完成后通过 `queueInLoop()` 把响应送回事件循环。`/api/status`、`/` 等快速接口始终即时响应，采样进行中也能实时查询状态。

**CPU 采样是独占的**：gperftools 的采样会话是进程级全局状态，因此同一时刻只允许一个 CPU 采样请求。第二个并发请求会**立即被拒绝**（而不是排队等待），与 Go 的 `net/http/pprof` 行为一致：

| 端点 | 并发请求的响应 |
|------|----------------|
| `/pprof/profile` | `500`，`text/plain`，`Could not enable CPU profiling: cpu profiling already in use`，并带 `X-Go-Pprof: 1` 头 |
| `/api/cpu/analyze`、`/api/cpu/svg_raw`、`/api/cpu/flamegraph_raw` | `409`，`{"error":"cpu profiling already in use"}` |

`X-Go-Pprof: 1` 是给 `go tool pprof` 的信号：告诉它响应体是错误消息而非 profile 数据。

快速接口（`/api/status`、`/`）不受采样影响，采样期间依然即时响应。

若自行接入其它 Web 框架，请同样把上述接口放到工作线程执行，否则会阻塞你的事件循环。

## CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_SHARED_LIBS` | `ON` | `ON` 构建动态库（`.so`），`OFF` 构建静态库（`.a`） |
| `REMOTE_PROFILER_INSTALL` | `ON` | 生成 install 规则 |
| `REMOTE_PROFILER_BUILD_EXAMPLES` | `ON` | 构建示例程序 `profiler_example` |
| `REMOTE_PROFILER_BUILD_TESTS` | `ON` | 构建测试程序 |
| `REMOTE_PROFILER_ENABLE_WEB` | `ON` | 编译 Web 层（需 Drogon），`OFF` 则完全不依赖 Drogon |
| `ENABLE_COVERAGE` | `OFF` | 生成覆盖率报告 |
| `BUILD_DOCS` | `OFF` | 生成 Doxygen API 文档（输出到 `build/docs/html/`） |

常见场景：

```bash
# 仅核心库，不依赖 Drogon、不构建示例与测试
cmake -S . -B build -DREMOTE_PROFILER_ENABLE_WEB=OFF \
    -DREMOTE_PROFILER_BUILD_EXAMPLES=OFF -DREMOTE_PROFILER_BUILD_TESTS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
```

> vcpkg 清单中的依赖是**无条件**查找的，即使关掉 `REMOTE_PROFILER_BUILD_TESTS` 也仍需要 gtest 等包在工具链中可见；用 `REMOTE_PROFILER_ENABLE_WEB=OFF` 才能真正去掉 Drogon。

## 安装与集成

```bash
cmake --install build                  # 默认前缀 /usr/local
cmake --install build --prefix /opt/cpp-remote-profiler
```

安装布局（`<libdir>` 在 Debian/Ubuntu 上为 `lib`，在 Fedora/RHEL 上为 `lib64`）：

```
<prefix>/
├── include/cpp-remote-profiler/
│   ├── profiler_manager.h  profiler_version.h  version.h
│   └── profiler/{http_handlers.h, log_sink.h, drogon_adapter.h}
├── <libdir>/
│   ├── libprofiler_core.so
│   ├── libprofiler_web.so                      # 启用 Web 时
│   └── cmake/cpp-remote-profiler/              # CMake package 配置
└── share/doc/cpp-remote-profiler/              # 启用 BUILD_DOCS 时
```

### 方式 1：`find_package`（推荐）

```cmake
find_package(cpp-remote-profiler REQUIRED)
target_link_libraries(my_app PRIVATE cpp-remote-profiler::profiler_core)
```

安装到非标准前缀时用 `CMAKE_PREFIX_PATH` 指向它，**不要**用 `cpp-remote-profiler_DIR` 硬编码 `lib` 路径（lib64 发行版上会失效）。

需要 Web 层时显式链接 Drogon —— `profiler_web` 对 Drogon 是私有依赖，**不会**自动传递：

```cmake
find_package(cpp-remote-profiler REQUIRED)
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE
    cpp-remote-profiler::profiler_web
    Drogon::Drogon)
```

### 方式 2：`FetchContent` / `add_subdirectory`

```cmake
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS    OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL        OFF CACHE BOOL "")
add_subdirectory(third_party/cpp-remote-profiler)
target_link_libraries(my_app PRIVATE profiler_core)
```

两种方式同样需要 vcpkg 工具链（或系统里已有 gtest/Backward/absl/OpenSSL/zlib）。可运行示例见 `cmake/examples/`。

### 嵌入方式 A：仅核心 profiling

```cpp
#include "profiler_manager.h"

int main() {
    profiler::ProfilerManager profiler;

    profiler.startCPUProfiler("cpu.prof");
    // ... 你的业务代码 ...
    profiler.stopCPUProfiler();
}
```

### 嵌入方式 B：完整 Web 界面

```cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <drogon/drogon.h>

int main() {
    profiler::ProfilerManager profiler;
    profiler::registerDrogonHandlers(profiler);   // 注册全部 /pprof/* 与 /api/* 路由
    drogon::app().addListener("0.0.0.0", 8080).run();
}
```

### 嵌入方式 C：接入任意 Web 框架

核心库的 `ProfilerHttpHandlers` 只依赖标准库，路由由你的框架自行注册：

```cpp
#include "profiler_manager.h"
#include "profiler/http_handlers.h"

profiler::ProfilerManager profiler;
profiler::ProfilerHttpHandlers handlers(profiler);

auto resp = handlers.handleCpuAnalyze(10, "flamegraph");
// resp.status / resp.content_type / resp.body / resp.headers → 用你的框架包一层
```

### 可选配置

```cpp
// 采集线程栈所用的信号，默认 SIGUSR1
profiler::ProfilerManager::setStackCaptureSignal(SIGRTMIN + 5);

// 接入宿主日志系统
class MyLogSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel level, const char* file, int line,
             const char* function, const char* message) override {
        MY_APP_LOG("[Profiler] {}:{} - {}", file, line, message);
    }
};
profiler.setLogSink(std::make_shared<MyLogSink>());
profiler.setLogLevel(profiler::LogLevel::Debug);
```

默认 sink 将 Trace/Debug/Info 写到 stdout，Warning 及以上写到 stderr。完整 API 见 [API 参考手册](docs/user_guide/02_api_reference.md)。

## API 端点

由 `registerDrogonHandlers()` 注册的全部路由：

| 端点 | 方法 | 说明 |
|------|------|------|
| **标准 pprof 接口** | | |
| `/pprof/profile` | GET | CPU profile 原始文件；`?seconds=N`，默认 **30**，范围 1–300 |
| `/pprof/heap` | GET | Heap 采样原始文本；需 `TCMALLOC_SAMPLE_PARAMETER` |
| `/pprof/growth` | GET | Heap growth 栈原始文本；无需上述环境变量 |
| `/pprof/symbol` | POST | 符号化接口（Go pprof symbolz 协议） |
| **一键分析（返回 SVG）** | | |
| `/api/cpu/analyze` | GET/POST | 采样并返回 CPU 图；`?duration=N` 默认 10，范围 1–300 |
| `/api/heap/analyze` | GET | 返回 Heap 图（固定 1 秒采样，**不接受** `duration`） |
| `/api/growth/analyze` | GET | 返回 Heap Growth 图 |
| **原始 SVG 下载** | | |
| `/api/cpu/svg_raw` | GET | pprof 生成的 CPU SVG；`?duration=N` 默认 10 |
| `/api/heap/svg_raw` | GET | pprof 生成的 Heap SVG |
| `/api/growth/svg_raw` | GET | pprof 生成的 Growth SVG |
| `/api/cpu/flamegraph_raw` | GET | FlameGraph 渲染的 CPU SVG；`?duration=N` 默认 10 |
| `/api/heap/flamegraph_raw` | GET | FlameGraph 渲染的 Heap SVG |
| `/api/growth/flamegraph_raw` | GET | FlameGraph 渲染的 Growth SVG |
| **线程分析** | | |
| `/api/thread/stacks` | GET | 所有线程的调用栈 |
| **辅助与页面** | | |
| `/` | GET | Web 控制面板 |
| `/api/status` | GET | 各 profiler 的运行状态与输出路径（JSON） |
| `/show_svg.html` | GET | CPU SVG 查看页 |
| `/show_heap_svg.html` | GET | Heap SVG 查看页 |
| `/show_growth_svg.html` | GET | Growth SVG 查看页 |

所有分析类接口都接受 `?output_type=flamegraph|pprof`：

- `pprof` — 由内置 pprof 脚本渲染的图形（**HTTP 层的默认值**）
- `flamegraph` — 由 FlameGraph 渲染的火焰图

> 注意默认值差异：C++ API `analyzeCPUProfile()` 的形参默认是 `"flamegraph"`，而 HTTP 路由在未传 `output_type` 时使用 `"pprof"`。想稳定拿到火焰图请显式写 `?output_type=flamegraph`。

### 使用示例

```bash
# CPU：拿原始 profile 交给 pprof 工具（默认 30 秒）
curl 'http://localhost:8080/pprof/profile?seconds=10' > cpu.prof
go tool pprof -http=:8081 cpu.prof

# CPU 火焰图（显式指定 output_type）
curl 'http://localhost:8080/api/cpu/analyze?duration=10&output_type=flamegraph' -o cpu.svg

# Heap（需先设置 TCMALLOC_SAMPLE_PARAMETER）
curl 'http://localhost:8080/api/heap/analyze?output_type=flamegraph' -o heap.svg

# 所有线程调用栈
curl 'http://localhost:8080/api/thread/stacks'
```

也支持直接让 pprof 从 URL 拉取：

```bash
go tool pprof -http=:8081 'http://localhost:8080/pprof/profile?seconds=10'
```

## 配置说明

### 环境变量 `TCMALLOC_SAMPLE_PARAMETER`

控制 tcmalloc 的堆采样间隔（单位字节）。**默认值为 `0`，即关闭采样**——不设置它，`/pprof/heap`、`/api/heap/*` 会因拿不到采样数据而失败。

tcmalloc 在**进程初始化时**读取该变量，因此必须在启动前用环境变量传入，**在代码里调用 `setenv()` 是无效的**：

```bash
export TCMALLOC_SAMPLE_PARAMETER=524288      # 512KB，开发环境常用
./build/profiler_example
```

| 场景 | 建议值 |
|------|--------|
| 开发/调试 | `524288`（512KB） |
| 生产 | `2097152`（2MB）或更大，降低开销 |

Heap Growth 采集（`/pprof/growth`、`/api/growth/*`）走 `GetHeapGrowthStacks()`，**不需要**该变量。

### 依赖版本

依赖清单固定在 `vcpkg.json`，`builtin-baseline` 为 `2cf2bcc60add50f79b2c418487d9cd1b6c7c1fec`（与 CI 中 `lukka/run-vcpkg` 的 `vcpkgGitCommitId` 一致）。升级依赖时同步更新两者。

## 注意事项

1. **编译时保留调试符号**（`-g`，项目默认开启），否则火焰图只显示地址
2. **CPU profiler 有 1–5% 性能开销**；采样频率可用 `CPUPROFILE_FREQUENCY` 调整
3. **CPU 采样独占**：同一时刻只允许一个 CPU 采样会话，并发请求会被拒绝（`409` / Go 风格的 `500`），不会排队。第二个请求不会影响正在进行的采样
4. **信号冲突**：默认 `SIGUSR1`；宿主程序若已占用，请用 `setStackCaptureSignal()` 换一个（推荐 `SIGRTMIN+n`）
5. **线程安全**：所有公共 API 可在任意线程调用，但第 3 条的单会话限制依然成立
6. **工作目录需可写**，见[运行时依赖](#运行时依赖)
7. **开发阶段软件**，不建议用于生产环境

## 项目结构

```
cpp-remote-profiler/
├── CMakeLists.txt              # 构建配置（核心库 / Web 层 / 示例 / 测试）
├── CMakePresets.json           # 与 CI 一致的构建预设
├── vcpkg.json                  # 依赖清单与 baseline
├── build.sh  start.sh          # 便捷构建 / 启动脚本
├── include/
│   ├── profiler_manager.h      # ProfilerManager 公共 API
│   ├── profiler_version.h.in   # 版本信息模板（CMake 生成）
│   ├── version.h               # 版本宏兼容层
│   └── profiler/
│       ├── http_handlers.h     # 框架无关的 HTTP 处理器
│       ├── drogon_adapter.h    # Drogon 适配层（可选）
│       └── log_sink.h          # 日志 Sink 接口
├── src/
│   ├── profiler_manager.cpp    # 核心实现
│   ├── http_handlers.cpp       # 处理器实现（框架无关）
│   ├── drogon_adapter.cpp      # 路由注册 + 请求/响应转换
│   ├── symbolize.cpp           # 符号化引擎（absl → dladdr → backward-cpp）
│   ├── web_resources.cpp       # 内置 Web 资源
│   └── internal/               # 内部实现：日志、符号化、内嵌 pprof / flamegraph.pl
├── example/  tests/            # 示例与 GoogleTest 测试
├── cmake/
│   ├── cpp-remote-profiler-config.cmake.in
│   └── examples/               # find_package / FetchContent 集成验证
├── docs/
│   ├── mainpage.dox  Doxyfile.in
│   └── user_guide/             # 用户文档（安装、API、集成、排错）
├── scripts/check-format.sh     # clang-format 检查（CI 使用 18）
└── .github/workflows/          # CI：构建、测试、ASan/UBSan、clang-tidy、格式检查
```

## 开发

```bash
cmake --preset=debug
cmake --build build/debug -j$(nproc)
ctest --test-dir build/debug --output-on-failure

./scripts/check-format.sh          # 检查格式
./scripts/check-format.sh --fix    # 自动修复
```

贡献流程见 [CONTRIBUTING.md](CONTRIBUTING.md)；版本历史见 [CHANGELOG.md](CHANGELOG.md)；后续计划见 [ROADMAP.md](ROADMAP.md)。

## 文档索引

| 文档 | 内容 |
|------|------|
| [快速开始](docs/user_guide/01_quick_start.md) | 5 分钟集成 |
| [API 参考](docs/user_guide/02_api_reference.md) | 完整函数签名与语义 |
| [集成示例](docs/user_guide/03_integration_examples.md) | 5 类集成场景 |
| [安装指南](docs/user_guide/05_installation.md) | 三种安装/引入方式 |
| [find_package 集成](docs/user_guide/06_using_find_package.md) | CMake 包使用细节 |
| [故障排除](docs/user_guide/04_troubleshooting.md) | 常见问题与排查 |
| [设计文档](plan.md) | 架构设计与技术决策 |

## 许可证

MIT License
