# TODO List

## 已完成的改进 (2025-01-07)

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

### 5. 移除 pprof 依赖，使用运行时栈收集 ✅ (2025-01-07 新增)
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

### 6. 前端火焰图使用真实 collapsed 数据 ✅ (2025-01-07 新增)
- 修改 `web/flamegraph.html` 从 `/api/cpu/flamegraph` 改为使用 `/api/cpu/collapsed`
- 添加 `parseCollapsedFormat()` 函数解析 collapsed 格式文本
- 自动构建调用树结构用于渲染
- 支持显示采样数统计

---

## 接下来需要完成的工作

### 高优先级 🔴

#### 1. 改进共享库符号解析
**当前问题**: StackCollector 使用 `dladdr()` 解析符号，但对于共享库（libc、libpthread 等）中的地址无法解析，返回十六进制地址

**可能的解决方案**:
- 方案 A: 使用 `addr2line` 工具解析共享库中的地址
- 方案 B: 使用 libdw (DWARF) 库进行符号解析
- 方案 C: 读取 `/proc/self/maps` 和共享库的符号表

**相关文件**:
- `src/stack_collector.cpp` - `resolveSymbol()` 方法

**实现思路**:
```cpp
// 1. 获取地址所属的共享库
// 2. 使用 addr2line 解析符号
std::string cmd = "addr2line -e " + lib_path + " -f -C " + address;
executeCommand(cmd, output);
```

#### 2. 实现 `/pprof/symbol` 接口的批量解析
**当前状态**: `/pprof/symbol` 接口已经实现，但前端没有使用

**正确的 brpc 方式**:
1. 前端从 profile 数据中提取所有地址
2. 批量 POST 地址到 `/pprof/symbol` 接口
3. 后端使用 `addr2line` 解析符号并返回
4. 前端用符号化的数据生成火焰图

**需要修改**:
- 前端: 提取地址、批量请求 symbol 接口
- 后端: `/pprof/symbol` 接口需要改进，支持批量解析
- 当前 `/pprof/symbol` 只支持单个地址解析

**批量解析示例**:
```javascript
// 前端代码
const addresses = extractAddressesFromProfile(profileData);
const response = await fetch('/pprof/symbol', {
    method: 'POST',
    body: addresses.join('\n')
});
const symbols = await response.text();
```

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
7fbbc6e feat: 添加真实的 collapsed 格式火焰图数据API
0d81f0d refactor: 清理构建脚本，统一使用vcpkg管理依赖
bfc8245 refactor: 清理pprof依赖，使用vcpkg管理依赖，增强火焰图功能
```

---

## 下次工作重点

1. **生成 Protobuf 代码** 🔴 (当前阻塞)
   ```bash
   cd /home/dodo/cpp-remote-profiler
   ./generate_proto.sh
   cd build && make
   ```
   详细步骤见: `README_BUILD.md`

2. **启用 ProfileParser 解析代码**
   - 编辑 `src/profile_parser.cpp`
   - 取消注释 protobuf 解析代码
   - 重新编译测试

3. **改进火焰图显示**
   - 测试 collapsed API
   - 验证符号解析正确性

---

## 最近更新 (2025-01-07 晚)

### 添加 Protobuf 支持
- ✅ 更新 vcpkg.json 添加 protobuf 依赖（含 tool 特性）
- ✅ 创建 `third_party/profile/profile.proto`
- ✅ 配置 CMakeLists.txt 使用 protobuf_generate_cpp
- ✅ 创建 `include/profile_parser.h` 和 `src/profile_parser.cpp`
- ⏸️ 等待生成 profile.pb.cc 和 profile.pb.h

### 创建的辅助文件
- `generate_proto.sh` - 生成 protobuf 代码的脚本
- `PROTO_BUILD.md` - 详细的构建步骤
- `README_BUILD.md` - 完整的构建指南

### 下一步操作
由于 Bash 工具暂时无法使用，需要手动执行：
```bash
cd /home/dodo/cpp-remote-profiler
./generate_proto.sh
cd build && make
```

成功后，在 `src/profile_parser.cpp` 中启用 protobuf 解析代码。

---

创建时间: 2025-01-07
最后更新: 2025-01-07
