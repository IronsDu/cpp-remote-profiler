# 安装指南

本文档详细介绍如何在不同环境下安装和集成 cpp-remote-profiler。

## 目录
- [方法概览](#方法概览)
- [方法 1: 使用 vcpkg](#方法-1-使用-vcpkg)
- [方法 2: 使用 Conan](#方法-2-使用-conan)
- [方法 3: 使用 FetchContent](#方法-3-使用-fetchcontent)
- [方法 4: 手动编译](#方法-4-手动编译)
- [方法 5: 系统包管理器](#方法-5-系统包管理器)
- [验证安装](#验证安装)
- [故障排除](#故障排除)

---

## 方法概览

| 方法 | 推荐度 | 适用场景 | 优点 | 缺点 |
|------|-------|---------|------|------|
| **vcpkg** | ⭐⭐⭐⭐⭐ | 跨平台项目 | 依赖管理完善、版本控制好 | 首次配置较慢 |
| **Conan** | ⭐⭐⭐⭐ | 企业环境 | 二进制缓存、灵活配置 | 学习曲线稍陡 |
| **FetchContent** | ⭐⭐⭐⭐ | 快速原型 | 无需预安装、最简单 | 每次重新编译 |
| **手动编译** | ⭐⭐⭐ | 定制需求 | 完全控制 | 手动管理依赖 |
| **系统包管理器** | ⭐⭐ | 生产部署 | 集成系统 | 版本可能滞后 |

---

## 方法 1: 使用 vcpkg

vcpkg 是微软开发的跨平台 C++ 包管理器，推荐使用。

### 步骤 1: 安装 vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Linux/macOS
./bootstrap-vcpkg.sh

# Windows
.\bootstrap-vcpkg.bat

# 添加到 PATH (可选)
export PATH=$PATH:$(pwd)
```

### 步骤 2: 安装 cpp-remote-profiler

#### 方法 A: 从端口安装（推荐）

```bash
# 方法 1: 使用 overlay ports
cd /path/to/your/project
vcpkg install cpp-remote-profiler --overlay-ports=/path/to/cpp-remote-profiler/ports

# 方法 2: 添加到 vcpkg registry
mkdir -p ~/.vcpkg-registries
git clone https://github.com/your-org/cpp-remote-profiler-vcpkg ~/.vcpkg-registries/cpp-remote-profiler

# 修改 vcpkg-configuration.json 添加 registry
```

#### 方法 B: 作为子模块使用

```bash
# 将 cpp-remote-profiler 添加为 git submodule
cd your_project
git submodule add https://github.com/your-org/cpp-remote-profiler.git third_party/cpp-remote-profiler

# 安装依赖
cd third_party/cpp-remote-profiler
vcpkg install --triplet=x64-linux-release
```

### 步骤 3: 在 CMake 项目中使用

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_TOOLCHAIN_FILE /path/to/vcpkg/scripts/buildsystems/vcpkg.cmake)

find_package(cpp-remote-profiler CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app cpp-remote-profiler::profiler_lib)
```

### 步骤 4: 编译和运行

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
make
./my_app
```

---

## 方法 2: 使用 Conan

Conan 是另一个流行的 C++ 包管理器，特别适合企业环境。

### 步骤 1: 安装 Conan

```bash
# 安装 Conan 2.x
pip install conan

# 或使用 pip3
pip3 install conan

# 验证安装
conan --version
```

### 步骤 2: 创建 Conan 配置

```bash
# 创建新配置
conan config init

# 添加 cpp-remote-profiler registry (如果已发布)
conan remote add cpp-remote-profiler https://your-conan-repo.com
```

### 步骤 3: 安装 cpp-remote-profiler

#### 方法 A: 从 Conan Center 安装

```bash
# 搜索包
conan search cpp-remote-profiler --remote=conancenter

# 安装
conan install --requires=cpp-remote-profiler/0.1.0
```

#### 方法 B: 从本地文件构建

```bash
cd /path/to/cpp-remote-profiler

# 创建包
conan create . \
    --build=missing \
    -s build_type=Release \
    -o cpp-remote-profiler/*:shared=True
```

### 步骤 4: 在 CMake 项目中使用

#### 方法 A: 使用 conan-cmake integration

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

find_package(CPPRemoteProfiler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app CPPRemoteProfiler::CPPRemoteProfiler)
```

#### 方法 B: 使用生成器

```bash
# 生成 conanbuildinfo.cmake
conan install . --output-folder=build --build=missing

# 在 CMakeLists.txt 中
include(${CMAKE_BINARY_DIR}/conan/conan_toolchain.cmake)
```

---

## 方法 3: 使用 FetchContent

FetchContent 是 CMake 内置的功能，无需额外安装包管理器。

### 优点
- ✅ 无需预安装任何包管理器
- ✅ 自动下载和集成
- ✅ 适合快速原型开发

### 缺点
- ❌ 每次都需要重新编译
- ❌ 无法跨项目共享二进制

### 使用步骤

#### 步骤 1: 修改 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# 包含 FetchContent
include(FetchContent)

# 声明 cpp-remote-profiler
FetchContent_Declare(
    cpp-remote-profiler
    GIT_REPOSITORY https://github.com/your-org/cpp-remote-profiler.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE
)

# 配置选项
set(REMOTE_PROFILER_BUILD_EXAMPLES OFF CACHE BOOL "")
set(REMOTE_PROFILER_BUILD_TESTS OFF CACHE BOOL "")
set(REMOTE_PROFILER_INSTALL OFF CACHE BOOL "")

# 获取并包含
FetchContent_MakeAvailable(cpp-remote-profiler)

# 你的可执行文件
add_executable(my_app main.cpp)

# 链接库
target_link_libraries(my_app
    profiler_lib
    Drogon::Drogon  # 如果使用 Web 功能
)
```

#### 步骤 2: 创建 main.cpp

```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();
    std::cout << "Profiler version: " << REMOTE_PROFILER_VERSION << std::endl;

    profiler.startCPUProfiler("my_profile.prof");
    // ... your code ...
    profiler.stopCPUProfiler();

    return 0;
}
```

#### 步骤 3: 编译

```bash
mkdir build && cd build
cmake ..
make
./my_app
```

### 完整示例

参考 `cmake/examples/` 目录中的完整示例项目。

---

## 方法 4: 手动编译

手动编译提供了最大的灵活性，适合需要定制化的场景。

### 步骤 1: 安装系统依赖

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential git pkg-config graphviz \
    libgoogle-perftools-dev libssl-dev zlib1g-dev
```

#### CentOS/RHEL/Fedora

```bash
# Fedora
sudo dnf install -y \
    cmake gcc-c++ git pkg-config graphviz \
    gperftools-devel openssl-devel zlib-devel

# CentOS/RHEL
sudo yum install -y \
    cmake gcc-c++ git pkgconfig graphviz \
    gperftools-devel openssl-devel zlib-devel
```

#### macOS

```bash
brew install cmake gperftools openssl pkg-config graphviz
```

### 步骤 2: 克隆并编译

```bash
# 克隆仓库
git clone https://github.com/your-org/cpp-remote-profiler.git
cd cpp-remote-profiler

# 编译
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON

make -j$(nproc)
```

### 步骤 3: 安装（可选）

```bash
sudo make install

# 默认安装路径：
#   /usr/local/lib/libprofiler_lib.so
#   /usr/local/include/cpp-remote-profiler/
```

### 步骤 4: 在你的项目中使用

#### 方法 A: 使用已安装的库

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PROFILER REQUIRED cpp-remote-profiler)

add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE ${PROFILER_INCLUDE_DIRS})
target_link_libraries(my_app ${PROFILER_LIBRARIES})
```

#### 方法 B: 直接链接源码

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

# 添加 cpp-remote-profiler 作为子目录
add_subdirectory(/path/to/cpp-remote-profiler build/cpp-remote-profiler)

add_executable(my_app main.cpp)
target_link_libraries(my_app profiler_lib)
```

---

## 方法 5: 系统包管理器

某些 Linux 发行版可能提供系统包。

### Ubuntu (PPA)

```bash
sudo add-apt-repository ppa:your-org/cpp-remote-profiler
sudo apt-get update
sudo apt-get install libcpp-remote-profiler-dev
```

### Arch Linux (AUR)

```bash
yay -S cpp-remote-profiler
# 或
paru -S cpp-remote-profiler
```

### Fedora

```bash
sudo dnf install cpp-remote-profiler-devel
```

---

## 验证安装

### 检查库文件

```bash
# 检查动态库
ldconfig -p | grep profiler_lib

# 或检查特定路径
ls -l /usr/local/lib/libprofiler_lib.*
```

### 检查头文件

```bash
ls -l /usr/local/include/cpp-remote-profiler/
# 应该看到：
#   profiler_manager.h
#   symbolize.h
#   web_server.h
#   version.h
```

### 测试程序

创建测试文件 `test_install.cpp`:

```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();

    std::cout << "C++ Remote Profiler" << std::endl;
    std::cout << "Version: " << REMOTE_PROFILER_VERSION << std::endl;
    std::cout << "Installation successful!" << std::endl;

    return 0;
}
```

编译并运行：

```bash
g++ -std=c++20 test_install.cpp -lprofiler_lib -o test_install
./test_install
```

预期输出：
```
C++ Remote Profiler
Version: 0.1.0
Installation successful!
```

---

## 故障排除

### 问题 1: 找不到库文件

**错误**:
```
error while loading shared libraries: libprofiler_lib.so: cannot open shared object file
```

**解决方案**:

```bash
# 方法 1: 添加到 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 方法 2: 永久添加
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/cpp-remote-profiler.conf
sudo ldconfig
```

---

### 问题 2: 找不到头文件

**错误**:
```
fatal error: profiler_manager.h: No such file or directory
```

**解决方案**:

```cmake
# 在 CMakeLists.txt 中明确指定
target_include_directories(your_app PRIVATE /usr/local/include/cpp-remote-profiler)
```

---

### 问题 3: CMake 找不到包

**错误**:
```
Could not find package cpp-remote-profiler
```

**解决方案**:

```bash
# vcpkg: 确保设置了工具链文件
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Conan: 先运行 conan install
conan install . --output-folder=build

# FetchContent: 检查网络连接和 Git 可用性
git --version
```

---

### 问题 4: 依赖冲突

**错误**:
```
Found both "libprofiler.a" and "libprofiler.so"
```

**解决方案**:

```cmake
# 明确指定使用共享库或静态库
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

# 或在 find_package 前设置
set(CMAKE_FIND_LIBRARY_SUFFIXES .so)
```

---

### 问题 5: 编译错误

**错误**:
```
error: 'std::shared_ptr' does not name a type
```

**原因**: C++ 标准版本不对

**解决方案**:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

## 最佳实践

### 1. 生产环境

```bash
# 使用固定版本
vcpkg install cpp-remote-profiler:x64-linux-release@0.1.0

# 或使用锁文件
conan lock create conanfile.py --user=your-org --channel=stable
```

### 2. 开发环境

```bash
# 使用 FetchContent，最简单
# 或使用 git submodules
git submodule add https://github.com/your-org/cpp-remote-profiler.git third_party/cpp-remote-profiler
```

### 3. CI/CD

```yaml
# GitHub Actions 示例
- name: Install dependencies
  run: |
    vcpkg install cpp-remote-profiler --triplet=x64-linux-release

- name: Configure CMake
  run: |
    cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

---

## 卸载

### vcpkg

```bash
vcpkg remove cpp-remote-profiler --triplet=x64-linux-release
```

### Conan

```bash
conan remove cpp-remote-profiler --all
```

### 手动安装

```bash
sudo rm -rf /usr/local/include/cpp-remote-profiler
sudo rm -f /usr/local/lib/libprofiler_lib.*
sudo ldconfig
```

---

## 更多信息

- 📖 [快速开始指南](01_quick_start.md)
- 📖 [API 参考手册](02_api_reference.md)
- 📖 [集成示例](03_integration_examples.md)
- 🏠 [返回文档首页](../README.md)
