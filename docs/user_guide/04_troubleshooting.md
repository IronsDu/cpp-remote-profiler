# 故障排除指南

本文档帮助你诊断和解决使用 C++ Remote Profiler 时遇到的常见问题。

## 目录
- [编译问题](#编译问题)
- [链接问题](#链接问题)
- [运行时错误](#运行时错误)
- [符号化问题](#符号化问题)
- [性能问题](#性能问题)
- [Heap Profiling 问题](#heap-profiling-问题)
- [Web 界面问题](#web-界面问题)
- [多线程问题](#多线程问题)
- [获取帮助](#获取帮助)

---

## 编译问题

### 问题:找不到 `profiler_manager.h`

**错误信息**:
```
fatal error: profiler_manager.h: No such file or directory
```

**原因**:
- 编译器不知道头文件的位置
- 没有正确设置 include 路径

**解决方案**:

方法 1: 在 CMakeLists.txt 中添加 include 路径
```cmake
target_include_directories(your_app PRIVATE
    /path/to/cpp-remote-profiler/include
)
```

方法 2: 使用 CMake 的 include 目录
```cmake
include_directories(/path/to/cpp-remote-profiler/include)
```

方法 3: 如果已安装到系统
```cmake
find_package(cpp-remote-profiler REQUIRED)
target_link_libraries(your_app cpp-remote-profiler::cpp-remote-profiler)
```

---

### 问题:C++20 标准不支持

**错误信息**:
```
error: 'constexpr' does not name a type
error: expected initializer before '<<' token
```

**原因**:
- 编译器不支持 C++20 或未启用 C++20

**解决方案**:

在 CMakeLists.txt 中设置：
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

或手动指定：
```bash
g++ -std=c++20 your_file.cpp -o your_app
```

检查编译器版本：
```bash
g++ --version  # 需要 g++ 10.0+
clang++ --version  # 需要 clang++ 12.0+
```

---

### 问题:gperftools 找不到

**错误信息**:
```
Could NOT find PkgConfig
libprofiler not found
```

**原因**:
- 没有安装 gperftools 开发库

**解决方案**:

```bash
# Ubuntu/Debian
sudo apt-get install libgoogle-perftools-dev

# CentOS/RHEL
sudo yum install gperftools-devel

# Fedora
sudo dnf install gperftools-devel

# macOS
brew install gperftools
```

---

## 链接问题

### 问题:未定义的引用 (undefined reference)

**错误信息**:
```
undefined reference to `profiler::ProfilerManager::getInstance()'
```

**原因**:
- 没有链接 profiler_lib
- 链接顺序错误

**解决方案**:

确保在 CMakeLists.txt 中正确链接：
```cmake
target_link_libraries(your_app
    profiler_lib           # profiler 库
    ${GPERFTOOLS_LIBRARIES} # gperftools 库
    pthread                 # 线程库
)
```

链接顺序很重要：
```cmake
# 错误的顺序
target_link_libraries(your_app pthread profiler_lib)

# 正确的顺序
target_link_libraries(your_app profiler_lib pthread)
```

---

### 问题:动态链接库找不到

**错误信息**:
```
error while loading shared libraries: libtcmalloc.so.4
```

**原因**:
- 运行时找不到动态库

**解决方案**:

方法 1: 添加库路径到 LD_LIBRARY_PATH
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./your_app
```

方法 2: 在编译时设置 RPATH
```cmake
set(CMAKE_INSTALL_RPATH "/usr/local/lib")
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
```

方法 3: 使用静态链接
```cmake
target_link_libraries(your_app
    /usr/local/lib/libtcmalloc.a
    /usr/local/lib/libprofiler.a
)
```

---

## 运行时错误

### 问题:ProfilerStart 失败

**错误信息**:
```
ProfilerStart failed: File already exists
```

**原因**:
- profile 文件已存在
- 上一次的 profiler 没有正确停止

**解决方案**:

```cpp
// 方法 1: 删除旧文件
unlink("my_profile.prof");
profiler.startCPUProfiler("my_profile.prof");

// 方法 2: 使用时间戳生成唯一文件名
auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
std::string filename = "profile_" + std::to_string(timestamp) + ".prof";
profiler.startCPUProfiler(filename);

// 方法 3: 检查状态后再启动
if (!profiler.isProfilerRunning(profiler::ProfilerType::CPU)) {
    profiler.startCPUProfiler("my_profile.prof");
}
```

---

### 问题:段错误 (Segmentation fault)

**错误信息**:
```
Segmentation fault (core dumped)
```

**原因**:
- 多种可能：内存访问错误、空指针解引用等

**调试步骤**:

1. **使用 GDB 调试**:
```bash
gdb ./your_app
(gdb) run
# 等待崩溃
(gdb) backtrace
(gdb) info locals
```

2. **使用 Address Sanitizer**:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
```

3. **检查堆栈捕获信号冲突**:
```cpp
// 如果程序使用 SIGUSR1，改为其他信号
profiler::ProfilerManager::setStackCaptureSignal(SIGUSR2);
```

4. **查看核心转储**:
```bash
ulimit -c unlimited  # 启用核心转储
./your_app
# 崩溃后
gdb ./your_app core
```

---

### 问题:权限错误

**错误信息**:
```
Permission denied when opening profile file
```

**原因**:
- 没有写入目录的权限

**解决方案**:

```cpp
// 使用 /tmp 目录（通常总是可写）
profiler.startCPUProfiler("/tmp/my_profile.prof");

// 或使用用户主目录
std::string home = getenv("HOME");
std::string path = home + "/my_profile.prof";
profiler.startCPUProfiler(path);
```

检查目录权限：
```bash
ls -ld /path/to/profile/dir
chmod u+w /path/to/profile/dir
```

---

## 符号化问题

### 问题:火焰图显示地址而非函数名

**现象**:
```
0x7f8a4b2c3d10
0x7f8a4b2c3d20
```

**原因**:
- 编译时没有包含调试符号
- 二进制文件被 strip

**解决方案**:

1. **编译时添加调试符号**:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -O2")
```

2. **不使用 strip**:
```cmake
# 确保没有 strip 命令
# 或者使用 RelWithDebInfo 而非 Release
set(CMAKE_BUILD_TYPE RelWithDebInfo)
```

3. **验证调试符号**:
```bash
file your_app
# 应该看到 "not stripped" 和 "with debug_info"

nm your_app | grep main  # 应该能看到符号
```

---

### 问题:部分符号无法解析

**现象**:
```
?? ??:0
```

**原因**:
- 系统库或第三方库没有调试符号
- 内联函数
- 优化后的代码

**解决方案**:

1. **安装系统调试符号** (Ubuntu):
```bash
sudo apt-get install-dbgsym
sudo apt-get install libc6-dbg
```

2. **降低优化级别**:
```cmake
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
```

3. **这是正常的**:
   - 系统库函数通常无法符号化
   - 不影响应用代码的分析

---

## 性能问题

### 问题:Profiling 开销太大

**现象**:
- 启用 profiling 后程序运行速度明显变慢
- CPU 使用率过高

**原因**:
- 采样频率过高
- 频繁启动/停止 profiler
- 符号化开销

**解决方案**:

1. **降低采样频率** (如果支持):
```cpp
// gperftools 默认 100Hz (每 10ms 采样一次)
// 可以通过环境变量调整
export CPUPROFILE_FREQUENCY=50  // 50Hz
```

2. **避免频繁重启**:
```cpp
// 不好的做法
for (int i = 0; i < 100; i++) {
    profiler.startCPUProfiler();
    doWork();
    profiler.stopCPUProfiler();
}

// 好的做法
profiler.startCPUProfiler();
for (int i = 0; i < 100; i++) {
    doWork();
}
profiler.stopCPUProfiler();
```

3. **仅在需要时启用**:
```cpp
// 使用条件触发
if (shouldProfile()) {
    profiler.analyzeCPUProfile(10);
}
```

---

### 问题:内存使用过高

**现象**:
- Profiling 时内存占用持续增长
- 出现内存泄漏警告

**原因**:
- 符号化缓存未清理
- Profile 数据未释放

**解决方案**:

1. **定期重启 profiler**:
```cpp
// 每小时重启一次
while (true) {
    profiler.startCPUProfiler("hourly.prof");
    std::this_thread::sleep_for(std::chrono::hours(1));
    profiler.stopCPUProfiler();
}
```

2. **清理旧文件**:
```cpp
void cleanupOldProfiles(const std::string& dir, int keep_count) {
    // 删除旧的 profile 文件
}

// 定期清理
cleanupOldProfiles("/tmp/profiling", 10);
```

---

## Heap Profiling 问题

### 问题:Heap profiling 不工作

**现象**:
- `getRawHeapSample()` 返回空数据
- 没有生成 heap profile

**原因**:
- 没有设置 `TCMALLOC_SAMPLE_PARAMETER` 环境变量
- 没有链接 tcmalloc

**解决方案**:

1. **设置环境变量**:
```bash
export TCMALLOC_SAMPLE_PARAMETER=524288  # 512KB
./your_app
```

或在代码中设置：
```cpp
setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 1);
```

2. **确保链接 tcmalloc**:
```cmake
target_link_libraries(your_app
    profiler_lib
    tcmalloc  # 或 ${GPERFTOOLS_LIBRARIES}
    pthread
)
```

3. **验证 tcmalloc 已加载**:
```bash
ldd your_app | grep tcmalloc
# 应该看到 libtcmalloc.so.4
```

---

### 问题:Heap 数据不准确

**现象**:
- 显示的内存分配与实际不符
- 遗漏某些分配

**原因**:
- 采样间隔太大
- 采样时间太短

**解决方案**:

1. **调整采样间隔**:
```bash
# 更小的采样间隔 = 更准确但更高开销
export TCMALLOC_SAMPLE_PARAMETER=262144  # 256KB (更准确)
export TCMALLOC_SAMPLE_PARAMETER=1048576 # 1MB (更低开销)
```

2. **延长采样时间**:
```cpp
// Heap profiling 需要更长的采样时间才能获得有意义的数据
profiler.startHeapProfiler("heap.prof");
std::this_thread::sleep_for(std::chrono::minutes(5));  // 5 分钟
profiler.stopHeapProfiler();
```

---

## Web 界面问题

### 问题:无法访问 Web 界面

**现象**:
```
Connection refused
curl: (7) Failed to connect to localhost port 8080
```

**原因**:
- 服务器未启动
- 端口被占用
- 防火墙阻止

**解决方案**:

1. **检查服务器是否运行**:
```bash
ps aux | grep profiler_example
netstat -tuln | grep 8080
```

2. **检查端口占用**:
```bash
lsof -i :8080
# 如果被占用，停止占用进程或使用其他端口
```

3. **检查防火墙**:
```bash
sudo ufw status
sudo ufw allow 8080
```

---

### 问题:API 返回 500 错误

**错误信息**:
```
HTTP/1.1 500 Internal Server Error
```

**原因**:
- Profiler 启动失败
- 权限问题
- 依赖工具未安装

**调试步骤**:

1. **查看服务器日志**:
```bash
./profiler_example 2>&1 | tee server.log
```

2. **检查 pprof 工具**:
```bash
which pprof
# 如果未安装
go install github.com/google/pprof@latest
```

3. **检查 FlameGraph 工具**:
```bash
ls -l /tmp/FlameGraph/flamegraph.pl
# 如果不存在
git clone https://github.com/brendangregg/FlameGraph /tmp/FlameGraph
```

---

## 多线程问题

### 问题:死锁或挂起

**现象**:
- 程序在调用 profiler 时停止响应
- 线程死锁

**原因**:
- 在信号处理器中调用非异步信号安全的函数
- 多线程竞争

**解决方案**:

1. **避免在信号处理器中使用 profiler**:
```cpp
// 不好的做法
void signalHandler(int sig) {
    profiler.startCPUProfiler();  // 可能死锁
}

// 好的做法
// 使用单独的线程控制 profiler
```

2. **设置信号链**:
```cpp
profiler::ProfilerManager::setSignalChaining(true);
```

3. **使用不同的信号**:
```cpp
// 如果 SIGUSR1 冲突，改用其他信号
profiler::ProfilerManager::setStackCaptureSignal(SIGUSR2);
```

---

## 获取帮助

### 检查清单

在报告问题前，请检查：

1. ✅ 编译时使用了 `-g` 选项
2. ✅ 设置了 `TCMALLOC_SAMPLE_PARAMETER` (用于 heap profiling)
3. ✅ 链接了所有必需的库（profiler_lib, tcmalloc, pthread）
4. ✅ 服务器端口未被占用
5. ✅ 有足够的磁盘空间存储 profile 文件
6. ✅ 使用了支持的操作系统（Linux）

### 调试模式

启用详细日志：

```cpp
// 在启动时设置环境变量
setenv("VERBOSE_LOGGING", "1", 1);

// 或在 CMakeLists.txt 中
add_definitions(-DVERBOSE_LOGGING)
```

### 报告问题

如果问题仍未解决，请报告到 GitHub Issues，并包含：

1. **系统信息**:
```bash
uname -a
cmake --version
g++ --version
ldd --version
```

2. **编译命令**:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release ...
```

3. **完整错误信息**:
```bash
./your_app 2>&1 | tee error.log
```

4. **最小复现示例**:
```cpp
// 最小化的代码示例
#include "profiler_manager.h"

int main() {
    auto& profiler = profiler::ProfilerManager::getInstance();
    profiler.startCPUProfiler();
    // ...
    return 0;
}
```

---

## 常见问题 FAQ

### Q: 可以在 Windows 上使用吗？
**A**: 目前主要支持 Linux。Windows 支持正在开发中。可以使用 WSL2 作为替代方案。

### Q: 可以在生产环境使用吗？
**A**: 当前版本是 v0.x.x（开发阶段），不建议用于生产环境。等待 v1.0.0 稳定版。

### Q: Profiling 会影响性能吗？
**A**: CPU profiling 通常有 1-5% 的性能开销。Heap profiling 开销取决于采样频率。

### Q: 可以同时进行 CPU 和 Heap profiling 吗？
**A**: 可以，但会增加开销。建议分开进行。

### Q: Profile 文件很大怎么办？
**A**: 可以使用压缩：
```bash
gzip my_profile.prof
# 使用时解压
gunzip -c my_profile.prof.gz | pprof -http=:8080 -
```

---

## 更多信息

- 📖 [API 参考手册](02_api_reference.md)
- 💡 [集成示例](03_integration_examples.md)
- 🏠 [快速开始](01_quick_start.md)
