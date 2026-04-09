# 使用 find_package 集成

本文档介绍如何通过 `find_package` 使用已安装的 cpp-remote-profiler。

## 目录
- [安装库](#安装库)
- [基本用法](#基本用法)
- [使用 Web 功能](#使用-web-功能)
- [完整示例](#完整示例)
- [故障排除](#故障排除)

---

## 安装库

首先需要编译并安装 cpp-remote-profiler：

```bash
# 1. 克隆或下载源码
git clone https://github.com/your-org/cpp-remote-profiler.git
cd cpp-remote-profiler

# 2. 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 安装（默认安装到 /usr/local）
sudo make install

# 或安装到自定义目录
sudo make install DESTDIR=/opt/cpp-remote-profiler
```

安装后的文件结构：
```
/usr/local/
├── lib/
│   ├── libprofiler_core.so           # 动态库
│   └── cmake/
│       └── cpp-remote-profiler/
│           ├── cpp-remote-profiler-config.cmake
│           ├── cpp-remote-profiler-config-version.cmake
│           └── cpp-remote-profiler-targets.cmake
└── include/
    └── cpp-remote-profiler/
        ├── profiler_manager.h
        ├── profiler_version.h
        ├── version.h
        ├── profiler/
        │   ├── drogon_adapter.h
        │   ├── http_handlers.h
        │   ├── log_sink.h
        └── version.h
```

---

## 基本用法

### 方法 1: 使用 CMAKE_PREFIX_PATH

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# 指定安装路径
set(CMAKE_PREFIX_PATH "/usr/local")  # 或 "/opt/cpp-remote-profiler/usr/local"

# 查找包
find_package(cpp-remote-profiler REQUIRED)

# 创建可执行文件
add_executable(my_app main.cpp)

# 链接库
target_link_libraries(my_app cpp-remote-profiler::profiler_core)
```

### 方法 2: 使用 CMAKE_MODULE_PATH

```cmake
# 指定 CMake 模块路径
list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/cpp-remote-profiler")

find_package(cpp-remote-profiler REQUIRED)
```

### 方法 3: 设置环境变量

```bash
# 设置 CMAKE_PREFIX_PATH 环境变量
export CMAKE_PREFIX_PATH=/usr/local
cmake ..

# 或在 cmake 命令行中指定
cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

---

## 使用 Web 功能

### 重要说明

cpp-remote-profiler 的核心库包含了 Web 服务器功能（基于 Drogon）。如果你想使用这些功能，需要：

1. **安装 Drogon**（如果系统中没有）
2. **在 CMakeLists.txt 中链接 Drogon**

### 示例：使用 Web 功能

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# 查找 Drogon
find_package(Drogon REQUIRED)

# 查找 cpp-remote-profiler
find_package(cpp-remote-profiler REQUIRED)

# 创建可执行文件
add_executable(my_app main.cpp)

# 链接库（使用 profiler_web 自动带上 Drogon）
target_link_libraries(my_app
    cpp-remote-profiler::profiler_web
)
```

### 如果不使用 Web 功能

如果你只想使用核心 profiling 功能，不使用 Web 界面，可以只链接 gperftools：

```cmake
# 使用 pkg-config 查找 gperftools
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

add_executable(my_app main.cpp)

target_link_libraries(my_app
    cpp-remote-profiler::profiler_core
    ${GPERFTOOLS_LIBRARIES}
)
```

---

## 完整示例

### 示例 1: 最简单的使用

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.15)
project(SimpleProfiler)

set(CMAKE_CXX_STANDARD 20)

find_package(cpp-remote-profiler REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app cpp-remote-profiler::profiler_core)
```

**main.cpp**:
```cpp
#include "profiler_manager.h"
#include <iostream>

int main() {
    profiler::ProfilerManager profiler;

    std::cout << "Profiler Version: " << REMOTE_PROFILER_VERSION << std::endl;

    profiler.startCPUProfiler("my_profile.prof");

    // 你的代码...

    profiler.stopCPUProfiler();

    return 0;
}
```

**编译和运行**:
```bash
mkdir build && cd build
cmake ..
make
./app
```

### 示例 2: 使用 Web 界面

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.15)
project(WebProfilerApp)

set(CMAKE_CXX_STANDARD 20)

# 查找依赖
find_package(Drogon REQUIRED)
find_package(cpp-remote-profiler REQUIRED)

add_executable(web_app main.cpp)

target_link_libraries(web_app
    cpp-remote-profiler::profiler_web
)
```

**main.cpp**:
```cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <iostream>

int main() {
    profiler::ProfilerManager profiler;

    // 注册 Web 界面到 Drogon
    profiler::registerDrogonHandlers(profiler);

    std::cout << "Web UI: http://localhost:8080" << std::endl;

    // 启动服务器
    drogon::app().addListener("0.0.0.0", 8080).run();

    return 0;
}
```

### 示例 3: 从 vcpkg 安装后使用

如果你使用 vcpkg 安装 cpp-remote-profiler：

```bash
# 通过 vcpkg 安装
vcpkg install cpp-remote-profiler --triplet=x64-linux-release
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_TOOLCHAIN_FILE /path/to/vcpkg/scripts/buildsystems/vcpkg.cmake)

find_package(cpp-remote-profiler CONFIG REQUIRED)
find_package(Drogon CONFIG REQUIRED)

add_executable(my_app main.cpp)

target_link_libraries(my_app
    cpp-remote-profiler::cpp-remote-profiler
    Drogon::Drogon
)
```

---

## 版本检查

### 检查特定版本

```cmake
# 要求至少 0.1.0 版本
find_package(cpp-remote-profiler 0.1.0 REQUIRED)

# 要求特定版本范围
find_package(cpp-remote-profiler 0.1.0 EXACT REQUIRED)
```

### 在代码中检查版本

```cpp
#include "version.h"

#if REMOTE_PROFILER_VERSION >= REMOTE_PROFILER_VERSION_INT(0, 2, 0)
    // 使用 0.2.0 的新 API
#else
    // 使用旧 API
#endif
```

---

## 故障排除

### 问题 1: 找不到包

**错误**:
```
Could not find package cpp-remote-profiler
```

**解决方案**:

```bash
# 检查安装路径
ls /usr/local/lib/cmake/cpp-remote-profiler/

# 指定 CMAKE_PREFIX_PATH
cmake .. -DCMAKE_PREFIX_PATH=/usr/local

# 或设置环境变量
export CMAKE_PREFIX_PATH=/usr/local
cmake ..
```

---

### 问题 2: 链接错误 - 找不到 Drogon

**错误**:
```
undefined reference to `drogon::...`
```

**原因**: profiler_web 依赖 Drogon，需要链接 Drogon

**解决方案**:

```cmake
# 在 CMakeLists.txt 中添加 Drogon
find_package(Drogon REQUIRED)
target_link_libraries(my_app
    cpp-remote-profiler::profiler_core
    Drogon::Drogon  # 添加这一行
)
```

---

### 问题 3: 找不到 gperftools

**错误**:
```
Could NOT find PkgConfig
libprofiler not found
```

**解决方案**:

```bash
# 安装 gperftools
sudo apt-get install libgoogle-perftools-dev  # Ubuntu/Debian
sudo yum install gperftools-devel              # CentOS/RHEL
brew install gperftools                         # macOS
```

---

### 问题 4: 头文件找不到

**错误**:
```
fatal error: profiler_manager.h: No such file or directory
```

**解决方案**:

```cmake
# 检查 include 目录是否正确安装
ls /usr/local/include/cpp-remote-profiler/

# 或明确指定
target_include_directories(my_app PRIVATE /usr/local/include/cpp-remote-profiler)
```

---

### 问题 5: 动态库找不到

**错误**:
```
error while loading shared libraries: libprofiler_core.so
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

## 与其他包管理器集成

### 与 vcpkg 集成

```bash
# 安装
vcpkg install cpp-remote-profiler --triplet=x64-linux-release

# 使用
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 与 Conan 集成

```bash
# 安装
conan install --requires=cpp-remote-profiler/0.1.0

# 使用（Conan 会自动生成配置）
conan install . --output-folder=build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
```

---

## 升级库

当升级到新版本时：

```bash
# 1. 下载新版本
cd cpp-remote-profiler
git pull

# 2. 重新编译
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 重新安装
sudo make install

# 4. 重新编译你的项目
cd /path/to/your/project
rm -rf build
cmake ..
make
```

---

## 卸载

```bash
# 删除安装的文件
sudo rm -rf /usr/local/include/cpp-remote-profiler
sudo rm -f /usr/local/lib/libprofiler_core.*
sudo rm -f /usr/local/lib/libprofiler_web.*
sudo rm -rf /usr/local/lib/cmake/cpp-remote-profiler

# 更新动态链接库缓存
sudo ldconfig
```

---

## 更多信息

- 📖 [安装指南](05_installation.md)
- 📖 [API 参考手册](02_api_reference.md)
- 📖 [快速开始](01_quick_start.md)
- 🏠 [返回文档首页](../README.md)
