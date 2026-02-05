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
- **操作系统**: Linux (已在 Ubuntu 20.04+, WSL2 上测试)
- **编译器**: g++ 10.0+ 或 clang++ 12.0+ (支持 C++20)
- **CMake**: 3.15+

### 依赖库
- gperftools (libprofiler, libtcmalloc)
- pthread (通常系统自带)
- backward-cpp (可选，用于符号化)

## 安装

### 方法 1: 使用 vcpkg (推荐)

```bash
# 1. 安装系统依赖
sudo apt-get update
sudo apt-get install -y cmake build-essential git pkg-config libgoogle-perftools-dev

# 2. 克隆项目
git clone https://github.com/your-org/cpp-remote-profiler.git
cd cpp-remote-profiler

# 3. 初始化 vcpkg (如果还没有)
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    cd ..
fi

# 4. 安装 vcpkg 依赖
cd vcpkg
./vcpkg install --triplet=x64-linux-release
cd ..

# 5. 编译库
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release

# 6. 安装到系统（可选）
sudo make install
```

### 方法 2: 直接集成源码

将 `include/` 和 `src/` 目录复制到你的项目中，直接链接源码。

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
    auto& profiler = profiler::ProfilerManager::getInstance();

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

### CMakeLists.txt 配置

在你的项目根目录创建 `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyProfilerApp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 方法 1: 如果已安装到系统
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

add_executable(my_app my_profiler_app.cpp)

# 链接 profiler 库
target_link_libraries(my_app
    profiler_lib           # C++ Remote Profiler 库
    ${GPERFTOOLS_LIBRARIES} # gperftools
    pthread
)

# 方法 2: 如果使用源码直接编译
# add_subdirectory(path/to/cpp-remote-profiler)
# target_link_libraries(my_app profiler_lib)
```

### 编译命令

```bash
# 如果使用 vcpkg
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../cpp-remote-profiler/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release

make

# 运行
./my_app
```

## 运行第一个 profiling

### 1. 运行你的程序

```bash
./my_app
```

### 2. 分析 profile 文件

#### 使用 Go pprof (推荐)

```bash
# 安装 Go (如果还没有)
wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin

# 使用 pprof 分析
go tool pprof -http=:8080 my_profile.prof
```

然后在浏览器中打开 `http://localhost:8080`

#### 使用 FlameGraph

```bash
# 1. 安装 FlameGraph 工具
git clone https://github.com/brendangregg/FlameGraph /tmp/FlameGraph

# 2. 使用 pprof 转换 profile 为 collapsed 格式
pprof -raw my_profile.app > /tmp/profile.raw

# 3. 生成火焰图
perl /tmp/FlameGraph/flamegraph.pl /tmp/profile.raw > flamegraph.svg

# 4. 在浏览器中查看
firefox flamegraph.svg
```

## 一键生成火焰图 (API 方式)

如果你想直接在代码中生成火焰图，可以使用 `analyzeCPUProfile()`:

```cpp
#include "profiler_manager.h"

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 采样 10 秒并生成火焰图
    std::string svg = profiler.analyzeCPUProfile(10, "flamegraph");

    // 保存 SVG 到文件
    std::ofstream out("flamegraph.svg");
    out << svg;
    out.close();

    std::cout << "火焰图已生成: flamegraph.svg" << std::endl;

    return 0;
}
```

## 带有 Web 界面的完整示例

如果你想要一个完整的 Web 界面来查看 profiling 结果：

```cpp
#include <drogon/drogon.h>
#include "profiler_manager.h"
#include "web_server.h"

int main() {
    // 启动 Drogon 服务器
    profiler::ProfilerManager& profiler = profiler::ProfilerManager::getInstance();

    // 注册所有 profiling 相关的 HTTP 端点
    profiler::registerHttpHandlers(profiler);

    // 监听 8080 端口
    drogon::app().addListener("0.0.0.0", 8080);
    std::cout << "Profiler Web UI: http://localhost:8080" << std::endl;

    // 启动服务器
    drogon::app().run();

    return 0;
}
```

访问 `http://localhost:8080` 即可看到 Web 界面，点击按钮即可生成火焰图。

## Heap Profiling 示例

Heap profiling 需要设置环境变量：

```cpp
#include <iostream>
#include "profiler_manager.h"

int main() {
    // 设置环境变量 (在程序启动前)
    setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 1);

    auto& profiler = profiler::ProfilerManager::getInstance();

    // 启动 heap profiler
    profiler.startHeapProfiler("heap.prof");

    // 分配一些内存
    int* data = new int[1000];

    // 停止 heap profiler
    profiler.stopHeapProfiler();

    delete[] data;

    // 获取 heap 采样数据
    std::string heap_data = profiler.getRawHeapSample();
    std::cout << "Heap data:\n" << heap_data << std::endl;

    return 0;
}
```

或者直接在命令行设置环境变量：

```bash
export TCMALLOC_SAMPLE_PARAMETER=524288
./my_app
```

## 下一步

- 📖 阅读 [API 参考手册](02_api_reference.md) 了解所有可用的 API
- 💡 查看 [集成示例](03_integration_examples.md) 学习更多使用场景
- 🔧 遇到问题？查看 [故障排除指南](04_troubleshooting.md)

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

### Q: 性能开销太大
**A**: CPU profiling 通常有 1-5% 的性能开销。如果开销过大，可以：
- 降低采样频率
- 仅在需要时启用 profiling
- 使用更长的采样间隔
