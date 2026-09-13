# 安装指南

本文档详细介绍如何编译、安装和集成 cpp-remote-profiler。

## 目录

- [方法概览](#方法概览)
- [方法 1: 从源码编译安装](#方法-1-从源码编译安装)
- [方法 2: FetchContent 集成](#方法-2-fetchcontent-集成)
- [方法 3: add_subdirectory 集成](#方法-3-add_subdirectory-集成)
- [验证安装](#验证安装)
- [故障排除](#故障排除)

---

## 方法概览

| 方法 | 适用场景 | 优点 | 缺点 |
|------|---------|------|------|
| **源码编译安装** | 系统级安装、多项目共享 | `find_package` 直接使用 | 需要手动管理更新 |
| **FetchContent** | 快速集成、单项目使用 | 无需预安装、自动下载 | 每次构建需重新编译 |
| **add_subdirectory** | 源码在同一工作区 | 完全控制、调试方便 | 需要管理源码位置 |

---

## 方法 1: 从源码编译安装

### 前置要求

- Linux 系统（已在 WSL2 和 Ubuntu 上测试）
- CMake 3.15+
- g++（支持 C++20）
- git

### 步骤 1: 安装系统依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential git pkg-config graphviz \
    libgoogle-perftools-dev
```

```bash
# Fedora/RHEL/CentOS
sudo dnf install -y \
    cmake gcc-c++ git pkg-config graphviz \
    gperftools-devel
```

### 步骤 2: 初始化 vcpkg

```bash
# 克隆项目
git clone https://github.com/IronsDu/cpp-remote-profiler.git
cd cpp-remote-profiler

# 初始化 vcpkg
if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
fi
```

> `vcpkg.json` 位于**仓库根目录**，因此不需要（也不能）在 `vcpkg/` 里执行 `./vcpkg install`
> ——那里没有清单文件。CMake 工具链会在首次 configure 时自动读取根目录清单并安装全部依赖。

### 步骤 3: 配置并编译

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release
cmake --build build -j$(nproc)
```

或使用与 CI 一致的预设（需 CMake 3.20+）：

```bash
cmake --preset=release
cmake --build build/release -j$(nproc)
```

可用的 CMake 选项：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_SHARED_LIBS` | `ON` | 构建动态库（`.so`），设为 `OFF` 则构建静态库（`.a`） |
| `REMOTE_PROFILER_INSTALL` | `ON` | 生成 install target |
| `REMOTE_PROFILER_BUILD_EXAMPLES` | `ON` | 构建示例程序 |
| `REMOTE_PROFILER_BUILD_TESTS` | `ON` | 构建测试程序 |
| `REMOTE_PROFILER_ENABLE_WEB` | `ON` | 启用 Web UI（依赖 Drogon） |
| `ENABLE_COVERAGE` | `OFF` | 启用代码覆盖率报告 |
| `BUILD_DOCS` | `OFF` | 构建 API 文档（需要 Doxygen） |

**常见配置示例**：

```bash
# 仅构建核心库（静态库，不含 Web UI）
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release \
    -DBUILD_SHARED_LIBS=OFF \
    -DREMOTE_PROFILER_ENABLE_WEB=OFF \
    -DREMOTE_PROFILER_BUILD_EXAMPLES=OFF \
    -DREMOTE_PROFILER_BUILD_TESTS=OFF
```

### 步骤 4: 安装

```bash
# 安装到默认路径（/usr/local）
sudo cmake --install .

# 或安装到指定路径
cmake --install . --prefix /opt/cpp-remote-profiler
```

安装后的文件布局（`<libdir>` 在 Debian/Ubuntu 上为 `lib`，在 Fedora/RHEL 上为 `lib64`）：

```
<prefix>/
├── include/cpp-remote-profiler/    # 头文件
│   ├── profiler_manager.h
│   ├── profiler_version.h
│   ├── version.h
│   ├── http_handlers.h             # 扁平副本
│   ├── log_sink.h                  # 扁平副本
│   └── profiler/
│       ├── http_handlers.h
│       ├── log_sink.h
│       └── drogon_adapter.h        # 仅启用 Web 时安装
├── <libdir>/
│   ├── libprofiler_core.so         # 核心库
│   ├── libprofiler_web.so          # Web 库（如果启用）
│   └── cmake/cpp-remote-profiler/  # CMake 配置文件
│       ├── cpp-remote-profiler-config.cmake
│       ├── cpp-remote-profiler-config-version.cmake
│       └── cpp-remote-profiler-targets.cmake
└── share/doc/cpp-remote-profiler/  # 文档（如果启用 BUILD_DOCS）
```

### 步骤 5: 在项目中使用

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpp-remote-profiler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cpp-remote-profiler::profiler_core)
```

如果安装到非标准路径，用 `CMAKE_PREFIX_PATH` 告诉 CMake 去哪里找：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/cpp-remote-profiler
```

> 静态库（`BUILD_SHARED_LIBS=OFF`）的导出目标会带上 `$<LINK_ONLY:...>` 形式的私有依赖，
> 下游链接时需要能找到 Backward / Abseil 等包。若遇到 "target was not found" 类错误，
> 请改用共享库构建，或确保这些依赖在工具链中可见。

---

## 方法 2: FetchContent 集成

使用 CMake 内置的 FetchContent，无需预编译安装，适合快速集成。

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 关闭 cpp-remote-profiler 自带的 examples/tests/install，避免冲突
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")

FetchContent_Declare(
    cpp-remote-profiler
    GIT_REPOSITORY https://github.com/IronsDu/cpp-remote-profiler.git
    GIT_TAG        main        # 仓库目前没有 tag；固定 commit 哈希更利于复现
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cpp-remote-profiler)

add_executable(my_app main.cpp)

# 源码内构建时使用无命名空间的目标名
target_link_libraries(my_app PRIVATE profiler_core)

# 如果需要 Web UI（需 Drogon::Drogon，且不会自动传递）
# target_link_libraries(my_app PRIVATE profiler_web Drogon::Drogon)
```

> ⚠️ 仍需要 vcpkg 工具链（或系统内已有 gtest/Backward/Abseil/OpenSSL/zlib）：
> `-DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake`。
> `REMOTE_PROFILER_BUILD_TESTS=OFF` 只是不构建测试，不会跳过这些 `find_package()`。
>
> 示例工程见 `cmake/examples/test_fetch_content/`（该示例实际以 `add_subdirectory`
> 方式引入本仓库，以便在 CI 中直接测试源码树）。

---

## 方法 3: add_subdirectory 集成

将源码放在项目目录中，通过 `add_subdirectory` 引入。

### 目录结构

```
my_project/
├── CMakeLists.txt
├── main.cpp
└── third_party/
    └── cpp-remote-profiler/   # git submodule 或源码
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关闭 cpp-remote-profiler 自带的 examples/tests/install
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")

add_subdirectory(third_party/cpp-remote-profiler)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE profiler_core)
```

> 完整示例参考 `cmake/examples/test_fetch_content/` 目录（该示例使用 `add_subdirectory` 进行 CI 测试）。

---

## 验证安装

### 使用 find_package 验证（方法 1 安装后）

创建 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.20)
project(test_install CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(cpp-remote-profiler REQUIRED)
add_executable(test_install main.cpp)
target_link_libraries(test_install PRIVATE cpp-remote-profiler::profiler_core)
```

创建 `main.cpp`：

```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    profiler::ProfilerManager profiler;
    std::cout << "Installation OK" << std::endl;
    return 0;
}
```

编译运行：

```bash
cmake -S . -B build
cmake --build build
./build/test_install
```

预期输出：

```
Installation OK
```

### 检查安装文件

注意 `<libdir>` 在 Fedora/RHEL 上是 `lib64`：

```bash
ls /usr/local/lib*/libprofiler_core.*
ls /usr/local/include/cpp-remote-profiler/
ls /usr/local/lib*/cmake/cpp-remote-profiler/
```

---

## 故障排除

### 找不到共享库

**错误**：
```
error while loading shared libraries: libprofiler_core.so: cannot open shared object file
```

**解决方案**：

```bash
# 方法 1: 添加到 LD_LIBRARY_PATH（临时；lib64 发行版请相应替换）
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 方法 2: 永久添加
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/cpp-remote-profiler.conf
sudo ldconfig
```

### CMake 找不到包

**错误**：
```
Could not find a package configuration file provided by "cpp-remote-profiler"
```

**解决方案**：

```bash
# 指定安装路径前缀（推荐）
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/cpp-remote-profiler

# 或直接指定 config 目录（注意 <libdir> 可能是 lib64）
cmake -S . -B build \
  -Dcpp-remote-profiler_DIR=/usr/local/lib/cmake/cpp-remote-profiler
```

### FetchContent 编译失败

**可能原因**：缺少 gtest / Backward / Abseil / OpenSSL / zlib（即使关掉测试与 Web 也会查找）

**解决方案**：挂上 vcpkg 工具链：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

或用 `REMOTE_PROFILER_ENABLE_WEB=OFF` 去掉 Drogon 依赖：

```cmake
set(REMOTE_PROFILER_ENABLE_WEB OFF CACHE BOOL "")
```

### gperftools 相关链接错误

**错误**：
```
undefined reference to `ProfilerStart'
```

**解决方案**：确保系统安装了 gperftools：

```bash
# Ubuntu/Debian
sudo apt-get install libgoogle-perftools-dev

# Fedora/RHEL
sudo dnf install gperftools-devel
```

---

## 更多信息

- [快速开始指南](01_quick_start.md)
- [API 参考手册](02_api_reference.md)
- [集成示例](03_integration_examples.md)
- [使用 find_package 集成](06_using_find_package.md)
- [返回文档首页](../README.md)
