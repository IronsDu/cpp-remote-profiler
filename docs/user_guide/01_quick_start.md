# 快速开始指南

本指南将帮助你在 5 分钟内将 C++ Remote Profiler 集成到你的项目中。

## 目录
- [前置要求](#前置要求)
- [安装](#安装)
- [最简单的示例](#最简单的示例)
- [编译你的项目](#编译你的项目)
- [运行第一个 profiling](#运行第一个-profiling)
- [下一步](#下一步)

## 前置要求

### 系统要求
- **操作系统**: Linux (已在 Ubuntu 20.04+, WSL2 上测试)；不支持 macOS / Windows
- **编译器**: g++ 10.0+ 或 clang++ 12.0+ (支持 C++20)
- **CMake**: 3.15+（使用仓库自带的 `CMakePresets.json` 时需 3.20+）

### 依赖库
- gperftools (libprofiler, libtcmalloc) — 通过 pkg-config 查找，通常由系统包提供
- backward-cpp、Abseil — 用于符号化，由 vcpkg 提供
- perl — **运行时**依赖，用于生成图表（见根目录 README 的"运行时依赖"）

## 安装

### 方法 1: 使用 vcpkg (推荐)

```bash
# 1. 安装系统依赖
sudo apt-get update
sudo apt-get install -y cmake build-essential git pkg-config graphviz perl libgoogle-perftools-dev

# 2. 克隆项目
git clone https://github.com/IronsDu/cpp-remote-profiler.git
cd cpp-remote-profiler

# 3. 初始化 vcpkg (如果还没有)
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
fi

# 4. 编译库（vcpkg.json 在仓库根目录，工具链会自动安装清单依赖）
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
cmake --build build -j$(nproc)

# 5. 安装，供后续 find_package() 使用
sudo cmake --install build
```

### 方法 2: 直接引入源码

用 `FetchContent` 或 `add_subdirectory` 把仓库加进你的工程，无需预先安装。
完整写法见 [使用 find_package](06_using_find_package.md#示例-3fetchcontent-直接引入源码)
或 [安装指南](05_installation.md)。

## 最简单的示例

创建一个简单的示例程序 `my_profiler_app.cpp`:

```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "profiler_manager.h"

// 模拟一些 CPU 密集型工作
void doSomeWork() {
    double result = 0;
    for (int i = 0; i < 1000000; i++) {
        result += i * 0.001;
    }
}

int main() {
    // 创建 ProfilerManager 实例（非单例模式）
    profiler::ProfilerManager profiler;

    std::cout << "开始 CPU profiling..." << std::endl;

    // 1. 启动 CPU profiler
    profiler.startCPUProfiler("my_profile.prof");

    // 2. 运行需要分析的代码
    for (int i = 0; i < 10; i++) {
        doSomeWork();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 3. 停止 profiler
    profiler.stopCPUProfiler();

    std::cout << "Profiling 完成！profile 文件: my_profile.prof" << std::endl;

    return 0;
}
```

## 编译你的项目

下面的例子用 `find_package()` 引用本库，所以**必须先完成上一步的编译 + 安装**（安装到默认前缀 `/usr/local`）：

```bash
# 在 cpp-remote-profiler 仓库内
sudo cmake --install build
```

如果不想安装，请改看 [安装指南](05_installation.md) 中的 `FetchContent` / `add_subdirectory` 方式。

### CMakeLists.txt 配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProfilerApp VERSION 1.0.0 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 链接 profiler 核心库（不需要 Drogon）
find_package(cpp-remote-profiler REQUIRED)

add_executable(my_app my_profiler_app.cpp)
target_link_libraries(my_app PRIVATE cpp-remote-profiler::profiler_core)
```

### 编译命令

```bash
cmake -S . -B build
cmake --build build
./build/my_app
```

## 运行第一个 profiling

### 1. 运行你的程序

```bash
./my_app
```

### 2. 分析 profile 文件

#### 使用 Go pprof (推荐)

```bash
# 使用 pprof 分析
go tool pprof -http=:8080 my_profile.prof
```

## 一键生成火焰图 (API 方式)

如果你想直接在代码中生成火焰图：

```cpp
#include "profiler_manager.h"
#include <fstream>

int main() {
    profiler::ProfilerManager profiler;

    // 采样 10 秒并生成火焰图
    std::string svg = profiler.analyzeCPUProfile(10, "flamegraph");

    // 保存 SVG 到文件
    std::ofstream out("flamegraph.svg");
    out << svg;
    out.close();

    return 0;
}
```

## 带有 Web 界面的完整示例

如果你使用 Drogon 框架，可以使用一键注册函数：

```cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <drogon/drogon.h>

int main() {
    profiler::ProfilerManager profiler;

    // 注册所有 profiling 相关的 HTTP 端点到 Drogon
    profiler::registerDrogonHandlers(profiler);

    // 启动服务器
    drogon::app().addListener("0.0.0.0", 8080).run();

    return 0;
}
```

**CMake 配置**:
```cmake
target_link_libraries(my_app
    cpp-remote-profiler::profiler_web
    Drogon::Drogon
)
```

## 使用其他 Web 框架

如果你使用的是 Drogon 以外的 Web 框架，可以使用 `ProfilerHttpHandlers`：

```cpp
#include "profiler_manager.h"
#include "profiler/http_handlers.h"

int main() {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    // 调用任意 handler，获得框架无关的响应
    profiler::ChartOptions options;   // renderer / duration / inline_display
    profiler::HandlerResponse resp = handlers.handleCpuChart(options);

    // resp.status, resp.content_type, resp.body
    // 用你自己的 Web 框架包装这些数据
}
```

**CMake 配置**:
```cmake
target_link_libraries(my_app cpp-remote-profiler::profiler_core)
# 不需要 Drogon
```

## Heap Profiling 示例

Heap profiling 需要设置环境变量：

```cpp
#include <iostream>
#include "profiler_manager.h"

int main() {
    profiler::ProfilerManager profiler;

    profiler.startHeapProfiler("heap.prof");

    // 分配一些内存
    int* data = new int[1000];

    profiler.stopHeapProfiler();

    delete[] data;

    // 获取 heap 采样数据
    std::string heap_data = profiler.getRawHeapSample();
    std::cout << "Heap data:\n" << heap_data << std::endl;

    return 0;
}
```

环境变量必须在**启动进程之前**设置（tcmalloc 在初始化阶段读取它，默认 `0` 表示关闭采样）：

```bash
export TCMALLOC_SAMPLE_PARAMETER=524288   # 512KB
./my_app
```

> ⚠️ 在 `main()` 里调用 `setenv("TCMALLOC_SAMPLE_PARAMETER", ...)` **不会生效**。

Heap Growth 采集（`getRawHeapGrowthStacks()`）不需要该变量。

## 下一步

- 阅读 [API 参考手册](02_api_reference.md) 了解所有可用的 API
- 查看 [集成示例](03_integration_examples.md) 学习更多使用场景
- 遇到问题？查看 [故障排除指南](04_troubleshooting.md)

## 常见问题

### Q: 编译时找不到 `profiler_manager.h`
**A**: 确保在 CMakeLists.txt 中正确设置了 include 路径：
```cmake
target_include_directories(my_app PRIVATE path/to/cpp-remote-profiler/include)
```

### Q: 运行时显示 "symbolize failed"
**A**: 确保编译时使用了 `-g` 选项保留调试符号：
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
```

### Q: Heap profiling 不工作
**A**: 确保设置了 `TCMALLOC_SAMPLE_PARAMETER` 环境变量，并且链接了 tcmalloc 库。

### Q: 如何在非 Drogon 的 Web 框架中使用？
**A**: 使用 `ProfilerHttpHandlers` 类。每个 handler 方法返回 `HandlerResponse` 结构体，你只需将其包装到你框架的 response 对象中。详见 [场景 3: 与任意 Web 框架集成](03_integration_examples.md#场景-3-与任意-web-框架集成)。
