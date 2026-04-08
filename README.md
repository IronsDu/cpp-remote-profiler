# C++ Remote Profiler

类似 Go pprof 和 brpc pprof service 的 C++ 远程性能分析工具，基于 gperftools 和 Drogon 框架实现。

**当前版本**: v0.1.0 (开发阶段)

> ⚠️ **注意**: 当前项目处于开发阶段（v0.x.x），API 可能随时变化。不建议用于生产环境。

## 📋 版本说明

### 当前版本: v0.1.0

本项目使用语义化版本号 (Semantic Versioning)：`MAJOR.MINOR.PATCH`

- **当前阶段**: v0.x.x (开发阶段)
  - API 可能随时变化
  - 不保证向后兼容
  - 欢迎反馈和建议

- **稳定版本**: v1.0.0 (未来)
  - 承诺 API 向后兼容
  - 推荐用于生产环境

### 版本检查

```cpp
#include "version.h"

#if REMOTE_PROFILER_VERSION_AT_LEAST(0, 2, 0)
    // 使用新 API
#else
    // 使用旧 API
#endif
```

详见 `plan.md` 中的 [API 稳定性策略](#api-稳定性策略)。

## 🎯 功能特性

- ✅ **CPU Profiling**: 使用 gperftools 进行 CPU 性能分析
- ✅ **Heap Profiling**: 内存使用分析和内存泄漏检测（调用 tcmalloc sample）
- ✅ **Heap Growth Profiling**: 堆增长分析，无需 TCMALLOC_SAMPLE_PARAMETER 环境变量
- ✅ **线程堆栈捕获**: 获取所有线程的调用堆栈，支持动态线程数
- ✅ **标准 pprof 接口**: 支持 Go pprof 工具直接访问
- ✅ **Web 界面**: 美观的 Web 控制面板，支持一键式火焰图分析
- ✅ **框架无关**: ProfilerHttpHandlers 提供框架无关的 handler，可集成任意 Web 框架
- ✅ **Drogon 可选**: Web 界面依赖 Drogon，但核心 profiling 功能完全独立
- ✅ **可配置日志系统**: 支持自定义 LogSink，集成到应用日志系统
- ✅ **RESTful API**: 完整的 HTTP API 接口
- ✅ **依赖管理**: 使用 vcpkg 管理所有依赖
- ✅ **信号处理器安全**: 保存并恢复用户程序的信号处理器

## 🎯 设计理念

本项目参考 Go pprof 标准接口设计，提供两种使用方式：

1. **标准 pprof 模式**: 提供 `/pprof/profile`、`/pprof/heap` 接口，返回原始 profile 文件，兼容 Go pprof 工具
2. **一键分析模式**: 提供 `/api/cpu/analyze` 等接口，直接返回 SVG，适合浏览器查看

**架构特点**:
- **核心库与 Web 解耦**: `profiler_core` 不依赖任何 Web 框架，`profiler_web` 是可选的 Drogon 适配层
- **框架无关的 Handler**: `ProfilerHttpHandlers` 返回 `HandlerResponse` 结构体，可与任意 Web 框架集成
- **非单例设计**: `ProfilerManager` 是普通类，用户自行管理生命周期

**接口命名规则**:
- `/pprof/*` - 标准 Go pprof 接口
- `/api/*` - 自定义分析接口（浏览器直接查看）

## 🚀 快速开始

### 前置要求

- Linux 系统 (已在 WSL2 和 Ubuntu 上测试)
- CMake 3.15+
- g++ (支持 C++20)
- git

### 1. 安装系统依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    git \
    pkg-config \
    graphviz \
    libgoogle-perftools-dev
```

```bash
# Fedora/RHEL/CentOS
sudo dnf install -y \
    cmake \
    gcc-c++ \
    make \
    git \
    pkg-config \
    graphviz \
    gperftools-devel
```

### 2. 初始化 vcpkg

```bash
# 克隆项目（如果还没有）
git clone <your-repo-url>
cd cpp-remote-profiler

# 如果 vcpkg 目录不存在，初始化它
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    cd ..
fi
```

### 3. 安装 vcpkg 依赖

```bash
cd vcpkg
./vcpkg install --triplet=x64-linux-release
```

这将自动安装以下依赖：
- drogon (Web 框架，可选)
- gtest (测试框架)
- nlohmann-json (JSON 库)
- openssl
- zlib
- protobuf
- backward-cpp (栈回溯)
- gperftools (CPU/Heap 性能分析)

### 4. 编译项目

项目提供了便捷的构建脚本：

```bash
./build.sh
```

或者手动编译：

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
make -j$(nproc)
```

### 5. 运行服务

```bash
./start.sh
```

或直接运行：

```bash
cd build
./profiler_example
```

服务将在 `http://localhost:8080` 启动。

## 📖 使用方法

### 方法 1: 使用 Go pprof 工具（推荐）

```bash
# CPU 采样 10 秒
go tool pprof http://localhost:8080/pprof/profile?seconds=10

# 或者先下载 profile 文件
curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof
go tool pprof -http=:8081 cpu.prof

# Heap profile（需要先设置环境变量）
curl http://localhost:8080/pprof/heap > heap.prof
go tool pprof -http=:8081 heap.prof
```

### 方法 2: 通过 Web 界面（快速查看）

1. 在浏览器中打开 `http://localhost:8080`
2. 点击"分析 CPU"按钮，等待采样完成（默认 10 秒）
3. 查看生成的火焰图
4. 可选择"下载 SVG"保存结果

### 方法 3: 通过 API 获取 SVG

```bash
# CPU 火焰图
curl http://localhost:8080/api/cpu/analyze?duration=10

# Heap 火焰图
curl http://localhost:8080/api/heap/analyze?duration=10
```

## 📊 如何查看火焰图

### 方法 1: 使用内置 Web 界面（最简单）

访问 `http://localhost:8080`，点击"分析 CPU"或"分析 Heap"按钮，自动生成并显示火焰图

### 方法 2: 使用 Go pprof 工具（功能强大）

安装 Go 和 pprof：
```bash
# 安装 Go
wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin

# 安装 pprof
go install github.com/google/pprof@latest
```

使用 pprof：
```bash
# 直接从 URL 分析
go tool pprof -http=:8081 http://localhost:8080/pprof/profile?seconds=10

# 或者先下载
curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof
go tool pprof -http=:8081 cpu.prof
```

### 方法 3: 使用 Speedscope

1. 下载 profile 文件：
   ```bash
   curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof
   ```

2. 访问 [https://www.speedscope.app/](https://www.speedscope.app/)

3. 上传 `cpu.prof` 文件

4. 查看交互式火焰图！

## 🔧 API 端点

| 端点 | 方法 | 描述 | 状态 |
|------|------|------|------|
| **标准 pprof 接口** ||||
| `/pprof/profile` | GET | CPU profile（兼容 Go pprof） | ✅ |
| `/pprof/heap` | GET | Heap profile（兼容 Go pprof） | ✅ |
| `/pprof/growth` | GET | Heap growth stacks（兼容 Go pprof） | ✅ |
| `/pprof/symbol` | POST | 符号化接口（兼容 Go pprof） | ✅ |
| **一键分析接口** ||||
| `/api/cpu/analyze` | GET | 采样并返回 CPU 火焰图 SVG | ✅ |
| `/api/heap/analyze` | GET | 采样并返回 Heap 火焰图 SVG | ✅ |
| `/api/growth/analyze` | GET | Heap Growth 火焰图 SVG | ✅ |
| **原始 SVG 下载接口** ||||
| `/api/cpu/svg_raw` | GET | CPU 原始 SVG（pprof 生成，下载） | ✅ |
| `/api/heap/svg_raw` | GET | Heap 原始 SVG（pprof 生成，下载） | ✅ |
| `/api/growth/svg_raw` | GET | Growth 原始 SVG（pprof 生成，下载） | ✅ |
| `/api/cpu/flamegraph_raw` | GET | CPU FlameGraph 原始 SVG（下载） | ✅ |
| `/api/heap/flamegraph_raw` | GET | Heap FlameGraph 原始 SVG（下载） | ✅ |
| `/api/growth/flamegraph_raw` | GET | Growth FlameGraph 原始 SVG（下载） | ✅ |
| **线程分析接口** ||||
| `/api/thread/stacks` | GET | 获取所有线程的调用堆栈 | ✅ |
| **辅助接口** ||||
| `/` | GET | Web 主界面 | ✅ |
| `/api/status` | GET | 获取全局状态 | ✅ |

### 使用示例

```bash
# CPU profile（返回原始文件，用于 pprof 工具）
curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof

# Heap profile（返回原始文件，用于 pprof 工具）
# 注意：需要设置 TCMALLOC_SAMPLE_PARAMETER 环境变量
curl http://localhost:8080/pprof/heap > heap.prof

# Heap growth stacks（返回调用堆栈）
curl http://localhost:8080/pprof/growth

# CPU 火焰图（返回 SVG，浏览器可直接显示）
curl http://localhost:8080/api/cpu/analyze?duration=10

# Heap 火焰图（返回 SVG，浏览器可直接显示）
curl http://localhost:8080/api/heap/analyze?duration=10

# 获取所有线程的调用堆栈
curl http://localhost:8080/api/thread/stacks
```

## 📁 项目结构

```
cpp-remote-profiler/
├── CMakeLists.txt              # 构建配置
├── README.md                   # 项目文档
├── build.sh                    # 构建脚本
├── start.sh                    # 启动脚本
├── include/
│   ├── profiler_manager.h      # Profiler 管理器（非单例）
│   ├── profiler_version.h.in   # 版本信息模板（CMake 生成）
│   ├── version.h               # 版本宏（向后兼容）
│   └── profiler/
│       ├── http_handlers.h     # 框架无关的 HTTP 处理器
│       ├── drogon_adapter.h    # Drogon 适配层（可选）
│       ├── log_sink.h          # 日志 Sink 接口
│       └── logger.h            # 日志配置接口
├── src/
│   ├── profiler_manager.cpp    # Profiler 管理器实现
│   ├── symbolize.cpp           # 符号化引擎
│   ├── http_handlers.cpp       # HTTP 处理器实现（框架无关）
│   ├── drogon_adapter.cpp      # Drogon 适配层实现（可选）
│   ├── web_resources.cpp       # 嵌入的 Web 资源
│   └── internal/               # 内部实现（不对外暴露）
│       ├── log_manager.h/cpp   # 日志管理器
│       ├── default_log_sink.h/cpp # 默认日志实现（std::cout/cerr）
│       ├── log_macros.h        # 内部日志宏
│       └── ...                 # 其他内部头文件
├── example/
│   ├── main.cpp                # 示例程序主入口
│   ├── workload.cpp            # 工作负载示例
│   ├── workload.h
│   └── custom_signal.cpp       # 自定义信号示例
├── tests/
│   ├── test_cpu_profile.cpp    # CPU profiling 测试
│   ├── test_full_flow.cpp      # 完整流程测试
│   └── test_logger.cpp         # 日志系统测试
├── docs/                       # 用户文档
│   ├── README.md               # 文档索引
│   └── user_guide/             # 用户指南
│       ├── 01_quick_start.md
│       ├── 02_api_reference.md
│       ├── 03_integration_examples.md
│       ├── 04_troubleshooting.md
│       ├── 05_installation.md
│       └── 06_using_find_package.md
├── scripts/
│   └── check-format.sh         # 代码格式检查脚本
└── vcpkg/                      # vcpkg 包管理器
```

## 🧪 运行测试

```bash
cd build
ctest --output-on-failure
# 或单独运行：
./test_cpu_profile
./test_full_flow
./test_logger
```

运行完整的火焰图测试：

```bash
# 确保服务正在运行
./start.sh

# 在浏览器访问
http://localhost:8080
```

## 🎨 代码格式检查

项目使用 `clang-format` 进行代码格式检查。提供的脚本会自动检测本地 clang-format 版本，如果版本匹配 CI（版本 18）则使用本地版本，否则使用 Docker 容器确保版本一致。

### 检查代码格式

检查所有源文件是否符合格式规范：

```bash
./scripts/check-format.sh
```

如果代码格式不符合规范，脚本会显示差异并提示修复方法。

### 自动格式化代码

如果代码格式有问题，可以自动修复所有文件：

```bash
./scripts/check-format.sh --fix
```

这会原地修改 `src/`、`include/`、`tests/`、`example/` 目录下的所有 `.cpp`、`.h`、`.hpp`、`.cc`、`.cxx` 文件。

### 格式检查范围

脚本会检查以下目录中的 C/C++ 源文件：
- `src/`
- `include/`
- `tests/`
- `example/`
- `cmake/examples/`

### CI 集成

代码格式检查已集成到 CI 流程中，提交代码前请确保格式正确，否则 CI 会失败。

## 💡 集成到你的项目

### 选项 1: 仅使用核心 profiling 功能（无 Web 依赖）

只需链接 `profiler_core`，不需要 Drogon：

```cmake
find_package(cpp-remote-profiler REQUIRED)
target_link_libraries(my_app cpp-remote-profiler::profiler_core)
```

```cpp
#include "profiler_manager.h"

int main() {
    profiler::ProfilerManager profiler;

    // 启动 CPU profiling
    profiler.startCPUProfiler("cpu.prof");

    // ... 运行你的代码 ...

    profiler.stopCPUProfiler();
    return 0;
}
```

### 选项 2: 使用 Drogon Web 界面（推荐）

链接 `profiler_web` 即可获得完整的 Web 控制面板：

```cmake
find_package(cpp-remote-profiler REQUIRED)
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(my_app
    cpp-remote-profiler::profiler_web
    Drogon::Drogon
)
```

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

### 选项 3: 与任意 Web 框架集成

使用框架无关的 `ProfilerHttpHandlers`，只链接 `profiler_core`：

```cpp
#include "profiler_manager.h"
#include "profiler/http_handlers.h"

profiler::ProfilerManager profiler;
profiler::ProfilerHttpHandlers handlers(profiler);

// 调用 handler，获得框架无关的响应
auto resp = handlers.handleCpuAnalyze(10, "flamegraph");
// resp.status, resp.content_type, resp.body → 用你的框架包装
```

### 配置信号（可选）

如果你的程序已经使用了 `SIGUSR1` 或 `SIGUSR2`，可以配置其他信号：

```cpp
#include "profiler_manager.h"

int main() {
    // 在创建 ProfilerManager 之前设置信号
    profiler::ProfilerManager::setStackCaptureSignal(SIGRTMIN + 5);

    profiler::ProfilerManager profiler;

    // ... 正常使用 profiler ...
}
```

**可用信号选项**：
- `SIGUSR1` (默认) - 大多数程序可用
- `SIGUSR2` - Drogon 可能使用 SIGUSR2
- `SIGRTMIN` 到 `SIGRTMAX` - 实时信号，更安全

**示例程序**：参考 `example/custom_signal.cpp` 查看详细用法。

### 配置日志（可选）

可以自定义日志输出，集成到你的应用日志系统：

```cpp
#include "profiler_manager.h"
#include "profiler/log_sink.h"

class MyAppLogSink : public profiler::LogSink {
public:
    void log(profiler::LogLevel level, const char* file, int line,
             const char* function, const char* message) override {
        // 转发到你的日志系统
        MY_APP_LOG("[Profiler] {}:{} - {}", file, line, message);
    }
};

int main() {
    profiler::ProfilerManager profiler;
    profiler.setLogSink(std::make_shared<MyAppLogSink>());
    profiler.setLogLevel(profiler::LogLevel::Debug);

    // ... 正常使用 profiler ...
}
```

## ⚙️ 配置说明

### 环境变量

**TCMALLOC_SAMPLE_PARAMETER**:
- 作用：设置 tcmalloc heap sampling 的采样间隔
- 单位：字节
- 默认值：524288 (512KB)
- 推荐值：
  - 开发环境：524288 (512KB)
  - 生产环境：2097152 (2MB) 或更大，减少开销
- 设置方式：
  ```bash
  # 方式 1: 导出环境变量
  export TCMALLOC_SAMPLE_PARAMETER=524288
  ./build/profiler_example

  # 方式 2: 直接在命令行设置
  env TCMALLOC_SAMPLE_PARAMETER=524288 ./build/profiler_example
  ```

**注意**: 只有设置此环境变量后，heap profiling 才能正常工作。

### vcpkg 依赖版本

所有依赖版本在 `vcpkg.json` 中定义，当前基线：`2cf2bcc60add50f79b2c418487d9cd1b6c7c1fec`

如需更新依赖版本：

```bash
cd vcpkg
./vcpkg upgrade --triplet=x64-linux-release
```

## ⚠️ 注意事项

1. **编译选项**: 使用 `-g` 编译选项保留调试符号，以便正确显示函数名
2. **性能开销**: CPU profiler 会有 1-5% 的性能开销
3. **Heap Profiler**: 需要 tcmalloc 内存分配器和 `TCMALLOC_SAMPLE_PARAMETER` 环境变量
4. **Heap Growth**: 无需 `TCMALLOC_SAMPLE_PARAMETER`，可即时获取堆增长数据
5. **生产环境**: 谨慎使用，建议在开发/测试环境中使用
6. **并发限制**: 同一时间只能有一个 CPU profiling 请求
7. **信号冲突**: 默认使用 SIGUSR1，如与你的程序冲突，请使用 `setStackCaptureSignal()` 配置其他信号
8. **线程安全**: 所有公共 API 都是线程安全的

## 🎨 与其他工具的对比

| 功能 | Go pprof | brpc pprof | C++ Remote Profiler |
|------|---------|-----------|---------------------|
| CPU Profiling | ✓ | ✓ | ✓ |
| Heap Profiling | ✓ | ✓ | ✓ |
| Thread Stack Capturing | ✓ | ✗ | ✓ |
| 标准接口 | ✓ | ✓ | ✓ |
| Web 界面 | ✓ | ✗ | ✓ |
| 一键分析 SVG | ✗ | ✗ | ✓ |
| 远程分析 | ✓ | ✓ | ✓ |
| Goroutine Profiling | ✓ | ✗ | ✗ |
| Growth Profiling | ✓ | ✓ | ✓ |

## 📝 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📞 联系方式

如有问题，请在 GitHub 上提 Issue。
