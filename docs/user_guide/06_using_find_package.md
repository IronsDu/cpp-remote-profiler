# 使用 find_package 集成

本文档介绍如何通过 CMake `find_package()` 使用已安装的 cpp-remote-profiler。

## 目录

- [安装库](#安装库)
- [基本用法](#基本用法)
- [使用 Web 功能](#使用-web-功能)
- [指定安装路径](#指定安装路径)
- [版本检查](#版本检查)
- [完整示例](#完整示例)
- [故障排除](#故障排除)
- [卸载](#卸载)

---

## 安装库

```bash
git clone https://github.com/IronsDu/cpp-remote-profiler.git
cd cpp-remote-profiler

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
cmake --build build -j$(nproc)

# 安装到默认前缀 /usr/local
sudo cmake --install build

# 或安装到自定义前缀
cmake --install build --prefix /opt/cpp-remote-profiler
```

> ⚠️ 不要用 `make install DESTDIR=/opt/...` 来"安装到 /opt"。`DESTDIR` 是**打包暂存目录**，
> 设成 `/opt/cpp-remote-profiler` 会让文件落到 `/opt/cpp-remote-profiler/usr/local/...`。
> 要换安装位置请用 `--prefix`（如上）。

安装后的文件结构（`<libdir>` 在 Debian/Ubuntu 上是 `lib`，在 Fedora/RHEL 上是 `lib64`）：

```
<prefix>/
├── include/cpp-remote-profiler/
│   ├── profiler_manager.h
│   ├── profiler_version.h
│   ├── version.h
│   ├── http_handlers.h        # 同时以扁平方式安装一份
│   ├── log_sink.h             # 同上
│   └── profiler/
│       ├── http_handlers.h
│       ├── log_sink.h
│       └── drogon_adapter.h   # 仅在启用 Web 时安装
└── <libdir>/
    ├── libprofiler_core.so
    ├── libprofiler_web.so                  # 仅在启用 Web 时
    └── cmake/cpp-remote-profiler/
        ├── cpp-remote-profiler-config.cmake
        ├── cpp-remote-profiler-config-version.cmake
        └── cpp-remote-profiler-targets.cmake
```

---

## 基本用法

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpp-remote-profiler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cpp-remote-profiler::profiler_core)
```

可用的导入目标只有两个：

| 目标 | 说明 |
|------|------|
| `cpp-remote-profiler::profiler_core` | 核心 profiling 库，不依赖任何 Web 框架 |
| `cpp-remote-profiler::profiler_web` | 可选的 Drogon 适配层（同时依赖 `profiler_core`） |

`find_package()` 会自动定位 gperftools（通过 pkg-config 的 `libprofiler` / `libtcmalloc`），无需手工 `pkg_check_modules`。

---

## 使用 Web 功能

Web 功能位于**独立的** `profiler_web` 目标中，核心库 `profiler_core` 完全不依赖 Drogon。

⚠️ **Drogon 不会自动传递**：`profiler_web` 对 `Drogon::Drogon` 是 **PRIVATE** 依赖，导出的
CMake package 配置也不会替你做 `find_dependency(Drogon)`。因此必须**自己查找并链接** Drogon：

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(cpp-remote-profiler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE
    cpp-remote-profiler::profiler_web
    Drogon::Drogon)              # 必须显式链接
```

`main.cpp`：

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

只用核心功能时不需要 `find_package(Drogon)`，也不需要链接它。

---

## 指定安装路径

`find_package()` 的 **config 模式不会搜索 `CMAKE_MODULE_PATH`**，用下面任一方式指定前缀：

```bash
# 推荐：命令行指定前缀（库装在 /usr/local 时可省略）
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/cpp-remote-profiler

# 或通过环境变量
export CMAKE_PREFIX_PATH=/opt/cpp-remote-profiler
cmake -S . -B build
```

只有在需要**精确指向**某个 package 配置目录时才用 `<包名>_DIR`（注意 `<libdir>` 可能是 `lib64`）：

```bash
cmake -S . -B build \
  -Dcpp-remote-profiler_DIR=/opt/cpp-remote-profiler/lib64/cmake/cpp-remote-profiler
```

---

## 版本检查

### 在 CMake 中要求版本

```cmake
find_package(cpp-remote-profiler 0.1.0 REQUIRED)         # 至少 0.1.0
find_package(cpp-remote-profiler 0.1.0 EXACT REQUIRED)   # 恰好 0.1.0
```

### 在代码中检查版本

使用 `PROFILER_VERSION_AT_LEAST()`（`include/profiler_version.h` 中的函数式宏）：

```cpp
#include "profiler_version.h"

#if PROFILER_VERSION_AT_LEAST(0, 2, 0)
    // 使用 0.2.0 起可用的新 API
#else
    // 兼容旧版本的写法
#endif
```

`version.h` 提供了同义的兼容别名 `REMOTE_PROFILER_VERSION_AT_LEAST()`，适用于旧代码。

> ⚠️ `PROFILER_VERSION` 是**字符串** `"0.1.0"`，无法参与数值比较；`PROFILER_VERSION_INT` 是
> **无参**宏（整体版本号）。因此 `#if REMOTE_PROFILER_VERSION >= REMOTE_PROFILER_VERSION_INT(0,2,0)`
> 这类写法编译不过，请使用 `*_AT_LEAST()`。

---

## 完整示例

### 示例 1：最小可用

**CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.20)
project(SimpleProfiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpp-remote-profiler REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE cpp-remote-profiler::profiler_core)
```

**main.cpp**
```cpp
#include "profiler_manager.h"
#include "profiler_version.h"
#include <iostream>

int main() {
    profiler::ProfilerManager profiler;

    std::cout << "Profiler version: " << PROFILER_VERSION << std::endl;

    profiler.startCPUProfiler("my_profile.prof");
    // ... 你的业务代码 ...
    profiler.stopCPUProfiler();

    return 0;
}
```

**构建与运行**（假设库已装在 `/usr/local`）
```bash
cmake -S . -B build
cmake --build build
./build/app                      # 产出 my_profile.prof
go tool pprof -http=:8081 my_profile.prof
```

### 示例 2：带 Web 界面

```cmake
cmake_minimum_required(VERSION 3.20)
project(WebProfilerApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(cpp-remote-profiler REQUIRED)

add_executable(web_app main.cpp)
target_link_libraries(web_app PRIVATE
    cpp-remote-profiler::profiler_web
    Drogon::Drogon)
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

启动后访问 `http://localhost:8080`。注意 heap 相关接口需要预先设置
`TCMALLOC_SAMPLE_PARAMETER`，且工作目录必须可写（详见根目录 README 的"运行时依赖"）。

### 示例 3：`FetchContent` 直接引入源码

无需预先安装，在构建时拉取源码：

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关闭上游自带的 examples / tests / install
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS    OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL        OFF CACHE BOOL "")

include(FetchContent)
FetchContent_Declare(
    cpp-remote-profiler
    GIT_REPOSITORY https://github.com/IronsDu/cpp-remote-profiler.git
    GIT_TAG        main          # 仓库目前没有 tag，固定到 commit 哈希更适合复现
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cpp-remote-profiler)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE profiler_core)   # 源码内构建时用无命名空间的目标名
```

> ⚠️ 这种方式仍需要工具链能找到 gtest、Backward、Abseil、OpenSSL、zlib（通常通过 vcpkg
> `-DCMAKE_TOOLCHAIN_FILE=...`）。`REMOTE_PROFILER_BUILD_TESTS=OFF` 只是不构建测试，
> 并不会跳过这些 `find_package()`。

---

## 故障排除

### 找不到包

```
Could not find a package configuration file provided by "cpp-remote-profiler"
```

```bash
# 确认配置文件确实装上了（注意 lib 可能是 lib64）
ls /usr/local/lib*/cmake/cpp-remote-profiler/

# 指定前缀
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local
```

不要用 `CMAKE_MODULE_PATH` 来让 config 模式的 `find_package()` 生效——它不参与该类查找。

### 链接错误：`undefined reference to drogon::...`

`profiler_web` 不会传递 Drogon。补上：

```cmake
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE
    cpp-remote-profiler::profiler_web
    Drogon::Drogon)
```

### 找不到 gperftools

```
Could not find a package configuration file provided by "PkgConfig"
libprofiler not found
```

```bash
sudo apt-get install libgoogle-perftools-dev   # Ubuntu/Debian
sudo dnf install gperftools-devel              # Fedora
sudo yum install gperftools-devel              # CentOS/RHEL
```

本库只支持 Linux（依赖 `/proc` 与 `sigaction`），macOS 不可用。

### 头文件找不到

```
fatal error: profiler_manager.h: No such file or directory
```

```bash
ls /usr/local/include/cpp-remote-profiler/
```

正常情况下 `target_link_libraries(my_app PRIVATE cpp-remote-profiler::profiler_core)` 会自动带上
include 路径。只有绕过导入目标时才需要手工 `target_include_directories()`。

### 运行时报找不到动态库

```
error while loading shared libraries: libprofiler_core.so
```

```bash
# 临时
export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH

# 永久（注意路径要与实际 libdir 一致）
echo "/usr/local/lib64" | sudo tee /etc/ld.so.conf.d/cpp-remote-profiler.conf
sudo ldconfig
```

---

## 卸载

```bash
sudo rm -rf /usr/local/include/cpp-remote-profiler
sudo rm -rf /usr/local/lib64/cmake/cpp-remote-profiler    # 若为 lib 发行版则改 lib
sudo rm -f  /usr/local/lib64/libprofiler_core.*
sudo rm -f  /usr/local/lib64/libprofiler_web.*
sudo ldconfig
```

`lib` / `lib64` 两种路径都检查一下，避免残留的 package 配置文件继续被 `find_package()` 找到。

---

## 相关文档

- [安装指南](05_installation.md) — 三种安装/引入方式对比
- [API 参考手册](02_api_reference.md)
- [快速开始](01_quick_start.md)
