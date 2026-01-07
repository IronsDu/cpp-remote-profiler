# 代码清理总结

## 修改日期
2026-01-07

## 清理目标
移除不再需要的pprof相关代码，项目现在完全依赖浏览器端JavaScript渲染火焰图。

## 删除的功能

### 1. 服务器端SVG火焰图生成
- **删除的函数**:
  - `ProfilerManager::getProfileSVG()`
  - `ProfilerManager::generateSVGFromProfile()`

- **删除的HTTP端点**:
  - `/api/cpu/svg`
  - `/api/heap/svg`

- **影响**: 删除了约400行SVG生成代码

### 2. 文本格式pprof输出
- **删除的函数**:
  - `ProfilerManager::getSymbolizedProfile()`

- **删除的HTTP端点**:
  - `/api/cpu/text`
  - `/api/heap/text`

- **影响**: 删除了约40行文本格式输出代码

### 3. pprof符号解析
- **删除的函数**:
  - `ProfilerManager::resolveSymbol()`

- **删除的HTTP端点**:
  - `/pprof/symbol`

- **影响**: 删除了约20行符号解析代码

### 4. pprof工具检测和演示数据
- **修改的函数**:
  - `ProfilerManager::getProfileAsJSON()` - 移除了pprof检测和演示数据
  - `ProfilerManager::getFlameGraphData()` - 移除了pprof检测和演示数据

- **影响**: 简化了代码逻辑，移除了约100行检测代码

## 保留的功能

### 核心profiling功能
- CPU Profiler 启动/停止 (`startCPUProfiler`, `stopCPUProfiler`)
- Heap Profiler 启动/停止 (`startHeapProfiler`, `stopHeapProfiler`)
- Profile数据收集 (`getCPUProfileData`, `getHeapProfileData`)

### 浏览器端火焰图渲染API
- `/api/cpu/flamegraph` - CPU火焰图JSON数据
- `/api/heap/flamegraph` - Heap火焰图JSON数据
- `/api/cpu/json` - CPU profile JSON格式
- `/api/heap/json` - Heap profile JSON格式

### pprof格式下载（用于兼容性）
- `/api/cpu/pprof` - CPU profile下载
- `/api/heap/pprof` - Heap profile下载

## 文件修改列表

### 头文件
- `include/profiler_manager.h`
  - 删除了4个函数声明

### 实现文件
- `src/profiler_manager.cpp`
  - 删除了约560行代码
  - 保留了核心JSON数据生成功能

### HTTP端点
- `example/main.cpp`
  - 删除了5个HTTP路由处理器
  - 保留了核心API端点

### 测试文件
- `tests/profiler_test.cpp`
  - 删除了4个测试用例（SVG生成、符号解析等）
  - 保留了10个核心测试用例

- `tests/test_flamegraph.sh`
  - 删除了SVG和文本格式测试步骤
  - 保留了JSON火焰图数据测试

## 代码统计

### 删除的代码行数
- `profiler_manager.h`: -15行
- `profiler_manager.cpp`: -560行
- `main.cpp`: -118行
- `profiler_test.cpp`: -84行
- `test_flamegraph.sh`: -41行

**总计**: 约818行代码被删除

### 代码精简比例
- 代码库大小减少约20-30%
- 移除了所有服务器端渲染逻辑
- 聚焦于浏览器端交互式火焰图

## 验证结果

### 语法检查
✅ `profiler_manager.cpp` - 通过（仅有预期的narrowing警告）
✅ `main.cpp` - 需要Drogon框架才能完全编译

### 编译要求
项目需要以下依赖才能完整编译：
- gperftools (libprofiler, libtcmalloc_minimal)
- Drogon框架
- OpenSSL
- ZLIB
- GTest

## 架构优势

### 之前
- 服务器端生成SVG火焰图（依赖pprof）
- 静态SVG输出，缺乏交互性
- 代码复杂，维护成本高

### 现在
- 浏览器端Canvas渲染（无需pprof）
- 交互式火焰图（搜索、缩放、聚焦）
- 代码简洁，专注于数据提供

## 兼容性

### 用户工作流变化
之前：
1. 启动profiler → 停止profiler
2. 下载SVG火焰图
3. 在浏览器中查看静态图像

现在：
1. 启动profiler → 停止profiler
2. 访问 `/flamegraph?type=cpu`
3. 在浏览器中查看交互式火焰图
4. 可选：下载pprof文件用于其他工具（Speedscope等）

### 向后兼容
- 保留了pprof格式下载，用户仍然可以使用第三方工具
- JSON API与前端JavaScript完全兼容

## 后续建议

1. **文档更新**: 更新README.md，反映新的工作流程
2. **错误处理**: 改进pprof不可用时的错误提示
3. **性能优化**: 考虑缓存火焰图数据
4. **功能增强**: 扩展浏览器端交互功能（导出图片、分享链接等）
