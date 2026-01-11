# C++ Remote Profiler

类似 Go pprof 和 brpc pprof service 的 C++ 远程性能分析工具，基于 gperftools 和 Drogon 框架实现。

## 🎯 功能特性

- ✅ **CPU Profiling**: 使用 gperftools 进行 CPU 性能分析
- ✅ **Heap Profiling**: 内存使用分析和内存泄漏检测（调用 tcmalloc sample）
- ✅ **标准 pprof 接口**: 兼容 brpc pprof service，支持 Go pprof 工具直接访问
- ✅ **Web 界面**: 美观的 Web 控制面板，支持一键式火焰图分析
- ✅ **RESTful API**: 完整的 HTTP API 接口
- ✅ **依赖管理**: 使用 vcpkg 管理所有依赖

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
    libgoogle-perftools-dev
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
| `/pprof/profile` | GET | CPU profile（兼容 Go pprof） | ⏳ |
| `/pprof/heap` | GET | Heap profile（兼容 Go pprof） | ⏳ |
| **一键分析接口** ||||
| `/api/cpu/analyze` | GET | 采样并返回 CPU 火焰图 SVG | ✅ |
| `/api/heap/analyze` | GET | 采样并返回 Heap 火焰图 SVG | ✅ |
| **辅助接口** ||||
| `/` | GET | Web 主界面 | ✅ |
| `/api/status` | GET | 获取全局状态 | ✅ |
| `/api/list` | GET | 列出所有 profile 文件 | ✅ |

### 使用示例

```bash
# CPU profile（返回原始文件，用于 pprof 工具）
curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof

# Heap profile（返回原始文件，用于 pprof 工具）
# 注意：需要设置 TCMALLOC_SAMPLE_PARAMETER 环境变量
curl http://localhost:8080/pprof/heap > heap.prof

# CPU 火焰图（返回 SVG，浏览器可直接显示）
curl http://localhost:8080/api/cpu/analyze?duration=10

# Heap 火焰图（返回 SVG，浏览器可直接显示）
curl http://localhost:8080/api/heap/analyze?duration=10
```

## 📁 项目结构

```
cpp-remote-profiler/
├── CMakeLists.txt              # 构建配置
├── README.md                   # 项目文档
├── vcpkg.json                  # vcpkg 依赖配置
├── build.sh                    # 构建脚本
├── start.sh                    # 启动脚本
├── include/
│   ├── profiler_manager.h      # Profiler 管理器
│   └── profiler_controller.h   # HTTP 控制器
├── src/
│   ├── profiler_manager.cpp
│   └── profiler_controller.cpp
├── example/
│   └── main.cpp                # 示例程序
├── tests/
│   ├── profiler_test.cpp       # 单元测试
│   └── test_flamegraph.sh      # 火焰图测试脚本
├── web/
│   ├── index.html              # Web 界面
│   └── flamegraph.html         # 火焰图查看器
└── vcpkg/                      # vcpkg 包管理器
```

## 🧪 运行测试

```bash
cd build
./profiler_test
```

运行完整的火焰图测试：

```bash
# 确保服务正在运行
./start.sh

# 在另一个终端运行测试
./tests/test_flamegraph.sh
```

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

## 🎨 与其他工具的对比

| 功能 | Go pprof | brpc pprof | C++ Remote Profiler |
|------|---------|-----------|---------------------|
| CPU Profiling | ✓ | ✓ | ✓ |
| Heap Profiling | ✓ | ✓ | ✓ |
| 标准接口 | ✓ | ✓ | ✓ (兼容 brpc) |
| Web 界面 | ✓ | ✗ | ✓ |
| 一键分析 SVG | ✗ | ✗ | ✓ |
| 远程分析 | ✓ | ✓ | ✓ |
| Goroutine Profiling | ✓ | ✗ | ✗ |
| Growth Profiling | ✓ | ✓ | 📋 |

## 📝 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📞 联系方式

如有问题，请在 GitHub 上提 Issue。
