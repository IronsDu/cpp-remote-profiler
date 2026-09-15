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
target_link_libraries(your_app PRIVATE cpp-remote-profiler::profiler_core)
```

方法 1 和 2 只解决了头文件路径问题，**不会**带来 gperftools 等链接依赖；
推荐始终使用 `find_package()` + 导入目标（方法 3）。

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
undefined reference to `profiler::ProfilerManager::startCPUProfiler(...)'
```

**原因**:
- 没有链接 profiler_core
- 链接顺序错误

**解决方案**:

确保在 CMakeLists.txt 中正确链接：
```cmake
target_link_libraries(your_app
    cpp-remote-profiler::profiler_core  # profiler 核心库
)
```

确保使用正确的 CMake 目标名：
```cmake
# 核心功能
target_link_libraries(your_app cpp-remote-profiler::profiler_core)

# 如果需要 Web 界面
target_link_libraries(your_app cpp-remote-profiler::profiler_web)
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
sudo apt-get install libc6-dbg        # glibc 调试符号
# 需要任意包的 -dbgsym 时，先装 debian-goodies 才有 apt-get install-dbgsym
sudo apt-get install debian-goodies
sudo apt-get install-dbgsym <package>
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

1. **设置环境变量**（必须在**进程启动前**，见下方说明）:
```bash
export TCMALLOC_SAMPLE_PARAMETER=524288  # 512KB
./your_app
```

> ⚠️ **不能在代码里设置**。tcmalloc 在进程初始化阶段通过 `GetenvBeforeMain()` 读取该变量
> （`gperftools-src/src/sampler.cc:74-75`），其默认值为 `0`，即**默认关闭采样**。
> 因此 `setenv("TCMALLOC_SAMPLE_PARAMETER", ...)` 写在 `main()` 里已经太晚，不会生效。
> 只能通过 shell 环境变量、容器 env、systemd `Environment=` 或进程启动器传入。

2. **确保链接 tcmalloc**:
```cmake
target_link_libraries(your_app
    cpp-remote-profiler::profiler_core
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

2. **检查工作目录下的 pprof 脚本**:

本库**不需要**外部安装 pprof —— `ProfilerManager` 构造时会把内置的 pprof 脚本写到进程当前工作目录：

```bash
ls -l ./pprof ./flamegraph.pl
# 若不存在，说明工作目录不可写（例如从 / 或只读挂载启动）
# 检查: touch ./writetest && rm ./writetest
```

真正的运行时依赖是 **perl**（pprof 与 flamegraph.pl 都是 Perl 脚本）：

```bash
perl --version
# 未安装: sudo apt-get install -y perl
```

> `go install github.com/google/pprof@latest` 安装的 `pprof` 是**另一个工具**（Go 版），
> 用于分析 `/pprof/*` 接口导出的原始 profile 文件，与这里的 `./pprof` 脚本无关。

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
3. ✅ 链接了所有必需的库（profiler_core, tcmalloc, pthread）
4. ✅ 服务器端口未被占用
5. ✅ 有足够的磁盘空间存储 profile 文件
6. ✅ 使用了支持的操作系统（Linux）

### 调试模式

启用详细日志：

```cpp
#include "profiler_manager.h"

int main() {
    profiler::ProfilerManager profiler;

    // 提高日志级别（默认 Info）
    profiler.setLogLevel(profiler::LogLevel::Debug);   // 或 LogLevel::Trace

    // 可选：把日志接入你自己的日志系统
    profiler.setLogSink(std::make_shared<MyLogSink>());

    // ...
}
```

默认 sink 会把 `Trace`/`Debug`/`Info` 写到 stdout、`Warning` 及以上写到 stderr，
因此直接 `./your_app 2>&1 | tee app.log` 就能拿到完整日志。

> 本库没有 `VERBOSE_LOGGING` 环境变量或同名编译宏，日志行为只由 `setLogLevel()` / `setLogSink()` 控制。

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
    profiler::ProfilerManager profiler;
    profiler.startCPUProfiler();
    // ...
    return 0;
}
```

---

## 常见问题 FAQ

### Q: 支持 macOS / Windows 吗？
**A**: 不支持。实现依赖 Linux 专有接口：`/proc/self/exe` 与 `/proc/self/task`（前者用于 `getExecutablePath()`，后者用于 `captureAllThreadStacks()`）、`sigaction`/`ucontext` 信号栈捕获。WSL2 可用。

### Q: 可以在生产环境使用吗？
**A**: 当前版本是 v0.x.x（开发阶段），不建议用于生产环境。等待 v1.0.0 稳定版。

### Q: Profiling 会影响性能吗？
**A**: CPU profiling 通常有 1-5% 的性能开销；采样频率可用 `CPUPROFILE_FREQUENCY` 调整（gperftools 默认 100Hz）。Heap profiling 开销取决于 `TCMALLOC_SAMPLE_PARAMETER` 的采样间隔。

### Q: 可以同时进行 CPU 和 Heap profiling 吗？
**A**: CPU 与 Heap 是两套独立的 profiling 状态，可以并存。但**同一时刻只能有一个 CPU 采样会话**：`analyzeCPUProfile()` / `getRawCPUProfile()` 会先原子认领会话，认领失败即拒绝，**不会**打断正在采样的另一方（包括宿主用 `startCPUProfiler()` 开的会话）。

### Q: Heap 窗口式采集的 `duration` 是什么意思？
**A**: 它决定**采集多久**（覆盖度），不是**采样率**（精度）。gperftools 的 heap profiling 按分配驱动：
`HeapProfilerStart()` 开始记录、`HeapProfilerDump()` 写出快照；采样率由**进程启动时**的
`TCMALLOC_SAMPLE_PARAMETER` 决定，运行时不可调。

所以 `duration` 越长，覆盖到的分配越多——分配稀疏的进程需要更长窗口才采得到东西。HTTP 层默认
**10 秒**（实测 3 秒窗口失败率 35–45%）。要提高**采样精度**则必须**在启动前**设置
`TCMALLOC_SAMPLE_PARAMETER=524288`（默认 `0` 表示不采样，此时快照会明显偏稀疏）。

### Q: heap 图是空的 / 报 "No heap profile data was produced"？
**A**: 说明该采集窗口内进程**没有发生内存分配**。heap profiler 记录的是**窗口内的分配事件**，不是当前堆的内容，所以不会凭空造出数据。请确保被分析进程正处于负载中，或在窗口内制造真实分配后再试。

### Q: 进程内存很高，为什么 heap 图却是空的？
**A**: `/api/pprof/heap` 是**窗口式**的：只记录采集窗口内**新发生**的分配（流量），不反映此刻堆里的存量。`duration` 决定采集多久，默认 10 秒——实测 3 秒窗口有 35–45% 的请求采不到样本而失败。如果内存是在采样开始前分配的、窗口内已停止分配，结果就是空的。

想看**当前堆的累计快照**，请改用**状态式**的 `/pprof/heap`：

```bash
# 需在启动前设置该变量（默认 0 = 关闭采样）
export TCMALLOC_SAMPLE_PARAMETER=524288
curl 'http://localhost:8080/pprof/heap' -o heap.prof
go tool pprof -http=:8081 ./your_app heap.prof
```

两者语义相反、不可互相替代；详见 [README 的说明](../../README.md#heap-的两个端点语义相反)。

### Q: 为什么长时间采样期间，其它接口还能正常返回？
**A**: 因为 Drogon 适配层不把 profiling 放在事件循环线程上执行。所有会产生图表的接口（`/pprof/profile`、`/api/pprof/{cpu,heap,growth}`、`/api/pprof/heap/snapshot`）都被投递到一个后台工作线程，完成后用 `queueInLoop()` 把响应送回事件循环。因此一个 300 秒的采样不会拖住 `/api/status` 或首页。

### Q: 并发请求 CPU 采样时返回 409 / 500 "cpu profiling already in use"
**A**: 这是**有意的拒绝**，不是故障。CPU 采样是独占的（gperftools 的采样会话是进程级全局状态），所以已有采样进行时，第二个请求会立即失败而**不会排队等待**——避免用户以为"已经在采样了"，实际却要排在别人后面等几十秒到几分钟。

具体响应与 Go 的 `net/http/pprof` 对齐：

- `/pprof/profile` → `500` + `text/plain` + `Could not enable CPU profiling: cpu profiling already in use`，并带 `X-Go-Pprof: 1`（`go tool pprof` 靠它识别这是错误而非 profile 数据）
- `/api/pprof/cpu` → `409` + `{"error":"cpu profiling already in use"}`

稍后重试即可。**正在进行的那个采样不受影响**，会正常完成。

Heap 分析（`/api/pprof/heap`）出于同样的原因也独占，并发时返回 `409` + `{"error":"heap profiling already in use"}`。注意 `HeapProfilerStart()` 与 CPU profiler 不同——它**没有失败模式**，重复调用会静默替换输出前缀，所以必须在调用之前原子认领，而不是"先查询再启动"。`/api/pprof/growth` 不占用会话，不受限制。

### Q: 接口返回 500 "Failed to get heap sample" / "heap sampling is off"
**A**: 状态式 heap 端点（`/pprof/heap`、`/api/pprof/heap/snapshot`）依赖 `TCMALLOC_SAMPLE_PARAMETER`，而它**必须在进程启动前**设置（默认 `0` 表示关闭采样）：

```bash
export TCMALLOC_SAMPLE_PARAMETER=524288
./your_app
```

窗口式端点（`/api/pprof/heap`）自己运行采集窗口，不受影响。

> 该变量缺失时，所有状态式端点现在都返回带原因的 500（此前是三种不同表现：200 + `%warn` 文本、500、200 + 空图）。

### Q: 接口返回 500 "No stack samples to render (profile is empty)"
**A**: 采样窗口内没有收集到足够的栈样本，`flamegraph.pl` 拒绝渲染。常见原因是采样时长太短，而目标进程在那段时间里正好空闲。加长 `duration`（例如 10–30 秒）或在有负载时重试。

> 以前这种情况会返回 **200** 和一张只含 `ERROR: No valid input provided to flamegraph.pl.` 文字的 SVG，看起来像成功。现已改为带原因的 500。

### Q: 接口返回 500 "pprof did not generate valid SVG"
**A**: 内置 pprof 脚本用 `dot`（graphviz）渲染，当 profile 采样点太少或数据异常时 `dot` 会失败并输出错误文本，因而被判定为"非 SVG"。请：① 安装 graphviz；② 加长采样时长（`?duration=30`）；③ 确认编译带 `-g`。这是已知的**既有缺陷**（错误信息未把 `dot` 的原始输出透出），记录在 [ROADMAP](../../ROADMAP.md)。

### Q: `renderer=flamegraph` 和 `renderer=callgraph` 有什么区别？
**A**: 是**两种不同的图形**，不是同一份数据换画法：

| 取值 | 产出 |
|------|------|
| `flamegraph`（默认） | **火焰图**：栈帧堆叠 |
| `callgraph` | **调用图**：graphviz 节点/边，能解析内联帧，符号化更完整 |

> ⚠️ 两者都**不能**在浏览器里缩放——产物内嵌的 pan/zoom 脚本依赖的元素/初始化挂钩并未出现（实测）。需要缩放请下载后用桌面工具打开。

### Q: Profile 文件很大怎么办？
**A**: 可以压缩后交给 pprof（`pprof` 支持从 stdin 读取）：
```bash
gzip my_profile.prof
gunzip -c my_profile.prof.gz | go tool pprof -http=:8080 -
```

---

## 更多信息

- [API 参考手册](02_api_reference.md)
- [集成示例](03_integration_examples.md)
- [快速开始](01_quick_start.md)
