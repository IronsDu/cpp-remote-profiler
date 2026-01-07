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

---

## 接下来需要完成的工作

### 高优先级 🔴

#### 1. 修复 collapsed API 的 pprof 执行问题
**问题**: 当前 `/api/cpu/collapsed` 返回错误 "pprof tool not available"
**可能原因**:
- 服务的环境变量 PATH 没有包含 `/root/go/bin`
- pprof 命令执行失败但没有正确捕获错误
- `executeCommand` 函数可能有问题

**解决方案**:
- 方案 A: 在 `ProfilerManager` 构造函数中检测 pprof 路径并缓存
- 方案 B: 使用绝对路径 `/root/go/bin/pprof` 而不是依赖 PATH
- 方案 C: 在启动脚本中设置环境变量

**相关文件**:
- `src/profiler_manager.cpp` - `getCollapsedStacks()` 方法
- `start.sh` - 启动脚本

**测试步骤**:
```bash
cd build
./profiler_example
# 在另一个终端
curl -X POST http://localhost:8080/api/cpu/start
sleep 5
curl -X POST http://localhost:8080/api/cpu/stop
curl http://localhost:8080/api/cpu/collapsed | head
```

#### 2. 前端火焰图使用真实数据
**当前问题**: 前端使用 `/api/cpu/flamegraph` 返回的是硬编码的 JSON 数据

**需要做的修改**:
1. 修改 `web/flamegraph.html` 或创建新的页面
2. 使用 `fetch('/api/cpu/collapsed')` 获取真实的 collapsed 格式数据
3. 解析 collapsed 格式并渲染火焰图

**参考实现**:
- brpc 的火焰图渲染方式
- Brendan Gregg 的 FlameGraph 工具

**collapsed 格式说明**:
```
func1;func2;func3 100
func1;func4 50
```
- 每行代表一个调用栈
- 函数用 `;` 分隔（从根到叶子）
- 最后一列是采样数

**渲染逻辑**:
1. 解析 collapsed 格式数据
2. 构建调用树结构
3. 计算每个函数的宽度和颜色
4. 使用 HTML Canvas 或 SVG 渲染

#### 3. 实现 `/pprof/symbol` 接口的正确使用
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

#### 4. 改进火焰图可视化
**当前问题**: 当前的火焰图是静态的，不够交互

**改进方向**:
1. 添加缩放功能
2. 添加搜索高亮功能
3. 点击函数显示详细信息
4. 支持导出为 PNG/SVG

**参考工具**:
- Speedscope (https://www.speedscope.app/)
- FlameGraph (https://github.com/brendangregg/flamegraph)

#### 5. 添加 Heap Profiler 支持
**当前状态**: Heap profiler 已经实现，但可能需要测试

**需要测试**:
- `/api/heap/start` 和 `/api/heap/stop`
- `/api/heap/collapsed` 数据格式
- 火焰图显示

#### 6. 错误处理和用户提示
**改进方向**:
- 当 pprof 工具不可用时，给出明确的安装提示
- 当没有收集到数据时，给出友好的提示
- 添加 API 错误码和错误消息

---

### 低优先级 🟢

#### 7. 性能优化
- 使用缓存避免重复解析 profile
- 优化大量数据的传输和渲染
- 添加分页或采样数据量控制

#### 8. 文档和测试
- 添加 API 文档
- 添加单元测试
- 添加集成测试

#### 9. 其他功能
- 支持多个 profile 文件的对比
- 支持历史 profile 数据的存储和查询
- 添加实时 profiling 模式

---

## 技术笔记

### pprof 工具安装
```bash
go install github.com/google/pprof@latest
export PATH=$PATH:~/go/bin  # 或 /root/go/bin
```

### 手动测试 collapsed 格式
```bash
/root/go/bin/pprof -traces build/cpu.prof | head
```

### collapsed 格式转换
traces 格式:
```
-----------+-------------------------------------------------------
      10ms   func1
               func2
                   func3
```

collapsed 格式:
```
func1;func2;func3 10
```

### 调试技巧
1. 检查 pprof 是否可用: `which pprof` 或 `/root/go/bin/pprof --version`
2. 检查 profile 文件: `ls -la build/cpu.prof`
3. 手动测试 pprof: `/root/go/bin/pprof -traces build/cpu.prof`
4. 查看服务日志: `tail -f /tmp/profiler.log`

---

## Git 提交记录

```
7fbbc6e feat: 添加真实的 collapsed 格式火焰图数据API
0d81f0d refactor: 清理构建脚本，统一使用vcpkg管理依赖
bfc8245 refactor: 清理pprof依赖，使用vcpkg管理依赖，增强火焰图功能
```

---

## 下次工作重点

1. **首先解决 collapsed API 的 pprof 执行问题**
   - 这是阻塞其他功能的关键问题
   - 需要在服务运行时正确调用 pprof 工具

2. **然后修改前端使用真实的 collapsed 数据**
   - 替换硬编码的假数据
   - 实现正确的火焰图渲染

3. **最后实现完整的 symbol 解析流程**
   - 前端提取地址
   - 批量请求 symbol 接口
   - 渲染符号化的火焰图

---

创建时间: 2025-01-07
最后更新: 2025-01-07
