# TODO List

## 已完成的改进 (2025-01-08)

### 1. 修复硬编码假数据问题 ✅
- 添加了 `getCollapsedStacks()` 方法，返回真实的 profile 数据
- 使用 `pprof -traces` 读取实际的调用栈数据
- 将 traces 格式转换为火焰图所需的 collapsed 格式

### 2. 添加新的 API 端点 ✅
- `/api/cpu/collapsed` - 返回 CPU profile 的 collapsed 格式数据
- `/api/heap/collapsed` - 返回 Heap profile 的 collapsed 格式数据
- 保留了旧的 `/api/cpu/flamegraph` 和 `/api/heap/flamegraph` 以保持兼容性

### 3. 修复路径问题 ✅
- 修改了 `startCPUProfiler` 和 `startHeapProfiler`
- 将相对路径自动转换为绝对路径
- 避免 pprof 找不到文件的问题

### 4. 改进构建脚本 ✅
- 更新 `start.sh`，设置 PATH 包含 pprof 工具路径

### 5. 移除 pprof 依赖，使用运行时栈收集 ✅ (2025-01-07)
- 创建了 `StackCollector` 类，使用 `backtrace()` 在运行时收集调用栈
- 在 CPU profiler 启动时自动启动栈采样线程
- 采样间隔可配置（默认 100ms）
- 收集的数据直接生成 collapsed 格式，无需外部工具
- 使用 `dladdr()` 和 `abi::__cxa_demangle` 进行符号解析

**新增文件**:
- `include/stack_collector.h` - 栈收集器头文件
- `src/stack_collector.cpp` - 栈收集器实现

**修改文件**:
- `src/profiler_manager.cpp` - 集成 StackCollector
- `include/profiler_manager.h` - 添加 StackCollector 依赖
- `CMakeLists.txt` - 添加新的源文件
- `tests/profiler_test.cpp` - 修复测试

### 6. 前端火焰图使用真实 collapsed 数据 ✅ (2025-01-07)
- 修改 `web/flamegraph.html` 从 `/api/cpu/flamegraph` 改为使用 `/api/cpu/collapsed`
- 添加 `parseCollapsedFormat()` 函数解析 collapsed 格式文本
- 自动构建调用树结构用于渲染
- 支持显示采样数统计

### 7. 修复 CPU/Heap Profiler 自动停止功能 ✅ (2025-01-08)
**问题**: `/api/cpu/start` 和 `/api/heap/start` 接口不支持 `duration` 参数，profiler 启动后不会自动停止

**修复内容**:
- 在 HTTP 处理器中添加 `duration` 参数解析
- 使用独立线程在指定时间后自动调用 `stopCPUProfiler()` / `stopHeapProfiler()`
- 支持 CPU 和 Heap 两种 profiler 类型
- 返回 `duration_ms` 字段确认配置成功

**修改文件**:
- `example/main.cpp` - 修改 `/api/cpu/start` 和 `/api/heap/start` 处理器

**测试结果**:
```bash
curl -X POST "http://localhost:8080/api/cpu/start?duration=5"
# 5 秒后 profiler 自动停止
```

### 8. 修复 CPU Profiler 使用正确的数据源 ✅ (2025-01-08)
**问题**: `getCollapsedStacks()` 尝试用 `ProfileParser::parseToCollapsed()` 解析 gperftools 二进制文件，但 gperftools 格式不是 protobuf 格式

**修复内容**:
- 修改 `ProfilerManager::getCollapsedStacks()` 对 CPU profile 直接使用 `StackCollector::getInstance().getCollapsedStacks()`
- StackCollector 已经在运行时收集了调用栈数据并生成 collapsed 格式
- Heap profile 继续使用文件解析（gperftools heap profile 是文本格式）

**修改文件**:
- `src/profiler_manager.cpp` - 修改 `getCollapsedStacks()` 方法

**测试结果**:
```bash
curl http://localhost:8080/api/cpu/collapsed
# 返回: # collapsed stack traces
#       # Total samples: 101
#       libc.so.6+0x129c6c;libc.so.6+0x9caa4;libstdc++.so.6+0xecdb4 101
```

---

## 当前已知问题 (2025-01-08)

### 高优先级 🔴

#### 1. CPU Profiler 火焰图只显示地址，不显示函数名
**现象**: 访问 `http://localhost:8080/flamegraph?type=cpu`，火焰图中显示的是 `libc.so.6+0x129c6c` 这样的地址，而不是函数名

**原因**: StackCollector 使用 `dladdr()` 解析符号，对于共享库（libc、libstdc++ 等）中的地址无法解析出函数名，只能返回库名+偏移量

**影响**: 用户无法直观地看到哪个函数占用了 CPU 时间

**解决方案**:
- 方案 A: 使用 `addr2line` 工具解析共享库中的地址（需要知道共享库路径）
- 方案 B: 使用 libdw (DWARF) 库进行符号解析
- 方案 C: 实现 `/pprof/symbol` 接口，前端批量请求符号解析

**相关文件**:
- `src/stack_collector.cpp` - `resolveSymbol()` 方法（第 79-128 行）

#### 2. Heap Profiler 火焰图显示空白
**现象**: 访问 `http://localhost:8080/flamegraph?type=heap`，火焰图区域是空白的

**原因**: 需要调查 Heap Profiler 的 collapsed 数据格式是否正确，以及前端是否正确解析

**影响**: 无法可视化 Heap Profiler 数据

**调查步骤**:
1. 检查 `/api/heap/collapsed` 返回的数据格式
2. 检查前端 `parseCollapsedFormat()` 是否正确处理 heap 数据
3. 验证 Heap Profiler 是否正确收集数据

**相关文件**:
- `src/profiler_manager.cpp` - `getCollapsedStacks()` 方法
- `web/flamegraph.html` - `parseCollapsedFormat()` 函数

---

## 接下来需要完成的工作

### 高优先级 🔴

#### 1. 修复 Heap Profiler 火焰图显示空白问题
**调查步骤**:
1. 检查 `/api/heap/collapsed` 返回的数据格式
2. 检查前端 `parseCollapsedFormat()` 是否正确处理 heap 数据
3. 验证 Heap Profiler 是否正确收集数据

**相关文件**:
- `src/profiler_manager.cpp` - `getCollapsedStacks()` 方法
- `web/flamegraph.html` - `parseCollapsedFormat()` 函数

#### 2. 改进 CPU Profiler 符号解析（显示函数名而非地址）
**当前问题**: 火焰图显示 `libc.so.6+0x129c6c` 而不是函数名

**可能的解决方案**:
- 方案 A: 在 StackCollector::resolveSymbol() 中使用 `addr2line` 工具解析共享库地址
- 方案 B: 使用 libdw (DWARF) 库进行符号解析
- 方案 C: 实现 `/pprof/symbol` 批量解析接口，前端调用

**相关文件**:
- `src/stack_collector.cpp` - `resolveSymbol()` 方法

---

### 中优先级 🟡

#### 3. 改进火焰图可视化
**当前状态**: 火焰图已经支持基本交互

**可以改进的方向**:
1. 优化颜色映射 - 使函数更容易区分
2. 添加火焰图和冰柱图切换
3. 改进搜索功能 - 支持正则表达式
4. 导出功能增强 - 支持 PNG、SVG、PDF

**参考工具**:
- Speedscope (https://www.speedscope.app/)
- FlameGraph (https://github.com/brendangregg/flamegraph)

#### 4. 添加 Heap Profiler 的实时监控
**当前状态**: Heap profiler 使用 gperftools 的快照功能

**可以改进**:
- 实时显示内存分配趋势
- 支持多次快照对比
- 检测内存泄漏

#### 5. 错误处理和用户提示
**改进方向**:
- 当没有收集到数据时，给出友好的提示
- 添加 API 错误码和错误消息
- 添加 profiler 状态的实时监控

---

### 低优先级 🟢

#### 6. 性能优化
- 使用缓存避免重复解析 profile
- 优化大量数据的传输和渲染
- 添加分页或采样数据量控制

#### 7. 文档和测试
- 添加 API 文档
- 添加集成测试
- 添加性能基准测试

#### 8. 其他功能
- 支持多个 profile 文件的对比
- 支持历史 profile 数据的存储和查询
- 添加实时 profiling 模式

---

## 技术笔记

### StackCollector 工作原理
```
CPU Profiler 启动
    ↓
StackCollector 启动采样线程 (100ms 间隔)
    ↓
每次采样使用 backtrace() 获取调用栈
    ↓
使用 dladdr() + abi::__cxa_demangle 解析符号
    ↓
存储样本到内存
    ↓
请求 collapsed 数据时，聚合样本并生成格式
    ↓
停止 profiler 时自动停止 StackCollector
```

### collapsed 格式说明
```
func1;func2;func3 100
func1;func4 50
```
- 每行代表一个调用栈
- 函数用 `;` 分隔（从根到叶子）
- 最后一列是采样数

### 调试技巧
1. 检查 StackCollector 状态: `curl http://localhost:8080/api/status`
2. 查看 collapsed 数据: `curl http://localhost:8080/api/cpu/collapsed`
3. 查看采样数: collapsed 数据中的 `# Total samples` 行
4. 查看火焰图: 访问 `http://localhost:8080/flamegraph`

---

## Git 提交记录

```
5c6f360 feat: 实现基于protobuf的profile解析，移除pprof依赖
3d77691 docs: 添加详细的 TODO 列表记录后续工作
7fbbc6e feat: 添加真实的 collapsed 格式火焰图数据API
0d81f0d refactor: 清理构建脚本，统一使用vcpkg管理依赖
bfc8245 refactor: 清理pprof依赖，使用vcpkg管理依赖，增强火焰图功能
```

---

创建时间: 2025-01-07
最后更新: 2025-01-08
