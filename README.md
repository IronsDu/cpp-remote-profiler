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
- ✅ **线程堆栈捕获**: 获取所有线程的调用堆栈，支持动态线程数
- ✅ **标准 pprof 接口**: 支持 Go pprof 工具直接访问
- ✅ **Web 界面**: 美观的 Web 控制面板，支持一键式火焰图分析
- ✅ **RESTful API**: 完整的 HTTP API 接口
- ✅ **依赖管理**: 使用 vcpkg 管理所有依赖
- ✅ **信号处理器安全**: 保存并恢复用户程序的信号处理器

## 🎯 设计理念

本项目参考 Go pprof 标准接口设计，提供两种使用方式：

1. **标准 pprof 模式**: 提供 `/pprof/profile`、`/pprof/heap` 接口，返回原始 profile 文件，兼容 Go pprof 工具
2. **一键分析模式**: 提供 `/api/cpu/analyze` 等接口，直接返回 SVG，适合浏览器查看

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
- drogon (Web 框架)
- gtest (测试框架)
- nlohmann-json (JSON 库)
- openssl
- zlib

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
│   ├── profiler_manager.h      # Profiler 管理器
│   ├── symbolize.h             # 符号化引擎
│   ├── web_resources.h         # Web 资源（嵌入的 HTML）
│   ├── web_server.h            # HTTP 服务器
│   └── version.h               # 版本信息
├── src/
│   ├── profiler_manager.cpp
│   ├── symbolize.cpp
│   ├── web_resources.cpp       # 嵌入的 Web 资源
│   └── web_server.cpp          # HTTP 路由处理
├── example/
│   ├── main.cpp                # 示例程序主入口
│   ├── workload.cpp            # 工作负载示例
│   ├── workload.h
│   └── custom_signal.cpp       # 自定义信号示例
├── tests/
│   ├── test_cpu_profile.cpp
│   └── test_full_flow.cpp
├── docs/                       # 用户文档
│   ├── README.md               # 文档索引
│   └── user_guide/             # 用户指南
│       ├── 01_quick_start.md
│       ├── 02_api_reference.md
│       ├── 03_integration_examples.md
│       ├── 04_troubleshooting.md
│       ├── 05_installation.md
│       └── 06_using_find_package.md
└── vcpkg/                      # vcpkg 包管理器
```

## 🧪 运行测试

```bash
cd build
./test_cpu_profile
./test_full_flow
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

### 选项 1: 作为 HTTP 服务集成（推荐）

将 profiler 作为一个独立的 HTTP 服务运行：

```cpp
#include <drogon/drogon.h>

int main() {
    // 启动 profiler HTTP 服务
    // 监听 8080 端口，提供 /prof 和 /heap 接口
    // 你的主程序可以在其他端口运行
    // 通过 HTTP 请求获取 profile 数据
}
```

### 选项 2: 使用 gperftools 直接集成

```cpp
#include <gperftools/profiler.h>

int main() {
    // 启动 CPU profiler
    ProfilerStart("/tmp/my_app.prof");

    // 运行需要分析的代码
    yourCodeToProfile();

    // 停止 CPU profiler
    ProfilerStop();

    // 使用 pprof 工具分析
    // go tool pprof /tmp/my_app.prof
    return 0;
}
```

### 编译你的程序

```bash
g++ -o your_app your_app.cpp \
    -ltcmalloc_and_profiler \
    -lprofiler \
    -lpthread
```

### 配置信号（可选）

如果你的程序已经使用了 `SIGUSR1` 或 `SIGUSR2`，可以配置其他信号：

```cpp
#include "profiler_manager.h"

int main() {
    // 在使用 Profiler 之前设置信号
    profiler::ProfilerManager::setStackCaptureSignal(SIGRTMIN + 5);

    auto& profiler = profiler::ProfilerManager::getInstance();

    // ... 正常使用 profiler ...
}
```

**可用信号选项**：
- `SIGUSR1` (默认) - 大多数程序可用
- `SIGUSR2` - Drogon 可能使用 SIGUSR2
- `SIGRTMIN` 到 `SIGRTMAX` - 实时信号，更安全

**示例程序**：参考 `example/custom_signal.cpp` 查看详细用法。

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
4. **生产环境**: 谨慎使用，建议在开发/测试环境中使用
5. **并发限制**: 同一时间只能有一个 CPU profiling 请求
6. **环境变量**: 使用 heap profiling 前必须设置 `TCMALLOC_SAMPLE_PARAMETER`
7. **信号冲突**: 默认使用 SIGUSR1，如与你的程序冲突，请使用 `setStackCaptureSignal()` 配置其他信号
8. **线程安全**: 线程堆栈捕获使用信号处理器，确保程序正确处理 SIGUSR1（或配置的信号）

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
