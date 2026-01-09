# CPU/Heap Profile 火焰图实现记录

## 📅 实施日期
2025-01-09

## 🎯 新架构方案

### 整体架构设计

按照 brpc pprof 标准重构了 Profile 数据流：

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

### 关键变更

#### 1. 移除 StackCollector 实时采集
- **变更**: 移除了 `StackCollector::start()` 调用
- **位置**: `src/profiler_manager.cpp` 的 `startCPUProfiler()`
- **原因**: 新架构不需要实时采集，只使用 gperftools 生成的文件

#### 2. 新增 HTTP API 接口

**`/api/cpu/addresses`** - 返回 CPU profile 地址栈（文本格式）
```cpp
// 位置: example/main.cpp
// 格式: "count @ addr1 addr2 addr3"
app().registerHandler("/api/cpu/addresses", ...
```

**`/api/cpu/profile`** - 返回 CPU 原始二进制文件
**`/api/heap/profile`** - 返回 Heap 原始文件

**`/pprof/symbol`** - 批量符号化（使用 backward-cpp）
```cpp
// 使用 backward-cpp 进行符号化，支持内联函数
// 返回格式: "address symbol_name"
// 内联函数使用 "--" 连接
```

#### 3. 新增后端方法

**`ProfilerManager::getCPUProfileAddresses()`**
```cpp
// 位置: src/profiler_manager.cpp
// 功能: 解析 gperftools 二进制格式，生成地址栈文本
// 返回格式: "count @ 0x... 0x... 0x..."
```

#### 4. 前端重构

**移除**:
- ❌ protobuf.js 依赖（不再需要）
- ❌ `/api/cpu/profile` 的 protobuf 解析

**新增**:
- ✅ `/api/cpu/addresses` 接口调用
- ✅ `processCPUProfileText()` - 解析地址栈文本
- ✅ 批量符号化逻辑
- ✅ 支持内联函数（`--` 分隔符）

#### 5. 符号化实现

使用 **backward-cpp** 替代之前的 addr2line：
- 支持内联函数
- 返回格式: `func1--inline1--inline2`

## ✅ 已完成的功能

### 1. 后端功能

| 功能 | 状态 | 说明 |
|------|------|------|
| gperftools 集成 | ✅ | CPU/Heap profile 生成 |
| `/api/cpu/addresses` | ✅ | 返回地址栈文本格式 |
| `/api/cpu/profile` | ✅ | 返回原始二进制文件 |
| `/api/heap/profile` | ✅ | 返回原始 heap 文件 |
| `/pprof/symbol` | ✅ | 批量符号化，支持内联 |
| `getCPUProfileAddresses()` | ✅ | 解析 gperftools 格式 |

### 2. 前端功能

| 功能 | 状态 | 说明 |
|------|------|------|
| `processCPUProfileText()` | ✅ | 解析地址栈文本 |
| 批量符号化 | ✅ | 每批 100 个地址 |
| `buildFlameGraph()` | ✅ | 构建火焰图数据结构 |
| 支持内联函数 | ✅ | 使用 `--` 分隔符解析 |
| `calculateTotal()` 修复 | ✅ | 递归计算节点 total |
| 内联节点 `inlineChildren` | ✅ | 支持多层内联 |

### 3. Bug 修复

**Bug 1**: `calculateTotal()` 不递归
- **位置**: `web/flamegraph.html:947`
- **修复**: 改为递归计算所有子孙节点

**Bug 2**: 内联函数节点缺少 `inlineChildren` 属性
- **位置**: `web/flamegraph.html:639`
- **修复**: 添加 `inlineChildren: {}`

## ❌ 当前存在的问题

### 问题 1: CPU 火焰图仍有部分地址未符号化

**现象**:
```
0x5ed893986398    # 未符号化
__random          # 已符号化 ✅
rand              # 已符号化 ✅
0x5ed8939889ad    # 未符号化
```

**原因分析**:
1. 部分地址不在当前可执行文件的符号表中
2. 可能是动态库的地址（需要加载共享库符号）
3. 编译时缺少调试符号 (`-g` 选项)
4. strip 命令移除了符号信息

**解决方案** (待实施):
- [ ] 编译时添加 `-g` 标志保留调试符号
- [ ] 不使用 `strip` 命令
- [ ] 使用 `backward-cpp` 的 `SignalHandling` 捕获更多符号
- [ ] 加载共享库的符号表

### 问题 2: Heap 火焰图显示为空

**现象**: Heap profile 有数据，但火焰图不显示

**可能原因**:
1. Heap profile 的解析逻辑不同 CPU
2. `processHeapProfile()` 函数可能有问题
3. Heap 数据格式与 CPU 不同
4. 符号化逻辑可能不适配 Heap

**需要调查**:
- [ ] 检查 `/api/heap/profile` 返回的数据格式
- [ ] 验证 `processHeapProfile()` 解析逻辑
- [ ] 检查 heap profile 的地址栈格式
- [ ] 确认符号化是否工作

### 问题 3: 采样数据量较少

**现象**: 只有 3-5 个样本

**原因**:
1. 后台工作负载较轻
2. 采样时间较短（5秒）
3. gperftools 采样频率可能不够

**建议**:
- [ ] 增加后台工作负载
- [ ] 延长采样时间
- [ ] 设置环境变量 `CPUPROFILE_FREQUENCY=100`

## 📝 文件变更清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/profiler_manager.cpp` | 新增 `getCPUProfileAddresses()` 方法 |
| `include/profiler_manager.h` | 新增 `getCPUProfileAddresses()` 声明 |
| `tests/test_cpu_profile.cpp` | CPU profile 单元测试 |
| `tests/test_full_flow.cpp` | 完整流程测试 |
| `tests/test_build_flamegraph.js` | buildFlameGraph 测试 |
| `tests/test_complete_flow.js` | 前端完整流程测试 |
| `tests/test_frontend_logic.js` | 前端逻辑测试 |
| `web/flamegraph.html` | 修复了两个 bug |
| `CPU_PROFILE_GUIDE.md` | CPU profile 指南 |
| `TEST_RESULTS.md` | 测试结果 |
| `UNIT_TEST_REPORT.md` | 单元测试报告 |
| `FINAL_TEST_SUMMARY.md` | 测试总结 |
| `BUG_FIX_REPORT.md` | Bug 修复报告 |

### 修改文件

| 文件 | 变更内容 |
|------|----------|
| `example/main.cpp` | 添加 `/api/cpu/addresses`, `/api/cpu/profile`, `/api/heap/profile` 接口 |
| `src/profiler_manager.cpp` | 移除 StackCollector 调用，添加 `getCPUProfileAddresses()` |
| `web/flamegraph.html` | 修复 `calculateTotal()` 和内联函数 bug |

## 🔬 技术细节

### gperftools Profile 格式

**CPU Profile** (二进制格式):
```
Header (24 bytes):
  [0-7]:   magic/version
  [8-15]:  sampling period (10000)
  [16-23]: padding
  [24-31]: sample count
  [32-39]: PC count

Samples:
  For each sample:
    - count (uint64)
    - pc_count (uint64)
    - PCs[pc_count] (uint64 array)
```

**Heap Profile** (文本格式):
```
heap profile: ...
1: 1048576 [1: 1048576] @ addr1 addr2 addr3
```

### backward-cpp 符号化

```cpp
// 位置: src/symbolize.cpp
std::vector<SymbolizedFrame> BackwardSymbolizer::symbolize(void* address) {
    // 使用 load_addresses 加载单个地址
    impl_->resolver_.load_addresses(&address, 1);

    // 解析地址
    backward::Trace trace;
    trace.addr = address;

    backward::ResolvedTrace resolved = impl_->resolver_.resolve(trace);

    // 返回主函数和内联函数
    // 内联函数使用 "--" 连接
}
```

### 前端数据流

```javascript
// 1. 获取地址
fetch('/api/cpu/addresses')
  .then(res => res.text())

// 2. 解析文本
lines.forEach(line => {
  const [count, addrs] = line.split('@');
  const stack = addrs.trim().split(/\s+/).reverse();
  samples.push({ stack, value: parseInt(count) });
});

// 3. 批量符号化
fetch('/pprof/symbol', { method: 'POST', body: addresses.join('\n') })

// 4. 构建火焰图
buildFlameGraph(samples, symbolMap)

// 5. 渲染
renderFlameGraph()
```

## 📚 参考文档

- [brpc pprof 实现](https://github.com/apache/brpc/blob/master/tools/pprof)
- [gperftools 文档](https://gperftools.github.io/gperftools/)
- [backward-cpp](https://github.com/bombela/backward-cpp)
- [Flame Graphs](http://www.brendangregg.com/flamegraphs.html)

## 🎯 下一步工作

### 高优先级

1. **修复 Heap 火焰图**
   - [ ] 调查 heap profile 数据格式
   - [ ] 修复 `processHeapProfile()` 解析逻辑
   - [ ] 验证 heap profile 地址栈格式

2. **提高 CPU 符号化率**
   - [ ] 编译时添加 `-g` 调试符号
   - [ ] 配置 backward-cpp 加载共享库符号
   - [ ] 验证符号表是否完整

### 中优先级

3. **增加采样数据量**
   - [ ] 增加后台工作负载
   - [ ] 设置 `CPUPROFILE_FREQUENCY`
   - [ ] 延长采样时间

4. **优化用户体验**
   - [ ] 添加加载状态提示
   - [ ] 优化渲染性能
   - [ ] 添加错误提示

### 低优先级

5. **功能增强**
   - [ ] 支持火焰图导出
   - [ ] 支持对比不同时间段
   - [ ] 添加性能指标统计

## 📊 测试数据

当前采样结果（2025-01-09）:
- CPU profile: 4-5 个样本
- 符号化率: ~50% (部分地址符号化成功)
- 内联函数: 发现 `execute_native_thread_routine--operator()--~unique_ptr`
- Heap profile: 数据存在，但火焰图为空

## 🔗 相关分支

- 主分支: `main`
- 当前工作: 待创建新分支提交

## 💡 经验总结

1. **架构选择很重要**: 从实时采集改为文件传输，简化了架构
2. **使用成熟工具**: backward-cpp 比自己实现符号化更可靠
3. **递归计算要注意**: calculateTotal 必须递归，不能只看一层
4. **数据结构设计**: 内联函数需要特殊处理，多层嵌套要正确
5. **单元测试很重要**: 发现了多个只有在实际运行时才会出现的 bug
