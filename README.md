# C++ Remote Profiler

类似 Go pprof 的 C++ 远程性能分析工具，基于 gperftools 和 Drogon 框架实现。

## 🎯 功能特性

- ✅ **CPU Profiling**: 使用 gperftools 进行 CPU 性能分析
- ✅ **Heap Profiling**: 内存使用分析和内存泄漏检测
- ✅ **Web 界面**: 美观的 Web 控制面板，支持交互式火焰图
- ✅ **RESTful API**: 完整的 HTTP API 接口
- ✅ **Profile 下载**: 支持 Go pprof 和 Speedscope 格式
- ✅ **实时状态**: 实时查看 profiler 运行状态
- ✅ **依赖管理**: 使用 vcpkg 管理所有依赖

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

### 方法 1: 通过 Web 界面（推荐）

1. 在浏览器中打开 `http://localhost:8080`
2. 点击"启动 CPU Profiler"开始性能分析
3. 运行你的程序或让服务自动生成负载
4. 点击"停止"结束分析
5. 在页面底部查看交互式火焰图
6. 可选择"下载 Profile"用于 Go pprof 或 Speedscope

### 方法 2: 通过 API

```bash
# 获取状态
curl http://localhost:8080/api/status

# 启动 CPU profiler
curl -X POST http://localhost:8080/api/cpu/start

# 停止 CPU profiler
curl -X POST http://localhost:8080/api/cpu/stop

# 下载 CPU profile（用于 Go pprof 或 Speedscope）
curl http://localhost:8080/api/cpu/pprof -o cpu.prof

# 获取火焰图 JSON 数据（用于自定义可视化）
curl http://localhost:8080/api/cpu/flamegraph -o flamegraph.json

# 查看 CPU profile 文本格式（需要安装 pprof 工具）
curl http://localhost:8080/api/cpu/text

# Heap profiler 操作类似
curl -X POST http://localhost:8080/api/heap/start
curl -X POST http://localhost:8080/api/heap/stop
curl http://localhost:8080/api/heap/pprof -o heap.prof
curl http://localhost:8080/api/heap/flamegraph -o heap_flamegraph.json
```

## 📊 如何查看火焰图

### 方法 1: 使用内置 Web 界面（最简单）

访问 `http://localhost:8080/flamegraph?type=cpu` 查看交互式火焰图

### 方法 2: 使用 Speedscope（推荐，功能强大）

1. 下载 profile 文件：
   ```bash
   curl http://localhost:8080/api/cpu/pprof -o cpu.prof
   ```

2. 访问 [https://www.speedscope.app/](https://www.speedscope.app/)

3. 上传 `cpu.prof` 文件

4. 查看交互式火焰图！

### 方法 3: 使用 Go pprof 工具

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
# 文本格式
go tool pprof -text cpu.prof

# 图形界面
go tool pprof -http=:8081 cpu.prof

# 火焰图
go tool pprof -http=:8081 -svg cpu.prof
```

## 🔧 API 端点

| 端点 | 方法 | 描述 |
|------|------|------|
| `/` | GET | Web 界面 |
| `/flamegraph` | GET | 交互式火焰图页面 (?type=cpu 或 ?type=heap) |
| `/api/status` | GET | 获取 profiler 状态 |
| `/api/cpu/start` | POST | 启动 CPU profiler |
| `/api/cpu/stop` | POST | 停止 CPU profiler |
| `/api/heap/start` | POST | 启动 Heap profiler |
| `/api/heap/stop` | POST | 停止 Heap profiler |
| `/api/cpu/text` | GET | 获取 CPU 文本格式分析 |
| `/api/heap/text` | GET | 获取 Heap 文本格式分析 |
| `/api/cpu/pprof` | GET | 下载 CPU profile (protobuf 格式) |
| `/api/heap/pprof` | GET | 下载 Heap profile |
| `/api/cpu/flamegraph` | GET | 获取 CPU 火焰图 JSON 数据 |
| `/api/heap/flamegraph` | GET | 获取 Heap 火焰图 JSON 数据 |

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

## 💡 在你的代码中使用

```cpp
#include "profiler_manager.h"

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 启动 CPU profiler
    profiler.startCPUProfiler("my_app.prof");

    // 运行需要分析的代码
    yourCodeToProfile();

    // 停止 CPU profiler
    profiler.stopCPUProfiler();

    // profile 数据已保存到 my_app.prof
    return 0;
}
```

### 编译你的程序

```bash
g++ -o your_app your_app.cpp \
    -I/path/to/cpp-remote-profiler/include \
    -L/path/to/cpp-remote-profiler/build \
    -lprofiler_lib \
    -ltcmalloc_and_profiler \
    -lprofiler \
    -lpthread
```

## ⚙️ 配置说明

### vcpkg 依赖版本

所有依赖版本在 `vcpkg.json` 中定义，当前基线：`2cf2bcc60add50f79b2c418487d9cd1b6c7c1fec`

如需更新依赖版本：

```bash
cd vcpkg
./vcpkg upgrade --triplet=x64-linux-release
```

## ⚠️ 注意事项

1. **编译选项**: 使用 `-g` 编译选项保留调试符号，以便正确显示函数名
2. **性能开销**: CPU profiler 会有 5-10% 的性能开销
3. **Heap Profiler**: 需要 tcmalloc 内存分配器
4. **生产环境**: 谨慎使用，建议在开发/测试环境中使用

## 🎨 与 Go pprof 的对比

| 功能 | Go pprof | C++ Remote Profiler |
|------|---------|---------------------|
| CPU Profiling | ✓ | ✓ |
| Heap Profiling | ✓ | ✓ |
| Web 界面 | ✓ | ✓ |
| 交互式火焰图 | ✓ | ✓ (内置) |
| 远程分析 | ✓ | ✓ |
| Goroutine Profiling | ✓ | ✗ |
| Block Profiling | ✓ | ✗ |
| Mutex Profiling | ✓ | ✗ |

## 📝 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📞 联系方式

如有问题，请在 GitHub 上提 Issue。
