# TODO List

## 已完成的改进 (2025-01-10)

### 9. 一键式 CPU/Heap 分析功能 ✅ (2025-01-10)
**目标**: 简化性能分析流程，提供一键式分析体验

**实现内容**:

#### CPU 一键分析
- ✅ 新增 `ProfilerManager::analyzeCPUProfile()` 方法
  - 自动启动/停止 CPU profiler
  - 使用 `pprof -svg` 生成 SVG 火焰图
  - 支持自定义采样时长（1-300秒）
  - 不需要可执行文件路径参数（避免符号冲突）

- ✅ 新增 `/api/cpu/analyze` HTTP 端点
  - 支持 `duration` 参数（采样时长）
  - 支持 `output_type` 参数（火焰图类型）
  - 返回 SVG 格式的火焰图

- ✅ 前端一键分析界面
  - 添加采样时长输入框
  - "⚡ 一键分析并生成SVG火焰图" 按钮
  - 新窗口打开 SVG 查看器（`show_svg.html`）

- ✅ SVG 查看器功能增强
  - 缩放控制（放大/缩小/1:1/适应宽度）
  - 自动滚动并高亮目标函数
  - 下载 SVG 文件功能
  - 查看提示和帮助信息

**使用方法**:
```bash
# 访问 http://localhost:8080
# 在 "CPU Profiler" 部分设置采样时长（如 10 秒）
# 点击 "⚡ 一键分析并生成SVG火焰图"
# 新窗口自动显示火焰图
```

**测试结果**:
```bash
curl "http://localhost:8080/api/cpu/analyze?duration=5"
# 返回: 完整的 SVG 火焰图（包含 cpuIntensiveTask、FibonacciCalculator 等函数）
```

#### Heap 一键分析
- ✅ 新增 `ProfilerManager::analyzeHeapProfile()` 方法
  - 自动启动/停止 Heap profiler
  - 后台线程持续分配内存确保采样
  - 自动查找最新的 heap profile 文件
  - 使用 `pprof -svg` 生成 SVG

- ✅ 新增 `/api/heap/analyze` HTTP 端点
  - 支持参数同 CPU 分析
  - 返回 SVG 格式的内存火焰图

- ✅ 前端 Heap 分析界面
  - 添加 Heap 采样时长输入框
  - 一键分析按钮
  - 独立的 Heap SVG 查看器（`show_heap_svg.html`）

**重要说明**:
⚠️ **Heap Profiler 需要特殊配置**:
- 必须使用 `HEAPPROFILE` 环境变量启动程序:
  ```bash
  HEAPPROFILE=/tmp/cpp_profiler/heap ./profiler_example
  ```
- 或者设置环境变量后启动（已集成到启动脚本）

**技术细节**:
- CPU 分析: 使用 `ProfilerStart/Stop` + `pprof -svg`
- Heap 分析: 使用 `HeapProfilerStart/Stop` + 环境变量
- SVG 生成: 直接调用 `pprof` 命令行工具
- 不需要可执行文件路径（让 pprof 自动从 prof 文件提取符号）

**新增文件**:
- `web/show_svg.html` - CPU SVG 查看器
- `web/show_heap_svg.html` - Heap SVG 查看器

**修改文件**:
- `include/profiler_manager.h` - 添加 analyze 方法声明
- `src/profiler_manager.cpp` - 实现一键分析逻辑
- `src/profiler_manager.cpp` - 添加 `findLatestHeapProfile()` 辅助方法
- `example/main.cpp` - 添加 HTTP 端点
- `web/index.html` - 添加一键分析按钮和处理函数

**Bug 修复**:
- 修复 SVG 中包含 "error" 字符串导致误判的问题
- 使用更精确的错误检查（检查 `<?xml` 或 `<svg` 标记）
- 使用 Blob URL 避免 JavaScript 模板字符串嵌套问题

---

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

### 7. 移除 CPU/Heap Profiler 开始/停止接口 ✅ (2025-01-11)
**变更**: 移除 `/api/cpu/start`、`/api/cpu/stop`、`/api/heap/start`、`/api/heap/stop` 接口

**原因**: 使用标准 Go pprof 接口 (`/pprof/profile`, `/pprof/heap`) 替代自定义控制接口

**修改文件**:
- `example/main.cpp` - 删除 start/stop 路由处理器
- `web/index.html` - 移除启动/停止按钮和相关 JavaScript 函数
- `UNIT_TEST_REPORT.md` - 更新测试示例使用 `/pprof/profile`
- `BUILD_STEPS.md` - 更新测试步骤使用 `/pprof/profile`

**新的使用方式**:
```bash
# CPU profile - 采样 5 秒并保存
curl "http://localhost:8080/pprof/profile?seconds=5" > cpu.prof

# Heap profile - 获取当前堆采样
curl "http://localhost:8080/pprof/heap" > heap.prof
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

## 最新实现: 基于 protobuf 的 profile 解析 (2025-01-09)

### 架构变更

按照 brpc pprof 标准重构了 Profile 数据流，**移除了 StackCollector 实时采集**：

```
采样阶段:
  gperftools → prof 文件 (cpu.prof / heap.prof)

传输阶段:
  前端 GET /api/cpu/profile 或 /api/heap/profile → 下载 prof 文件

解析阶段:
  前端解析 prof 文件 → 提取地址列表

符号化阶段:
  前端 POST /pprof/symbol (批量地址) → backward-cpp 符号化 → 返回函数名（支持内联）

渲染阶段:
  前端构建火焰图 → Canvas 渲染（紫色显示内联函数）
```

### 新增功能

**后端**:
- ✅ 新增 `/api/cpu/addresses` 接口返回地址栈文本格式
- ✅ 新增 `/api/cpu/profile` 接口返回 CPU 原始二进制文件
- ✅ 新增 `/api/heap/profile` 接口返回 Heap 原始文件
- ✅ 实现 `ProfilerManager::getCPUProfileAddresses()` 解析 gperftools 格式
- ✅ `/pprof/symbol` 接口改用 backward-cpp，支持内联函数（使用 "--" 连接）

**前端**:
- ✅ 新增 `processCPUProfileText()` 解析地址栈文本
- ✅ 实现批量符号化请求（每批 100 个地址）
- ✅ `buildFlameGraph()` 支持内联函数解析
- ✅ 修复 `calculateTotal()` 递归计算节点 total 值
- ✅ 修复内联函数节点缺少 `inlineChildren` 属性的 bug

### Bug 修复

**Bug 1**: `calculateTotal()` 不递归
- **位置**: `web/flamegraph.html:947`
- **问题**: 只计算直接子节点的 value，导致返回 0
- **修复**: 改为递归计算所有子孙节点

**Bug 2**: 内联函数节点缺少 `inlineChildren` 属性
- **位置**: `web/flamegraph.html:639`
- **问题**: 多层内联函数时报错
- **修复**: 添加 `inlineChildren: {}` 属性

### 新增文档

- `IMPLEMENTATION_RECORD.md` - 完整实施记录
- `BUG_FIX_REPORT.md` - Bug 修复报告
- `UNIT_TEST_REPORT.md` - 单元测试报告
- `CPU_PROFILE_GUIDE.md` - CPU profile 使用指南
- `FINAL_TEST_SUMMARY.md` - 测试总结

---

## 当前已知问题 (2025-01-09)

### 已解决 ✅

#### 1. CPU 符号化率低问题 - 已解决 (2025-01-09)
**解决方案**:
- ✅ 添加absl::Symbolize作为主要符号化方法
- ✅ 实现三层符号化策略：absl::Symbolize → dladdr → backward-cpp → addr2line
- ✅ 符号化率提升到 **72.7%**
- ✅ 支持PIE可执行文件的符号化（计算相对地址）

**测试结果**:
```
符号化率: 72.7% (8/11)
成功符号化:
  ✅ cpuIntensiveTask()
  ✅ memoryIntensiveTask()
  ✅ FibonacciCalculator::recursive()
  ✅ std::thread::_State_impl<>::_M_run()
  ✅ rand
  ✅ random
```

#### 2. 前端架构重构 - 已完成 (2025-01-09)
**实现内容**:
- ✅ 前端直接下载prof文件并解析（参考pprof标准流程）
- ✅ 前端使用JavaScript解析gperftools二进制格式
- ✅ 前端批量调用/pprof/symbol进行符号化
- ✅ 移除服务器端地址栈解析接口

**新架构数据流**:
```
gperftools → cpu.prof (二进制)
    ↓
前端 GET /api/cpu/profile → 下载ArrayBuffer
    ↓
前端 parseGperftoolsProfile() → 提取地址
    ↓
前端 POST /pprof/symbol → 批量符号化
    ↓
前端 buildFlameGraph() → Canvas渲染
```

#### 3. 文本分析接口 - 已修复 (2025-01-09)
**问题**: `/api/cpu/text` 和 `/api/heap/text` 返回固定的假数据
**解决**:
- ✅ 修改`getProfileSamples()`调用`getCollapsedStacks()`
- ✅ 返回真实的符号化后的collapsed格式数据
- ✅ 与火焰图使用相同的解析和符号化流程

---

## 接下来需要完成的工作

### 中优先级 🟡

#### 1. 进一步提升符号化率
**当前**: 72.7%
**目标**: >90%

**优化方向**:
- [ ] 添加共享库符号表加载（/proc/self/maps解析）
- [ ] 支持更多的共享库符号化（libc, libstdc++等）
- [ ] 优化PIE地址计算

#### 2. Heap 火焰图显示优化
**当前**: 基本可用，但可能需要优化
**待测试**:
- [ ] 验证heap profile的解析逻辑
- [ ] 确认heap火焰图显示正常

**相关文件**:
- `src/profiler_manager.cpp` - `getCollapsedStacks()` 方法
- `web/flamegraph.html` - `processHeapProfile()` 函数

#### 2. 提高 CPU 符号化率
**目标**: 从当前 ~50% 提升到 >90%

**实施步骤**:
1. 修改 `CMakeLists.txt` 添加 `-g` 编译选项
2. 确认不使用 `strip` 命令
3. 重新编译并测试
4. 验证符号化效果

**相关文件**:
- `CMakeLists.txt`
- `src/symbolize.cpp`

---

### 中优先级 🟡

#### 3. 增加采样数据量
**当前**: 只有 3-5 个 CPU 样本

**改进**:
- [ ] 在 `example/main.cpp` 中增加后台工作负载
- [ ] 设置 `CPUPROFILE_FREQUENCY=100` 提高采样频率
- [ ] 延长采样时间到 10-15 秒

#### 4. 改进火焰图可视化
**当前状态**: 火焰图已经支持基本交互

**可以改进的方向**:
1. 优化颜色映射 - 使函数更容易区分
2. 添加火焰图和冰柱图切换
3. 改进搜索功能 - 支持正则表达式
4. 导出功能增强 - 支持 PNG、SVG、PDF

**参考工具**:
- Speedscope (https://www.speedscope.app/)
- FlameGraph (https://github.com/brendangregg/flamegraph)

---

### 低优先级 🟢

#### 5. 性能优化
- 使用缓存避免重复解析 profile
- 优化大量数据的传输和渲染
- 添加分页或采样数据量控制

#### 6. 文档和测试
- 添加 API 文档
- 添加集成测试
- 添加性能基准测试

#### 7. 其他功能
- 支持多个 profile 文件的对比
- 支持历史 profile 数据的存储和查询
- 添加实时 profiling 模式

---

## 技术笔记

### 新架构数据流

**CPU Profile**:
```
gperftools ProfilerStart()
    ↓
cpu.prof (二进制格式)
    ↓
getCPUProfileAddresses() 解析
    ↓
"count @ 0x... 0x..." (文本格式)
    ↓
前端 processCPUProfileText()
    ↓
批量 POST /pprof/symbol
    ↓
backward-cpp 符号化（支持内联）
    ↓
buildFlameGraph() 构建树
    ↓
Canvas 渲染
```

**符号格式**:
- 普通函数: `function_name`
- 内联函数: `main_func--inline_func1--inline_func2`
- 未符号化: `0x12345678`

### gperftools Profile 格式

**CPU Profile** (二进制):
```
Header (24 bytes):
  [0-7]:   magic/version
  [8-15]:  sampling period
  [16-23]: padding
  [24-31]: sample count
  [32-39]: PC count

Samples:
  - count (uint64)
  - pc_count (uint64)
  - PCs[pc_count] (uint64 array)
```

**Heap Profile** (文本):
```
heap profile: ...
1: 1048576 [1: 1048576] @ addr1 addr2 addr3
```

### 调试技巧
1. 检查 profiler 状态: `curl http://localhost:8080/api/status`
2. 查看 CPU 地址栈: `curl http://localhost:8080/api/cpu/addresses`
3. 测试符号化: `echo "0x123456" | curl -X POST http://localhost:8080/pprof/symbol --data-binary @-`
4. 查看火焰图: 访问 `http://localhost:8080/flamegraph`

---

## Git 提交记录

```
7546b05 feat: 实现基于protobuf的profile解析和火焰图渲染
5c6f360 feat: 实现基于protobuf的profile解析，移除pprof依赖
3d77691 docs: 添加详细的 TODO 列表记录后续工作
7fbbc6e feat: 添加真实的 collapsed 格式火焰图数据API
0d81f0d refactor: 清理构建脚本，统一使用vcpkg管理依赖
bfc8245 refactor: 清理pprof依赖，使用vcpkg管理依赖，增强火焰图功能
```

---

创建时间: 2025-01-07
最后更新: 2025-01-09
